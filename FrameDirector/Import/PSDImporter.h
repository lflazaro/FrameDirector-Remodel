#pragma once

#include <QList>
#include <QString>
#include "LayerData.h"

// Importer for Adobe Photoshop (PSD) files.
// Relies on libpsd to extract layer information.
class PSDImporter {
public:
    // Reads the PSD file at the given path and returns the layers
    // in drawing order (bottom to top).
    static QList<LayerData> importPSD(const QString& filePath);
};

