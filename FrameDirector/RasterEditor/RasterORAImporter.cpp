#include "RasterORAImporter.h"

#include "RasterDocument.h"

#include "../Import/ZipReader.h"

#include <QBuffer>
#include <QFileInfo>
#include <QImageReader>
#include <QObject>
#include <QtGlobal>
#include <QXmlStreamReader>

namespace
{
QPainter::CompositionMode defaultMode() { return QPainter::CompositionMode_SourceOver; }

QPainter::CompositionMode parseCompositeOp(const QString& value)
{
    if (value.isEmpty()) {
        return defaultMode();
    }

    static const struct
    {
        const char* name;
        QPainter::CompositionMode mode;
    } kMappings[] = {
        { "svg:src-over", QPainter::CompositionMode_SourceOver },
        { "svg:multiply", QPainter::CompositionMode_Multiply },
        { "svg:screen", QPainter::CompositionMode_Screen },
        { "svg:overlay", QPainter::CompositionMode_Overlay },
        { "svg:darken", QPainter::CompositionMode_Darken },
        { "svg:lighten", QPainter::CompositionMode_Lighten },
        { "svg:color-dodge", QPainter::CompositionMode_ColorDodge },
        { "svg:color-burn", QPainter::CompositionMode_ColorBurn },
        { "svg:hard-light", QPainter::CompositionMode_HardLight },
        { "svg:soft-light", QPainter::CompositionMode_SoftLight },
        { "svg:difference", QPainter::CompositionMode_Difference },
        { "svg:exclusion", QPainter::CompositionMode_Exclusion },
        { "svg:src-in", QPainter::CompositionMode_SourceIn },
        { "svg:src-out", QPainter::CompositionMode_SourceOut },
        { "svg:src-atop", QPainter::CompositionMode_SourceAtop },
        { "svg:destination-over", QPainter::CompositionMode_DestinationOver },
        { "svg:destination-in", QPainter::CompositionMode_DestinationIn },
        { "svg:destination-out", QPainter::CompositionMode_DestinationOut },
        { "svg:destination-atop", QPainter::CompositionMode_DestinationAtop },
        { "svg:xor", QPainter::CompositionMode_Xor }
    };

    for (const auto& mapping : kMappings) {
        if (value.compare(QLatin1String(mapping.name), Qt::CaseInsensitive) == 0) {
            return mapping.mode;
        }
    }

    return defaultMode();
}
} // namespace

bool RasterORAImporter::importFile(const QString& filePath, RasterDocument* document, QString* error)
{
    if (!document) {
        if (error) {
            *error = QObject::tr("No document available for import.");
        }
        return false;
    }

    QSize canvasSize;
    QVector<ParsedLayer> layers;
    if (!loadArchive(filePath, layers, canvasSize, error)) {
        return false;
    }

    QVector<RasterLayerDescriptor> descriptors;
    descriptors.reserve(layers.size());
    for (const ParsedLayer& layer : layers) {
        descriptors.append(layer.descriptor);
    }

    document->loadFromDescriptors(canvasSize, descriptors, 1);
    return true;
}

QVector<RasterLayerDescriptor> RasterORAImporter::readLayers(const QString& filePath, QSize* canvasSize, QString* error)
{
    QSize size;
    QVector<ParsedLayer> layers;
    if (!loadArchive(filePath, layers, size, error)) {
        return {};
    }

    if (canvasSize) {
        *canvasSize = size;
    }

    QVector<RasterLayerDescriptor> descriptors;
    descriptors.reserve(layers.size());
    for (const ParsedLayer& layer : layers) {
        descriptors.append(layer.descriptor);
    }

    return descriptors;
}

