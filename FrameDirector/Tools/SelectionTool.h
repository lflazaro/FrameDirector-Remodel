// Tools/SelectionTool.h - Enhanced with comprehensive undo support
#ifndef SELECTIONTOOL_H
#define SELECTIONTOOL_H

#include "Tool.h"
#include <QGraphicsItem>
#include <QPointF>
#include <QMenu>
#include <QKeyEvent>
#include <QHash>

class SelectionTool : public Tool
{
    Q_OBJECT

public:
    explicit SelectionTool(MainWindow* mainWindow, QObject* parent = nullptr);

    void mousePressEvent(QMouseEvent* event, const QPointF& scenePos) override;
    void mouseMoveEvent(QMouseEvent* event, const QPointF& scenePos) override;
    void mouseReleaseEvent(QMouseEvent* event, const QPointF& scenePos) override;
    void keyPressEvent(QKeyEvent* event) override;
    QCursor getCursor() const override;

private slots:
    void deleteSelectedItems();
    void groupSelectedItems();
    void ungroupSelectedItems();
    void duplicateSelectedItems();

private:
    void moveSelectedItems(const QPointF& delta, bool largeStep = false);
    void showContextMenu(const QPoint& globalPos);
    void updateSelectionHandles();

    // Dragging state
    bool m_dragging;
    QPointF m_dragStart;
    QGraphicsItem* m_dragItem;
    QList<QGraphicsItem*> m_selectedItems;

    // Store initial positions for undo support
    QHash<QGraphicsItem*, QPointF> m_initialPositions;
};

#endif // SELECTIONTOOL_H