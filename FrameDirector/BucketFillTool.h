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
#include <QTimer>
#include <QRectF>

class BucketFillTool : public Tool
{
    Q_OBJECT

public:
    explicit BucketFillTool(MainWindow* mainWindow, QObject* parent = nullptr);

    void mousePressEvent(QMouseEvent* event, const QPointF& scenePos) override;
    void mouseMoveEvent(QMouseEvent* event, const QPointF& scenePos) override;
    void mouseReleaseEvent(QMouseEvent* event, const QPointF& scenePos) override;
    QCursor getCursor() const override;

    // Settings and configuration
    void setFillColor(const QColor& color);
    void setTolerance(int tolerance);
    void setFillMode(int mode); // 0 = Vector Fill, 1 = Raster Fill
    void setSearchRadius(qreal radius);
    void setConnectionTolerance(qreal tolerance);
    void setDebugMode(bool enabled);

    // Getters
    QColor getFillColor() const;
    int getTolerance() const;
    int getFillMode() const;
    qreal getSearchRadius() const;
    qreal getConnectionTolerance() const;
    bool isDebugMode() const;
    void hideFillPreview();

private:
    // Data structures for vector filling
    struct PathSegment {
        QPainterPath path;        // Original item path (centerline or fill geometry)
        QPainterPath geometry;    // Stroked/fill shape used for boolean operations
        QGraphicsItem* item;
        QRectF bounds;
        qreal strokeWidth;
        bool hasStroke;
        bool hasFill;
        bool isClosed;

        PathSegment()
            : item(nullptr)
            , strokeWidth(0.0)
            , hasStroke(false)
            , hasFill(false)
            , isClosed(false)
        {
        }
    };

    struct ClosedRegion {
        QPainterPath outerBoundary;
        QList<QPainterPath> innerHoles;
        QRectF bounds;
        bool isValid;

        ClosedRegion() : isValid(false) {}
    };

    // Note: Contour tracing uses simple QPoint lists for better performance

    // Vector-based filling methods
    ClosedRegion findEnclosedRegion(const QPointF& point);
    QList<PathSegment> collectNearbyPaths(const QPointF& center, qreal searchRadius = 50.0);
    ClosedRegion composeRegionFromSegments(const QList<PathSegment>& segments,
        const QPointF& seedPoint, qreal gapPadding);
    QPainterPath mergeIntersectingPaths(const QList<PathSegment>& segments);
    QPainterPath createClosedPath(const QList<PathSegment>& segments, const QPointF& seedPoint);
    bool isPathClosed(const QPainterPath& path, qreal tolerance = 2.0);
    QPainterPath closeOpenPath(const QPainterPath& path, qreal tolerance = 5.0);

    // New: create stroked outlines of vector paths and merge them. This helps
    // detect shapes that are closed only when multiple strokes are considered.
    QPainterPath strokeAndMergePaths(const QList<PathSegment>& segments, qreal strokeWidth = 6.0);

    // Path connection and optimization
    bool pathsIntersect(const QPainterPath& path1, const QPainterPath& path2, qreal tolerance = 1.0);
    QList<QPointF> findPathIntersections(const QPainterPath& path1, const QPainterPath& path2);
    QPainterPath connectPathsByProximity(const QList<PathSegment>& segments, qreal maxDistance = 10.0);
    QList<PathSegment> optimizePathSegments(const QList<PathSegment>& segments);
    QPainterPath simplifyPath(const QPainterPath& path, qreal tolerance = 1.0);

    // Raster-based filling methods
    void performRasterFill(const QPointF& point);
    QImage renderSceneToImage(const QRectF& region, qreal scale = 2.0);
    QColor getPixelColor(const QImage& image, const QPoint& point);
    void floodFillImage(QImage& image, const QPoint& startPoint,
        const QColor& targetColor, const QColor& fillColor);
    int floodFillImageLimited(QImage& image, const QPoint& startPoint,
        const QColor& targetColor, const QColor& fillColor, int maxPixels);

    // Advanced contour tracing (Moore neighborhood algorithm)
    QPainterPath traceFilledRegion(const QImage& image, const QColor& fillColor);
    QPainterPath traceContour(const QImage& image, const QPoint& startPoint, const QColor& fillColor);
    QPoint findStartPoint(const QImage& image, const QColor& fillColor);
    QList<QPoint> getNeighbors8(const QPoint& point);
    QPoint getNextNeighbor(const QPoint& current, int direction);
    int getDirection(const QPoint& from, const QPoint& to);
    QPainterPath pointsToPath(const QList<QPoint>& points);
    QPainterPath smoothContour(const QPainterPath& roughPath, qreal smoothing = 2.0);

    // Shape detection and analysis
    QPainterPath detectShape(const QPointF& point);
    bool isPointInsideEnclosedArea(const QPointF& point, const QList<PathSegment>& paths);
    QPainterPath createBoundingShape(const QList<PathSegment>& segments);

    // Fill creation and management
    QGraphicsPathItem* createFillItem(const QPainterPath& fillPath, const QColor& color);
    void addFillToCanvas(QGraphicsPathItem* fillItem);

    // Visual feedback and preview
    void showFillPreview(const QPainterPath& path);

    // Performance optimization
    void cacheNearbyItems(const QRectF& region);
    void clearCache();

    // Debug visualization (can be disabled in release)
    void debugDrawPaths(const QList<PathSegment>& segments);
    void debugDrawIntersections(const QList<QPointF>& intersections);
    void debugDrawContour(const QList<QPoint>& contour);

    // Settings
    QColor m_fillColor;
    int m_tolerance;
    int m_fillMode; // 0 = Vector, 1 = Raster
    qreal m_searchRadius;
    qreal m_connectionTolerance;
    bool m_debugMode;

    // Cache for performance
    QList<PathSegment> m_cachedPaths;
    QRectF m_cachedRegion;
    bool m_cacheValid;

    // Visual feedback
    QGraphicsPathItem* m_previewItem;

    // Constants for contour tracing
    static const int DIRECTION_COUNT = 8;
    static const QPoint DIRECTIONS[8];
};

#endif // BUCKETFILLTOOL_H