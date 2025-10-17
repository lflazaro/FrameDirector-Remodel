#pragma once

#include <QObject>
#include <QHash>
#include <QImage>
#include <QVector>

class MainWindow;

class RasterOnionSkinProvider : public QObject
{
    Q_OBJECT

public:
    explicit RasterOnionSkinProvider(MainWindow* mainWindow, QObject* parent = nullptr);

    void setLayerFilter(const QVector<int>& layers);
    QVector<int> layerFilter() const { return m_layerFilter; }

    QImage frameSnapshot(int frame) const;
    QImage frameSnapshot(int frame, const QVector<int>& layers) const;

public slots:
    void invalidate();

signals:
    void cacheInvalidated();

private:
    QString cacheKey(int frame, const QVector<int>& layers) const;
    QVector<int> normalizedLayers(const QVector<int>& layers) const;

    MainWindow* m_mainWindow;
    QVector<int> m_layerFilter;
    mutable QHash<QString, QImage> m_cache;
};

