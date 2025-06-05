// Tools/Tool.h
#ifndef TOOL_H
#define TOOL_H

#include <QObject>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QCursor>
#include <QPointF>
#include <QGraphicsItem>
#include <QPen>
#include <QBrush>

class MainWindow;
class Canvas;

class Tool : public QObject
{
    Q_OBJECT

public:
    explicit Tool(MainWindow* mainWindow, QObject* parent = nullptr);
    virtual ~Tool() = default;

    virtual void mousePressEvent(QMouseEvent* event, const QPointF& scenePos) = 0;
    virtual void mouseMoveEvent(QMouseEvent* event, const QPointF& scenePos) = 0;
    virtual void mouseReleaseEvent(QMouseEvent* event, const QPointF& scenePos) = 0;
    virtual void keyPressEvent(QKeyEvent* event) {}
    virtual QCursor getCursor() const { return Qt::ArrowCursor; }

signals:
    void itemCreated(QGraphicsItem* item);
    void toolFinished();

protected:
    // Helper method for tools to add items to the current layer
    void addItemToCanvas(QGraphicsItem* item);

    MainWindow* m_mainWindow;
    Canvas* m_canvas;
};

#endif