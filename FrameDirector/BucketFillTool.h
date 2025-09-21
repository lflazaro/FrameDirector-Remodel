#ifndef BUCKETFILLTOOL_H
#define BUCKETFILLTOOL_H

#include "Tools/Tool.h"

#include <QColor>
#include <QCursor>
#include <QGraphicsItem>
#include <QGraphicsPathItem>
#include <QImage>
#include <QPainterPath>
#include <QPoint>
#include <QPointF>
#include <QQueue>
#include <QRectF>
#include <QSet>
#include <QTimer>

class BucketFillTool : public Tool
{
    Q_OBJECT

public:
    explicit BucketFillTool(MainWindow* mainWindow, QObject* parent = nullptr);

    // Tool overrides
    void mousePressEvent(QMouseEvent* event, const QPointF& scenePos) override;
    void mouseMoveEvent(QMouseEvent* event, const QPointF& scenePos) override;
    void mouseReleaseEvent(QMouseEvent* event, const QPointF& scenePos) override;
    QCursor getCursor() const override;

    // Settings and configuration
    void setFillColor(const QColor& color) { m_fillColor = color; }
    void setTolerance(int tolerance) { m_tolerance = tolerance; }
    // 0 = Vector-first (enhanced), 1 = Raster probe
    void setFillMode(int mode) { m_fillMode = mode; }
    void setSearchRadius(qreal radius) { m_searchRadius = radius; }
    void setConnectionTolerance(qreal tolerance) { m_connectionTolerance = tolerance; }
    void setDebugMode(bool enabled) { m_debugMode = enabled; }

    // Getters
    QColor getFillColor() const { return m_fillColor; }
    int getTolerance() const { return m_tolerance; }
    int getFillMode() const { return m_fillMode; }
    qreal getSearchRadius() const { return m_searchRadius; }
    qreal getConnectionTolerance() const { return m_connectionTolerance; }
    bool isDebugMode() const { return m_debugMode; }

    // Preview
    void hideFillPreview();

private:
    // ===== Core data structures =====
    struct PathSegment {
        QPainterPath path;
        QGraphicsItem* item;
        QRectF bounds;
        PathSegment() : item(nullptr) {}
    };

    struct ClosedRegion {
        QPainterPath outerBoundary;
        QList<QPainterPath> innerHoles;
        QRectF bounds;
        bool isValid;
        ClosedRegion() : isValid(false) {}
    };

    // Extended segment used by the enhanced engine
    struct PathSegmentEx {
        QPainterPath path;
        QPainterPath shape;          // full drawable shape (scene coordinates)
        QGraphicsItem* item = nullptr;
        QRectF bounds;
        qreal distanceToPoint = 0.0;
        qreal strokeWidth = 1.0;     // cached stroke width for barrier expansion
        bool hasFill = false;        // whether the original item carried a fill
    };

    // ===== Enhanced vector region detection =====
    ClosedRegion findEnclosedRegionEnhanced(const QPointF& point);
    QList<PathSegmentEx> collectPathsInRadius(const QPointF& center, qreal radius);
    PathSegmentEx extractPathFromItem(QGraphicsItem* item);
    QPainterPath connectPathsAdvanced(const QList<PathSegmentEx>& segments, const QPointF& seedPoint);
    qreal calculateMinimumDistance(const QPainterPath& a, const QPainterPath& b);
    void dfsCollectComponent(int index,
        const QVector<QVector<bool>>& adjacency,
        QVector<bool>& visited,
        QList<int>& component);
    QPainterPath mergeComponentPaths(const QList<PathSegmentEx>& segments, const QList<int>& indices);
    QPainterPath closePathIntelligently(const QPainterPath& path);
    QPainterPath findBridgingPath(const QPainterPath& path);
    QPainterPath createSinglePointPath(const QPointF& p);
    ClosedRegion findEnclosureByRayCasting(const QPointF& point, const QList<PathSegmentEx>& paths);
    QPointF findNearestIntersection(const QPointF& origin, const QPointF& dir, const QList<PathSegmentEx>& paths);

    ClosedRegion resolveRegionByMask(const QPointF& point, const QList<PathSegmentEx>& segments, const QRectF& canvasRect);
    QRectF uniteSegmentBounds(const QList<PathSegmentEx>& segments) const;
    bool touchesImageBorder(const QImage& image, const QColor& fillColor) const;

    // Smoothing (Catmull–Rom) for polygonal boundary
    QPainterPath smoothPath(const QPainterPath& path, qreal smoothingFactor = 2.0);
    QPainterPath smoothPolygon(const QPainterPath& path, qreal tension = 2.0) { return smoothPath(path, tension); }
    QPointF catmullRomInterpolate(const QPointF& p0, const QPointF& p1,
        const QPointF& p2, const QPointF& p3, qreal t);

