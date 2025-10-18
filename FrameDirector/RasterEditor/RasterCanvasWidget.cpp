#include "RasterCanvasWidget.h"

#include "RasterDocument.h"
#include "RasterOnionSkinProvider.h"
#include "RasterTools.h"

#include <QCursor>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QSizePolicy>
#include <QtMath>
#include <QtGlobal>

namespace
{
constexpr int kCheckerSize = 16;
constexpr qreal kDefaultZoom = 1.0;

QColor beforeOnionTint()
{
    return QColor(255, 120, 120, 128);
}

QColor afterOnionTint()
{
    return QColor(120, 180, 255, 128);
}

QPointF eventPosition(const QMouseEvent* event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return event->position();
#else
    return event->localPos();
#endif
}
}

RasterCanvasWidget::RasterCanvasWidget(QWidget* parent)
    : QWidget(parent)
    , m_document(nullptr)
    , m_activeTool(nullptr)
    , m_backgroundColor(Qt::white)
    , m_zoomFactor(kDefaultZoom)
    , m_mouseDown(false)
    , m_lastCanvasPosition()
{
    setAttribute(Qt::WA_OpaquePaintEvent);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::CrossCursor);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void RasterCanvasWidget::setDocument(RasterDocument* document)
{
    if (m_document == document) {
        return;
    }

    if (m_document) {
        disconnect(m_document, nullptr, this, nullptr);
    }

    m_document = document;

    if (m_document) {
        connect(m_document, &RasterDocument::frameImageChanged, this, &RasterCanvasWidget::onDocumentChanged);
        connect(m_document, &RasterDocument::layerListChanged, this, &RasterCanvasWidget::onDocumentChanged);
        connect(m_document, &RasterDocument::layerPropertyChanged, this, &RasterCanvasWidget::onDocumentChanged);
        connect(m_document, &RasterDocument::activeFrameChanged, this, &RasterCanvasWidget::onDocumentChanged);
        connect(m_document, &RasterDocument::activeLayerChanged, this, &RasterCanvasWidget::onDocumentChanged);
        connect(m_document, &RasterDocument::onionSkinSettingsChanged, this, &RasterCanvasWidget::onDocumentChanged);
        connect(m_document, &RasterDocument::documentReset, this, &RasterCanvasWidget::onDocumentChanged);
        connect(m_document, &RasterDocument::canvasSizeChanged, this, &RasterCanvasWidget::onDocumentChanged);
    }

    update();
}

void RasterCanvasWidget::setActiveTool(RasterTool* tool)
{
    if (m_activeTool == tool) {
        return;
    }

    m_activeTool = tool;
}

void RasterCanvasWidget::setOnionSkinProvider(RasterOnionSkinProvider* provider)
{
    if (m_onionSkinProvider == provider) {
        return;
    }

    if (!m_onionSkinProvider.isNull()) {
        disconnect(m_onionSkinProvider, nullptr, this, nullptr);
    }

    m_onionSkinProvider = provider;

    if (!m_onionSkinProvider.isNull()) {
        connect(m_onionSkinProvider, &RasterOnionSkinProvider::cacheInvalidated,
                this, &RasterCanvasWidget::onDocumentChanged);
    }

    update();
}

void RasterCanvasWidget::setBackgroundColor(const QColor& color)
{
    if (m_backgroundColor == color) {
        return;
    }

    m_backgroundColor = color;
    update();
}

void RasterCanvasWidget::setZoomFactor(qreal zoom)
{
    const qreal clamped = qMax<qreal>(zoom, 0.1);
    if (qFuzzyCompare(m_zoomFactor, clamped)) {
        return;
    }

    m_zoomFactor = clamped;
    update();
}

void RasterCanvasWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.fillRect(rect(), palette().window());

    if (!m_document || m_document->canvasSize().isEmpty()) {
        return;
    }

    const QRectF canvasRect = canvasRectInWidget();

    painter.save();
    painter.translate(canvasRect.topLeft());
    painter.scale(m_zoomFactor, m_zoomFactor);

    drawCheckerboard(painter, m_document->canvasSize());
    drawFrameStack(painter);

    painter.restore();

    painter.setPen(QPen(Qt::black, 1));
    painter.drawRect(canvasRect);
}

void RasterCanvasWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    update();
}

