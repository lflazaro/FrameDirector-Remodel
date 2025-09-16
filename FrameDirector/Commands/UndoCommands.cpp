// Commands/UndoCommands.cpp - Fixed to prevent double deletion
#include "UndoCommands.h"
#include "../Canvas.h"
#include <QGraphicsScene>
#include <QGraphicsItemGroup>
#include <QGraphicsRectItem>
#include <QGraphicsEllipseItem>
#include <QGraphicsLineItem>
#include <QGraphicsTextItem>
#include <QGraphicsPathItem>
#include <QGraphicsBlurEffect>
#include <QPen>
#include <QBrush>
#include <QTransform>
#include <QDebug>

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
        if (item && isItemValid(item)) {
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
        if (item && isItemValid(item)) {
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

    // Only merge if moving the same items
    if (moveCommand->m_items.size() != m_items.size()) {
        return false;
    }

    // Check if all items match and are still valid
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items[i] != moveCommand->m_items[i] || !isItemValid(m_items[i])) {
            return false;
        }
    }

    // Merge the deltas
    m_delta += moveCommand->m_delta;
    return true;
}

// FIXED: Add item validation helper
bool GraphicsItemCommand::isItemValid(QGraphicsItem* item)
{
    if (!item || !m_canvas) {
        return false;
    }

    QGraphicsScene* scene = m_canvas->scene();
    if (scene && item->scene() == scene) {
        return true;
    }

    // Fall back to the canvas tracking data for items that were temporarily
    // removed from the scene (for example, during erase operations).
    return m_canvas->isValidItem(item);
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
    // FIXED: Only delete if we own the item and it's not in a scene
    if (!m_itemAdded && m_item && !m_item->scene()) {
        qDebug() << "AddItemCommand: Cleaning up non-added item";
        delete m_item;
        m_item = nullptr;
    }
}

void AddItemCommand::redo()
{
    if (m_canvas && m_item) {
        // Only add if item is not already in scene
        if (!m_item->scene()) {
            m_canvas->addItemToCurrentLayer(m_item);
            m_itemAdded = true;
            m_canvas->storeCurrentFrameState();
        }
    }
}

void AddItemCommand::undo()
{
    if (!m_canvas || !m_item) {
        return;
    }

    // Purge the item from all canvas bookkeeping before it potentially gets
    // deleted by the command lifecycle. Otherwise stale pointers linger in the
    // layer tracking structures and later queries may treat a freed item as
    // still alive, leading to crashes deep inside Qt.
    m_canvas->removeItemFromAllFrames(m_item);

    m_itemAdded = false;

    if (QGraphicsScene* scene = m_canvas->scene()) {
        if (m_item->scene() == scene) {
            scene->removeItem(m_item);
        }
        m_canvas->storeCurrentFrameState();
    }
}

// RemoveItemCommand implementation - FIXED VERSION
RemoveItemCommand::RemoveItemCommand(Canvas* canvas, const QList<QGraphicsItem*>& items,
    QUndoCommand* parent)
    : GraphicsItemCommand(canvas, parent)
    , m_items(items)
    , m_itemsRemoved(false)
    , m_frame(canvas ? canvas->getCurrentFrame() : -1)
{
    setText(QString("Remove %1 item(s)").arg(items.count()));

    // FIXED: Filter out invalid items immediately
    QList<QGraphicsItem*> validItems;
    for (QGraphicsItem* item : items) {
        if (item && isItemValid(item)) {
            validItems.append(item);
        }
    }
    m_items = validItems;
}

RemoveItemCommand::~RemoveItemCommand()
{
    if (m_itemsRemoved) {
        qDebug() << "RemoveItemCommand: Cleaning up" << m_items.size() << "removed items";

        for (QGraphicsItem* item : m_items) {
            if (!item) {
                continue;
            }

            bool inScene = false;
            if (m_canvas && m_canvas->scene()) {
                inScene = m_canvas->scene()->items().contains(item);
            }

            if (!inScene) {
                const bool stillTracked = m_canvas && m_canvas->isValidItem(item);
                if (!stillTracked) {
                    try {
                        delete item;
                    }
                    catch (...) {
                        // Ignore deletion errors - item might already be deleted
                        qDebug() << "RemoveItemCommand: Error deleting item (probably already deleted)";
                    }
                }
                else {
                    qDebug() << "RemoveItemCommand: Item still tracked, skipping deletion";
                }
            }
            else {
                qDebug() << "RemoveItemCommand: Item still in scene, not deleting";
            }
        }
    }
}
//needs tweaking
void RemoveItemCommand::redo()
{
    if (!m_canvas) {
        return;
    }

    QGraphicsScene* scene = m_canvas->scene();
    QList<QGraphicsItem*> actuallyRemoved;
    bool changed = false;

    const int targetFrame = (m_frame > 0) ? m_frame : (m_canvas ? m_canvas->getCurrentFrame() : -1);

    for (QGraphicsItem* item : m_items) {
        if (!item) {
            continue;
        }

        const bool wasInScene = scene && item->scene() == scene;
        const bool wasTracked = m_canvas && m_canvas->isValidItem(item);

        if (wasInScene) {
            if (item->graphicsEffect()) {
                item->setGraphicsEffect(nullptr);
            }
            scene->removeItem(item);
        }

        if (wasTracked && m_canvas) {
            m_canvas->detachItemFromFrame(item, targetFrame);
        }

        if (wasInScene || wasTracked) {
            actuallyRemoved.append(item);
            changed = true;
        }
    }

    m_items = actuallyRemoved;
    m_itemsRemoved = !m_items.isEmpty();

    if (changed && m_canvas && targetFrame == m_canvas->getCurrentFrame()) {
        m_canvas->storeCurrentFrameState();
    }
}

