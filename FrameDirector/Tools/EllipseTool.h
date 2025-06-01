#ifndef ELLIPSETOOL_H
#define ELLIPSETOOL_H

#include "Tool.h"
#include <QGraphicsEllipseItem>
#include <QPointF>
#include <QRectF>

class EllipseTool : public Tool
{
    Q_OBJECT

public:
    explicit EllipseTool(MainWindow* mainWindow, QObject* parent = nullptr);

    void mousePressEvent(QMouseEvent* event, const QPointF& scenePos) override;
    void mouseMoveEvent(QMouseEvent* event, const QPointF& scenePos) override;
    void mouseReleaseEvent(QMouseEvent* event, const QPointF& scenePos) override;
    QCursor getCursor() const override;

private:
    bool m_drawing;
    QGraphicsEllipseItem* m_currentEllipse;
    QPointF m_startPoint;
};

#endif // ELLIPSETOOL_H