#include "ORAImporter.h"

#include <QFileInfo>
#include <QXmlStreamReader>
#include <QImage>
#include <QImageReader>
#include <QDebug>
#include <QBuffer>
#include <QFile>
#include <QGraphicsPixmapItem>
#include <functional>
#include "ZipReader.h"

namespace {
struct LayerInfo {
    QString name;
    QString src;
    bool visible;
    double opacity; 
};

// Recursively parse <stack> elements so that layer order matches the ORA
// specification (top-most layer last).
void parseStack(QXmlStreamReader &xml, QList<LayerInfo> &infos) {
    while (xml.readNextStartElement()) {
        if (xml.name() == QLatin1String("layer")) {
            LayerInfo info;
            auto attrs = xml.attributes();
            info.name = attrs.value("name").toString();
            info.src = attrs.value("src").toString();
            info.opacity = attrs.value("opacity").toDouble();
            QString vis = attrs.value("visibility").toString();
            info.visible = vis != QLatin1String("hidden");
            qDebug() << "Parsed layer entry" << info.name << "src" << info.src
                     << "opacity" << info.opacity << "visible" << info.visible;
            infos.prepend(info);
            xml.skipCurrentElement();
        } else if (xml.name() == QLatin1String("stack")) {
            parseStack(xml, infos);
        } else {
            qDebug() << "Skipping unexpected tag" << xml.name();
            xml.skipCurrentElement();
        }
    }
}

bool validatePngData(const QByteArray &data) {
    if (data.size() < 8) return false;
    static const char pngSignature[] = {'\x89', 'P', 'N', 'G', '\r', '\n', '\x1a', '\n'};
    return memcmp(data.constData(), pngSignature, 8) == 0;
}
} // namespace

QList<std::pair<LayerData, QImage>> importORAWithImages(const QString& filePath)
{
    QList<std::pair<LayerData, QImage>> result;

    QFileInfo fi(filePath);
    if (!fi.exists()) {
        qWarning() << "ORA file does not exist" << filePath;
        return result;
    }

    ZipReader zip(filePath);
    if (!zip.isOpen()) {
        qWarning() << "Failed to open ORA" << filePath;
        return result;
    }

    QByteArray xmlData = zip.fileData("stack.xml");
    if (xmlData.isEmpty()) {
        qWarning() << "ORA missing stack.xml";
        return result;
    }

    QList<LayerInfo> infos;
    QXmlStreamReader xml(xmlData);
    while (xml.readNextStartElement()) {
        if (xml.name() == QLatin1String("image")) {
            while (xml.readNextStartElement()) {
                if (xml.name() == QLatin1String("stack"))
                    parseStack(xml, infos);
                else
                    xml.skipCurrentElement();
            }
        } else if (xml.name() == QLatin1String("stack")) {
            parseStack(xml, infos);
        } else {
            qDebug() << "Skipping unexpected root tag" << xml.name();
            xml.skipCurrentElement();
        }
    }

    for (const LayerInfo& info : infos) {
        LayerData layer = LayerData::fromRaster(info.name, info.visible,
                                               info.opacity,
                                               QPainter::CompositionMode_SourceOver);
        QImage image;
        if (!info.src.isEmpty()) {
            QByteArray imgData = zip.fileData(info.src);
            if (!imgData.isEmpty() && validatePngData(imgData)) {
                QBuffer buffer;
                buffer.setData(imgData);
                if (buffer.open(QIODevice::ReadOnly)) {
                    QImageReader reader(&buffer);
                    reader.setAutoTransform(true);
                    QImage img = reader.read();
                    if (!img.isNull() && reader.error() == 0) {
                        image = img.copy();
                        // Convert the decoded image into a graphics item so the
                        // layer has something to display when added to the scene.
                        QGraphicsPixmapItem *item =
                            new QGraphicsPixmapItem(QPixmap::fromImage(image));
                        layer.items.append(item);
                    }
                }
            }
        }
        result.append(std::make_pair(layer, image));
    }
    return result;
}

QList<LayerData> ORAImporter::importORA(const QString& filePath)
{
    QList<LayerData> layers;
    auto pairs = importORAWithImages(filePath);
    for (const auto& pair : pairs) {
        layers.append(pair.first);
    }
    return layers;
}

