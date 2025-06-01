#include "DrawingTool.h"
#include "../MainWindow.h"
#include "../Canvas.h"
#include <QGraphicsScene>
#include <QPen>

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
        m_canvas->scene()->addItem(m_currentPath);
    }
}

void DrawingTool::mouseMoveEvent(QMouseEvent* event, const QPointF& scenePos)
{
    if (m_drawing && m_currentPath) {
        m_path.lineTo(scenePos);
        m_currentPath->setPath(m_path);
        m_lastPoint = scenePos;
    }
}

void DrawingTool::mouseReleaseEvent(QMouseEvent* event, const QPointF& scenePos)
{
    if (event->button() == Qt::LeftButton && m_drawing) {
        m_drawing = false;

        if (m_currentPath) {
            emit itemCreated(m_currentPath);
            m_currentPath = nullptr;
        }

        m_path = QPainterPath();
    }
}

QCursor DrawingTool::getCursor() const
{
    return Qt::CrossCursor;
}