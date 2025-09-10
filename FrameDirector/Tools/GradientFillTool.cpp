#include "GradientFillTool.h"
#include "MainWindow.h"
#include "Canvas.h"
#include "GradientDialog.h"
#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QAbstractGraphicsShapeItem>
#include <QLinearGradient>

GradientFillTool::GradientFillTool(MainWindow* mainWindow, QObject* parent)
    : Tool(mainWindow, parent)
{
}

void GradientFillTool::mousePressEvent(QMouseEvent* event, const QPointF& scenePos)
{
    Q_UNUSED(scenePos);
    if (!m_canvas || !m_canvas->scene()) return;
    if (event->button() != Qt::LeftButton) return;

    QList<QGraphicsItem*> selected = m_canvas->scene()->selectedItems();
    if (selected.isEmpty()) return;

    QGradientStops stops;
    QColor fill = m_canvas->getFillColor();
    stops << QGradientStop(0.0, fill) << QGradientStop(1.0, fill);

    GradientDialog dlg(stops, m_canvas);
    if (dlg.exec() == QDialog::Accepted) {
        QGradientStops chosen = dlg.getStops();
        for (QGraphicsItem* item : selected) {
            if (auto shape = qgraphicsitem_cast<QAbstractGraphicsShapeItem*>(item)) {
                QRectF bounds = shape->boundingRect();
                QLinearGradient grad(bounds.topLeft(), bounds.topRight());
                grad.setStops(chosen);
                shape->setBrush(QBrush(grad));
            }
        }
        m_canvas->scene()->update();
    }
}

void GradientFillTool::mouseMoveEvent(QMouseEvent*, const QPointF&)
{
}

void GradientFillTool::mouseReleaseEvent(QMouseEvent*, const QPointF&)
{
}

QCursor GradientFillTool::getCursor() const
{
    return QCursor(Qt::PointingHandCursor);
}