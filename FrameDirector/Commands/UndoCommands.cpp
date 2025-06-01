// Commands/UndoCommands.cpp
#include "UndoCommands.h"
#include "../Canvas.h"
#include <QGraphicsScene>
#include <QGraphicsItemGroup>

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
    }
}

void AddItemCommand::redo()
{
    if (m_canvas && m_canvas->scene() && m_item) {
        m_canvas->scene()->addItem(m_item);
        m_itemAdded = true;
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
                m_canvas->scene()->addItem(item);
            }
        }
        m_itemsRemoved = false;
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
    }
}