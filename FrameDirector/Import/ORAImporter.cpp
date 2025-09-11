#include "ORAImporter.h"

#include <QFileInfo>
#include <QXmlStreamReader>
#include <QImage>
#include <QDebug>
#include <QtCore/private/qzipreader_p.h>

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
        zip.close();
        return result;
    }

    QByteArray xmlData = zip.fileData("stack.xml");
    if (xmlData.isEmpty()) {
        qWarning() << "ORA missing stack.xml";
        zip.close();
        return result;
    }

    struct LayerInfo {
        QString name;
        QString src;
        bool visible;
        double opacity;
    };

    QList<LayerInfo> infos;

    // Recursively parse <stack> elements so that layer order matches the ORA
    // specification (top-most layer last).  Using readNextStartElement and
    // skipCurrentElement ensures the parser doesn't get stuck on unexpected
    // tags and avoids accessing invalid memory.
    std::function<void(QXmlStreamReader&)> parseStack = [&](QXmlStreamReader& xml) {
        while (xml.readNextStartElement()) {
            if (xml.name() == "layer") {
                LayerInfo info;
                auto attrs = xml.attributes();
                info.name = attrs.value("name").toString();
                info.src = attrs.value("src").toString();
                info.opacity = attrs.value("opacity").toDouble();
                QString vis = attrs.value("visibility").toString();
                info.visible = vis != "hidden";
                infos.prepend(info);
                xml.skipCurrentElement();
            } else if (xml.name() == "stack") {
                parseStack(xml);
            } else {
                xml.skipCurrentElement();
            }
        }
    };

    QXmlStreamReader xml(xmlData);
    while (xml.readNextStartElement()) {
        if (xml.name() == "stack")
            parseStack(xml);
        else
            xml.skipCurrentElement();
    }
    if (xml.hasError()) {
        qWarning() << "Failed to parse stack.xml" << xml.errorString();
        zip.close();
        return result;
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

