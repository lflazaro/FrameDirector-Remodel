// Commands/UndoCommands.cpp - Enhanced Implementation
#include "UndoCommands.h"
#include "../Canvas.h"
#include <QGraphicsScene>
#include <QGraphicsItemGroup>
#include <QGraphicsRectItem>
#include <QGraphicsEllipseItem>
#include <QGraphicsLineItem>
#include <QGraphicsTextItem>
#include <QGraphicsPathItem>
#include <QPen>
#include <QBrush>
#include <QTransform>

GraphicsItemCommand::GraphicsItemCommand(Canvas* canvas, QUndoCommand* parent)
    : QUndoCommand(parent)
    , m_canvas(canvas)
{
}

// MoveCommand implementation
MoveCommand::MoveCommand(Canvas* canvas, const QList<QGraphicsItem*>& items,
    const QPointF& delta, QUndoCommand* parent)
    : GraphicsItemCommand(canvas, parent)
    , m_items(items)
    , m_delta(delta)
    , m_firstTime(true)
{
    setText(QString("Move %1 item(s)").arg(items.count()));
}

void MoveCommand::undo()
{
    for (QGraphicsItem* item : m_items) {
        if (item) {
            item->setPos(item->pos() - m_delta);
        }
    }
    if (m_canvas) {
        m_canvas->storeCurrentFrameState();
    }
}

void MoveCommand::redo()
{
    if (m_firstTime) {
        m_firstTime = false;
        return; // Items are already moved when command is created
    }

    for (QGraphicsItem* item : m_items) {
        if (item) {
            item->setPos(item->pos() + m_delta);
        }
    }
    if (m_canvas) {
        m_canvas->storeCurrentFrameState();
    }
}

bool MoveCommand::mergeWith(const QUndoCommand* other)
{
    const MoveCommand* moveCommand = static_cast<const MoveCommand*>(other);

    if (moveCommand->m_items != m_items) {
        return false;
    }

    m_delta += moveCommand->m_delta;
    return true;
}

// AddItemCommand implementation
AddItemCommand::AddItemCommand(Canvas* canvas, QGraphicsItem* item, QUndoCommand* parent)
    : GraphicsItemCommand(canvas, parent)
    , m_item(item)
    , m_itemAdded(false)
{
    setText("Add item");
}

AddItemCommand::~AddItemCommand()
{
    if (!m_itemAdded && m_item) {
        delete m_item;
    }
}

void AddItemCommand::undo()
{
    if (m_canvas && m_canvas->scene() && m_item) {
        m_canvas->scene()->removeItem(m_item);
        m_itemAdded = false;
        m_canvas->storeCurrentFrameState();
    }
}

void AddItemCommand::redo()
{
    if (m_canvas && m_canvas->scene() && m_item) {
        m_canvas->addItemToCurrentLayer(m_item);
        m_itemAdded = true;
        m_canvas->storeCurrentFrameState();
    }
}

// RemoveItemCommand implementation
RemoveItemCommand::RemoveItemCommand(Canvas* canvas, const QList<QGraphicsItem*>& items,
    QUndoCommand* parent)
    : GraphicsItemCommand(canvas, parent)
    , m_items(items)
    , m_itemsRemoved(false)
{
    setText(QString("Remove %1 item(s)").arg(items.count()));
}

RemoveItemCommand::~RemoveItemCommand()
{
    if (m_itemsRemoved) {
        qDeleteAll(m_items);
    }
}

void RemoveItemCommand::undo()
{
    if (m_canvas && m_canvas->scene()) {
        for (QGraphicsItem* item : m_items) {
            if (item) {
                m_canvas->addItemToCurrentLayer(item);
            }
        }
        m_itemsRemoved = false;
        m_canvas->storeCurrentFrameState();
    }
}

void RemoveItemCommand::redo()
{
    if (m_canvas && m_canvas->scene()) {
        for (QGraphicsItem* item : m_items) {
            if (item) {
                m_canvas->scene()->removeItem(item);
            }
        }
        m_itemsRemoved = true;
        m_canvas->storeCurrentFrameState();
    }
}

// TransformCommand implementation
TransformCommand::TransformCommand(Canvas* canvas, QGraphicsItem* item,
    const QTransform& oldTransform, const QTransform& newTransform,
    QUndoCommand* parent)
    : GraphicsItemCommand(canvas, parent)
    , m_item(item)
    , m_oldTransform(oldTransform)
    , m_newTransform(newTransform)
{
    setText("Transform item");
}

