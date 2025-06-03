#ifndef ERASETOOL_H
#define ERASETOOL_H

#include "Tools/Tool.h"
#include <QGraphicsPathItem>
#include <QGraphicsEllipseItem>
#include <QPainterPath>
#include <QPointF>
#include <QLineF>
#include <QList>
#include <QGraphicsItem>

class EraseTool : public Tool
{
    Q_OBJECT

public:
    explicit EraseTool(MainWindow* mainWindow, QObject* parent = nullptr);

    void mousePressEvent(QMouseEvent* event, const QPointF& scenePos) override;
    void mouseMoveEvent(QMouseEvent* event, const QPointF& scenePos) override;
    void mouseReleaseEvent(QMouseEvent* event, const QPointF& scenePos) override;
    QCursor getCursor() const override;

    // Settings
    void setEraserSize(double size);
    void setEraserMode(int mode); // 0 = Vector Erase, 1 = Object Erase
    double getEraserSize() const;
    int getEraserMode() const;

    // FIXED: Add cleanup method for when tool is deactivated
    void cleanup();

private:
    struct EraseOperation {
        QGraphicsItem* originalItem;
        QGraphicsItem* newItem;
        bool itemDeleted;
    };

    // Core erasing methods
    void performErase(const QPointF& position);
    void vectorErase(const QPointF& position, double radius);
    void objectErase(const QPointF& position);

    // FIXED: Enhanced path processing that preserves stroke width
    bool quickIntersectionTest(const QPainterPath& path, const QPointF& center, double radius);
    QList<QPainterPath> eraseFromPath(const QPainterPath& path, const QPointF& center, double radius);
    QList<QPainterPath> eraseFromStrokedPath(const QPainterPath& path, const QPen& pen, const QPointF& center, double radius);

    // Geometric utilities
    QList<QLineF> pathToLineSegments(const QPainterPath& path);
    QPointF bezierPoint(const QPointF& p0, const QPointF& p1, const QPointF& p2, const QPointF& p3, qreal t);
    QList<QLineF> eraseFromLineSegment(const QLineF& segment, const QPointF& center, double radius);
    QPointF closestPointOnSegment(const QLineF& segment, const QPointF& point);
    QList<QPointF> lineCircleIntersection(const QLineF& line, const QPointF& center, double radius);
    QList<QPainterPath> lineSegmentsToPaths(const QList<QLineF>& segments);

    // Item management
    void copyItemProperties(QGraphicsItem* source, QGraphicsItem* target);
    void recordEraseOperation(QGraphicsItem* original, QGraphicsItem* newItem, bool deleted);
    void commitEraseOperations();

    // Visual feedback
    void updateErasePreview(const QPointF& position);
    void showErasePreview(const QPointF& position);
    void hideErasePreview();

    // Settings
    double m_eraserSize;
    int m_eraserMode;

    // State
    bool m_erasing;
    QPointF m_lastErasePos;
    QList<EraseOperation> m_currentOperations;
    QList<QGraphicsItem*> m_affectedItems;

    // Preview
    QGraphicsEllipseItem* m_previewCircle;
};

#endif // ERASETOOL_H