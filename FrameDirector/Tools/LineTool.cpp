
#include "LineTool.h"
#include "../MainWindow.h"
#include "../Canvas.h"
#include <QGraphicsScene>
#include <QPen>

LineTool::LineTool(MainWindow* mainWindow, QObject* parent)
    : Tool(mainWindow, parent)
    , m_drawing(false)
    , m_currentLine(nullptr)
{
}

void LineTool::mousePressEvent(QMouseEvent* event, const QPointF& scenePos)
{
    if (!m_canvas) return;

    if (event->button() == Qt::LeftButton) {
        m_drawing = true;
        m_startPoint = scenePos;

        m_currentLine = new QGraphicsLineItem();
        m_currentLine->setLine(QLineF(scenePos, scenePos));

        QPen pen(m_canvas->getStrokeColor(), m_canvas->getStrokeWidth());
        pen.setCapStyle(Qt::RoundCap);
        m_currentLine->setPen(pen);

        m_currentLine->setFlags(QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIsMovable);
        m_canvas->scene()->addItem(m_currentLine);
    }
}

void LineTool::mouseMoveEvent(QMouseEvent* event, const QPointF& scenePos)
{
    if (m_drawing && m_currentLine) {
        m_currentLine->setLine(QLineF(m_startPoint, scenePos));
    }
}

void LineTool::mouseReleaseEvent(QMouseEvent* event, const QPointF& scenePos)
{
    if (event->button() == Qt::LeftButton && m_drawing) {
        m_drawing = false;

        if (m_currentLine) {
            emit itemCreated(m_currentLine);
            m_currentLine = nullptr;
        }
    }
}

QCursor LineTool::getCursor() const
{
    return Qt::CrossCursor;
}