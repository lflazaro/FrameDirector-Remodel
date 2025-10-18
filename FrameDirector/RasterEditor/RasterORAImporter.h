#pragma once

#include <QPainter>
#include <QSize>
#include <QString>
#include <QVector>

#include "RasterDocument.h"

class RasterDocument;

class RasterORAImporter
{
public:
    static bool importFile(const QString& filePath, RasterDocument* document, QString* error = nullptr);
    static QVector<RasterLayerDescriptor> readLayers(const QString& filePath, QSize* canvasSize = nullptr, QString* error = nullptr);

private:
    struct ParsedLayer
    {
        RasterLayerDescriptor descriptor;
        QString source;
    };

    static bool loadArchive(const QString& filePath, QVector<ParsedLayer>& layers, QSize& canvasSize, QString* error);
    static void parseStack(class QXmlStreamReader& xml, QVector<ParsedLayer>& layers, double offsetX = 0.0, double offsetY = 0.0);
};
