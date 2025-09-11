#include "PSDImporter.h"

#include <QImage>
#include <QByteArray>
#include <QDebug>
#include <QFileInfo>
#include <third_party/include/libpsd.h>

// Helper to convert libpsd blend-mode enum to Qt composition mode
static QPainter::CompositionMode convertBlendModeFromEnum(int mode)
{
    switch (mode) {
    case psd_blend_mode_normal: return QPainter::CompositionMode_SourceOver;
    case psd_blend_mode_multiply: return QPainter::CompositionMode_Multiply;
    case psd_blend_mode_screen: return QPainter::CompositionMode_Screen;
    case psd_blend_mode_overlay: return QPainter::CompositionMode_Overlay;
    case psd_blend_mode_darken: return QPainter::CompositionMode_Darken;
    case psd_blend_mode_lighten: return QPainter::CompositionMode_Lighten;
    case psd_blend_mode_difference: return QPainter::CompositionMode_Difference;
    case psd_blend_mode_exclusion: return QPainter::CompositionMode_ColorBurn; // best-effort mapping
    default: return QPainter::CompositionMode_SourceOver;
    }
}

QList<LayerData> PSDImporter::importPSD(const QString& filePath)
{
    QList<LayerData> result;

    // Avoid passing a non-existent path to libpsd.  On some platforms the
    // loader can crash when given an invalid filename.
    QFileInfo fi(filePath);
    if (!fi.exists()) {
        qWarning() << "PSD file does not exist" << filePath;
        return result;
    }

    // Load only the layer information from the PSD.  Using psd_image_load() would
    // attempt to parse additional sections (merged image, thumbnails, EXIF, ...)
    // and fail for perfectly valid files when those features are unsupported by
    // the bundled libpsd.  psd_image_load_layer() focuses on layer and mask data
    // which is all we require.
    psd_context* context = nullptr;
    psd_status status = psd_image_load_layer(&context,
        const_cast<psd_char*>(filePath.toUtf8().constData()));
    if (status != psd_status_done || !context) {
        qWarning() << "Failed to load PSD layers:" << filePath << "status:" << status;
        if (context)
            psd_image_free(context);
        return result;
    }

    if (context->layer_count <= 0) {
        qWarning() << "PSD contains no layers" << filePath;
        psd_image_free(context);
        return result;
    }

    // context->layer_count and context->layer_records are provided by this libpsd
    for (int i = 0; i < context->layer_count; ++i) {
        psd_layer_record* layerRecord = &context->layer_records[i];

        // Skip folder layers (the enum defines folder type)
        if (layerRecord->layer_type == psd_layer_type_folder)
            continue;

        LayerData layer;
        // Prefer unicode name if available, otherwise the Pascal name field
        if (layerRecord->unicode_name_length > 0 && layerRecord->unicode_name) {
            // unicode_name is psd_ushort* (UTF-16-like). Use the char16_t* overload of QString::fromUtf16:
            layer.name = QString::fromUtf16(reinterpret_cast<const char16_t*>(layerRecord->unicode_name),
                                            layerRecord->unicode_name_length);
        } else {
            layer.name = QString::fromUtf8((const char*)layerRecord->layer_name);
        }

        layer.visible = layerRecord->visible ? true : false;
        layer.opacity = static_cast<double>(layerRecord->opacity) / 255.0;
        layer.blendMode = convertBlendModeFromEnum(layerRecord->blend_mode);

        // If layer has ARGB pixel buffer, copy into QImage.
        if (layerRecord->image_data != NULL && layerRecord->width > 0 && layerRecord->height > 0) {
            // psd_argb_color is unsigned int (AARRGGBB). QImage::Format_ARGB32 expects 0xAARRGGBB on little-endian.
            const uchar* data = reinterpret_cast<const uchar*>(layerRecord->image_data);
            int bytesPerLine = layerRecord->width * 4;
            QImage img(data, layerRecord->width, layerRecord->height, bytesPerLine, QImage::Format_ARGB32);
            layer.image = img.copy(); // deep copy because libpsd will free context later
        }

        result.append(layer);
    }

    psd_image_free(context);
    return result;
}
