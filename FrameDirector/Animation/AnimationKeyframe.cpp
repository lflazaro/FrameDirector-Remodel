#include "AnimationKeyframe.h"
#include <QGraphicsItem>
#include <QGraphicsRectItem>
#include <QGraphicsEllipseItem>
#include <QGraphicsPathItem>
#include <QGraphicsTextItem>
#include <QGraphicsPixmapItem>
#include <QGraphicsBlurEffect>
#include <QtMath>
#include <QPen>        // Add this
#include <QBrush>      // Add this
#include <QPainter>    // Add this
#include <qgraphicssvgitem.h>


AnimationKeyframe::AnimationKeyframe(int frame)
    : m_frame(frame)
    , m_type(Linear)
    , m_easing(QEasingCurve::Linear)
    , m_selected(false)
{
}

AnimationKeyframe::~AnimationKeyframe()
{
}

int AnimationKeyframe::getFrame() const
{
    return m_frame;
}

void AnimationKeyframe::setFrame(int frame)
{
    m_frame = frame;
}

void AnimationKeyframe::captureItemState(QGraphicsItem* item)
{
    if (!item) return;

    ItemState state;
    state.position = item->pos();
    state.rotation = item->rotation();
    state.scale = QPointF(item->transform().m11(), item->transform().m22());
    state.opacity = item->opacity();
    state.transform = item->transform();
    state.visible = item->isVisible();
    if (auto blur = dynamic_cast<QGraphicsBlurEffect*>(item->graphicsEffect())) {
        state.blurRadius = blur->blurRadius();
    }
    else {
        state.blurRadius = 0.0;
    }

    // Get size from bounding rect
    state.size = item->boundingRect().size();

    // Try to get colors and stroke width based on item type
    if (auto rectItem = qgraphicsitem_cast<QGraphicsRectItem*>(item)) {
        state.strokeColor = rectItem->pen().color();
        state.fillColor = rectItem->brush().color();
        state.strokeWidth = rectItem->pen().widthF();
    }
    else if (auto ellipseItem = qgraphicsitem_cast<QGraphicsEllipseItem*>(item)) {
        state.strokeColor = ellipseItem->pen().color();
        state.fillColor = ellipseItem->brush().color();
        state.strokeWidth = ellipseItem->pen().widthF();
    }
    else if (auto pathItem = qgraphicsitem_cast<QGraphicsPathItem*>(item)) {
        state.strokeColor = pathItem->pen().color();
        state.fillColor = pathItem->brush().color();
        state.strokeWidth = pathItem->pen().widthF();
    }
    else if (auto textItem = qgraphicsitem_cast<QGraphicsTextItem*>(item)) {
        state.strokeColor = textItem->defaultTextColor();
        state.fillColor = Qt::transparent;
        state.strokeWidth = 0;
    }
    else if (qgraphicsitem_cast<QGraphicsPixmapItem*>(item)) {
        state.strokeColor = Qt::transparent;
        state.fillColor = Qt::transparent;
        state.strokeWidth = 0;
    }
    else if (qgraphicsitem_cast<QGraphicsSvgItem*>(item)) {
        state.strokeColor = Qt::transparent;
        state.fillColor = Qt::transparent;
        state.strokeWidth = 0;
    }
    else {
        state.strokeColor = Qt::black;
        state.fillColor = Qt::transparent;
        state.strokeWidth = 1.0;
    }

    m_itemStates[item] = state;
}

void AnimationKeyframe::applyItemState(QGraphicsItem* item) const
{
    auto it = m_itemStates.find(item);
    if (it == m_itemStates.end()) return;

    const ItemState& state = it->second;

    item->setPos(state.position);
    item->setRotation(state.rotation);
    item->setOpacity(state.opacity);
    item->setVisible(state.visible);
    if (state.blurRadius > 0) {
        QGraphicsBlurEffect* blurEffect = dynamic_cast<QGraphicsBlurEffect*>(item->graphicsEffect());
        if (!blurEffect) {
            blurEffect = new QGraphicsBlurEffect();
            item->setGraphicsEffect(blurEffect);
        }
        blurEffect->setBlurRadius(state.blurRadius);
    }
    else if (item->graphicsEffect()) {
        item->setGraphicsEffect(nullptr);
    }

    // Apply colors and stroke width based on item type
    if (auto rectItem = qgraphicsitem_cast<QGraphicsRectItem*>(item)) {
        QPen pen = rectItem->pen();
        pen.setColor(state.strokeColor);
        pen.setWidthF(state.strokeWidth);
        rectItem->setPen(pen);
        rectItem->setBrush(QBrush(state.fillColor));
    }
    else if (auto ellipseItem = qgraphicsitem_cast<QGraphicsEllipseItem*>(item)) {
        QPen pen = ellipseItem->pen();
        pen.setColor(state.strokeColor);
        pen.setWidthF(state.strokeWidth);
        ellipseItem->setPen(pen);
        ellipseItem->setBrush(QBrush(state.fillColor));
    }
    else if (auto pathItem = qgraphicsitem_cast<QGraphicsPathItem*>(item)) {
        QPen pen = pathItem->pen();
        pen.setColor(state.strokeColor);
        pen.setWidthF(state.strokeWidth);
        pathItem->setPen(pen);
        pathItem->setBrush(QBrush(state.fillColor));
    }
    else if (auto textItem = qgraphicsitem_cast<QGraphicsTextItem*>(item)) {
        textItem->setDefaultTextColor(state.strokeColor);
    }
    else if (qgraphicsitem_cast<QGraphicsPixmapItem*>(item)) {
        // No additional properties for pixmap items
    }
    else if (qgraphicsitem_cast<QGraphicsSvgItem*>(item)) {
        // No additional properties for SVG items
    }
}

