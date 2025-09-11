#pragma once

#include <QList>
#include <QString>
#include "LayerData.h"

// Importer for OpenRaster (.ora) image files.
class ORAImporter {
public:
    // Reads the ORA file at the given path and returns layers
    // in drawing order (bottom to top).
    static QList<LayerData> importORA(const QString& filePath);
};

