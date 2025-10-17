#pragma once

#include <QObject>
#include <QImage>
#include <QPointF>
#include <QPainter>
#include <QRect>
#include <QString>
#include <QVector>
#include <QJsonObject>
#include <QJsonArray>

class RasterFrame
{
public:
    RasterFrame();
    explicit RasterFrame(const QSize& size);

    void resize(const QSize& size);
    void clear();

    QImage& image() { return m_tile; }
    const QImage& image() const { return m_tile; }

private:
    QImage m_tile;
};

class RasterLayer
{
public:
    RasterLayer();
    RasterLayer(const QString& name, int frameCount, const QSize& canvasSize);

    const QString& name() const { return m_name; }
    void setName(const QString& name);

    bool isVisible() const { return m_visible; }
    void setVisible(bool visible);

    double opacity() const { return m_opacity; }
    void setOpacity(double opacity);

    QPainter::CompositionMode blendMode() const { return m_blendMode; }
    void setBlendMode(QPainter::CompositionMode mode);

    QPointF offset() const { return m_offset; }
    void setOffset(const QPointF& offset);

    int frameCount() const { return m_frames.size(); }
    RasterFrame& frameAt(int index);
    const RasterFrame& frameAt(int index) const;
    void setFrameCount(int frameCount, const QSize& canvasSize);

private:
    void ensureFrameCount(int frameCount, const QSize& canvasSize);

    QString m_name;
    bool m_visible;
    double m_opacity;
    QPainter::CompositionMode m_blendMode;
    QPointF m_offset;
    QVector<RasterFrame> m_frames;
};

struct RasterLayerDescriptor
{
    QString name;
    bool visible = true;
    double opacity = 1.0;
    QPainter::CompositionMode blendMode = QPainter::CompositionMode_SourceOver;
    QPointF offset;
    QImage image;
    QVector<QImage> frames;
};

class RasterDocument : public QObject
{
    Q_OBJECT

public:
    explicit RasterDocument(QObject* parent = nullptr);

    void setCanvasSize(const QSize& size);
    QSize canvasSize() const { return m_canvasSize; }

    int frameCount() const { return m_frameCount; }
    void setFrameCount(int frameCount);

    int layerCount() const { return m_layers.size(); }
    int activeLayer() const { return m_activeLayer; }
    void setActiveLayer(int index);

    int activeFrame() const { return m_activeFrame; }
    void setActiveFrame(int frameIndex);

    RasterLayer& layerAt(int index);
    const RasterLayer& layerAt(int index) const;

    int addLayer(const QString& name = QString());
    void removeLayer(int index);

    void moveLayer(int from, int to);

    void renameLayer(int index, const QString& name);
    void setLayerVisible(int index, bool visible);
    void setLayerOpacity(int index, double opacity);
    void setLayerBlendMode(int index, QPainter::CompositionMode mode);

    void loadFromDescriptors(const QSize& canvasSize, const QVector<RasterLayerDescriptor>& layers, int frameCount = 1);
    QVector<RasterLayerDescriptor> layerDescriptors() const;

    QImage* frameImage(int layerIndex, int frameIndex);
    const QImage* frameImage(int layerIndex, int frameIndex) const;

    void notifyFrameImageChanged(int layerIndex, int frameIndex, const QRect& rect = QRect());

    bool onionSkinEnabled() const { return m_onionSkinEnabled; }
    void setOnionSkinEnabled(bool enabled);

    int onionSkinBefore() const { return m_onionSkinBefore; }
    int onionSkinAfter() const { return m_onionSkinAfter; }
    void setOnionSkinRange(int before, int after);

    bool useProjectOnionSkin() const { return m_useProjectOnionSkin; }
    void setUseProjectOnionSkin(bool enabled);

    QImage flattenFrame(int frameIndex) const;

    QJsonObject toJson() const;
    bool fromJson(const QJsonObject& json);

signals:
    void documentReset();
    void layerListChanged();
    void layerPropertyChanged(int index);
    void activeLayerChanged(int index);
    void activeFrameChanged(int index);
    void frameImageChanged(int layerIndex, int frameIndex, const QRect& rect);
    void onionSkinSettingsChanged();
    void canvasSizeChanged(const QSize& size);

private:
    void clampActiveLayer();
    void clampActiveFrame();

    QVector<RasterLayer> m_layers;
    QSize m_canvasSize;
    int m_frameCount;
    int m_activeLayer;
    int m_activeFrame;
    bool m_onionSkinEnabled;
    int m_onionSkinBefore;
    int m_onionSkinAfter;
    bool m_useProjectOnionSkin;
};

