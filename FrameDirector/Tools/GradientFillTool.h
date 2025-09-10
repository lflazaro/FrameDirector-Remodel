#ifndef GRADIENTFILLTOOL_H
#define GRADIENTFILLTOOL_H

#include "Tools/Tool.h"
#include <QGradientStops>

class GradientFillTool : public Tool
{
    Q_OBJECT
public:
    explicit GradientFillTool(MainWindow* mainWindow, QObject* parent = nullptr);

    void mousePressEvent(QMouseEvent* event, const QPointF& scenePos) override;
    void mouseMoveEvent(QMouseEvent* event, const QPointF& scenePos) override;
    void mouseReleaseEvent(QMouseEvent* event, const QPointF& scenePos) override;
    QCursor getCursor() const override;
};

#endif // GRADIENTFILLTOOL_H