void TransformCommand::undo()
{
    if (m_item) {
        m_item->setTransform(m_oldTransform);
        if (m_canvas) {
            m_canvas->storeCurrentFrameState();
        }
    }
}

void TransformCommand::redo()
{
    if (m_item) {
        m_item->setTransform(m_newTransform);
        if (m_canvas) {
            m_canvas->storeCurrentFrameState();
        }
    }
}

// StyleChangeCommand implementation
StyleChangeCommand::StyleChangeCommand(Canvas* canvas, QGraphicsItem* item,
    const QString& property, const QVariant& oldValue,
    const QVariant& newValue, QUndoCommand* parent)
    : GraphicsItemCommand(canvas, parent)
    , m_item(item)
    , m_property(property)
    , m_oldValue(oldValue)
    , m_newValue(newValue)
{
    setText(QString("Change %1").arg(property));
}

void StyleChangeCommand::undo()
{
    if (m_item) {
        applyStyle(m_item, m_property, m_oldValue);
        if (m_canvas) {
            m_canvas->storeCurrentFrameState();
        }
    }
}

void StyleChangeCommand::redo()
{
    if (m_item) {
        applyStyle(m_item, m_property, m_newValue);
        if (m_canvas) {
            m_canvas->storeCurrentFrameState();
        }
    }
}

void StyleChangeCommand::applyStyle(QGraphicsItem* item, const QString& property, const QVariant& value)
{
    if (property == "strokeColor") {
        QColor color = value.value<QColor>();
        if (auto rectItem = qgraphicsitem_cast<QGraphicsRectItem*>(item)) {
            QPen pen = rectItem->pen();
            pen.setColor(color);
            rectItem->setPen(pen);
        }
        else if (auto ellipseItem = qgraphicsitem_cast<QGraphicsEllipseItem*>(item)) {
            QPen pen = ellipseItem->pen();
            pen.setColor(color);
            ellipseItem->setPen(pen);
        }
        else if (auto lineItem = qgraphicsitem_cast<QGraphicsLineItem*>(item)) {
            QPen pen = lineItem->pen();
            pen.setColor(color);
            lineItem->setPen(pen);
        }
        else if (auto pathItem = qgraphicsitem_cast<QGraphicsPathItem*>(item)) {
            QPen pen = pathItem->pen();
            pen.setColor(color);
            pathItem->setPen(pen);
        }
    }
    else if (property == "fillColor") {
        QColor color = value.value<QColor>();
        if (auto rectItem = qgraphicsitem_cast<QGraphicsRectItem*>(item)) {
            rectItem->setBrush(QBrush(color));
        }
        else if (auto ellipseItem = qgraphicsitem_cast<QGraphicsEllipseItem*>(item)) {
            ellipseItem->setBrush(QBrush(color));
        }
        else if (auto pathItem = qgraphicsitem_cast<QGraphicsPathItem*>(item)) {
            pathItem->setBrush(QBrush(color));
        }
    }
    else if (property == "strokeWidth") {
        double width = value.toDouble();
        if (auto rectItem = qgraphicsitem_cast<QGraphicsRectItem*>(item)) {
            QPen pen = rectItem->pen();
            pen.setWidthF(width);
            rectItem->setPen(pen);
        }
        else if (auto ellipseItem = qgraphicsitem_cast<QGraphicsEllipseItem*>(item)) {
            QPen pen = ellipseItem->pen();
            pen.setWidthF(width);
            ellipseItem->setPen(pen);
        }
        else if (auto lineItem = qgraphicsitem_cast<QGraphicsLineItem*>(item)) {
            QPen pen = lineItem->pen();
            pen.setWidthF(width);
            lineItem->setPen(pen);
        }
        else if (auto pathItem = qgraphicsitem_cast<QGraphicsPathItem*>(item)) {
            QPen pen = pathItem->pen();
            pen.setWidthF(width);
            pathItem->setPen(pen);
        }
    }
    else if (property == "opacity") {
        item->setOpacity(value.toDouble());
    }
}

// GroupCommand implementation
GroupCommand::GroupCommand(Canvas* canvas, const QList<QGraphicsItem*>& items,
    QUndoCommand* parent)
    : GraphicsItemCommand(canvas, parent)
    , m_items(items)
    , m_group(nullptr)
    , m_grouped(false)
{
    setText(QString("Group %1 items").arg(items.count()));
}

GroupCommand::~GroupCommand()
{
    if (!m_grouped && m_group) {
        delete m_group;
    }
}

