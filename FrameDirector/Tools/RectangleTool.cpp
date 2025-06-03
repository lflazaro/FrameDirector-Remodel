// Tools/RectangleTool.cpp - Enhanced with undo support
#include "RectangleTool.h"
#include "../MainWindow.h"
#include "../Canvas.h"
#include "../Commands/UndoCommands.h"
#include <QGraphicsScene>
#include <QPen>
#include <QBrush>
#include <QUndoStack>

RectangleTool::RectangleTool(MainWindow* mainWindow, QObject* parent)
    : Tool(mainWindow, parent)
    , m_drawing(false)
    , m_currentRect(nullptr)
{
}

void RectangleTool::mousePressEvent(QMouseEvent* event, const QPointF& scenePos)
{
    if (!m_canvas) return;

    if (event->button() == Qt::LeftButton) {
        m_drawing = true;
        m_startPoint = scenePos;

        m_currentRect = new QGraphicsRectItem();
        m_currentRect->setRect(QRectF(scenePos, scenePos));

        QPen pen(m_canvas->getStrokeColor(), m_canvas->getStrokeWidth());
        QBrush brush(m_canvas->getFillColor());
        m_currentRect->setPen(pen);
        m_currentRect->setBrush(brush);

        m_currentRect->setFlags(QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIsMovable);

        // Add to scene temporarily for preview
        m_canvas->scene()->addItem(m_currentRect);
    }
}

void RectangleTool::mouseMoveEvent(QMouseEvent* event, const QPointF& scenePos)
{
    if (m_drawing && m_currentRect) {
        QRectF rect = QRectF(m_startPoint, scenePos).normalized();
        m_currentRect->setRect(rect);
    }
}

void RectangleTool::mouseReleaseEvent(QMouseEvent* event, const QPointF& scenePos)
{
    if (event->button() == Qt::LeftButton && m_drawing) {
        m_drawing = false;

        if (m_currentRect) {
            QRectF rect = m_currentRect->rect();
            if (rect.width() > 1 && rect.height() > 1) {
                // Remove from scene temporarily
                m_canvas->scene()->removeItem(m_currentRect);

                // Add through undo system
                if (m_mainWindow && m_mainWindow->m_undoStack) {
                    DrawCommand* command = new DrawCommand(m_canvas, m_currentRect);
                    m_mainWindow->m_undoStack->push(command);
                }
                else {
                    // Fallback: add directly
                    addItemToCanvas(m_currentRect);
                }
            }
            else {
                m_canvas->scene()->removeItem(m_currentRect);
                delete m_currentRect;
            }
            m_currentRect = nullptr;
        }
    }
}

QCursor RectangleTool::getCursor() const
{
    return Qt::CrossCursor;
}