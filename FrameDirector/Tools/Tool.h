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

    void checkAutoConversion(Canvas* canvas, int layer, int frame)
    {
        if (canvas && canvas->isExtendedFrame(frame, layer)) {
            qDebug() << "Auto-converting extended frame before drawing";
            canvas->convertExtendedFrameToKeyframe(frame, layer);
        }
    }

    // NEW: Check if drawing is allowed
    bool canDrawOnCurrentFrame(Canvas* canvas, int layer, int frame)
    {
        if (!canvas) return false;

        if (!canvas->canDrawOnFrame(frame, layer)) {
            // Show warning about tweening
            QMessageBox::information(nullptr, "Drawing Disabled",
                "Cannot draw on tweened frames. Remove tweening first or create a new keyframe.");
            return false;
        }

        return true;
    }
    MainWindow* m_mainWindow;
    Canvas* m_canvas;
};

#endif