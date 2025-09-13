// Tools/SelectionTool.cpp - Enhanced with comprehensive undo support
#include "SelectionTool.h"
#include "../Common/FrameTypes.h"
#include "../MainWindow.h"
#include "../Canvas.h"
#include "../Commands/UndoCommands.h"
#include "../VectorGraphics/VectorGraphicsItem.h"

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

            // Check if this item can be moved
            if (item->flags() & QGraphicsItem::ItemIsMovable) {
                // Start dragging
                m_dragging = true;
                m_dragStart = scenePos;
                m_dragItem = item;
                m_selectedItems = scene->selectedItems();

                // Store initial positions for undo/redo
                m_initialPositions.clear();
                for (QGraphicsItem* selectedItem : m_selectedItems) {
                    m_initialPositions[selectedItem] = selectedItem->pos();
                }

                qDebug() << "SelectionTool: Started dragging" << m_selectedItems.size() << "items";
            }
        }
        else {
            // Clear selection if clicking on empty space
            if (!(event->modifiers() & Qt::ControlModifier)) {
                scene->clearSelection();
            }
        }
    }
    else if (event->button() == Qt::RightButton) {
        // Right-click context menu
        showContextMenu(event->globalPosition().toPoint());
    }

    updateSelectionHandles();
}

void SelectionTool::mouseMoveEvent(QMouseEvent* event, const QPointF& scenePos)
{
    if (m_dragging && !m_selectedItems.isEmpty()) {
        QPointF delta = scenePos - m_dragStart;

        // Move all selected items
        for (QGraphicsItem* item : m_selectedItems) {
            QPointF initialPos = m_initialPositions[item];
            item->setPos(initialPos + delta);
        }
    }
}

void SelectionTool::mouseReleaseEvent(QMouseEvent* event, const QPointF& scenePos)
{
    if (event->button() == Qt::LeftButton) {
        if (m_dragging) {
            QPointF totalDelta = scenePos - m_dragStart;

            // Only create undo command if items actually moved
            if (totalDelta.manhattanLength() > 2.0) { // Minimum movement threshold
                // Create move command for undo system
                if (m_mainWindow && m_mainWindow->m_undoStack && !m_selectedItems.isEmpty()) {
                    MoveCommand* moveCommand = new MoveCommand(m_canvas, m_selectedItems, totalDelta);
                    m_mainWindow->m_undoStack->push(moveCommand);
                    qDebug() << "SelectionTool: Created move command for" << m_selectedItems.size() << "items";
                }

                // Update canvas frame state to save the move
                if (m_canvas) {
                    m_canvas->storeCurrentFrameState();
                }
            }
            else {
                // Reset positions if movement was too small
                for (QGraphicsItem* item : m_selectedItems) {
                    item->setPos(m_initialPositions[item]);
                }
            }

            m_dragging = false;
            m_dragItem = nullptr;
            m_selectedItems.clear();
            m_initialPositions.clear();
        }
    }

    if (m_canvas && event->button() == Qt::LeftButton) {
        if (m_canvas->getFrameType(m_canvas->getCurrentFrame()) == FrameDirector::FrameType::ExtendedFrame) {
            m_canvas->convertCurrentExtendedFrameToKeyframe();
        }
        m_canvas->saveStateAfterTransform();
    }

}

void SelectionTool::keyPressEvent(QKeyEvent* event)
{
    if (!m_canvas || !m_canvas->scene()) return;

    QList<QGraphicsItem*> selectedItems = m_canvas->scene()->selectedItems();

    switch (event->key()) {
    case Qt::Key_Delete:
    case Qt::Key_Backspace:
        deleteSelectedItems();
        break;

    case Qt::Key_Left:
        moveSelectedItems(QPointF(-1, 0), event->modifiers() & Qt::ShiftModifier);
        break;

    case Qt::Key_Right:
        moveSelectedItems(QPointF(1, 0), event->modifiers() & Qt::ShiftModifier);
        break;

    case Qt::Key_Up:
        moveSelectedItems(QPointF(0, -1), event->modifiers() & Qt::ShiftModifier);
        break;

    case Qt::Key_Down:
        moveSelectedItems(QPointF(0, 1), event->modifiers() & Qt::ShiftModifier);
        break;

    case Qt::Key_G:
        if (event->modifiers() & Qt::ControlModifier) {
            groupSelectedItems();
        }
        break;

    case Qt::Key_U:
        if (event->modifiers() & Qt::ControlModifier) {
            ungroupSelectedItems();
        }
        break;

    default:
        Tool::keyPressEvent(event);
    }
}