void RasterCanvasWidget::mousePressEvent(QMouseEvent* event)
{
    if (!m_document || !m_activeTool || event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    const QPointF canvasPos = mapToCanvas(eventPosition(event));
    if (!isInsideCanvas(canvasPos)) {
        QWidget::mousePressEvent(event);
        return;
    }

    if (m_activeTool->isStrokeTool()) {
        m_activeTool->beginStroke(m_document, m_document->activeLayer(), m_document->activeFrame(), canvasPos);
        m_mouseDown = true;
        m_lastCanvasPosition = canvasPos;
        if (m_document) {
            m_document->notifyFrameImageChanged(m_document->activeLayer(), m_document->activeFrame(), m_activeTool->dirtyRect());
        }
        update();
    } else {
        m_activeTool->applyClick(m_document, m_document->activeLayer(), m_document->activeFrame(), canvasPos);
        if (m_document) {
            m_document->notifyFrameImageChanged(m_document->activeLayer(), m_document->activeFrame(), m_activeTool->dirtyRect());
        }
        update();
    }

    event->accept();
}

void RasterCanvasWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (!m_mouseDown || !m_document || !m_activeTool || !m_activeTool->isStrokeTool()) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    const QPointF canvasPos = mapToCanvas(eventPosition(event));
    if (!isInsideCanvas(canvasPos)) {
        return;
    }

    m_activeTool->strokeTo(canvasPos);
    m_lastCanvasPosition = canvasPos;

    if (m_document) {
        m_document->notifyFrameImageChanged(m_document->activeLayer(), m_document->activeFrame(), m_activeTool->dirtyRect());
    }

    update();
    event->accept();
}

void RasterCanvasWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (!m_mouseDown || event->button() != Qt::LeftButton) {
        QWidget::mouseReleaseEvent(event);
        return;
    }

    m_mouseDown = false;

    if (m_document && m_activeTool && m_activeTool->isStrokeTool()) {
        const QPointF canvasPos = mapToCanvas(eventPosition(event));
        if (isInsideCanvas(canvasPos)) {
            m_activeTool->strokeTo(canvasPos);
        }
        m_activeTool->endStroke();
        m_document->notifyFrameImageChanged(m_document->activeLayer(), m_document->activeFrame(), m_activeTool->dirtyRect());
        update();
    }

    event->accept();
}

void RasterCanvasWidget::leaveEvent(QEvent* event)
{
    if (m_mouseDown && m_activeTool && m_activeTool->isStrokeTool()) {
        m_activeTool->endStroke();
        m_mouseDown = false;
        if (m_document) {
            m_document->notifyFrameImageChanged(m_document->activeLayer(), m_document->activeFrame(), m_activeTool->dirtyRect());
        }
        update();
    }

    QWidget::leaveEvent(event);
}

void RasterCanvasWidget::onDocumentChanged()
{
    update();
}

QRectF RasterCanvasWidget::canvasRectInWidget() const
{
    if (!m_document || m_document->canvasSize().isEmpty()) {
        return QRectF();
    }

    const QSize size = m_document->canvasSize();
    const qreal widthScaled = size.width() * m_zoomFactor;
    const qreal heightScaled = size.height() * m_zoomFactor;
    const qreal x = (width() - widthScaled) / 2.0;
    const qreal y = (height() - heightScaled) / 2.0;
    return QRectF(x, y, widthScaled, heightScaled);
}

QPointF RasterCanvasWidget::mapToCanvas(const QPointF& pos) const
{
    const QRectF canvasRect = canvasRectInWidget();
    if (canvasRect.isNull()) {
        return QPointF();
    }

    const QPointF delta = pos - canvasRect.topLeft();
    return QPointF(delta.x() / m_zoomFactor, delta.y() / m_zoomFactor);
}

bool RasterCanvasWidget::isInsideCanvas(const QPointF& canvasPos) const
{
    if (!m_document || m_document->canvasSize().isEmpty()) {
        return false;
    }

    const QRectF bounds(QPointF(0.0, 0.0), QSizeF(m_document->canvasSize()));
    return bounds.contains(canvasPos);
}

void RasterCanvasWidget::drawCheckerboard(QPainter& painter, const QSize& size)
{
    painter.save();
    painter.fillRect(QRect(QPoint(0, 0), size), m_backgroundColor);

    QColor light(245, 245, 245);
    QColor dark(220, 220, 220);

    for (int y = 0; y < size.height(); y += kCheckerSize) {
        for (int x = 0; x < size.width(); x += kCheckerSize) {
            const bool even = ((x / kCheckerSize) + (y / kCheckerSize)) % 2 == 0;
            painter.fillRect(QRect(x, y, kCheckerSize, kCheckerSize), even ? light : dark);
        }
    }

    painter.restore();
}

