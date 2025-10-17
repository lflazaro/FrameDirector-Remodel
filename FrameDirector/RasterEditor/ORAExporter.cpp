#include "ORAExporter.h"

#include "RasterDocument.h"

#include "../Import/ZipWriter.h"

#include <QBuffer>
#include <QImage>
#include <QImageWriter>
#include <QPainter>
#include <QObject>
#include <QVector>
#include <QXmlStreamWriter>

namespace
{
struct BlendModeMapping
{
    QPainter::CompositionMode mode;
    const char* name;
};

constexpr BlendModeMapping kBlendModeMappings[] = {
    { QPainter::CompositionMode_SourceOver, "svg:src-over" },
    { QPainter::CompositionMode_Multiply, "svg:multiply" },
    { QPainter::CompositionMode_Screen, "svg:screen" },
    { QPainter::CompositionMode_Overlay, "svg:overlay" },
    { QPainter::CompositionMode_Darken, "svg:darken" },
    { QPainter::CompositionMode_Lighten, "svg:lighten" },
    { QPainter::CompositionMode_ColorDodge, "svg:color-dodge" },
    { QPainter::CompositionMode_ColorBurn, "svg:color-burn" },
    { QPainter::CompositionMode_HardLight, "svg:hard-light" },
    { QPainter::CompositionMode_SoftLight, "svg:soft-light" },
    { QPainter::CompositionMode_Difference, "svg:difference" },
    { QPainter::CompositionMode_Exclusion, "svg:exclusion" },
    { QPainter::CompositionMode_SourceIn, "svg:src-in" },
    { QPainter::CompositionMode_SourceOut, "svg:src-out" },
    { QPainter::CompositionMode_SourceAtop, "svg:src-atop" },
    { QPainter::CompositionMode_DestinationOver, "svg:destination-over" },
    { QPainter::CompositionMode_DestinationIn, "svg:destination-in" },
    { QPainter::CompositionMode_DestinationOut, "svg:destination-out" },
    { QPainter::CompositionMode_DestinationAtop, "svg:destination-atop" },
    { QPainter::CompositionMode_Xor, "svg:xor" }
};

QString blendModeName(QPainter::CompositionMode mode)
{
    for (const auto& mapping : kBlendModeMappings) {
        if (mapping.mode == mode) {
            return QString::fromLatin1(mapping.name);
        }
    }
    return QStringLiteral("svg:src-over");
}
}

bool ORAExporter::exportDocument(const RasterDocument& document, const QString& filePath, QString* error)
{
    QVector<RasterLayerDescriptor> layers = document.layerDescriptors();

    ZipWriter zip(filePath);
    if (!zip.isOpen()) {
        if (error) {
            *error = QObject::tr("Unable to open ORA file for writing: %1").arg(filePath);
        }
        return false;
    }

    QVector<QString> layerSources(layers.size());

    for (int index = 0; index < layers.size(); ++index) {
        const RasterLayerDescriptor& layer = layers.at(index);
        const QString fileName = QStringLiteral("data/layer%1.png").arg(index, 4, 10, QChar('0'));
        QImage sourceImage = layer.image;
        if (sourceImage.isNull()) {
            sourceImage = QImage(document.canvasSize(), QImage::Format_ARGB32_Premultiplied);
            sourceImage.fill(Qt::transparent);
        } else if (sourceImage.format() != QImage::Format_ARGB32_Premultiplied) {
            sourceImage = sourceImage.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        }

        QBuffer buffer;
        buffer.open(QIODevice::WriteOnly);
        QImageWriter writer(&buffer, QByteArrayLiteral("png"));
        writer.setCompression(9);
        if (!writer.write(sourceImage)) {
            if (error) {
                *error = QObject::tr("Failed to encode layer %1: %2").arg(layer.name, writer.errorString());
            }
            return false;
        }

        if (!zip.addFile(fileName, buffer.data())) {
            if (error) {
                *error = QObject::tr("Failed to store layer image: %1").arg(fileName);
            }
            return false;
        }

        layerSources[index] = fileName;
    }

    QByteArray xmlBuffer;
    QXmlStreamWriter xml(&xmlBuffer);
    xml.setAutoFormatting(true);
    xml.writeStartDocument();
    xml.writeStartElement(QStringLiteral("image"));
    xml.writeAttribute(QStringLiteral("w"), QString::number(document.canvasSize().width()));
    xml.writeAttribute(QStringLiteral("h"), QString::number(document.canvasSize().height()));

    xml.writeStartElement(QStringLiteral("stack"));
    xml.writeAttribute(QStringLiteral("name"), QStringLiteral("root"));

    for (int index = layers.size() - 1; index >= 0; --index) {
        const RasterLayerDescriptor& layer = layers.at(index);
        xml.writeStartElement(QStringLiteral("layer"));
        xml.writeAttribute(QStringLiteral("name"), layer.name);
        xml.writeAttribute(QStringLiteral("src"), layerSources.value(index));
        xml.writeAttribute(QStringLiteral("opacity"), QString::number(layer.opacity, 'f', 3));
        xml.writeAttribute(QStringLiteral("visibility"), layer.visible ? QStringLiteral("visible") : QStringLiteral("hidden"));
        xml.writeAttribute(QStringLiteral("composite-op"), blendModeName(layer.blendMode));
        xml.writeAttribute(QStringLiteral("x"), QString::number(layer.offset.x()));
        xml.writeAttribute(QStringLiteral("y"), QString::number(layer.offset.y()));
        xml.writeEndElement();
    }

    xml.writeEndElement();
    xml.writeEndElement();
    xml.writeEndDocument();

    if (!zip.addFile(QStringLiteral("stack.xml"), xmlBuffer)) {
        if (error) {
            *error = QObject::tr("Failed to write stack.xml");
        }
        return false;
    }

    if (!zip.close()) {
        if (error) {
            *error = QObject::tr("Failed to finalize ORA archive");
        }
        return false;
    }

    return true;
}
