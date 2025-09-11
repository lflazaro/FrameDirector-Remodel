#include "ORAImporter.h"

#include <QFileInfo>
#include <QXmlStreamReader>
#include <QImage>
#include <QDebug>
#include <private/qzipreader_p.h>

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

    struct LayerInfo {
        QString name;
        QString src;
        bool visible;
        double opacity;
    };

    QList<LayerInfo> infos;

    QXmlStreamReader xml(xmlData);
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement() && xml.name() == "layer") {
            LayerInfo info;
            auto attrs = xml.attributes();
            info.name = attrs.value("name").toString();
            info.src = attrs.value("src").toString();
            info.opacity = attrs.value("opacity").toDouble();
            QString vis = attrs.value("visibility").toString();
            info.visible = vis != "hidden";
            infos.prepend(info); // prepend so that last layer becomes top-most
        }
    }

    for (const LayerInfo& info : infos) {
        LayerData layer;
        layer.name = info.name;
        layer.opacity = info.opacity;
        layer.visible = info.visible;
        QByteArray imgData = zip.fileData(info.src);
        if (!imgData.isEmpty()) {
            layer.image.loadFromData(imgData, "PNG");
        }
        result.append(layer);
    }

    zip.close();
    return result;
}

