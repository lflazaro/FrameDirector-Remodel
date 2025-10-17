#pragma once

#include <QString>

class RasterDocument;

class ORAExporter
{
public:
    static bool exportDocument(const RasterDocument& document, const QString& filePath, QString* error = nullptr);
};
