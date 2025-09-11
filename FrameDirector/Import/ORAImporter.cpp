#include "ORAImporter.h"

#include <QFileInfo>
#include <QXmlStreamReader>
#include <QImage>
#include <QDebug>
#include <QBuffer>
#include <functional>
#include "ZipReader.h"

namespace {
struct LayerInfo {
    QString name;
    QString src;
    bool visible;
    double opacity;
};

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
    // Check PNG signature
    if (data.size() < 8) return false;
    static const char pngSignature[] = {'\x89', 'P', 'N', 'G', '\r', '\n', '\x1a', '\n'};
    return memcmp(data.constData(), pngSignature, 8) == 0;
}

bool tryLoadImage(QImage &image, const QByteArray &data, const QString &format = "PNG") {
    if (data.isEmpty()) return false;

    // Try loading directly first
    if (image.loadFromData(data, format.toLatin1())) {
        return true;
    }

    // If that fails, try through a QBuffer for better error handling
    QBuffer buffer;
    buffer.setData(data);
    if (!buffer.open(QIODevice::ReadOnly)) {
        return false;
    }

    return image.load(&buffer, format.toLatin1());
}
} // namespace

QList<LayerData> ORAImporter::importORA(const QString& filePath)
{
    QList<LayerData> result;

    QFileInfo fi(filePath);
    if (!fi.exists()) {
        qWarning() << "ORA file does not exist" << filePath;
        return result;
    }

    qDebug() << "Opening ORA" << filePath;
    ZipReader zip(filePath);
    if (!zip.isOpen()) {
        qWarning() << "Failed to open ORA" << filePath;
        return result;
    }

    QByteArray xmlData = zip.fileData("stack.xml");
    qDebug() << "stack.xml size" << xmlData.size();
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
    if (xml.hasError())
        qWarning() << "XML parse error" << xml.errorString() << "at line" << xml.lineNumber();

    qDebug() << "Parsed" << infos.size() << "layers from ORA";

    for (const LayerInfo& info : infos) {
        LayerData layer;
        layer.name = info.name;
        layer.opacity = info.opacity;
        layer.visible = info.visible;

        if (info.src.isEmpty()) {
            qWarning() << "Layer" << info.name << "missing source image";
            continue;
        }

        QByteArray imgData = zip.fileData(info.src);
        qDebug() << "Extracting" << info.src << "size" << imgData.size();

        if (imgData.isEmpty()) {
            qWarning() << "Failed to extract" << info.src << "from ORA" << filePath;
            continue;
        }

        // Validate PNG data before attempting to load
        if (!validatePngData(imgData)) {
            qWarning() << "Invalid PNG data in" << info.src;
            
            // Debug: dump corrupt data
            QString debugPath = QString("debug_corrupt_%1").arg(info.src.section('/', -1));
            QFile debugFile(debugPath);
            if (debugFile.open(QIODevice::WriteOnly)) {
                debugFile.write(imgData);
                debugFile.close();
                qWarning() << "Wrote corrupt PNG data to" << debugPath;
            }
            continue;
        }

        // Try to load the image with robust error handling
        if (!tryLoadImage(layer.image, imgData)) {
            qWarning() << "Failed to decode" << info.src << "in ORA" << filePath;
            continue;
        }

        if (layer.image.isNull()) {
            qWarning() << "Loaded image is null for" << info.src;
            continue;
        }

        result.append(layer);
    }

    qDebug() << "Finished ORA import with" << result.size() << "layers";
    return result;
}

