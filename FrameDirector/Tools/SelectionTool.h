#ifndef SELECTIONTOOL_H
#define SELECTIONTOOL_H

#include "Tool.h"
#include <QGraphicsItem>
#include <QPointF>

class SelectionTool : public Tool
{
    Q_OBJECT

public:
    explicit SelectionTool(MainWindow* mainWindow, QObject* parent = nullptr);

    void mousePressEvent(QMouseEvent* event, const QPointF& scenePos) override;
    void mouseMoveEvent(QMouseEvent* event, const QPointF& scenePos) override;
    void mouseReleaseEvent(QMouseEvent* event, const QPointF& scenePos) override;
    QCursor getCursor() const override;

private:
    bool m_dragging;
    QPointF m_dragStart;
    QGraphicsItem* m_dragItem;
    QList<QGraphicsItem*> m_selectedItems;
    QPointF m_initialPos;
};

#endif // SELECTIONTOOL_H