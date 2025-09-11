#include "PSDImporter.h"

#include <QImage>
#include <QByteArray>
#include <QDebug>

// libpsd header - assumes the library is available during build
#include <libpsd/libpsd.h>

// Helper to convert PSD blend mode to Qt composition mode
static QPainter::CompositionMode convertBlendMode(const char* mode)
{
    if (!mode)
        return QPainter::CompositionMode_SourceOver;

    QByteArray key(mode, 4);
    if (key == "norm") return QPainter::CompositionMode_SourceOver;
    if (key == "mul ") return QPainter::CompositionMode_Multiply;
    if (key == "scrn") return QPainter::CompositionMode_Screen;
    if (key == "over") return QPainter::CompositionMode_Overlay;
    if (key == "dark") return QPainter::CompositionMode_Darken;
    if (key == "lite") return QPainter::CompositionMode_Lighten;
    if (key == "diff") return QPainter::CompositionMode_Difference;
    if (key == "smud") return QPainter::CompositionMode_ColorBurn;
    return QPainter::CompositionMode_SourceOver;
}

QList<LayerData> PSDImporter::importPSD(const QString& filePath)
{
    QList<LayerData> result;

    psd_context* context = psd_new();
    if (!context) {
        qWarning() << "Failed to create PSD context";
        return result;
    }

    if (psd_load(context, filePath.toUtf8().constData()) != 0) {
        qWarning() << "Failed to load PSD" << filePath;
        psd_free(context);
        return result;
    }

    psd_image* image = psd_image_load(context);
    if (!image) {
        qWarning() << "Failed to parse PSD image";
        psd_free(context);
        return result;
    }

    for (int i = 0; i < image->layer_count; ++i) {
        psd_layer_record* layerRecord = &image->layers[i];

        // Skip folders or unsupported types
        if (layerRecord->type != PSD_LAYER_RECORD_LAYER)
            continue;

        LayerData layer;
        layer.name = QString::fromUtf8(layerRecord->name);
        layer.visible = !layerRecord->flags.hidden;
        layer.opacity = layerRecord->opacity / 255.0;
        layer.blendMode = convertBlendMode(layerRecord->blend_mode);

        psd_bitmap* bmp = psd_render_layer(context, layerRecord);
        if (bmp && bmp->data) {
            QImage img(bmp->data, bmp->width, bmp->height, QImage::Format_ARGB32);
            layer.image = img.copy(); // Deep copy since psd frees its data
            psd_bitmap_free(bmp);
        }

        result.append(layer);
    }

    psd_image_free(image);
    psd_free(context);
    return result;
}

