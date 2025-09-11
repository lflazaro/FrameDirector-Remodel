#pragma once

#include <QImage>
#include <QPixmap>
#include <QString>
#include <QPainter>

// Generic layer container used by importers
struct LayerData {
    QString name;                           // Layer name
    QImage image;                           // Pixel data
    bool visible = true;                    // Visibility flag
    double opacity = 1.0;                   // Opacity in range 0..1
    QPainter::CompositionMode blendMode = QPainter::CompositionMode_SourceOver; // Blending mode

    // Convenience conversion
    QPixmap toPixmap() const { return QPixmap::fromImage(image); }
};

