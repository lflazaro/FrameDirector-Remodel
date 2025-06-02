#ifndef BUCKETFILLTOOL_H
#define BUCKETFILLTOOL_H
#include "Tools/Tool.h"
#include <QGraphicsPathItem>
#include <QPainterPath>
#include <QPointF>
#include <QColor>
#include <QPixmap>
#include <QImage>
#include <QSet>
#include <QQueue>
#include <QGraphicsItem>

class BucketFillTool : public Tool
{
    Q_OBJECT

public:
    explicit BucketFillTool(MainWindow* mainWindow, QObject* parent = nullptr);

    void mousePressEvent(QMouseEvent* event, const QPointF& scenePos) override;
    void mouseMoveEvent(QMouseEvent* event, const QPointF& scenePos) override;
    void mouseReleaseEvent(QMouseEvent* event, const QPointF& scenePos) override;
    QCursor getCursor() const override;

    // Flood fill settings
    void setFillColor(const QColor& color);
    void setTolerance(int tolerance);
    void setFillMode(int mode); // 0 = Vector Fill, 1 = Raster Fill

private:
    struct PathSegment {
        QPainterPath path;
        QGraphicsPathItem* item;
        QRectF bounds;
    };

    struct ClosedRegion {
        QPainterPath outerBoundary;
        QList<QPainterPath> innerHoles;
        QRectF bounds;
        bool isValid;
    };

    // Vector-based filling methods
    ClosedRegion findEnclosedRegion(const QPointF& point);
    QList<PathSegment> collectNearbyPaths(const QPointF& center, qreal searchRadius = 50.0);
    QPainterPath mergeIntersectingPaths(const QList<PathSegment>& segments);
    QPainterPath createClosedPath(const QList<PathSegment>& segments, const QPointF& seedPoint);
    bool isPathClosed(const QPainterPath& path, qreal tolerance = 2.0);
    QPainterPath closeOpenPath(const QPainterPath& path, qreal tolerance = 5.0);

    // Collision detection methods
    bool pathsIntersect(const QPainterPath& path1, const QPainterPath& path2, qreal tolerance = 1.0);
    QList<QPointF> findPathIntersections(const QPainterPath& path1, const QPainterPath& path2);
    QPainterPath connectPathsByProximity(const QList<PathSegment>& segments, qreal maxDistance = 10.0);

    // Raster-based filling methods (fallback for complex cases)
    void performRasterFill(const QPointF& point);
    QImage renderSceneToImage(const QRectF& region, qreal scale = 2.0);
    QColor getPixelColor(const QImage& image, const QPoint& point);
    void floodFillImage(QImage& image, const QPoint& startPoint, const QColor& targetColor, const QColor& fillColor);
    QPainterPath traceFilledRegion(const QImage& image, const QColor& fillColor);

    // Advanced shape detection
    QPainterPath detectShape(const QPointF& point);
    bool isPointInsideEnclosedArea(const QPointF& point, const QList<PathSegment>& paths);
    QPainterPath createBoundingShape(const QList<PathSegment>& segments);

    // Optimization methods
    QList<PathSegment> optimizePathSegments(const QList<PathSegment>& segments);
    QPainterPath simplifyPath(const QPainterPath& path, qreal tolerance = 1.0);
    void cacheNearbyItems(const QRectF& region);

    // Fill creation methods
    QGraphicsPathItem* createFillItem(const QPainterPath& fillPath, const QColor& color);
    void addFillToCanvas(QGraphicsPathItem* fillItem);

    // Settings
    QColor m_fillColor;
    int m_tolerance;
    int m_fillMode; // 0 = Vector, 1 = Raster
    qreal m_searchRadius;
    qreal m_connectionTolerance;

    // Cache for performance
    QList<PathSegment> m_cachedPaths;
    QRectF m_cachedRegion;
    bool m_cacheValid;

    // Visual feedback
    void showFillPreview(const QPainterPath& path);
    void hideFillPreview();
    QGraphicsPathItem* m_previewItem;

    // Debug visualization
    void debugDrawPaths(const QList<PathSegment>& segments);
    void debugDrawIntersections(const QList<QPointF>& intersections);
    bool m_debugMode;
};

#endif // BUCKETFILLTOOL_H