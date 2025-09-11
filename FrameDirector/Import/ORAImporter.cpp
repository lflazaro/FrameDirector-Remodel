#include "ORAImporter.h"

#include <QFileInfo>
#include <QXmlStreamReader>
#include <QImage>
#include <QDebug>
#include <6.9.0/QtCore/private/qzipreader_p.h>
#include <functional>

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
            infos.prepend(info);
            xml.skipCurrentElement();
        } else if (xml.name() == QLatin1String("stack")) {
            parseStack(xml, infos);
        } else {
            xml.skipCurrentElement();
        }
    }
}
} // namespace

QList<LayerData> ORAImporter::importORA(const QString& filePath)
{
    QList<LayerData> result;

    // Ensure the file exists before constructing the zip reader.  Passing a
    // non-existent path to QZipReader can result in undefined behaviour on
    // some platforms.
    QFileInfo fi(filePath);
    if (!fi.exists()) {
        qWarning() << "ORA file does not exist" << filePath;
        return result;
    }

    QZipReader zip(filePath);
    if (!zip.isReadable() || zip.status() != QZipReader::NoError) {
        qWarning() << "Failed to open ORA" << filePath << "status" << zip.status();
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
            xml.skipCurrentElement();
        }
    }

    for (const LayerInfo& info : infos) {
        LayerData layer;
        layer.name = info.name;
        layer.opacity = info.opacity;
        layer.visible = info.visible;
        if (info.src.isEmpty()) {
            qWarning() << "Layer" << info.name << "missing source image";
        } else {
            QByteArray imgData = zip.fileData(info.src);
            if (zip.status() != QZipReader::NoError) {
                qWarning() << "Failed to extract" << info.src << "from ORA" << filePath
                           << "status" << zip.status();
            } else if (!imgData.isEmpty()) {
                if (!layer.image.loadFromData(imgData, "PNG"))
                    qWarning() << "Failed to decode" << info.src << "in ORA" << filePath;
            }
        }
        result.append(layer);
    }

    return result;
}

