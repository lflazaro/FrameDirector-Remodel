#include "DrawingTool.h"
#include "../MainWindow.h"
#include "../Canvas.h"
#include "../Commands/UndoCommands.h"
#include <QGraphicsScene>
#include <QPen>
#include <QUndoStack>
#include <QDebug>

DrawingTool::DrawingTool(MainWindow* mainWindow, QObject* parent)
    : Tool(mainWindow, parent)
    , m_drawing(false)
    , m_currentPath(nullptr)
{
}

void DrawingTool::mousePressEvent(QMouseEvent* event, const QPointF& scenePos)
{
    if (!m_canvas) return;

    if (event->button() == Qt::LeftButton) {
        m_drawing = true;
        m_path = QPainterPath();
        m_path.moveTo(scenePos);
        m_lastPoint = scenePos;

        m_currentPath = new QGraphicsPathItem();
        m_currentPath->setPath(m_path);

        QPen pen(m_canvas->getStrokeColor(), m_canvas->getStrokeWidth());
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);
        m_currentPath->setPen(pen);

        m_currentPath->setFlags(QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIsMovable);

        // Add to scene temporarily for preview
        m_canvas->scene()->addItem(m_currentPath);

        qDebug() << "DrawingTool: Started drawing at" << scenePos;
    }
}

void DrawingTool::mouseMoveEvent(QMouseEvent* event, const QPointF& scenePos)
{
    if (m_drawing && m_currentPath) {
        // Only add points that are far enough apart to avoid too many tiny segments
        qreal distance = QLineF(m_lastPoint, scenePos).length();
        if (distance >= 2.0) { // Minimum distance threshold
            m_path.lineTo(scenePos);
            m_currentPath->setPath(m_path);
            m_lastPoint = scenePos;
        }
    }
}

void DrawingTool::mouseReleaseEvent(QMouseEvent* event, const QPointF& scenePos)
{
    if (event->button() == Qt::LeftButton && m_drawing) {
        m_drawing = false;

        if (m_currentPath) {
            // Check if we have a valid path (more than just a single point)
            if (m_path.elementCount() > 1) {
                // Remove from scene temporarily
                m_canvas->scene()->removeItem(m_currentPath);

                // Add through undo system
                if (m_mainWindow && m_mainWindow->m_undoStack) {
                    DrawCommand* command = new DrawCommand(m_canvas, m_currentPath);
                    m_mainWindow->m_undoStack->push(command);
                    qDebug() << "DrawingTool: Added drawing to undo stack";
                }
                else {
                    // Fallback: add directly if undo system not available
                    qDebug() << "DrawingTool: Warning - undo stack not available, adding directly";
                    addItemToCanvas(m_currentPath);
                }
            }
            else {
                // Path too short, delete it
                m_canvas->scene()->removeItem(m_currentPath);
                delete m_currentPath;
                qDebug() << "DrawingTool: Path too short, discarded";
            }

            m_currentPath = nullptr;
        }

        m_path = QPainterPath();
    }
}

QCursor DrawingTool::getCursor() const
{
    return Qt::CrossCursor;
}