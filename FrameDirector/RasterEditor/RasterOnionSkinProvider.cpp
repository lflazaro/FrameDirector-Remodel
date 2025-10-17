#include "RasterOnionSkinProvider.h"

#include "../MainWindow.h"

#include <QStringList>
#include <algorithm>

namespace
{
QString serializeLayers(const QVector<int>& layers)
{
    QStringList parts;
    parts.reserve(layers.size());
    for (int value : layers) {
        parts.append(QString::number(value));
    }
    return parts.join(',');
}
}

RasterOnionSkinProvider::RasterOnionSkinProvider(MainWindow* mainWindow, QObject* parent)
    : QObject(parent)
    , m_mainWindow(mainWindow)
{
}

void RasterOnionSkinProvider::setLayerFilter(const QVector<int>& layers)
{
    QVector<int> normalized = normalizedLayers(layers);
    if (normalized == m_layerFilter) {
        return;
    }

    m_layerFilter = normalized;
    invalidate();
}

QImage RasterOnionSkinProvider::frameSnapshot(int frame) const
{
    return frameSnapshot(frame, m_layerFilter);
}

QImage RasterOnionSkinProvider::frameSnapshot(int frame, const QVector<int>& layers) const
{
    if (!m_mainWindow || frame < 1) {
        return QImage();
    }

    QVector<int> normalized = normalizedLayers(layers.isEmpty() ? m_layerFilter : layers);
    const QString key = cacheKey(frame, normalized);

    auto it = m_cache.find(key);
    if (it != m_cache.end()) {
        return it.value();
    }

    QImage snapshot;
    if (normalized.isEmpty()) {
        snapshot = m_mainWindow->flattenedFrameImage(frame);
    }
    else {
        snapshot = m_mainWindow->flattenedFrameImage(frame, normalized);
    }

    if (!snapshot.isNull()) {
        m_cache.insert(key, snapshot);
    }

    return snapshot;
}

void RasterOnionSkinProvider::invalidate()
{
    m_cache.clear();
    emit cacheInvalidated();
}

QString RasterOnionSkinProvider::cacheKey(int frame, const QVector<int>& layers) const
{
    return QString::number(frame) + QLatin1Char('|') + serializeLayers(layers);
}

QVector<int> RasterOnionSkinProvider::normalizedLayers(const QVector<int>& layers) const
{
    QVector<int> normalized = layers;
    if (normalized.isEmpty()) {
        return normalized;
    }

    std::sort(normalized.begin(), normalized.end());
    normalized.erase(std::unique(normalized.begin(), normalized.end()), normalized.end());
    return normalized;
}