void SelectionTool::deleteSelectedItems()
{
    if (!m_canvas || !m_canvas->scene()) return;

    QList<QGraphicsItem*> selectedItems = m_canvas->scene()->selectedItems();
    if (selectedItems.isEmpty()) return;

    // Create remove command for undo system
    if (m_mainWindow && m_mainWindow->m_undoStack) {
        RemoveItemCommand* removeCommand = new RemoveItemCommand(m_canvas, selectedItems);
        m_mainWindow->m_undoStack->push(removeCommand);
        qDebug() << "SelectionTool: Created remove command for" << selectedItems.size() << "items";
    }
    else {
        // Fallback: delete directly
        for (QGraphicsItem* item : selectedItems) {
            m_canvas->scene()->removeItem(item);
            delete item;
        }

        if (m_canvas) {
            m_canvas->storeCurrentFrameState();
        }
    }

    updateSelectionHandles();
}

void SelectionTool::moveSelectedItems(const QPointF& delta, bool largeStep)
{
    if (!m_canvas || !m_canvas->scene()) return;

    QList<QGraphicsItem*> selectedItems = m_canvas->scene()->selectedItems();
    if (selectedItems.isEmpty()) return;

    QPointF actualDelta = delta;
    if (largeStep) {
        actualDelta *= 10; // Move 10 pixels instead of 1
    }

    // Create move command for undo system
    if (m_mainWindow && m_mainWindow->m_undoStack) {
        MoveCommand* moveCommand = new MoveCommand(m_canvas, selectedItems, actualDelta);
        m_mainWindow->m_undoStack->push(moveCommand);
        qDebug() << "SelectionTool: Created keyboard move command";
    }
    else {
        // Fallback: move directly
        for (QGraphicsItem* item : selectedItems) {
            item->setPos(item->pos() + actualDelta);
        }

        if (m_canvas) {
            m_canvas->storeCurrentFrameState();
        }
    }
}

void SelectionTool::updateSelectionHandles()
{
    if (!m_canvas || !m_canvas->scene()) return;

    for (QGraphicsItem* item : m_canvas->scene()->items()) {
        if (auto vgItem = dynamic_cast<VectorGraphicsItem*>(item)) {
            vgItem->setShowSelectionHandles(item->isSelected());
        }
    }
}

void SelectionTool::groupSelectedItems()
{
    if (!m_canvas || !m_canvas->scene()) return;

    QList<QGraphicsItem*> selectedItems = m_canvas->scene()->selectedItems();
    if (selectedItems.size() < 2) return;

    // Create group command for undo system
    if (m_mainWindow && m_mainWindow->m_undoStack) {
        GroupCommand* groupCommand = new GroupCommand(m_canvas, selectedItems);
        m_mainWindow->m_undoStack->push(groupCommand);
        qDebug() << "SelectionTool: Created group command for" << selectedItems.size() << "items";
    }
    else {
        // Fallback: group directly
        QGraphicsItemGroup* group = m_canvas->scene()->createItemGroup(selectedItems);
        group->setFlag(QGraphicsItem::ItemIsSelectable, true);
        group->setFlag(QGraphicsItem::ItemIsMovable, true);

        if (m_canvas) {
            m_canvas->storeCurrentFrameState();
        }
    }

    updateSelectionHandles();
}

