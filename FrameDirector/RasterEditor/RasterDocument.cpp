#include "RasterDocument.h"

#include <QtMath>
#include <QBuffer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QImageWriter>

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
    , m_useProjectOnionSkin(false)
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

        const int framesToCopy = qMin(layer.frameCount(), descriptor.frames.size());
        for (int frameIndex = 0; frameIndex < framesToCopy; ++frameIndex) {
            QImage frameImage = descriptor.frames.at(frameIndex);
            if (!frameImage.isNull() && frameImage.format() != QImage::Format_ARGB32_Premultiplied) {
                frameImage = frameImage.convertToFormat(QImage::Format_ARGB32_Premultiplied);
            }
            layer.frameAt(frameIndex).image() = frameImage;
        }

        if (framesToCopy == 0 && layer.frameCount() > 0) {
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
            descriptor.frames.reserve(layer.frameCount());
            for (int frameIndex = 0; frameIndex < layer.frameCount(); ++frameIndex) {
                descriptor.frames.append(layer.frameAt(frameIndex).image());
            }
            descriptor.image = descriptor.frames.first();
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

void RasterDocument::setUseProjectOnionSkin(bool enabled)
{
    if (m_useProjectOnionSkin == enabled) {
        return;
    }

    m_useProjectOnionSkin = enabled;
    emit onionSkinSettingsChanged();
}

QImage RasterDocument::flattenFrame(int frameIndex) const
{
    if (frameIndex < 0 || frameIndex >= m_frameCount) {
        return QImage();
    }

    if (!m_canvasSize.isValid() || m_canvasSize.isEmpty()) {
        return QImage();
    }

    QImage result(m_canvasSize, QImage::Format_ARGB32_Premultiplied);
    result.fill(Qt::transparent);

    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    for (const RasterLayer& layer : m_layers) {
        if (!layer.isVisible()) {
            continue;
        }

        if (frameIndex >= layer.frameCount()) {
            continue;
        }

        const QImage& source = layer.frameAt(frameIndex).image();
        if (source.isNull() || source.isEmpty()) {
            continue;
        }

        QImage image = source;
        if (image.format() != QImage::Format_ARGB32_Premultiplied) {
            image = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        }

        painter.save();
        painter.setOpacity(qBound(0.0, layer.opacity(), 1.0));
        painter.setCompositionMode(layer.blendMode());
        painter.drawImage(layer.offset(), image);
        painter.restore();
    }

    painter.end();
    return result;
}

QJsonObject RasterDocument::toJson() const
{
    QJsonObject root;
    root[QStringLiteral("canvasWidth")] = m_canvasSize.width();
    root[QStringLiteral("canvasHeight")] = m_canvasSize.height();
    root[QStringLiteral("frameCount")] = m_frameCount;
    root[QStringLiteral("activeLayer")] = m_activeLayer;
    root[QStringLiteral("activeFrame")] = m_activeFrame;
    root[QStringLiteral("onionSkinEnabled")] = m_onionSkinEnabled;
    root[QStringLiteral("onionBefore")] = m_onionSkinBefore;
    root[QStringLiteral("onionAfter")] = m_onionSkinAfter;
    root[QStringLiteral("useProjectOnion")] = m_useProjectOnionSkin;

    QJsonArray layerArray;
    for (const RasterLayer& layer : m_layers) {
        QJsonObject layerObject;
        layerObject[QStringLiteral("name")] = layer.name();
        layerObject[QStringLiteral("visible")] = layer.isVisible();
        layerObject[QStringLiteral("opacity")] = layer.opacity();
        layerObject[QStringLiteral("blendMode")] = static_cast<int>(layer.blendMode());
        layerObject[QStringLiteral("offsetX")] = layer.offset().x();
        layerObject[QStringLiteral("offsetY")] = layer.offset().y();

        QJsonArray framesArray;
        const int frameLimit = qMin(layer.frameCount(), m_frameCount);
        for (int frame = 0; frame < frameLimit; ++frame) {
            QJsonObject frameObject;
            frameObject[QStringLiteral("index")] = frame;

            const QImage& image = layer.frameAt(frame).image();
            if (!image.isNull() && !image.isEmpty()) {
                QImage exportImage = image;
                if (exportImage.format() != QImage::Format_ARGB32_Premultiplied) {
                    exportImage = exportImage.convertToFormat(QImage::Format_ARGB32_Premultiplied);
                }

                QByteArray encoded;
                QBuffer buffer(&encoded);
                buffer.open(QIODevice::WriteOnly);
                exportImage.save(&buffer, "PNG");
                frameObject[QStringLiteral("data")] = QString::fromLatin1(encoded.toBase64());
            }

            framesArray.append(frameObject);
        }

        layerObject[QStringLiteral("frames")] = framesArray;
        layerArray.append(layerObject);
    }

    root[QStringLiteral("layers")] = layerArray;
    return root;
}

bool RasterDocument::fromJson(const QJsonObject& json)
{
    if (json.isEmpty()) {
        return false;
    }

    const int width = json.value(QStringLiteral("canvasWidth")).toInt(m_canvasSize.width());
    const int height = json.value(QStringLiteral("canvasHeight")).toInt(m_canvasSize.height());
    const int frameCount = qMax(1, json.value(QStringLiteral("frameCount")).toInt(m_frameCount));

    QVector<RasterLayerDescriptor> descriptors;
    QJsonArray layerArray = json.value(QStringLiteral("layers")).toArray();
    descriptors.reserve(layerArray.size());

    for (const QJsonValue& layerValue : layerArray) {
        QJsonObject layerObject = layerValue.toObject();
        RasterLayerDescriptor descriptor;
        descriptor.name = layerObject.value(QStringLiteral("name")).toString();
        descriptor.visible = layerObject.value(QStringLiteral("visible")).toBool(true);
        descriptor.opacity = layerObject.value(QStringLiteral("opacity")).toDouble(1.0);
        descriptor.blendMode = static_cast<QPainter::CompositionMode>(
            layerObject.value(QStringLiteral("blendMode")).toInt(QPainter::CompositionMode_SourceOver));
        descriptor.offset = QPointF(layerObject.value(QStringLiteral("offsetX")).toDouble(),
            layerObject.value(QStringLiteral("offsetY")).toDouble());

        QJsonArray framesArray = layerObject.value(QStringLiteral("frames")).toArray();
        if (!framesArray.isEmpty()) {
            descriptor.frames.resize(frameCount);
            for (const QJsonValue& frameValue : framesArray) {
                QJsonObject frameObject = frameValue.toObject();
                const int index = frameObject.value(QStringLiteral("index")).toInt(-1);
                if (index < 0 || index >= frameCount) {
                    continue;
                }

                const QString encoded = frameObject.value(QStringLiteral("data")).toString();
                if (encoded.isEmpty()) {
                    continue;
                }

                QByteArray bytes = QByteArray::fromBase64(encoded.toLatin1());
                QImage image;
                image.loadFromData(bytes, "PNG");
                if (!image.isNull() && image.format() != QImage::Format_ARGB32_Premultiplied) {
                    image = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
                }
                descriptor.frames[index] = image;
            }

            if (!descriptor.frames.isEmpty()) {
                descriptor.image = descriptor.frames.constFirst();
            }
        }

        descriptors.append(descriptor);
    }

    loadFromDescriptors(QSize(width, height), descriptors, frameCount);

    m_onionSkinEnabled = json.value(QStringLiteral("onionSkinEnabled")).toBool(m_onionSkinEnabled);
    m_onionSkinBefore = json.value(QStringLiteral("onionBefore")).toInt(m_onionSkinBefore);
    m_onionSkinAfter = json.value(QStringLiteral("onionAfter")).toInt(m_onionSkinAfter);
    m_useProjectOnionSkin = json.value(QStringLiteral("useProjectOnion")).toBool(m_useProjectOnionSkin);

    setActiveLayer(json.value(QStringLiteral("activeLayer")).toInt(m_activeLayer));
    setActiveFrame(json.value(QStringLiteral("activeFrame")).toInt(m_activeFrame));

    emit onionSkinSettingsChanged();
    return true;
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