void RasterCanvasWidget::drawFrameStack(QPainter& painter)
{
    if (!m_document) {
        return;
    }

    const int activeFrame = m_document->activeFrame();

    if (m_document->onionSkinEnabled()) {
        if (m_document->useProjectOnionSkin() && !m_onionSkinProvider.isNull()) {
            drawProjectOnionFrames(painter, activeFrame);
        }
        drawDocumentOnionFrames(painter, activeFrame);
    }

    drawFrameComposite(painter, activeFrame, 1.0, QColor());
}

void RasterCanvasWidget::drawFrameComposite(QPainter& painter, int frameIndex, qreal opacity, const QColor& tint)
{
    if (!m_document || frameIndex < 0 || frameIndex >= m_document->frameCount()) {
        return;
    }

    const bool applyTint = tint.isValid();

    for (int layerIndex = 0; layerIndex < m_document->layerCount(); ++layerIndex) {
        const RasterLayer& layer = m_document->layerAt(layerIndex);
        if (!layer.isVisible()) {
            continue;
        }

        const QImage* image = m_document->frameImage(layerIndex, frameIndex);
        if (!image || image->isNull()) {
            continue;
        }

        painter.save();
        painter.setOpacity(opacity * layer.opacity());
        painter.setCompositionMode(layer.blendMode());
        const QPointF origin = layer.offset();
        painter.drawImage(origin, *image);

        if (applyTint) {
            painter.setCompositionMode(QPainter::CompositionMode_SourceAtop);
            QColor overlay = tint;
            overlay.setAlphaF(qBound(0.0, overlay.alphaF() * opacity, 1.0));
            painter.fillRect(QRectF(origin, QSizeF(image->size())), overlay);
        }

        painter.restore();
    }
}

void RasterCanvasWidget::drawDocumentOnionFrames(QPainter& painter, int activeFrame)
{
    if (!m_document) {
        return;
    }

    for (int offset = m_document->onionSkinBefore(); offset >= 1; --offset) {
        drawFrameComposite(painter, activeFrame - offset, 0.25, beforeOnionTint());
    }
    for (int offset = 1; offset <= m_document->onionSkinAfter(); ++offset) {
        drawFrameComposite(painter, activeFrame + offset, 0.25, afterOnionTint());
    }
}

void RasterCanvasWidget::drawProjectOnionFrames(QPainter& painter, int activeFrame)
{
    if (!m_document || m_onionSkinProvider.isNull()) {
        return;
    }

    const QSize canvasSize = m_document->canvasSize();
    if (canvasSize.isEmpty()) {
        return;
    }

    const QRectF canvasRect(QPointF(0, 0), QSizeF(canvasSize));
    const int timelineFrame = activeFrame + 1;
    const int beforeCount = m_document->onionSkinBefore();
    const int afterCount = m_document->onionSkinAfter();
    const qreal baseOpacity = 0.25;

    for (int offset = 1; offset <= beforeCount; ++offset) {
        const int frameNumber = timelineFrame - offset;
        if (frameNumber < 1) {
            break;
        }

        const QImage snapshot = m_onionSkinProvider->frameSnapshot(frameNumber);
        if (snapshot.isNull()) {
            continue;
        }

        const qreal factor = baseOpacity * (beforeCount - offset + 1) / qMax(1, beforeCount);
        painter.save();
        painter.setOpacity(factor);
        painter.drawImage(canvasRect, snapshot);
        painter.setCompositionMode(QPainter::CompositionMode_SourceAtop);
        QColor overlay = beforeOnionTint();
        overlay.setAlphaF(qBound(0.0, overlay.alphaF() * factor, 1.0));
        painter.fillRect(canvasRect, overlay);
        painter.restore();
    }

    for (int offset = 1; offset <= afterCount; ++offset) {
        const int frameNumber = timelineFrame + offset;
        if (frameNumber < 1) {
            continue;
        }

        const QImage snapshot = m_onionSkinProvider->frameSnapshot(frameNumber);
        if (snapshot.isNull()) {
            continue;
        }

        const qreal factor = baseOpacity * (afterCount - offset + 1) / qMax(1, afterCount);
        painter.save();
        painter.setOpacity(factor);
        painter.drawImage(canvasRect, snapshot);
        painter.setCompositionMode(QPainter::CompositionMode_SourceAtop);
        QColor overlay = afterOnionTint();
        overlay.setAlphaF(qBound(0.0, overlay.alphaF() * factor, 1.0));
        painter.fillRect(canvasRect, overlay);
        painter.restore();
    }
}