void RemoveItemCommand::undo()
{
    if (m_canvas && m_canvas->scene()) {
        for (QGraphicsItem* item : m_items) {
            if (item && !item->scene()) { // Only add if not already in scene
                m_canvas->addItemToCurrentLayer(item);
            }
        }
        m_itemsRemoved = false;

        if (m_canvas && m_canvas->getCurrentFrame() == m_frame) {
            m_canvas->storeCurrentFrameState();
        }
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
    if (m_item && isItemValid(m_item)) {
        m_item->setTransform(m_oldTransform);
        if (m_canvas) {
            m_canvas->storeCurrentFrameState();
        }
    }
}

void TransformCommand::redo()
{
    if (m_item && isItemValid(m_item)) {
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
    if (m_item && isItemValid(m_item)) {
        applyStyle(m_item, m_property, m_oldValue);
        if (m_canvas) {
            m_canvas->storeCurrentFrameState();
        }
    }
}

void StyleChangeCommand::redo()
{
    if (m_item && isItemValid(m_item)) {
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
        item->setData(0, value.toDouble());
        item->setOpacity(value.toDouble());
    }
    else if (property == "blur") {
        double radius = value.toDouble();
        if (radius > 0) {
            QGraphicsBlurEffect* blur = dynamic_cast<QGraphicsBlurEffect*>(item->graphicsEffect());
            if (!blur) {
                blur = new QGraphicsBlurEffect();
                item->setGraphicsEffect(blur);
            }
            blur->setBlurRadius(radius);
        }
        else {
            if (item->graphicsEffect()) {
                item->setGraphicsEffect(nullptr);
            }
        }
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

    // FIXED: Filter valid items
    QList<QGraphicsItem*> validItems;
    for (QGraphicsItem* item : items) {
        if (item && isItemValid(item)) {
            validItems.append(item);
        }
    }
    m_items = validItems;
}

GroupCommand::~GroupCommand()
{
    // FIXED: Safe cleanup
    if (!m_grouped && m_group && !m_group->scene()) {
        delete m_group;
    }
}

void GroupCommand::undo()
{
    if (!m_canvas || !m_group || !isItemValid(m_group)) {
        return;
    }

    QGraphicsScene* scene = m_canvas->scene();
    if (!scene) {
        return;
    }

    QList<QGraphicsItem*> children = m_group->childItems();

    // Ensure the temporary group item disappears from every tracking
    // structure before Qt destroys it so we never hold dangling pointers to
    // the soon-to-be-deleted group.
    m_canvas->removeItemFromAllFrames(m_group);

    scene->destroyItemGroup(m_group);
    m_group = nullptr;
    m_grouped = false;
    scene->clearSelection();

    for (QGraphicsItem* child : children) {
        if (isItemValid(child)) {
            child->setSelected(true);
        }
    }

    m_canvas->storeCurrentFrameState();
}

void GroupCommand::redo()
{
    if (m_canvas && m_canvas->scene() && !m_items.isEmpty()) {
        // Validate all items still exist in scene
        QList<QGraphicsItem*> validItems;
        for (QGraphicsItem* item : m_items) {
            if (item && isItemValid(item)) {
                validItems.append(item);
            }
        }

        if (validItems.size() >= 2) {
            m_group = m_canvas->scene()->createItemGroup(validItems);
            m_group->setFlag(QGraphicsItem::ItemIsSelectable, true);
            m_group->setFlag(QGraphicsItem::ItemIsMovable, true);
            m_canvas->addItemToCurrentLayer(m_group);
            m_canvas->scene()->clearSelection();
            m_group->setSelected(true);
            m_grouped = true;
            m_canvas->storeCurrentFrameState();
        }
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
        // Validate items before grouping
        QList<QGraphicsItem*> validItems;
        for (QGraphicsItem* item : m_items) {
            if (item && isItemValid(item)) {
                validItems.append(item);
            }
        }

        if (!validItems.isEmpty()) {
            m_group = m_canvas->scene()->createItemGroup(validItems);
            m_group->setFlag(QGraphicsItem::ItemIsSelectable, true);
            m_group->setFlag(QGraphicsItem::ItemIsMovable, true);
            m_canvas->addItemToCurrentLayer(m_group);
            m_canvas->scene()->clearSelection();
            m_group->setSelected(true);
            m_ungrouped = false;
            m_canvas->storeCurrentFrameState();
        }
    }
}

void UngroupCommand::redo()
{
    if (!m_canvas || !m_group || !isItemValid(m_group)) {
        return;
    }

    QGraphicsScene* scene = m_canvas->scene();
    if (!scene) {
        return;
    }

    QList<QGraphicsItem*> children = m_group->childItems();

    m_canvas->removeItemFromAllFrames(m_group);

    scene->destroyItemGroup(m_group);
    m_group = nullptr;
    scene->clearSelection();

    for (QGraphicsItem* child : children) {
        if (isItemValid(child)) {
            child->setSelected(true);
        }
    }

    m_ungrouped = true;
    m_canvas->storeCurrentFrameState();
}

// PropertyChangeCommand implementation
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
    if (m_item && isItemValid(m_item)) {
        applyProperty(m_item, m_property, m_oldValue);
        if (m_canvas) {
            m_canvas->storeCurrentFrameState();
        }
    }
}

void PropertyChangeCommand::redo()
{
    if (m_item && isItemValid(m_item)) {
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
        QPointF sceneCenter = item->mapToScene(item->boundingRect().center());
        item->setTransformOriginPoint(item->boundingRect().center());
        item->setPos(sceneCenter - item->transformOriginPoint());
        item->setRotation(value.toDouble());
    }
    else if (property == "scale") {
        QPointF scale = value.toPointF();
        QTransform transform;
        transform.scale(scale.x(), scale.y());
        item->setTransform(transform);
    }
    else if (property == "opacity") {
        item->setData(0, value.toDouble());
        item->setOpacity(value.toDouble());
    }
    else if (property == "zValue") {
        item->setZValue(value.toDouble());
    }
    else if (property == "visible") {
        item->setVisible(value.toBool());
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
    // ... other property handling (same as before)
}

// DrawCommand implementation
DrawCommand::DrawCommand(Canvas* canvas, QGraphicsItem* item, QUndoCommand* parent)
    : GraphicsItemCommand(canvas, parent)
    , m_item(item)
    , m_itemAdded(false)
{
    setText("Draw");
}

DrawCommand::~DrawCommand()
{
    // FIXED: Safe deletion
    if (!m_itemAdded && m_item && !m_item->scene()) {
        delete m_item;
        m_item = nullptr;
    }
}

AddKeyframeCommand::AddKeyframeCommand(Canvas* canvas, int layer, int frame, QUndoCommand* parent)
    : QUndoCommand(parent), m_canvas(canvas), m_layer(layer), m_frame(frame) {
    if (m_canvas) {
        m_previous = m_canvas->exportFrameData(layer, frame);
    }
}

void AddKeyframeCommand::redo() {
    if (m_canvas) {
        m_canvas->createKeyframe(m_frame);
    }
}

void AddKeyframeCommand::undo() {
    if (m_canvas) {
        m_canvas->removeKeyframe(m_layer, m_frame);
        m_canvas->importFrameData(m_layer, m_frame, m_previous);
    }
}

RemoveKeyframeCommand::RemoveKeyframeCommand(Canvas* canvas, int layer, int frame, QUndoCommand* parent)
    : QUndoCommand(parent), m_canvas(canvas), m_layer(layer), m_frame(frame) {
    if (m_canvas) {
        m_removed = m_canvas->exportFrameData(layer, frame);
    }
}

void RemoveKeyframeCommand::redo() {
    if (m_canvas) {
        m_canvas->removeKeyframe(m_layer, m_frame);
    }
}

void RemoveKeyframeCommand::undo() {
    if (m_canvas) {
        m_canvas->createKeyframe(m_frame);
        m_canvas->importFrameData(m_layer, m_frame, m_removed);
    }
}

void DrawCommand::undo()
{
    if (!m_canvas || !m_item) {
        return;
    }

    // Remove the drawn item from every bookkeeping structure before it is
    // potentially destroyed by the undo stack to avoid leaving dangling
    // pointers inside the canvas' frame caches.
    m_canvas->removeItemFromAllFrames(m_item);

    m_itemAdded = false;

    if (QGraphicsScene* scene = m_canvas->scene()) {
        if (m_item->scene() == scene) {
            scene->removeItem(m_item);
        }
        m_canvas->storeCurrentFrameState();
    }
}

void DrawCommand::redo()
{
    if (m_canvas && m_item && !m_item->scene()) {
        m_canvas->addItemToCurrentLayer(m_item);
        m_itemAdded = true;
        m_canvas->storeCurrentFrameState();
    }
}

AddKeyframeCommand::~AddKeyframeCommand() {}
RemoveKeyframeCommand::~RemoveKeyframeCommand() {}