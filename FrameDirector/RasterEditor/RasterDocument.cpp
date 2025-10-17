#include "RasterDocument.h"

#include <QtMath>

namespace
{
constexpr double kDefaultOpacity = 1.0;
}

RasterFrame::RasterFrame()
    : m_tile()
{
}

RasterFrame::RasterFrame(const QSize& size)
    : m_tile()
{
    resize(size);
}

void RasterFrame::resize(const QSize& size)
{
    if (size.isEmpty()) {
        m_tile = QImage();
        return;
    }

    if (m_tile.size() == size && m_tile.format() == QImage::Format_ARGB32_Premultiplied) {
        return;
    }

    QImage newImage(size, QImage::Format_ARGB32_Premultiplied);
    newImage.fill(Qt::transparent);

    if (!m_tile.isNull()) {
        QPainter painter(&newImage);
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.drawImage(QPoint(0, 0), m_tile);
    }

    m_tile = newImage;
}

void RasterFrame::clear()
{
    if (!m_tile.isNull()) {
        m_tile.fill(Qt::transparent);
    }
}

RasterLayer::RasterLayer()
    : m_name(QObject::tr("Layer"))
    , m_visible(true)
    , m_opacity(kDefaultOpacity)
    , m_blendMode(QPainter::CompositionMode_SourceOver)
    , m_offset(0.0, 0.0)
{
}

RasterLayer::RasterLayer(const QString& name, int frameCount, const QSize& canvasSize)
    : m_name(name)
    , m_visible(true)
    , m_opacity(kDefaultOpacity)
    , m_blendMode(QPainter::CompositionMode_SourceOver)
    , m_offset(0.0, 0.0)
{
    ensureFrameCount(frameCount, canvasSize);
}

void RasterLayer::setName(const QString& name)
{
    if (m_name == name) {
        return;
    }
    m_name = name;
}

void RasterLayer::setVisible(bool visible)
{
    m_visible = visible;
}

void RasterLayer::setOpacity(double opacity)
{
    m_opacity = qBound(0.0, opacity, 1.0);
}

void RasterLayer::setBlendMode(QPainter::CompositionMode mode)
{
    m_blendMode = mode;
}

void RasterLayer::setOffset(const QPointF& offset)
{
    if (m_offset == offset) {
        return;
    }
    m_offset = offset;
}

RasterFrame& RasterLayer::frameAt(int index)
{
    Q_ASSERT(index >= 0 && index < m_frames.size());
    return m_frames[index];
}

const RasterFrame& RasterLayer::frameAt(int index) const
{
    Q_ASSERT(index >= 0 && index < m_frames.size());
    return m_frames[index];
}

void RasterLayer::setFrameCount(int frameCount, const QSize& canvasSize)
{
    ensureFrameCount(frameCount, canvasSize);
}

void RasterLayer::ensureFrameCount(int frameCount, const QSize& canvasSize)
{
    if (frameCount < 0) {
        frameCount = 0;
    }

    if (m_frames.size() != frameCount) {
        QVector<RasterFrame> frames;
        frames.reserve(frameCount);
        for (int i = 0; i < frameCount; ++i) {
            if (i < m_frames.size()) {
                frames.append(m_frames.at(i));
            } else {
                frames.append(RasterFrame(canvasSize));
            }
        }
        m_frames = frames;
    }

    if (!canvasSize.isEmpty()) {
        for (RasterFrame& frame : m_frames) {
            frame.resize(canvasSize);
        }
    }
}

RasterDocument::RasterDocument(QObject* parent)
    : QObject(parent)
    , m_layers()
    , m_canvasSize(1024, 768)
    , m_frameCount(1)
    , m_activeLayer(0)
    , m_activeFrame(0)
    , m_onionSkinEnabled(true)
    , m_onionSkinBefore(1)
    , m_onionSkinAfter(1)
{
    addLayer(tr("Layer 1"));
}

void RasterDocument::setCanvasSize(const QSize& size)
{
    if (!size.isValid() || size == m_canvasSize) {
        return;
    }

    m_canvasSize = size;
    for (RasterLayer& layer : m_layers) {
        layer.setFrameCount(m_frameCount, m_canvasSize);
    }

    emit canvasSizeChanged(m_canvasSize);
    emit documentReset();
}

