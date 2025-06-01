#ifndef DRAWINGTOOL_H
#define DRAWINGTOOL_H

#include "Tool.h"
#include <QGraphicsPathItem>
#include <QPainterPath>
#include <QPointF>

class DrawingTool : public Tool
{
    Q_OBJECT

public:
    explicit DrawingTool(MainWindow* mainWindow, QObject* parent = nullptr);

    void mousePressEvent(QMouseEvent* event, const QPointF& scenePos) override;
    void mouseMoveEvent(QMouseEvent* event, const QPointF& scenePos) override;
    void mouseReleaseEvent(QMouseEvent* event, const QPointF& scenePos) override;
    QCursor getCursor() const override;

private:
    bool m_drawing;
    QGraphicsPathItem* m_currentPath;
    QPainterPath m_path;
    QPointF m_lastPoint;
};

#endif // DRAWINGTOOL_H