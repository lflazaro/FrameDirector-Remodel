// VectorGraphics/VectorGraphicsItem.cpp
#include "VectorGraphicsItem.h"
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QJsonObject>
#include <QJsonArray>
#include <QCursor>
#include <QtMath>
#include "Canvas.h"

VectorGraphicsItem::VectorGraphicsItem(QGraphicsItem* parent)
    : QGraphicsItem(parent)
    , m_stroke(QPen(Qt::black, 2.0))
    , m_fill(QBrush(Qt::transparent))
    , m_animationFrame(1)
    , m_isKeyframe(false)
    , m_showSelectionHandles(false)
    , m_resizing(false)
    , m_resizeHandle(-1)
{
    setFlags(ItemIsSelectable | ItemIsMovable | ItemSendsGeometryChanges);
    setAcceptHoverEvents(true);

    // Initialize stroke with round caps and joins for better appearance
    m_stroke.setCapStyle(Qt::RoundCap);
    m_stroke.setJoinStyle(Qt::RoundJoin);
}

QRectF VectorGraphicsItem::boundingRect() const
{
    // Default implementation - subclasses should override this
    if (m_boundingRect.isNull()) {
        return QRectF(-50, -50, 100, 100);
    }

    // Add stroke width to bounding rect
    qreal strokeWidth = m_stroke.widthF();
    qreal halfStroke = strokeWidth / 2.0;

    QRectF rect = m_boundingRect.adjusted(-halfStroke, -halfStroke, halfStroke, halfStroke);

    // Add space for selection handles if shown
    if (m_showSelectionHandles) {
        rect = rect.adjusted(-8, -8, 8, 8);
    }

    return rect;
}

void VectorGraphicsItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    Q_UNUSED(option)
        Q_UNUSED(widget)

        // Set up painter
        painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(m_stroke);
    painter->setBrush(m_fill);

    // Subclasses should override this to do actual painting
    // Default implementation draws a simple rectangle
    painter->drawRect(m_boundingRect);

    // Draw selection handles if selected and enabled
    if (isSelected() && m_showSelectionHandles) {
        drawSelectionHandles(painter);
    }

    // Draw keyframe indicator if this is a keyframe
    if (m_isKeyframe) {
        painter->save();
        painter->setPen(QPen(QColor(255, 140, 0), 2));
        painter->setBrush(QBrush(QColor(255, 140, 0, 100)));
        QRectF keyframeRect = boundingRect().adjusted(2, 2, -2, -2);
        painter->drawRect(keyframeRect);
        painter->restore();
    }
}

QPainterPath VectorGraphicsItem::shape() const
{
    // Default implementation returns the bounding rect as a path
    // Subclasses can override for more precise hit testing
    QPainterPath path;
    path.addRect(boundingRect());
    return path;
}

int VectorGraphicsItem::type() const
{
    // Return a unique type identifier for this item type
    // QGraphicsItem::UserType + some offset
    return QGraphicsItem::UserType + 1000;
}

void VectorGraphicsItem::setStroke(const QPen& pen)
{
    if (m_stroke != pen) {
        prepareGeometryChange();
        m_stroke = pen;
        update();
    }
}

QPen VectorGraphicsItem::getStroke() const
{
    return m_stroke;
}

void VectorGraphicsItem::setFill(const QBrush& brush)
{
    if (m_fill != brush) {
        m_fill = brush;
        update();
    }
}

QBrush VectorGraphicsItem::getFill() const
{
    return m_fill;
}

void VectorGraphicsItem::setAnimationFrame(int frame)
{
    if (m_animationFrame != frame) {
        m_animationFrame = frame;
        // Subclasses can override to handle frame-specific updates
        update();
    }
}

int VectorGraphicsItem::getAnimationFrame() const
{
    return m_animationFrame;
}

void VectorGraphicsItem::setKeyframe(bool keyframe)
{
    if (m_isKeyframe != keyframe) {
        m_isKeyframe = keyframe;
        update();
    }
}

bool VectorGraphicsItem::isKeyframe() const
{
    return m_isKeyframe;
}