void RasterDocument::setFrameCount(int frameCount)
{
    if (frameCount < 1) {
        frameCount = 1;
    }

    if (m_frameCount == frameCount) {
        for (RasterLayer& layer : m_layers) {
            layer.setFrameCount(m_frameCount, m_canvasSize);
        }
        return;
    }

    m_frameCount = frameCount;

    for (RasterLayer& layer : m_layers) {
        layer.setFrameCount(m_frameCount, m_canvasSize);
    }

    clampActiveFrame();

    emit documentReset();
    emit activeFrameChanged(m_activeFrame);
}

RasterLayer& RasterDocument::layerAt(int index)
{
    Q_ASSERT(index >= 0 && index < m_layers.size());
    return m_layers[index];
}

const RasterLayer& RasterDocument::layerAt(int index) const
{
    Q_ASSERT(index >= 0 && index < m_layers.size());
    return m_layers.at(index);
}

int RasterDocument::addLayer(const QString& name)
{
    const QString layerName = name.isEmpty() ? tr("Layer %1").arg(m_layers.size() + 1) : name;
    m_layers.append(RasterLayer(layerName, m_frameCount, m_canvasSize));
    const int index = m_layers.size() - 1;
    emit layerListChanged();
    setActiveLayer(index);
    return index;
}

void RasterDocument::removeLayer(int index)
{
    if (index < 0 || index >= m_layers.size() || m_layers.size() <= 1) {
        return;
    }

    m_layers.removeAt(index);
    if (m_activeLayer >= m_layers.size()) {
        m_activeLayer = m_layers.size() - 1;
    }

    emit layerListChanged();
    emit activeLayerChanged(m_activeLayer);
}

void RasterDocument::moveLayer(int from, int to)
{
    if (from < 0 || from >= m_layers.size() || to < 0 || to >= m_layers.size() || from == to) {
        return;
    }

    m_layers.move(from, to);
    emit layerListChanged();
    setActiveLayer(to);
}

void RasterDocument::renameLayer(int index, const QString& name)
{
    if (index < 0 || index >= m_layers.size()) {
        return;
    }

    RasterLayer& layer = m_layers[index];
    if (layer.name() == name) {
        return;
    }

    layer.setName(name);
    emit layerListChanged();
    emit layerPropertyChanged(index);
}

void RasterDocument::setLayerVisible(int index, bool visible)
{
    if (index < 0 || index >= m_layers.size()) {
        return;
    }

    RasterLayer& layer = m_layers[index];
    if (layer.isVisible() == visible) {
        return;
    }

    layer.setVisible(visible);
    emit layerPropertyChanged(index);
    emit documentReset();
}

void RasterDocument::setLayerOpacity(int index, double opacity)
{
    if (index < 0 || index >= m_layers.size()) {
        return;
    }

    RasterLayer& layer = m_layers[index];
    const double clamped = qBound(0.0, opacity, 1.0);
    if (qFuzzyCompare(layer.opacity(), clamped)) {
        return;
    }

    layer.setOpacity(clamped);
    emit layerPropertyChanged(index);
    emit documentReset();
}

void RasterDocument::setLayerBlendMode(int index, QPainter::CompositionMode mode)
{
    if (index < 0 || index >= m_layers.size()) {
        return;
    }

    RasterLayer& layer = m_layers[index];
    if (layer.blendMode() == mode) {
        return;
    }

    layer.setBlendMode(mode);
    emit layerPropertyChanged(index);
    emit documentReset();
}

void RasterDocument::loadFromDescriptors(const QSize& canvasSize, const QVector<RasterLayerDescriptor>& layers, int frameCount)
{
    const int clampedFrameCount = qMax(1, frameCount);
    const QSize newCanvas = canvasSize.isValid() ? canvasSize : m_canvasSize;

    m_canvasSize = newCanvas;
    m_frameCount = clampedFrameCount;
    m_activeLayer = 0;
    m_activeFrame = 0;
    m_layers.clear();
    m_layers.reserve(layers.size());

    for (const RasterLayerDescriptor& descriptor : layers) {
        RasterLayer layer(descriptor.name, m_frameCount, m_canvasSize);
        layer.setVisible(descriptor.visible);
        layer.setOpacity(descriptor.opacity);
        layer.setBlendMode(descriptor.blendMode);
        layer.setOffset(descriptor.offset);

        if (layer.frameCount() > 0) {
            QImage& image = layer.frameAt(0).image();
            if (!descriptor.image.isNull()) {
                image = descriptor.image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
            } else {
                image.fill(Qt::transparent);
            }
        }

        m_layers.append(layer);
    }

    if (m_layers.isEmpty()) {
        m_layers.append(RasterLayer(tr("Layer 1"), m_frameCount, m_canvasSize));
    }

    clampActiveLayer();
    clampActiveFrame();

    emit canvasSizeChanged(m_canvasSize);
    emit layerListChanged();
    emit documentReset();
    emit activeLayerChanged(m_activeLayer);
    emit activeFrameChanged(m_activeFrame);
}

