#ifndef TEXTTOOL_H
#define TEXTTOOL_H

#include "Tool.h"
#include <QGraphicsTextItem>
#include <QPointF>
#include <QFont>

class TextTool : public Tool
{
    Q_OBJECT

public:
    explicit TextTool(MainWindow* mainWindow, QObject* parent = nullptr);

    void mousePressEvent(QMouseEvent* event, const QPointF& scenePos) override;
    void mouseMoveEvent(QMouseEvent* event, const QPointF& scenePos) override;
    void mouseReleaseEvent(QMouseEvent* event, const QPointF& scenePos) override;
    QCursor getCursor() const override;

private:
    QGraphicsTextItem* m_currentText;
    QFont m_font;
};

#endif