QJsonObject VectorGraphicsItem::toJson() const
{
    QJsonObject json;

    // Basic properties
    json["type"] = static_cast<int>(getItemType());
    json["x"] = pos().x();
    json["y"] = pos().y();
    json["rotation"] = rotation();
    json["scaleX"] = transform().m11();
    json["scaleY"] = transform().m22();
    json["visible"] = isVisible();
    json["zValue"] = zValue();

    // Animation properties
    json["animationFrame"] = m_animationFrame;
    json["isKeyframe"] = m_isKeyframe;

    // Stroke properties
    QJsonObject strokeJson;
    strokeJson["color"] = m_stroke.color().name();
    strokeJson["width"] = m_stroke.widthF();
    strokeJson["style"] = static_cast<int>(m_stroke.style());
    strokeJson["capStyle"] = static_cast<int>(m_stroke.capStyle());
    strokeJson["joinStyle"] = static_cast<int>(m_stroke.joinStyle());
    json["stroke"] = strokeJson;

    // Fill properties
    QJsonObject fillJson;
    fillJson["color"] = m_fill.color().name();
    fillJson["style"] = static_cast<int>(m_fill.style());
    json["fill"] = fillJson;

    // Bounding rect
    QJsonObject rectJson;
    rectJson["x"] = m_boundingRect.x();
    rectJson["y"] = m_boundingRect.y();
    rectJson["width"] = m_boundingRect.width();
    rectJson["height"] = m_boundingRect.height();
    json["boundingRect"] = rectJson;

    return json;
}

void VectorGraphicsItem::fromJson(const QJsonObject& json)
{
    // Basic properties
    setPos(json["x"].toDouble(), json["y"].toDouble());
    setRotation(json["rotation"].toDouble());

    QTransform transform;
    transform.scale(json["scaleX"].toDouble(1.0), json["scaleY"].toDouble(1.0));
    setTransform(transform);

    setVisible(json["visible"].toBool(true));
    setZValue(json["zValue"].toDouble(0.0));

    // Animation properties
    m_animationFrame = json["animationFrame"].toInt(1);
    m_isKeyframe = json["isKeyframe"].toBool(false);

    // Stroke properties
    QJsonObject strokeJson = json["stroke"].toObject();
    QPen stroke;
    stroke.setColor(QColor(strokeJson["color"].toString("#000000")));
    stroke.setWidthF(strokeJson["width"].toDouble(2.0));
    stroke.setStyle(static_cast<Qt::PenStyle>(strokeJson["style"].toInt(Qt::SolidLine)));
    stroke.setCapStyle(static_cast<Qt::PenCapStyle>(strokeJson["capStyle"].toInt(Qt::RoundCap)));
    stroke.setJoinStyle(static_cast<Qt::PenJoinStyle>(strokeJson["joinStyle"].toInt(Qt::RoundJoin)));
    setStroke(stroke);

    // Fill properties
    QJsonObject fillJson = json["fill"].toObject();
    QBrush fill;
    fill.setColor(QColor(fillJson["color"].toString("#ffffff")));
    fill.setStyle(static_cast<Qt::BrushStyle>(fillJson["style"].toInt(Qt::NoBrush)));
    setFill(fill);

    // Bounding rect
    QJsonObject rectJson = json["boundingRect"].toObject();
    m_boundingRect = QRectF(
        rectJson["x"].toDouble(),
        rectJson["y"].toDouble(),
        rectJson["width"].toDouble(),
        rectJson["height"].toDouble()
    );

    update();
}

void VectorGraphicsItem::setShowSelectionHandles(bool show)
{
    if (m_showSelectionHandles != show) {
        prepareGeometryChange();
        m_showSelectionHandles = show;
        update();
    }
}

bool VectorGraphicsItem::showSelectionHandles() const
{
    return m_showSelectionHandles;
}

void VectorGraphicsItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && m_showSelectionHandles) {
        // Check if clicking on a resize handle
        QPointF localPos = event->pos();
        int handleIndex = getHandleAtPoint(localPos);

        if (handleIndex >= 0) {
            m_resizing = true;
            m_resizeHandle = handleIndex;
            m_lastMousePos = event->scenePos();
            event->accept();
            return;
        }
    }

    // Default handling for selection and movement
    QGraphicsItem::mousePressEvent(event);
}

void VectorGraphicsItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    if (m_resizing && m_resizeHandle >= 0) {
        QPointF delta = event->scenePos() - m_lastMousePos;
        QRectF rect = m_boundingRect;

        // Resize based on which handle is being dragged
        switch (m_resizeHandle) {
        case 0: // Top-left
            rect.setTopLeft(rect.topLeft() + delta);
            break;
        case 1: // Top-center
            rect.setTop(rect.top() + delta.y());
            break;
        case 2: // Top-right
            rect.setTopRight(rect.topRight() + delta);
            break;
        case 3: // Middle-right
            rect.setRight(rect.right() + delta.x());
            break;
        case 4: // Bottom-right
            rect.setBottomRight(rect.bottomRight() + delta);
            break;
        case 5: // Bottom-center
            rect.setBottom(rect.bottom() + delta.y());
            break;
        case 6: // Bottom-left
            rect.setBottomLeft(rect.bottomLeft() + delta);
            break;
        case 7: // Middle-left
            rect.setLeft(rect.left() + delta.x());
            break;
        }

        // Ensure minimum size
        if (rect.width() < 10) {
            if (m_resizeHandle == 0 || m_resizeHandle == 6 || m_resizeHandle == 7) {
                rect.setLeft(rect.right() - 10);
            }
            else {
                rect.setRight(rect.left() + 10);
            }
        }
        if (rect.height() < 10) {
            if (m_resizeHandle == 0 || m_resizeHandle == 1 || m_resizeHandle == 2) {
                rect.setTop(rect.bottom() - 10);
            }
            else {
                rect.setBottom(rect.top() + 10);
            }
        }

        prepareGeometryChange();
        m_boundingRect = rect;
        update();

        m_lastMousePos = event->scenePos();
        event->accept();
        return;
    }

    QGraphicsItem::mouseMoveEvent(event);
}

void VectorGraphicsItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    if (m_resizing) {
        m_resizing = false;
        m_resizeHandle = -1;
        setCursor(Qt::ArrowCursor);

        // Persist resized state to current frame
        if (scene()) {
            const auto views = scene()->views();
            for (QGraphicsView* view : views) {
                if (auto canvas = dynamic_cast<Canvas*>(view)) {
                    canvas->storeCurrentFrameState();
                    break;
                }
            }
        }

        event->accept();
        return;
    }

    QGraphicsItem::mouseReleaseEvent(event);
}

void VectorGraphicsItem::drawSelectionHandles(QPainter* painter)
{
    painter->save();

    // Draw selection outline
    QPen outlinePen(QColor(0, 122, 204), 1);
    outlinePen.setStyle(Qt::DashLine);
    painter->setPen(outlinePen);
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(m_boundingRect);

    // Draw resize handles
    QPen handlePen(QColor(0, 122, 204), 1);
    QBrush handleBrush(Qt::white);
    painter->setPen(handlePen);
    painter->setBrush(handleBrush);

    // Draw 8 resize handles around the bounding rect
    for (int i = 0; i < 8; ++i) {
        QRectF handleRect = getSelectionHandleRect(i);
        painter->drawRect(handleRect);
    }

    painter->restore();
}

QRectF VectorGraphicsItem::getSelectionHandleRect(int handleIndex) const
{
    const qreal handleSize = 6.0;
    const qreal halfHandle = handleSize / 2.0;
    QRectF rect = m_boundingRect;

    QPointF center;
    switch (handleIndex) {
    case 0: center = rect.topLeft(); break;         // Top-left
    case 1: center = QPointF(rect.center().x(), rect.top()); break;    // Top-center
    case 2: center = rect.topRight(); break;       // Top-right
    case 3: center = QPointF(rect.right(), rect.center().y()); break;  // Middle-right
    case 4: center = rect.bottomRight(); break;    // Bottom-right
    case 5: center = QPointF(rect.center().x(), rect.bottom()); break; // Bottom-center
    case 6: center = rect.bottomLeft(); break;     // Bottom-left
    case 7: center = QPointF(rect.left(), rect.center().y()); break;   // Middle-left
    default: return QRectF();
    }

    return QRectF(center.x() - halfHandle, center.y() - halfHandle, handleSize, handleSize);
}

int VectorGraphicsItem::getHandleAtPoint(const QPointF& point) const
{
    if (!m_showSelectionHandles) {
        return -1;
    }

    for (int i = 0; i < 8; ++i) {
        QRectF handleRect = getSelectionHandleRect(i);
        if (handleRect.contains(point)) {
            return i;
        }
    }

    return -1;
}

// Specific vector item implementations

// VectorRectangleItem
class VectorRectangleItem : public VectorGraphicsItem
{
public:
    VectorRectangleItem(const QRectF& rect, QGraphicsItem* parent = nullptr)
        : VectorGraphicsItem(parent)
    {
        m_boundingRect = rect;
    }

    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override
    {
        Q_UNUSED(option)
            Q_UNUSED(widget)

            painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setPen(getStroke());
        painter->setBrush(getFill());
        painter->drawRect(m_boundingRect);

        if (isSelected() && showSelectionHandles()) {
            drawSelectionHandles(painter);
        }
    }

    ItemType getItemType() const override { return VectorRectangle; }
};

// VectorEllipseItem
class VectorEllipseItem : public VectorGraphicsItem
{
public:
    VectorEllipseItem(const QRectF& rect, QGraphicsItem* parent = nullptr)
        : VectorGraphicsItem(parent)
    {
        m_boundingRect = rect;
    }

    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override
    {
        Q_UNUSED(option)
            Q_UNUSED(widget)

            painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setPen(getStroke());
        painter->setBrush(getFill());
        painter->drawEllipse(m_boundingRect);

        if (isSelected() && showSelectionHandles()) {
            drawSelectionHandles(painter);
        }
    }