void GroupCommand::undo()
{
    if (m_canvas && m_canvas->scene() && m_group) {
        m_canvas->scene()->destroyItemGroup(m_group);
        m_grouped = false;
        m_canvas->storeCurrentFrameState();
    }
}

void GroupCommand::redo()
{
    if (m_canvas && m_canvas->scene() && !m_items.isEmpty()) {
        m_group = m_canvas->scene()->createItemGroup(m_items);
        m_group->setFlag(QGraphicsItem::ItemIsSelectable, true);
        m_group->setFlag(QGraphicsItem::ItemIsMovable, true);
        m_grouped = true;
        m_canvas->storeCurrentFrameState();
    }
}

// UngroupCommand implementation
UngroupCommand::UngroupCommand(Canvas* canvas, QGraphicsItemGroup* group,
    QUndoCommand* parent)
    : GraphicsItemCommand(canvas, parent)
    , m_group(group)
    , m_ungrouped(false)
{
    setText("Ungroup items");

    // Store the items in the group
    if (group) {
        m_items = group->childItems();
    }
}

void UngroupCommand::undo()
{
    if (m_canvas && m_canvas->scene() && !m_items.isEmpty()) {
        m_group = m_canvas->scene()->createItemGroup(m_items);
        m_group->setFlag(QGraphicsItem::ItemIsSelectable, true);
        m_group->setFlag(QGraphicsItem::ItemIsMovable, true);
        m_ungrouped = false;
        m_canvas->storeCurrentFrameState();
    }
}

void UngroupCommand::redo()
{
    if (m_canvas && m_canvas->scene() && m_group) {
        m_canvas->scene()->destroyItemGroup(m_group);
        m_ungrouped = true;
        m_canvas->storeCurrentFrameState();
    }
}

// NEW: PropertyChangeCommand for properties panel
PropertyChangeCommand::PropertyChangeCommand(Canvas* canvas, QGraphicsItem* item,
    const QString& property, const QVariant& oldValue, const QVariant& newValue,
    QUndoCommand* parent)
    : GraphicsItemCommand(canvas, parent)
    , m_item(item)
    , m_property(property)
    , m_oldValue(oldValue)
    , m_newValue(newValue)
{
    setText(QString("Change %1").arg(property));
}

void PropertyChangeCommand::undo()
{
    if (m_item) {
        applyProperty(m_item, m_property, m_oldValue);
        if (m_canvas) {
            m_canvas->storeCurrentFrameState();
        }
    }
}

void PropertyChangeCommand::redo()
{
    if (m_item) {
        applyProperty(m_item, m_property, m_newValue);
        if (m_canvas) {
            m_canvas->storeCurrentFrameState();
        }
    }
}

void PropertyChangeCommand::applyProperty(QGraphicsItem* item, const QString& property, const QVariant& value)
{
    if (property == "position") {
        item->setPos(value.toPointF());
    }
    else if (property == "rotation") {
        item->setRotation(value.toDouble());
    }
    else if (property == "scale") {
        QPointF scale = value.toPointF();
        QTransform transform;
        transform.scale(scale.x(), scale.y());
        item->setTransform(transform);
    }
    else if (property == "opacity") {
        item->setOpacity(value.toDouble());
    }
    else if (property == "size") {
        QSizeF size = value.toSizeF();
        if (auto rectItem = qgraphicsitem_cast<QGraphicsRectItem*>(item)) {
            QRectF rect = rectItem->rect();
            rect.setSize(size);
            rectItem->setRect(rect);
        }
        else if (auto ellipseItem = qgraphicsitem_cast<QGraphicsEllipseItem*>(item)) {
            QRectF rect = ellipseItem->rect();
            rect.setSize(size);
            ellipseItem->setRect(rect);
        }
    }
}

// NEW: DrawCommand for drawing operations
DrawCommand::DrawCommand(Canvas* canvas, QGraphicsItem* item, QUndoCommand* parent)
    : GraphicsItemCommand(canvas, parent)
    , m_item(item)
    , m_itemAdded(false)
{
    setText("Draw");
}

DrawCommand::~DrawCommand()
{
    if (!m_itemAdded && m_item) {
        delete m_item;
    }
}

void DrawCommand::undo()
{
    if (m_canvas && m_canvas->scene() && m_item) {
        m_canvas->scene()->removeItem(m_item);
        m_itemAdded = false;
        m_canvas->storeCurrentFrameState();
    }
}

void DrawCommand::redo()
{
    if (m_canvas && m_item) {
        m_canvas->addItemToCurrentLayer(m_item);
        m_itemAdded = true;
        m_canvas->storeCurrentFrameState();
    }
}