void SelectionTool::ungroupSelectedItems()
{
    if (!m_canvas || !m_canvas->scene()) return;

    QList<QGraphicsItem*> selectedItems = m_canvas->scene()->selectedItems();

    for (QGraphicsItem* item : selectedItems) {
        QGraphicsItemGroup* group = qgraphicsitem_cast<QGraphicsItemGroup*>(item);
        if (group) {
            // Create ungroup command for undo system
            if (m_mainWindow && m_mainWindow->m_undoStack) {
                UngroupCommand* ungroupCommand = new UngroupCommand(m_canvas, group);
                m_mainWindow->m_undoStack->push(ungroupCommand);
                qDebug() << "SelectionTool: Created ungroup command";
            }
            else {
                // Fallback: ungroup directly
                m_canvas->scene()->destroyItemGroup(group);

                if (m_canvas) {
                    m_canvas->storeCurrentFrameState();
                }
            }
            break; // Only ungroup one at a time
        }
    }

    updateSelectionHandles();
}
void SelectionTool::showContextMenu(const QPoint& globalPos)
{
    if (!m_canvas || !m_canvas->scene()) return;

    QList<QGraphicsItem*> selectedItems = m_canvas->scene()->selectedItems();

    QMenu contextMenu;
    contextMenu.setStyleSheet(
        "QMenu {"
        "    background-color: #3E3E42;"
        "    color: #FFFFFF;"
        "    border: 1px solid #5A5A5C;"
        "    border-radius: 3px;"
        "}"
        "QMenu::item {"
        "    padding: 8px 16px;"
        "    border: none;"
        "}"
        "QMenu::item:selected {"
        "    background-color: #4A4A4F;"
        "}"
        "QMenu::separator {"
        "    height: 1px;"
        "    background-color: #5A5A5C;"
        "    margin: 4px 8px;"
        "}"
    );

    if (!selectedItems.isEmpty()) {
        // Item-specific context menu
        QAction* cutAction = contextMenu.addAction("Cut");
        QAction* copyAction = contextMenu.addAction("Copy");
        contextMenu.addSeparator();

        QAction* deleteAction = contextMenu.addAction("Delete");
        contextMenu.addSeparator();

        if (selectedItems.size() > 1) {
            QAction* groupAction = contextMenu.addAction("Group");
            connect(groupAction, &QAction::triggered, this, &SelectionTool::groupSelectedItems);
        }

        // Check for groups in selection
        bool hasGroups = false;
        for (QGraphicsItem* item : selectedItems) {
            if (qgraphicsitem_cast<QGraphicsItemGroup*>(item)) {
                hasGroups = true;
                break;
            }
        }

        if (hasGroups) {
            QAction* ungroupAction = contextMenu.addAction("Ungroup");
            connect(ungroupAction, &QAction::triggered, this, &SelectionTool::ungroupSelectedItems);
        }

        contextMenu.addSeparator();

        // Arrange menu
        QMenu* arrangeMenu = contextMenu.addMenu("Arrange");
        QAction* bringToFrontAction = arrangeMenu->addAction("Bring to Front");
        QAction* bringForwardAction = arrangeMenu->addAction("Bring Forward");
        QAction* sendBackwardAction = arrangeMenu->addAction("Send Backward");
        QAction* sendToBackAction = arrangeMenu->addAction("Send to Back");

        // Connect actions
        connect(cutAction, &QAction::triggered, [this]() {
            if (m_mainWindow) {
                m_mainWindow->cut();
            }
            });

        connect(copyAction, &QAction::triggered, [this]() {
            if (m_mainWindow) {
                m_mainWindow->copy();
            }
            });

        connect(deleteAction, &QAction::triggered, this, &SelectionTool::deleteSelectedItems);

        connect(bringToFrontAction, &QAction::triggered, m_mainWindow, &MainWindow::bringToFront);
        connect(bringForwardAction, &QAction::triggered, m_mainWindow, &MainWindow::bringForward);
        connect(sendBackwardAction, &QAction::triggered, m_mainWindow, &MainWindow::sendBackward);
        connect(sendToBackAction, &QAction::triggered, m_mainWindow, &MainWindow::sendToBack);
    }
    else {
        // General canvas context menu
        QAction* pasteAction = contextMenu.addAction("Paste");
        pasteAction->setEnabled(m_mainWindow && m_mainWindow->hasClipboardItems());
        contextMenu.addSeparator();
        QAction* selectAllAction = contextMenu.addAction("Select All");

        connect(pasteAction, &QAction::triggered, [this]() {
            if (m_mainWindow) {
                m_mainWindow->paste();
            }
            });

        connect(selectAllAction, &QAction::triggered, [this]() {
            if (m_canvas) {
                m_canvas->selectAll();
            }
            });
    }

    contextMenu.exec(globalPos);
}

