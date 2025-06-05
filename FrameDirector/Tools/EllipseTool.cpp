// Tools/EllipseTool.cpp - Enhanced with undo support
#include "EllipseTool.h"
#include "../MainWindow.h"
#include "../Canvas.h"
#include "../Commands/UndoCommands.h"
#include <QGraphicsScene>
#include <QPen>
#include <QBrush>
#include <QUndoStack>

EllipseTool::EllipseTool(MainWindow* mainWindow, QObject* parent)
    : Tool(mainWindow, parent)
    , m_drawing(false)
    , m_currentEllipse(nullptr)
{
}

void EllipseTool::mousePressEvent(QMouseEvent* event, const QPointF& scenePos)
{
    if (!m_canvas) return;

    int currentLayer = m_canvas->getCurrentLayer();
    int currentFrame = m_canvas->getCurrentFrame();

    // Check if drawing is allowed
    if (!canDrawOnCurrentFrame(m_canvas, currentLayer, currentFrame)) {
        return;
    }

    // Auto-convert extended frame to keyframe
    checkAutoConversion(m_canvas, currentLayer, currentFrame);
    if (event->button() == Qt::LeftButton) {
        m_drawing = true;
        m_startPoint = scenePos;

        m_currentEllipse = new QGraphicsEllipseItem();
        m_currentEllipse->setRect(QRectF(scenePos, scenePos));

        QPen pen(m_canvas->getStrokeColor(), m_canvas->getStrokeWidth());
        QBrush brush(m_canvas->getFillColor());
        m_currentEllipse->setPen(pen);
        m_currentEllipse->setBrush(brush);

        m_currentEllipse->setFlags(QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIsMovable);

        // Add to scene temporarily for preview
        m_canvas->scene()->addItem(m_currentEllipse);
    }
}

void EllipseTool::mouseMoveEvent(QMouseEvent* event, const QPointF& scenePos)
{
    if (m_drawing && m_currentEllipse) {
        QRectF rect = QRectF(m_startPoint, scenePos).normalized();
        m_currentEllipse->setRect(rect);
    }
}

void EllipseTool::mouseReleaseEvent(QMouseEvent* event, const QPointF& scenePos)
{
    if (event->button() == Qt::LeftButton && m_drawing) {
        m_drawing = false;

        if (m_currentEllipse) {
            QRectF rect = m_currentEllipse->rect();
            if (rect.width() > 1 && rect.height() > 1) {
                // Remove from scene temporarily
                m_canvas->scene()->removeItem(m_currentEllipse);

                // Add through undo system
                if (m_mainWindow && m_mainWindow->m_undoStack) {
                    DrawCommand* command = new DrawCommand(m_canvas, m_currentEllipse);
                    m_mainWindow->m_undoStack->push(command);
                }
                else {
                    // Fallback: add directly
                    addItemToCanvas(m_currentEllipse);
                }
            }
            else {
                m_canvas->scene()->removeItem(m_currentEllipse);
                delete m_currentEllipse;
            }
            m_currentEllipse = nullptr;
        }
    }
}

QCursor EllipseTool::getCursor() const
{
    return Qt::CrossCursor;
}