    // Validation
    bool isValidFillRegion(const ClosedRegion& region, const QRectF& canvasRect);

    // ===== Raster fallback (ARGB-safe) =====
    void performEnhancedRasterFill(const QPointF& point);
    qreal calculateAdaptiveFillSize(const QPointF& point);
    bool colorsMatch(const QColor& c1, const QColor& c2, int tolerance);
    bool shouldFill(const QColor& target, const QColor& fill);

    // Flood fill (enhanced) + cleanup
    int enhancedFloodFill(QImage& image, const QPoint& start,
        const QColor& targetColor,
        const QColor& fillColor,
        int maxPixels);
    void cleanupEdgePixel(QImage& image, const QPoint& pos, const QColor& fillColor);

    // Raster ? Vector tracing
    QPainterPath traceFilledRegionEnhanced(const QImage& image, const QColor& fillColor);

    // ===== Legacy / compatibility helpers (used by existing code paths) =====
    ClosedRegion findEnclosedRegion(const QPointF& point);
    QList<PathSegment> collectNearbyPaths(const QPointF& center, qreal searchRadius = 50.0);
    QPainterPath mergeIntersectingPaths(const QList<PathSegment>& segments);
    QPainterPath createClosedPath(const QList<PathSegment>& segments, const QPointF& seedPoint);
    bool isPathClosed(const QPainterPath& path, qreal tolerance = 2.0) const;
    QPainterPath closeOpenPath(const QPainterPath& path, qreal tolerance = 5.0);

    bool pathsIntersect(const QPainterPath& path1, const QPainterPath& path2, qreal tolerance = 1.0);
    QList<QPointF> findPathIntersections(const QPainterPath& path1, const QPainterPath& path2);
    QPainterPath connectPathsByProximity(const QList<PathSegment>& segments, qreal maxDistance = 10.0);
    QList<PathSegment> optimizePathSegments(const QList<PathSegment>& segments);
    QPainterPath simplifyPath(const QPainterPath& path, qreal tolerance = 1.0);

    // Basic raster helpers (kept for compatibility; enhanced versions are preferred)
    void performRasterFill(const QPointF& point);
    QImage renderSceneToImage(const QRectF& region, qreal scale = 2.0) const;
    QColor getPixelColor(const QImage& image, const QPoint& point) const;
    void floodFillImage(QImage& image, const QPoint& startPoint,
        const QColor& targetColor, const QColor& fillColor);
    int floodFillImageLimited(QImage& image, const QPoint& startPoint,
        const QColor& targetColor, const QColor& fillColor, int maxPixels);

    // Older contour tracing names routed to the enhanced tracer
    QPainterPath traceFilledRegion(const QImage& image, const QColor& fillColor);
    QPainterPath traceContour(const QImage& image, const QPoint& startPoint, const QColor& fillColor);
    QPoint findStartPoint(const QImage& image, const QColor& fillColor);
    QList<QPoint> getNeighbors8(const QPoint& point);
    QPoint getNextNeighbor(const QPoint& current, int direction);
    int getDirection(const QPoint& from, const QPoint& to);
    QPainterPath pointsToPath(const QList<QPoint>& points);
    QPainterPath smoothContour(const QPainterPath& roughPath, qreal smoothing = 2.0);

    // Shape detection (optional utilities)
    QPainterPath detectShape(const QPointF& point);
    bool isPointInsideEnclosedArea(const QPointF& point, const QList<PathSegment>& paths);
    QPainterPath createBoundingShape(const QList<PathSegment>& segments);

    // Fill creation and scene management
    QGraphicsPathItem* createFillItem(const QPainterPath& fillPath, const QColor& color);
    void addFillToCanvas(QGraphicsPathItem* fillItem);

    // Visual feedback
    void showFillPreview(const QPainterPath& path);

    // Performance cache (can be no-op depending on your use)
    void cacheNearbyItems(const QRectF& region);
    void clearCache();

    // Debug drawing (optional)
    void debugDrawPaths(const QList<PathSegment>& segments);
    void debugDrawIntersections(const QList<QPointF>& intersections);
    void debugDrawContour(const QList<QPoint>& contour);

private:
    // ===== Settings =====
    QColor m_fillColor;
    int    m_tolerance;              // color tolerance for raster probe
    int    m_fillMode;               // 0 = Vector-first, 1 = Raster-first
    qreal  m_searchRadius;           // base search radius for vector detection
    qreal  m_connectionTolerance;    // allowed gap to auto-close paths
    bool   m_debugMode;

    // Cache
    QList<PathSegment> m_cachedPaths;
    QRectF m_cachedRegion;
    bool   m_cacheValid;

    // Preview item
    QGraphicsPathItem* m_previewItem;

    // Directions (8-connected neighborhood)
    static const int DIRECTION_COUNT = 8;
    static const QPoint DIRECTIONS[8];
};

#endif // BUCKETFILLTOOL_H
