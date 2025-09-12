#pragma once

#include <QString>
#include <QPainter>
#include <QList>
#include <QHash>
#include <QGraphicsItem>

// Robust LayerData: only POD and Qt containers of POD, no QImage/QPixmap
struct LayerData {
    QString name; // Layer name
    QString uuid;
    bool visible = true;
    bool locked = false;
    double opacity = 1.0;
    QPainter::CompositionMode blendMode = QPainter::CompositionMode_SourceOver;
    QList<QGraphicsItem*> items;
    QHash<int, QList<QGraphicsItem*>> frameItems;
    QSet<QGraphicsItem*> allTimeItems;

    LayerData() = default;
    LayerData(const QString& layerName)
        : name(layerName), visible(true), locked(false), opacity(1.0),
          blendMode(QPainter::CompositionMode_SourceOver) {}

    // Factory for raster importers
    static LayerData fromRaster(const QString& name, bool visible, double opacity, QPainter::CompositionMode blendMode) {
        LayerData layer(name);
        layer.visible = visible;
        layer.opacity = opacity;
        layer.blendMode = blendMode;
        return layer;
    }
};

