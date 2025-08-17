// Commands/UndoCommands.h - Enhanced Header with item validation
#ifndef UNDOCOMMANDS_H
#define UNDOCOMMANDS_H

#include "../Common/FrameTypes.h"
#include <QUndoCommand>
#include <QGraphicsItem>

class Canvas;

using namespace FrameDirector;

// Base command for graphics items
class GraphicsItemCommand : public QUndoCommand
{
public:
    explicit GraphicsItemCommand(Canvas* canvas, QUndoCommand* parent = nullptr);

protected:
    Canvas* m_canvas;

    // FIXED: Add item validation helper
    bool isItemValid(QGraphicsItem* item);
};

// Move command
class MoveCommand : public GraphicsItemCommand
{
public:
    MoveCommand(Canvas* canvas, const QList<QGraphicsItem*>& items,
        const QPointF& delta, QUndoCommand* parent = nullptr);

    void undo() override;
    void redo() override;
    bool mergeWith(const QUndoCommand* other) override;
    int id() const override { return 1; }

private:
    QList<QGraphicsItem*> m_items;
    QPointF m_delta;
    bool m_firstTime;
};

// Add item command
class AddItemCommand : public GraphicsItemCommand
{
public:
    AddItemCommand(Canvas* canvas, QGraphicsItem* item, QUndoCommand* parent = nullptr);
    ~AddItemCommand();

    void undo() override;
    void redo() override;

private:
    QGraphicsItem* m_item;
    bool m_itemAdded;
};

// Remove item command
class RemoveItemCommand : public GraphicsItemCommand
{
public:
    RemoveItemCommand(Canvas* canvas, const QList<QGraphicsItem*>& items,
        QUndoCommand* parent = nullptr);
    ~RemoveItemCommand();

    void undo() override;
    void redo() override;

private:
    QList<QGraphicsItem*> m_items;
    bool m_itemsRemoved;
};

// Transform command (rotate, scale)
class TransformCommand : public GraphicsItemCommand
{
public:
    TransformCommand(Canvas* canvas, QGraphicsItem* item,
        const QTransform& oldTransform, const QTransform& newTransform,
        QUndoCommand* parent = nullptr);

    void undo() override;
    void redo() override;

private:
    QGraphicsItem* m_item;
    QTransform m_oldTransform;
    QTransform m_newTransform;
};

// Style change command
class StyleChangeCommand : public GraphicsItemCommand
{
public:
    StyleChangeCommand(Canvas* canvas, QGraphicsItem* item,
        const QString& property, const QVariant& oldValue,
        const QVariant& newValue, QUndoCommand* parent = nullptr);

    void undo() override;
    void redo() override;

private:
    void applyStyle(QGraphicsItem* item, const QString& property, const QVariant& value);

    QGraphicsItem* m_item;
    QString m_property;
    QVariant m_oldValue;
    QVariant m_newValue;
};

// Group/Ungroup commands
class GroupCommand : public GraphicsItemCommand
{
public:
    GroupCommand(Canvas* canvas, const QList<QGraphicsItem*>& items,
        QUndoCommand* parent = nullptr);
    ~GroupCommand();

    void undo() override;
    void redo() override;

private:
    QList<QGraphicsItem*> m_items;
    QGraphicsItemGroup* m_group;
    bool m_grouped;
};

class UngroupCommand : public GraphicsItemCommand
{
public:
    UngroupCommand(Canvas* canvas, QGraphicsItemGroup* group,
        QUndoCommand* parent = nullptr);

    void undo() override;
    void redo() override;

private:
    QGraphicsItemGroup* m_group;
    QList<QGraphicsItem*> m_items;
    bool m_ungrouped;
};

// Property change command for properties panel
class PropertyChangeCommand : public GraphicsItemCommand
{
public:
    PropertyChangeCommand(Canvas* canvas, QGraphicsItem* item,
        const QString& property, const QVariant& oldValue, const QVariant& newValue,
        QUndoCommand* parent = nullptr);

    void undo() override;
    void redo() override;

private:
    void applyProperty(QGraphicsItem* item, const QString& property, const QVariant& value);

    QGraphicsItem* m_item;
    QString m_property;
    QVariant m_oldValue;
    QVariant m_newValue;
};

// Draw command for drawing operations
class DrawCommand : public GraphicsItemCommand
{
public:
    DrawCommand(Canvas* canvas, QGraphicsItem* item, QUndoCommand* parent = nullptr);
    ~DrawCommand();

    void undo() override;
    void redo() override;

private:
    QGraphicsItem* m_item;
    bool m_itemAdded;
};

// Keyframe commands
class AddKeyframeCommand : public QUndoCommand
{
public:
    AddKeyframeCommand(Canvas* canvas, int layer, int frame, QUndoCommand* parent = nullptr);
    ~AddKeyframeCommand();
    void undo() override;
    void redo() override;

private:
    Canvas* m_canvas;
    int m_layer;
    int m_frame;
    FrameData m_previous;
};

class RemoveKeyframeCommand : public QUndoCommand
{
public:
    RemoveKeyframeCommand(Canvas* canvas, int layer, int frame, QUndoCommand* parent = nullptr);
    ~RemoveKeyframeCommand();
    void undo() override;
    void redo() override;

private:
    Canvas* m_canvas;
    int m_layer;
    int m_frame;
    FrameData m_removed;
};

#endif // UNDOCOMMANDS_H