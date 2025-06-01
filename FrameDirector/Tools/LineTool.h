#ifndef LINETOOL_H
#define LINETOOL_H

#include "Tool.h"
#include <QGraphicsLineItem>
#include <QPointF>

class LineTool : public Tool
{
    Q_OBJECT

public:
    explicit LineTool(MainWindow* mainWindow, QObject* parent = nullptr);

    void mousePressEvent(QMouseEvent* event, const QPointF& scenePos) override;
    void mouseMoveEvent(QMouseEvent* event, const QPointF& scenePos) override;
    void mouseReleaseEvent(QMouseEvent* event, const QPointF& scenePos) override;
    QCursor getCursor() const override;

private:
    bool m_drawing;
    QGraphicsLineItem* m_currentLine;
    QPointF m_startPoint;
};

#endif // LINETOOL_H