    QPainterPath shape() const override
    {
        QPainterPath path;
        path.addEllipse(m_boundingRect);
        return path;
    }

    ItemType getItemType() const override { return VectorEllipse; }
};

// VectorLineItem
class VectorLineItem : public VectorGraphicsItem
{
public:
    VectorLineItem(const QLineF& line, QGraphicsItem* parent = nullptr)
        : VectorGraphicsItem(parent), m_line(line)
    {
        updateBoundingRect();
    }

    void setLine(const QLineF& line)
    {
        prepareGeometryChange();
        m_line = line;
        updateBoundingRect();
        update();
    }

    QLineF getLine() const { return m_line; }

    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override
    {
        Q_UNUSED(option)
            Q_UNUSED(widget)

            painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setPen(getStroke());
        painter->drawLine(m_line);

        if (isSelected() && showSelectionHandles()) {
            drawSelectionHandles(painter);
        }
    }

    QPainterPath shape() const override
    {
        QPainterPath path;
        QPainterPathStroker stroker;
        stroker.setWidth(qMax(getStroke().widthF(), 5.0)); // Minimum 5px for easier selection

        QPainterPath linePath;
        linePath.moveTo(m_line.p1());
        linePath.lineTo(m_line.p2());

        return stroker.createStroke(linePath);
    }

    ItemType getItemType() const override { return VectorLine; }

    QJsonObject toJson() const override
    {
        QJsonObject json = VectorGraphicsItem::toJson();
        json["x1"] = m_line.x1();
        json["y1"] = m_line.y1();
        json["x2"] = m_line.x2();
        json["y2"] = m_line.y2();
        return json;
    }

    void fromJson(const QJsonObject& json) override
    {
        VectorGraphicsItem::fromJson(json);
        m_line = QLineF(
            json["x1"].toDouble(),
            json["y1"].toDouble(),
            json["x2"].toDouble(),
            json["y2"].toDouble()
        );
        updateBoundingRect();
    }

private:
    void updateBoundingRect()
    {
        qreal x1 = m_line.x1(), y1 = m_line.y1();
        qreal x2 = m_line.x2(), y2 = m_line.y2();
        qreal strokeWidth = getStroke().widthF();
        qreal halfStroke = strokeWidth / 2.0;

        m_boundingRect = QRectF(
            qMin(x1, x2) - halfStroke,
            qMin(y1, y2) - halfStroke,
            qAbs(x2 - x1) + strokeWidth,
            qAbs(y2 - y1) + strokeWidth
        );
    }

    QLineF m_line;
};

// VectorPathItem
class VectorPathItem : public VectorGraphicsItem
{
public:
    VectorPathItem(const QPainterPath& path, QGraphicsItem* parent = nullptr)
        : VectorGraphicsItem(parent), m_path(path)
    {
        m_boundingRect = m_path.boundingRect();
    }

    void setPath(const QPainterPath& path)
    {
        prepareGeometryChange();
        m_path = path;
        m_boundingRect = m_path.boundingRect();
        update();
    }

    QPainterPath getPath() const { return m_path; }

    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override
    {
        Q_UNUSED(option)
            Q_UNUSED(widget)

            painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setPen(getStroke());
        painter->setBrush(getFill());
        painter->drawPath(m_path);

        if (isSelected() && showSelectionHandles()) {
            drawSelectionHandles(painter);
        }
    }

    QPainterPath shape() const override
    {
        return m_path;
    }

    ItemType getItemType() const override { return VectorPath; }

private:
    QPainterPath m_path;
};

VectorGraphicsItem* cloneVectorGraphicsItem(const VectorGraphicsItem* item)
{
    if (!item) {
        return nullptr;
    }

    QJsonObject json = item->toJson();
    VectorGraphicsItem* newItem = nullptr;

    switch (item->getItemType()) {
    case VectorGraphicsItem::VectorRectangle:
        newItem = new VectorRectangleItem(QRectF());
        break;
    case VectorGraphicsItem::VectorEllipse:
        newItem = new VectorEllipseItem(QRectF());
        break;
    case VectorGraphicsItem::VectorLine:
        newItem = new VectorLineItem(QLineF());
        break;
    case VectorGraphicsItem::VectorPath:
        newItem = new VectorPathItem(QPainterPath());
        break;
    default:
        break;
    }

    if (newItem) {
        newItem->fromJson(json);
    }

    return newItem;
}
