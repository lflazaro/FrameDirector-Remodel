#include "SelectionTool.h"
#include "../MainWindow.h"
#include "../Canvas.h"
#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QApplication>

SelectionTool::SelectionTool(MainWindow* mainWindow, QObject* parent)
    : Tool(mainWindow, parent)
    , m_dragging(false)
    , m_dragItem(nullptr)
{
}

void SelectionTool::mousePressEvent(QMouseEvent* event, const QPointF& scenePos)
{
    if (!m_canvas) return;

    QGraphicsScene* scene = m_canvas->scene();
    if (!scene) return;

    if (event->button() == Qt::LeftButton) {
        QGraphicsItem* item = scene->itemAt(scenePos, m_canvas->transform());

        if (item) {
            // Select item if not already selected
            if (!item->isSelected()) {
                if (!(event->modifiers() & Qt::ControlModifier)) {
                    scene->clearSelection();
                }
                item->setSelected(true);
            }

            // Start dragging
            m_dragging = true;
            m_dragStart = scenePos;
            m_dragItem = item;
            m_selectedItems = scene->selectedItems();

            // Store initial positions for undo/redo
            for (QGraphicsItem* selectedItem : m_selectedItems) {
                selectedItem->setData(0, selectedItem->pos());
            }
        }
        else {
            // Clear selection if clicking on empty space
            if (!(event->modifiers() & Qt::ControlModifier)) {
                scene->clearSelection();
            }
        }
    }
}

void SelectionTool::mouseMoveEvent(QMouseEvent* event, const QPointF& scenePos)
{
    if (m_dragging && !m_selectedItems.isEmpty()) {
        QPointF delta = scenePos - m_dragStart;

        for (QGraphicsItem* item : m_selectedItems) {
            QPointF initialPos = item->data(0).toPointF();
            item->setPos(initialPos + delta);
        }
    }
}

void SelectionTool::mouseReleaseEvent(QMouseEvent* event, const QPointF& scenePos)
{
    if (event->button() == Qt::LeftButton) {
        if (m_dragging) {
            m_dragging = false;
            m_dragItem = nullptr;
            m_selectedItems.clear();

            // TODO: Add move command to undo stack
        }
    }
}

QCursor SelectionTool::getCursor() const
{
    return Qt::ArrowCursor;
}