bool AnimationKeyframe::hasItemState(QGraphicsItem* item) const
{
    return m_itemStates.find(item) != m_itemStates.end();
}

void AnimationKeyframe::removeItemState(QGraphicsItem* item)
{
    m_itemStates.erase(item);
}

void AnimationKeyframe::setEasing(QEasingCurve::Type easing)
{
    m_easing = easing;
}

QEasingCurve::Type AnimationKeyframe::getEasing() const
{
    return m_easing;
}

void AnimationKeyframe::interpolateBetween(const AnimationKeyframe* from,
    const AnimationKeyframe* to,
    double t,
    QGraphicsItem* item)
{
    if (!from || !to || !item) return;

    auto fromIt = from->m_itemStates.find(item);
    auto toIt = to->m_itemStates.find(item);

    if (fromIt == from->m_itemStates.end() || toIt == to->m_itemStates.end()) {
        return;
    }

    const ItemState& fromState = fromIt->second;
    const ItemState& toState = toIt->second;

    // Apply easing
    QEasingCurve easingCurve(to->getEasing());
    double easedT = easingCurve.valueForProgress(t);

    // Interpolate position
    QPointF pos = interpolatePoint(fromState.position, toState.position, easedT);
    item->setPos(pos);

    // Interpolate rotation
    double rotation = interpolateValue(fromState.rotation, toState.rotation, easedT);
    item->setRotation(rotation);

    // Interpolate opacity
    double opacity = interpolateValue(fromState.opacity, toState.opacity, easedT);
    item->setOpacity(opacity);

    // Interpolate blur
    double blur = interpolateValue(fromState.blurRadius, toState.blurRadius, easedT);
    if (blur > 0) {
        QGraphicsBlurEffect* blurEffect = dynamic_cast<QGraphicsBlurEffect*>(item->graphicsEffect());
        if (!blurEffect) {
            blurEffect = new QGraphicsBlurEffect();
            item->setGraphicsEffect(blurEffect);
        }
        blurEffect->setBlurRadius(blur);
    }
    else if (item->graphicsEffect()) {
        item->setGraphicsEffect(nullptr);
    }

    // Interpolate colors and stroke width for supported item types
    if (auto rectItem = qgraphicsitem_cast<QGraphicsRectItem*>(item)) {
        QColor strokeColor = interpolateColor(fromState.strokeColor, toState.strokeColor, easedT);
        QColor fillColor = interpolateColor(fromState.fillColor, toState.fillColor, easedT);
        double strokeWidth = interpolateValue(fromState.strokeWidth, toState.strokeWidth, easedT);

        QPen pen = rectItem->pen();
        pen.setColor(strokeColor);
        pen.setWidthF(strokeWidth);
        rectItem->setPen(pen);
        rectItem->setBrush(QBrush(fillColor));
    }
    else if (auto ellipseItem = qgraphicsitem_cast<QGraphicsEllipseItem*>(item)) {
        QColor strokeColor = interpolateColor(fromState.strokeColor, toState.strokeColor, easedT);
        QColor fillColor = interpolateColor(fromState.fillColor, toState.fillColor, easedT);
        double strokeWidth = interpolateValue(fromState.strokeWidth, toState.strokeWidth, easedT);

        QPen pen = ellipseItem->pen();
        pen.setColor(strokeColor);
        pen.setWidthF(strokeWidth);
        ellipseItem->setPen(pen);
        ellipseItem->setBrush(QBrush(fillColor));
    }
    else if (auto pathItem = qgraphicsitem_cast<QGraphicsPathItem*>(item)) {
        QColor strokeColor = interpolateColor(fromState.strokeColor, toState.strokeColor, easedT);
        QColor fillColor = interpolateColor(fromState.fillColor, toState.fillColor, easedT);
        double strokeWidth = interpolateValue(fromState.strokeWidth, toState.strokeWidth, easedT);

        QPen pen = pathItem->pen();
        pen.setColor(strokeColor);
        pen.setWidthF(strokeWidth);
        pathItem->setPen(pen);
        pathItem->setBrush(QBrush(fillColor));
    }
    else if (auto textItem = qgraphicsitem_cast<QGraphicsTextItem*>(item)) {
        QColor color = interpolateColor(fromState.strokeColor, toState.strokeColor, easedT);
        textItem->setDefaultTextColor(color);
    }
    // Pixmap and SVG items do not require color interpolation
}

double AnimationKeyframe::interpolateValue(double from, double to, double t)
{
    return from + (to - from) * t;
}

QPointF AnimationKeyframe::interpolatePoint(const QPointF& from, const QPointF& to, double t)
{
    return QPointF(
        interpolateValue(from.x(), to.x(), t),
        interpolateValue(from.y(), to.y(), t)
    );
}

QColor AnimationKeyframe::interpolateColor(const QColor& from, const QColor& to, double t)
{
    return QColor(
        static_cast<int>(interpolateValue(from.red(), to.red(), t)),
        static_cast<int>(interpolateValue(from.green(), to.green(), t)),
        static_cast<int>(interpolateValue(from.blue(), to.blue(), t)),
        static_cast<int>(interpolateValue(from.alpha(), to.alpha(), t))
    );
}

void AnimationKeyframe::setType(KeyframeType type)
{
    m_type = type;
}

AnimationKeyframe::KeyframeType AnimationKeyframe::getType() const
{
    return m_type;
}

void AnimationKeyframe::setSelected(bool selected)
{
    m_selected = selected;
}

bool AnimationKeyframe::isSelected() const
{
    return m_selected;
}