void SelectionTool::duplicateSelectedItems()
{
    if (!m_canvas || !m_canvas->scene()) return;

    QList<QGraphicsItem*> selectedItems = m_canvas->scene()->selectedItems();
    if (selectedItems.isEmpty()) return;

    QList<QGraphicsItem*> duplicatedItems;

    for (QGraphicsItem* item : selectedItems) {
        QGraphicsItem* duplicate = nullptr;

        // Create duplicates based on item type
        if (auto rectItem = qgraphicsitem_cast<QGraphicsRectItem*>(item)) {
            auto newRect = new QGraphicsRectItem(rectItem->rect());
            newRect->setPen(rectItem->pen());
            newRect->setBrush(rectItem->brush());
            newRect->setTransform(rectItem->transform());
            duplicate = newRect;
        }
        else if (auto ellipseItem = qgraphicsitem_cast<QGraphicsEllipseItem*>(item)) {
            auto newEllipse = new QGraphicsEllipseItem(ellipseItem->rect());
            newEllipse->setPen(ellipseItem->pen());
            newEllipse->setBrush(ellipseItem->brush());
            newEllipse->setTransform(ellipseItem->transform());
            duplicate = newEllipse;
        }
        else if (auto lineItem = qgraphicsitem_cast<QGraphicsLineItem*>(item)) {
            auto newLine = new QGraphicsLineItem(lineItem->line());
            newLine->setPen(lineItem->pen());
            newLine->setTransform(lineItem->transform());
            duplicate = newLine;
        }
        else if (auto pathItem = qgraphicsitem_cast<QGraphicsPathItem*>(item)) {
            auto newPath = new QGraphicsPathItem(pathItem->path());
            newPath->setPen(pathItem->pen());
            newPath->setBrush(pathItem->brush());
            newPath->setTransform(pathItem->transform());
            duplicate = newPath;
        }
        else if (auto textItem = qgraphicsitem_cast<QGraphicsTextItem*>(item)) {
            auto newText = new QGraphicsTextItem(textItem->toPlainText());
            newText->setFont(textItem->font());
            newText->setDefaultTextColor(textItem->defaultTextColor());
            newText->setTransform(textItem->transform());
            duplicate = newText;
        }

        if (duplicate) {
            // Position duplicate slightly offset
            duplicate->setPos(item->pos() + QPointF(10, 10));
            duplicate->setFlags(item->flags());
            duplicate->setZValue(item->zValue());
            duplicatedItems.append(duplicate);
        }
    }

    if (!duplicatedItems.isEmpty()) {
        // Add duplicated items through undo system
        if (m_mainWindow && m_mainWindow->m_undoStack) {
            // Create a compound command for all duplicated items
            m_mainWindow->m_undoStack->beginMacro("Duplicate Items");
            for (QGraphicsItem* item : duplicatedItems) {
                AddItemCommand* addCommand = new AddItemCommand(m_canvas, item);
                m_mainWindow->m_undoStack->push(addCommand);
            }
            m_mainWindow->m_undoStack->endMacro();

            qDebug() << "SelectionTool: Duplicated" << duplicatedItems.size() << "items";
        }

        // Select the duplicated items
        m_canvas->scene()->clearSelection();
        for (QGraphicsItem* item : duplicatedItems) {
            item->setSelected(true);
        }
    }
}

QCursor SelectionTool::getCursor() const
{
    return Qt::ArrowCursor;
}