#include "ORAImporter.h"

#include <QFileInfo>
#include <QXmlStreamReader>
#include <QImage>
#include <QDebug>
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
// specification (top-most layer last). Using readNextStartElement and
// skipCurrentElement ensures the parser doesn't get stuck on unexpected tags
// and avoids accessing invalid memory.
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
} // namespace

QList<LayerData> ORAImporter::importORA(const QString& filePath)
{
    QList<LayerData> result;

    // Ensure the file exists before constructing the ZIP reader to avoid
    // undefined behaviour on some platforms.
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
        } else {
            QByteArray imgData = zip.fileData(info.src);

            qDebug() << "Extracting" << info.src << "size" << imgData.size();
            if (imgData.isEmpty()) {
                qWarning() << "Failed to extract" << info.src << "from ORA" << filePath;
            } else if (!layer.image.loadFromData(imgData, "PNG")) {
                qWarning() << "Failed to decode" << info.src << "in ORA" << filePath;
            }
        }
        result.append(layer);
    }
    qDebug() << "Finished ORA import with" << result.size() << "layers";
    return result;
}