bool RasterORAImporter::loadArchive(const QString& filePath, QVector<ParsedLayer>& layers, QSize& canvasSize, QString* error)
{
    ZipReader zip(filePath);
    if (!zip.isOpen()) {
        if (error) {
            *error = QObject::tr("Failed to open ORA archive: %1").arg(QFileInfo(filePath).fileName());
        }
        return false;
    }

    QByteArray xmlData = zip.fileData(QStringLiteral("stack.xml"));
    if (xmlData.isEmpty()) {
        if (error) {
            *error = QObject::tr("ORA archive is missing stack.xml");
        }
        return false;
    }

    QXmlStreamReader xml(xmlData);
    QVector<ParsedLayer> parsedLayers;
    QSize parsedCanvasSize;

    while (xml.readNextStartElement()) {
        if (xml.name() == QLatin1String("image")) {
            const auto attrs = xml.attributes();
            bool okW = false;
            bool okH = false;
            const int width = attrs.value(QLatin1String("w")).toInt(&okW);
            const int height = attrs.value(QLatin1String("h")).toInt(&okH);
            if (okW && okH) {
                parsedCanvasSize = QSize(width, height);
            }

            while (xml.readNextStartElement()) {
                if (xml.name() == QLatin1String("stack")) {
                    parseStack(xml, parsedLayers);
                } else {
                    xml.skipCurrentElement();
                }
            }
        } else if (xml.name() == QLatin1String("stack")) {
            parseStack(xml, parsedLayers);
        } else {
            xml.skipCurrentElement();
        }
    }

    if (xml.hasError()) {
        if (error) {
            *error = QObject::tr("Failed to parse stack.xml: %1").arg(xml.errorString());
        }
        return false;
    }

    if (!parsedCanvasSize.isValid()) {
        parsedCanvasSize = QSize(1024, 768);
    }

    for (ParsedLayer& layer : parsedLayers) {
        if (layer.source.isEmpty()) {
            continue;
        }

        const QByteArray imageData = zip.fileData(layer.source);
        if (imageData.isEmpty()) {
            continue;
        }

        QBuffer buffer;
        buffer.setData(imageData);
        if (!buffer.open(QIODevice::ReadOnly)) {
            continue;
        }

        QImageReader reader(&buffer, QByteArrayLiteral("png"));
        reader.setAutoTransform(true);
        QImage image = reader.read();
        if (image.isNull()) {
            continue;
        }

        layer.descriptor.image = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    }

    layers = parsedLayers;
    canvasSize = parsedCanvasSize;
    return true;
}

void RasterORAImporter::parseStack(QXmlStreamReader& xml, QVector<ParsedLayer>& layers, double offsetX, double offsetY)
{
    while (xml.readNextStartElement()) {
        if (xml.name() == QLatin1String("layer")) {
            const auto attrs = xml.attributes();
            ParsedLayer layer;
            layer.descriptor.name = attrs.value(QLatin1String("name")).toString();
            if (layer.descriptor.name.isEmpty()) {
                layer.descriptor.name = QObject::tr("Layer");
            }
            layer.source = attrs.value(QLatin1String("src")).toString();
            layer.descriptor.opacity = 1.0;
            bool okOpacity = false;
            const double rawOpacity = attrs.value(QLatin1String("opacity")).toDouble(&okOpacity);
            if (okOpacity) {
                layer.descriptor.opacity = qBound(0.0, rawOpacity, 1.0);
            }
            const QString visibility = attrs.value(QLatin1String("visibility")).toString();
            layer.descriptor.visible = visibility != QLatin1String("hidden");
            const double x = attrs.value(QLatin1String("x")).toDouble();
            const double y = attrs.value(QLatin1String("y")).toDouble();
            layer.descriptor.offset = QPointF(offsetX + x, offsetY + y);
            layer.descriptor.blendMode = parseCompositeOp(attrs.value(QLatin1String("composite-op")).toString());

            layers.prepend(layer);
            xml.skipCurrentElement();
        } else if (xml.name() == QLatin1String("stack")) {
            const auto attrs = xml.attributes();
            const double x = attrs.value(QLatin1String("x")).toDouble();
            const double y = attrs.value(QLatin1String("y")).toDouble();
            parseStack(xml, layers, offsetX + x, offsetY + y);
        } else {
            xml.skipCurrentElement();
        }
    }
}

