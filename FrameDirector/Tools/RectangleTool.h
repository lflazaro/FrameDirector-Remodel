#ifndef RECTANGLETOOL_H
#define RECTANGLETOOL_H

#include "Tool.h"
#include <QGraphicsRectItem>
#include <QPointF>
#include <QRectF>

class RectangleTool : public Tool
{
    Q_OBJECT

public:
    explicit RectangleTool(MainWindow* mainWindow, QObject* parent = nullptr);

    void mousePressEvent(QMouseEvent* event, const QPointF& scenePos) override;
    void mouseMoveEvent(QMouseEvent* event, const QPointF& scenePos) override;
    void mouseReleaseEvent(QMouseEvent* event, const QPointF& scenePos) override;
    QCursor getCursor() const override;

private:
    bool m_drawing;
    QGraphicsRectItem* m_currentRect;
    QPointF m_startPoint;
};

#endif // RECTANGLETOOL_H