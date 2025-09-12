/* 
Pseudocódigo (plan detallado):
1. Validar que la ruta del archivo exista; si no existe, registrar advertencia y devolver lista vacía.
2. Intentar cargar solo la sección de capas con psd_image_load_layer().
   - Si falla, liberar contexto si existe y reintentar con psd_image_load() (carga completa).
   - Si la segunda carga falla o el contexto es nulo, registrar advertencia, liberar y devolver lista vacía.
3. Verificar que context->layer_count > 0; si no, advertir, liberar y devolver lista vacía.
4. Iterar sobre context->layer_records:
   - Omitir carpetas (folder layers).
   - Rellenar LayerData: nombre (preferir unicode), visible, opacity, blendMode.
   - Si hay image_data válida, construir QImage apuntando a los datos ARGB4 y hacer copia profunda.
   - Añadir LayerData al resultado.
5. Liberar el contexto y devolver la lista de capas.
Notas:
- Corregir error de sintaxis C1075 añadiendo llave de cierre faltante para el bloque if que maneja la re-intentada carga.
- Mantener compatibilidad con C++14 y API de libpsd/QT usada.
*/

#include "PSDImporter.h"

#include <QImage>
#include <QByteArray>
#include <QDebug>
#include <QFile>
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

    // Load only the layer information from the PSD.  Older versions of libpsd
    // occasionally fail when asked to load just the layer section.  In that
    // situation fall back to loading the full image which provides the same
    // layer data but also parses additional sections (merged image, thumbnails,
    // EXIF, ...).  The extra information is ignored but allows valid files to
    // be imported instead of being rejected outright.
    psd_context* context = nullptr;
    // libpsd expects a path encoded for the local filesystem (typically the
    // current locale's 8-bit encoding on Windows).  Passing UTF-8 here causes
    // file-open failures when the path contains non-ASCII characters.  Use
    // QFile::encodeName to obtain a correctly encoded byte array.
    QByteArray nativePath = QFile::encodeName(filePath);
    qDebug() << "Attempting to load PSD layers from" << filePath;
    psd_status status = psd_image_load_layer(&context,
        const_cast<psd_char*>(nativePath.constData()));
    qDebug() << "psd_image_load_layer returned" << status << "context" << context;
    // Some libpsd versions return an error status but still produce a usable
    // context containing partial layer information.  Only retry with a full
    // load when no context is returned at all.
    if (!context) {
        qWarning() << "psd_image_load_layer failed; retrying with psd_image_load";
        status = psd_image_load(&context,
                                const_cast<psd_char*>(nativePath.constData()));
        qDebug() << "psd_image_load returned" << status << "context" << context;
    }
    // If loading produced no context the PSD cannot be imported.  Otherwise,
    // proceed even when libpsd reports recoverable errors (for example unknown
    // blend mode signatures) as the layer data may still be usable.
    if (!context) {
        qWarning() << "Failed to load PSD layers:" << filePath
                   << "status:" << status;
        return result;
    }
    if (status != psd_status_done &&
        status != psd_status_invalid_blending_channels &&
        status != psd_status_blend_mode_signature_error &&
        status != psd_status_unsupport_blend_mode &&
        status != psd_status_additional_layer_signature_error) {
        qWarning() << "PSD load returned status" << status << "for" << filePath;
    }

    qDebug() << "PSD layer count" << context->layer_count;
    if (context->layer_count <= 0) {
        qWarning() << "PSD contains no layers" << filePath;
        psd_image_free(context);
        return result;
    }

    // context->layer_count and context->layer_records are provided by libpsd
    for (int i = 0; i < context->layer_count; ++i) {
        psd_layer_record* layerRecord = &context->layer_records[i];

        // Skip folder layers (the enum defines folder type)
        if (layerRecord->layer_type == psd_layer_type_folder) {
            qDebug() << "Skipping folder layer" << i;
            continue;
        }

        LayerData layer;
        // Prefer unicode name if available, otherwise the Pascal name field
        if (layerRecord->unicode_name_length > 0 && layerRecord->unicode_name) {
            // unicode_name is psd_ushort* (UTF-16-like). Use the char16_t* overload of QString::fromUtf16:
            layer.name = QString::fromUtf16(reinterpret_cast<const char16_t*>(layerRecord->unicode_name),
                                            layerRecord->unicode_name_length);
        } else {
            layer.name = QString::fromUtf8((const char*)layerRecord->layer_name);
        }
        qDebug() << "Processing layer" << i << layer.name
                 << "type" << layerRecord->layer_type
                 << "size" << layerRecord->width << "x" << layerRecord->height;

        layer.visible = layerRecord->visible ? true : false;
        layer.opacity = static_cast<double>(layerRecord->opacity) / 255.0;
        layer.blendMode = convertBlendModeFromEnum(layerRecord->blend_mode);

        // If layer has ARGB pixel buffer, copy into QImage.
        if (layerRecord->image_data != NULL && layerRecord->width > 0 && layerRecord->height > 0) {
            // psd_argb_color is unsigned int (AARRGGBB). QImage::Format_ARGB32 expects 0xAARRGGBB on little-endian.
            const uchar* data = reinterpret_cast<const uchar*>(layerRecord->image_data);
            int bytesPerLine = layerRecord->width * 4;
            QImage img(data, layerRecord->width, layerRecord->height, bytesPerLine, QImage::Format_ARGB32);
            // No longer store image in LayerData
            // QImage imageCopy = img.copy();
            // If you need to keep the image, store externally or return a tuple
            qDebug() << "Layer" << i << "image bytes" << bytesPerLine * layerRecord->height;
        } else {
            qDebug() << "Layer" << i << "has no image data";
        }

        result.append(layer);
    }

    psd_image_free(context);
    return result;
}