QVector<RasterLayerDescriptor> RasterDocument::layerDescriptors() const
{
    QVector<RasterLayerDescriptor> descriptors;
    descriptors.reserve(m_layers.size());

    for (const RasterLayer& layer : m_layers) {
        RasterLayerDescriptor descriptor;
        descriptor.name = layer.name();
        descriptor.visible = layer.isVisible();
        descriptor.opacity = layer.opacity();
        descriptor.blendMode = layer.blendMode();
        descriptor.offset = layer.offset();
        if (layer.frameCount() > 0) {
            descriptor.image = layer.frameAt(0).image();
        }
        descriptors.append(descriptor);
    }

    return descriptors;
}

QImage* RasterDocument::frameImage(int layerIndex, int frameIndex)
{
    if (layerIndex < 0 || layerIndex >= m_layers.size()) {
        return nullptr;
    }

    RasterLayer& layer = m_layers[layerIndex];
    if (frameIndex < 0 || frameIndex >= layer.frameCount()) {
        return nullptr;
    }

    return &layer.frameAt(frameIndex).image();
}

const QImage* RasterDocument::frameImage(int layerIndex, int frameIndex) const
{
    if (layerIndex < 0 || layerIndex >= m_layers.size()) {
        return nullptr;
    }

    const RasterLayer& layer = m_layers.at(layerIndex);
    if (frameIndex < 0 || frameIndex >= layer.frameCount()) {
        return nullptr;
    }

    return &layer.frameAt(frameIndex).image();
}

void RasterDocument::notifyFrameImageChanged(int layerIndex, int frameIndex, const QRect& rect)
{
    if (layerIndex < 0 || layerIndex >= m_layers.size()) {
        return;
    }

    const RasterLayer& layer = m_layers.at(layerIndex);
    if (frameIndex < 0 || frameIndex >= layer.frameCount()) {
        return;
    }

    QRect area = rect;
    if (area.isNull() || !area.isValid()) {
        area = QRect(QPoint(0, 0), m_canvasSize);
    }

    emit frameImageChanged(layerIndex, frameIndex, area);
}

void RasterDocument::setActiveLayer(int index)
{
    if (m_layers.isEmpty()) {
        m_activeLayer = -1;
        return;
    }

    if (index < 0) {
        index = 0;
    }
    if (index >= m_layers.size()) {
        index = m_layers.size() - 1;
    }

    if (m_activeLayer == index) {
        return;
    }

    m_activeLayer = index;
    emit activeLayerChanged(m_activeLayer);
}

void RasterDocument::setActiveFrame(int frameIndex)
{
    if (frameIndex < 0) {
        frameIndex = 0;
    }
    if (frameIndex >= m_frameCount) {
        frameIndex = m_frameCount - 1;
    }

    if (m_activeFrame == frameIndex) {
        return;
    }

    m_activeFrame = frameIndex;
    emit activeFrameChanged(m_activeFrame);
}


void RasterDocument::setOnionSkinEnabled(bool enabled)
{
    if (m_onionSkinEnabled == enabled) {
        return;
    }

    m_onionSkinEnabled = enabled;
    emit onionSkinSettingsChanged();
}

void RasterDocument::setOnionSkinRange(int before, int after)
{
    before = qMax(0, before);
    after = qMax(0, after);

    if (m_onionSkinBefore == before && m_onionSkinAfter == after) {
        return;
    }

    m_onionSkinBefore = before;
    m_onionSkinAfter = after;
    emit onionSkinSettingsChanged();
}

void RasterDocument::clampActiveLayer()
{
    if (m_layers.isEmpty()) {
        m_activeLayer = -1;
        return;
    }

    if (m_activeLayer < 0) {
        m_activeLayer = 0;
    } else if (m_activeLayer >= m_layers.size()) {
        m_activeLayer = m_layers.size() - 1;
    }
}

void RasterDocument::clampActiveFrame()
{
    if (m_frameCount < 1) {
        m_frameCount = 1;
    }

    if (m_activeFrame < 0) {
        m_activeFrame = 0;
    } else if (m_activeFrame >= m_frameCount) {
        m_activeFrame = m_frameCount - 1;
    }
}

