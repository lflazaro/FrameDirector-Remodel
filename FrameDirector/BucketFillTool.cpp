#include "BucketFillTool.h"
#include "MainWindow.h"
#include "Canvas.h"
#include "Commands/UndoCommands.h"

#include <QApplication>
#include <QAbstractGraphicsShapeItem>
#include <QColor>
#include <QDebug>
#include <QElapsedTimer>
#include <QGraphicsEllipseItem>
#include <QGraphicsItem>
#include <QGraphicsLineItem>
#include <QGraphicsPathItem>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QImage>
#include <QLineF>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QQueue>
#include <QTimer>
#include <QtMath>
#include <algorithm>
#include <limits>
#include <cmath>
#include <vector>
#include <cstdint>

// ===============================
// Direction vectors (8-neighborhood)
// ===============================
const QPoint BucketFillTool::DIRECTIONS[8] = {
    QPoint(1, 0),   // East
    QPoint(1, 1),   // Southeast
    QPoint(0, 1),   // South
    QPoint(-1, 1),  // Southwest
    QPoint(-1, 0),  // West
    QPoint(-1, -1), // Northwest
    QPoint(0, -1),  // North
    QPoint(1, -1)   // Northeast
};

// ===============================
// Construction
// ===============================
BucketFillTool::BucketFillTool(MainWindow* mainWindow, QObject* parent)
    : Tool(mainWindow, parent)
    , m_fillColor(Qt::red)
    , m_tolerance(10)
    , m_fillMode(0)
    , m_searchRadius(300.0)
    , m_connectionTolerance(15.0)
    , m_debugMode(false)
    , m_cacheValid(false)
    , m_previewItem(nullptr)
{
    if (m_canvas) {
        m_fillColor = m_canvas->getFillColor();
    }
    qDebug() << "BucketFillTool created with enhanced shape recognition";
}

// ===============================
// Events
// ===============================
void BucketFillTool::mousePressEvent(QMouseEvent* event, const QPointF& scenePos)
{
    if (!m_canvas || !m_canvas->scene()) return;

    if (event->button() == Qt::LeftButton) {
        m_fillColor = m_canvas->getFillColor();

        QElapsedTimer timer;
        timer.start();

        qDebug() << "BucketFill: Starting enhanced fill at" << scenePos;
        hideFillPreview();

        const QRectF canvasRect = m_canvas->getCanvasRect();
        if (!canvasRect.contains(scenePos)) {
            qDebug() << "BucketFill: Click outside canvas bounds";
            return;
        }

        try {
            if (m_fillMode == 0) {
                // Vector-first approach
                ClosedRegion region = findEnclosedRegionEnhanced(scenePos);

                if (region.isValid && !region.outerBoundary.isEmpty()) {
                    if (isValidFillRegion(region, canvasRect)) {
                        if (QGraphicsPathItem* fillItem = createFillItem(region.outerBoundary, m_fillColor)) {
                            addFillToCanvas(fillItem);
                            qDebug() << "Vector fill completed in" << timer.elapsed() << "ms";
                        }
                    }
                    else {
                        qDebug() << "Region validation failed";
                    }
                }
                else {
                    // Raster probe -> vector tracing fallback
                    qDebug() << "No vector region found, using enhanced raster fill";
                    performEnhancedRasterFill(scenePos);
                }
            }
            else {
                // Explicit raster mode
                performEnhancedRasterFill(scenePos);
            }

            qDebug() << "Fill operation completed in" << timer.elapsed() << "ms";
        }
        catch (const std::exception& e) {
            qDebug() << "BucketFill: Exception:" << e.what();
        }
    }
}

// ===============================
// Vector region detection (enhanced)
// ===============================
BucketFillTool::ClosedRegion BucketFillTool::findEnclosedRegionEnhanced(const QPointF& point)
{
    ClosedRegion region;
    region.isValid = false;

    if (m_canvas && !m_canvas->getCanvasRect().contains(point)) {
        return region;
    }

    // Collect nearby paths; expand search if sparse
    QList<PathSegmentEx> nearbyPaths;
    qreal currentRadius = m_searchRadius;
    for (int attempt = 0; attempt < 3 && nearbyPaths.size() < 4; ++attempt) {
        nearbyPaths = collectPathsInRadius(point, currentRadius);
        currentRadius *= 1.5;
    }
    if (nearbyPaths.isEmpty()) {
        qDebug() << "No paths found even with expanded search";
        return region;
    }

    // 1) Quick win: closed shape that contains the point
    for (const PathSegmentEx& seg : nearbyPaths) {
        if (isPathClosed(seg.path, m_connectionTolerance * 2) && seg.path.contains(point)) {
            region.outerBoundary = seg.path;
            region.bounds = seg.bounds;
            region.isValid = true;
            qDebug() << "Found simple enclosed shape";
            return region;
        }
    }

    // 2) mask-based extraction for complex/large regions
    ClosedRegion maskRegion = resolveRegionByMask(point, nearbyPaths, m_canvas->getCanvasRect());
    if (maskRegion.isValid) {
        qDebug() << "Resolved region via barrier mask";
        return maskRegion;
    }

    // 3) connect components
    {
        QPainterPath connected = connectPathsAdvanced(nearbyPaths, point);
        if (!connected.isEmpty() && connected.contains(point)) {
            region.outerBoundary = connected;
            region.bounds = connected.boundingRect();
            region.isValid = true;
            qDebug() << "Created connected path from segments";
            return region;
        }
    }

    // 4) ray-cast synthesized enclosure
    region = findEnclosureByRayCasting(point, nearbyPaths);
    return region;
}

QList<BucketFillTool::PathSegmentEx>
BucketFillTool::collectPathsInRadius(const QPointF& center, qreal radius)
{
    QList<PathSegmentEx> segments;
    if (!m_canvas || !m_canvas->scene()) return segments;

    QRectF searchRect(center.x() - radius, center.y() - radius, radius * 2, radius * 2);
    searchRect = searchRect.intersected(m_canvas->getCanvasRect());

    const QList<QGraphicsItem*> items = m_canvas->scene()->items(searchRect, Qt::IntersectsItemBoundingRect);
    for (QGraphicsItem* item : items) {
        // Skip "background" or intentionally hidden low-z items (project-specific)
        if (item->zValue() <= -999) continue;

        PathSegmentEx seg = extractPathFromItem(item);
        if (!seg.path.isEmpty()) {
            seg.distanceToPoint = QLineF(center, seg.bounds.center()).length();
            segments.append(seg);
        }
    }

    std::sort(segments.begin(), segments.end(),
        [](const PathSegmentEx& a, const PathSegmentEx& b) { return a.distanceToPoint < b.distanceToPoint; });
    return segments;
}

BucketFillTool::PathSegmentEx BucketFillTool::extractPathFromItem(QGraphicsItem* item)
{
    PathSegmentEx seg;
    if (!item) return seg;

    seg.item = item;

    const QTransform sceneTransform = item->sceneTransform();

    auto assignMapped = [&](const QPainterPath& path, const QRectF& bounds) {
        seg.path = sceneTransform.map(path);
        seg.bounds = sceneTransform.mapRect(bounds);
    };

    if (auto pathItem = qgraphicsitem_cast<QGraphicsPathItem*>(item)) {
        assignMapped(pathItem->path(), pathItem->boundingRect());
    }
    else if (auto rectItem = qgraphicsitem_cast<QGraphicsRectItem*>(item)) {
        QPainterPath p; p.addRect(rectItem->rect());
        assignMapped(p, rectItem->boundingRect());
    }
    else if (auto ellipseItem = qgraphicsitem_cast<QGraphicsEllipseItem*>(item)) {
        QPainterPath p; p.addEllipse(ellipseItem->rect());
        assignMapped(p, ellipseItem->boundingRect());
    }
    else if (auto lineItem = qgraphicsitem_cast<QGraphicsLineItem*>(item)) {
        const QLineF line = lineItem->line();
        const qreal width = qMax<qreal>(lineItem->pen().widthF(), 2.0);
        QPointF n(line.dy(), -line.dx());
        const qreal len = std::hypot(n.x(), n.y());

        QPainterPath p;
        if (len > 0) {
            n /= len; n *= (width * 0.5);
            p.moveTo(line.p1() + n);
            p.lineTo(line.p2() + n);
            p.lineTo(line.p2() - n);
            p.lineTo(line.p1() - n);
            p.closeSubpath();
        }
        else {
            p.moveTo(line.p1());
            p.lineTo(line.p2());
        }
        assignMapped(p, p.boundingRect());
    }

    if (seg.path.isEmpty()) {
        seg.path = item->mapToScene(item->shape());
        seg.bounds = seg.path.boundingRect();
    }

    seg.shape = item->mapToScene(item->shape());
    if (seg.shape.isEmpty()) seg.shape = seg.path;

    seg.bounds = seg.shape.boundingRect();

    if (auto shapeItem = qgraphicsitem_cast<QAbstractGraphicsShapeItem*>(item)) {
        seg.strokeWidth = shapeItem->pen().widthF();
        if (shapeItem->pen().isCosmetic())
            seg.strokeWidth = qMax(seg.strokeWidth, 1.0);
        if (seg.strokeWidth <= 0.0) seg.strokeWidth = 1.0;
        seg.hasFill = shapeItem->brush().style() != Qt::NoBrush
            && shapeItem->brush().color().alpha() > 0;
    }
    else if (auto lineItem = qgraphicsitem_cast<QGraphicsLineItem*>(item)) {
        seg.strokeWidth = qMax<qreal>(lineItem->pen().widthF(), 1.0);
    }

    return seg;
}

QPainterPath BucketFillTool::connectPathsAdvanced(const QList<PathSegmentEx>& segments, const QPointF& seedPoint)
{
    if (segments.isEmpty()) return QPainterPath();

    const int n = segments.size();
    QVector<QVector<bool>> connectivity(n, QVector<bool>(n, false));

    // Pairwise proximity graph
    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            const qreal d = calculateMinimumDistance(segments[i].path, segments[j].path);
            if (d <= m_connectionTolerance * 2) {
                connectivity[i][j] = connectivity[j][i] = true;
            }
        }
    }

    // Connected components
    QVector<bool> visited(n, false);
    QList<QList<int>> components;
    for (int i = 0; i < n; ++i) {
        if (!visited[i]) {
            QList<int> comp;
            dfsCollectComponent(i, connectivity, visited, comp);
            components.append(comp);
        }
    }

    // Merge each component, try to close small gaps, and test containment
    for (const QList<int>& comp : components) {
        QPainterPath merged = mergeComponentPaths(segments, comp);
        if (merged.isEmpty()) continue;

        merged = closePathIntelligently(merged);
        if (merged.contains(seedPoint)) return merged;
    }

    return QPainterPath();
}

qreal BucketFillTool::calculateMinimumDistance(const QPainterPath& path1, const QPainterPath& path2)
{
    qreal minDist = std::numeric_limits<qreal>::max();
    const int S = 24;

    for (int i = 0; i <= S; ++i) {
        const QPointF p1 = path1.pointAtPercent(i / qreal(S));
        for (int j = 0; j <= S; ++j) {
            const QPointF p2 = path2.pointAtPercent(j / qreal(S));
            const qreal d = QLineF(p1, p2).length();
            if (d < minDist) minDist = d;
            if (minDist <= 1.0) return minDist; // early exit
        }
    }
    return minDist;
}

void BucketFillTool::dfsCollectComponent(int index,
    const QVector<QVector<bool>>& adjacency,
    QVector<bool>& visited,
    QList<int>& component)
{
    visited[index] = true;
    component.append(index);
    for (int i = 0; i < adjacency[index].size(); ++i) {
        if (adjacency[index][i] && !visited[i]) {
            dfsCollectComponent(i, adjacency, visited, component);
        }
    }
}

QPainterPath BucketFillTool::mergeComponentPaths(const QList<PathSegmentEx>& segments,
    const QList<int>& componentIndices)
{
    if (componentIndices.isEmpty()) return QPainterPath();

    QPainterPath merged = segments[componentIndices[0]].path;
    for (int i = 1; i < componentIndices.size(); ++i) {
        merged = merged.united(segments[componentIndices[i]].path);
    }
    return merged;
}

QPainterPath BucketFillTool::closePathIntelligently(const QPainterPath& path)
{
    if (path.isEmpty()) return path;

    const QPointF s = path.pointAtPercent(0.0);
    const QPointF e = path.pointAtPercent(1.0);
    const qreal gap = QLineF(s, e).length();

    if (gap <= m_connectionTolerance * 3) {
        QPainterPath closed = path;
        closed.lineTo(s);
        closed.closeSubpath();
        return closed;
    }

    return findBridgingPath(path);
}

QPainterPath BucketFillTool::findBridgingPath(const QPainterPath& path)
{
    // Try to bridge start/end through a nearby path
    const QPointF s = path.pointAtPercent(0.0);
    const QPointF e = path.pointAtPercent(1.0);

    QList<PathSegmentEx> bridges = collectPathsInRadius((s + e) / 2.0, m_connectionTolerance * 5);
    for (const PathSegmentEx& b : bridges) {
        const qreal ds = calculateMinimumDistance(createSinglePointPath(s), b.path);
        const qreal de = calculateMinimumDistance(createSinglePointPath(e), b.path);
        if (ds <= m_connectionTolerance && de <= m_connectionTolerance) {
            QPainterPath u = path.united(b.path);
            u.closeSubpath();
            return u;
        }
    }

    // Direct connection if no bridge found
    QPainterPath u = path;
    u.lineTo(s);
    u.closeSubpath();
    return u;
}

QPainterPath BucketFillTool::createSinglePointPath(const QPointF& point)
{
    QPainterPath p; p.addEllipse(point, 1, 1); return p;
}

BucketFillTool::ClosedRegion BucketFillTool::findEnclosureByRayCasting(
    const QPointF& point, const QList<PathSegmentEx>& paths)
{
    ClosedRegion region; region.isValid = false;

    const int numRays = 16;
    QList<QPointF> boundaryPoints; boundaryPoints.reserve(numRays);

    for (int i = 0; i < numRays; ++i) {
        const qreal angle = (2 * M_PI * i) / numRays;
        const QPointF dir(std::cos(angle), std::sin(angle));
        QPointF hit = findNearestIntersection(point, dir, paths);
        if (!hit.isNull()) boundaryPoints.append(hit);
    }

    if (boundaryPoints.size() < numRays * 0.75) return region;

    QPainterPath boundary;
    boundary.moveTo(boundaryPoints.first());
    for (int i = 1; i < boundaryPoints.size(); ++i) boundary.lineTo(boundaryPoints[i]);
    boundary.closeSubpath();

    boundary = smoothPath(boundary, 2.0);

    if (boundary.contains(point)) {
        region.outerBoundary = boundary;
        region.bounds = boundary.boundingRect();
        region.isValid = true;
        qDebug() << "Created region by ray casting";
    }
    return region;
}

QPointF BucketFillTool::findNearestIntersection(const QPointF& origin,
    const QPointF& direction,
    const QList<PathSegmentEx>& paths)
{
    QLineF ray(origin, origin + direction * m_searchRadius * 2.0);
    qreal best = std::numeric_limits<qreal>::max();
    QPointF bestPt;

    for (const PathSegmentEx& seg : paths) {
        const int S = 48;
        for (int i = 0; i < S; ++i) {
            const QPointF a = seg.path.pointAtPercent(i / qreal(S));
            const QPointF b = seg.path.pointAtPercent((i + 1) / qreal(S));
            QLineF edge(a, b);
            QPointF isect;
            if (ray.intersects(edge, &isect) == QLineF::BoundedIntersection) {
                const qreal d = QLineF(origin, isect).length();
                if (d > 1.0 && d < best) { // ignore self-near hits
                    best = d; bestPt = isect;
                }
            }
        }
    }
    return bestPt;
}

BucketFillTool::ClosedRegion BucketFillTool::resolveRegionByMask(const QPointF& point,
    const QList<PathSegmentEx>& segments,
    const QRectF& canvasRect)
{
    ClosedRegion region;
    region.isValid = false;

    if (segments.isEmpty()) return region;

    QRectF domainBounds = uniteSegmentBounds(segments);
    if (!domainBounds.contains(point)) {
        domainBounds = domainBounds.united(QRectF(point.x() - 1.0, point.y() - 1.0, 2.0, 2.0));
    }

    const qreal margin = qMax<qreal>(m_connectionTolerance * 4.0, 24.0);
    domainBounds = domainBounds.adjusted(-margin, -margin, margin, margin);
    domainBounds = domainBounds.intersected(canvasRect.adjusted(-margin, -margin, margin, margin));
    if (domainBounds.isEmpty()) domainBounds = canvasRect;

    const qreal maxDimension = qMax(domainBounds.width(), domainBounds.height());
    qreal scale = 2.5;
    if (maxDimension > 0.0) {
        const qreal targetResolution = 1400.0;
        scale = qBound(1.0, targetResolution / maxDimension, 4.0);
    }

    QSize maskSize(qMax(2, int(std::ceil(domainBounds.width() * scale))),
        qMax(2, int(std::ceil(domainBounds.height() * scale))));

    const int maxArea = 12'000'000; // keep memory reasonable
    if (maskSize.width() * maskSize.height() > maxArea) {
        const qreal factor = std::sqrt(maxArea / qreal(maskSize.width() * maskSize.height()));
        scale *= factor;
        maskSize = QSize(qMax(2, int(std::ceil(domainBounds.width() * scale))),
            qMax(2, int(std::ceil(domainBounds.height() * scale))));
    }

    if (maskSize.isEmpty()) return region;

    QImage mask(maskSize, QImage::Format_ARGB32);
    mask.fill(Qt::white);

    QPainter painter(&mask);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::black);

    QTransform toImage;
    toImage.translate(-domainBounds.left(), -domainBounds.top());
    toImage.scale(scale, scale);

    for (const PathSegmentEx& seg : segments) {
        QPainterPath shapePath = seg.shape.isEmpty() ? seg.path : seg.shape;
        if (shapePath.isEmpty()) continue;

        painter.drawPath(toImage.map(shapePath));
    }

    // Enclose the domain to avoid leaking to infinity if shapes are open
    painter.setBrush(Qt::black);
    painter.drawRect(QRectF(0, 0, mask.width(), 1));
    painter.drawRect(QRectF(0, mask.height() - 1, mask.width(), 1));
    painter.drawRect(QRectF(0, 0, 1, mask.height()));
    painter.drawRect(QRectF(mask.width() - 1, 0, 1, mask.height()));
    painter.end();

    const QPoint imagePoint(int((point.x() - domainBounds.left()) * scale),
        int((point.y() - domainBounds.top()) * scale));
    if (!mask.rect().contains(imagePoint)) return region;

    const QColor startColor = getPixelColor(mask, imagePoint);
    if (startColor == QColor(Qt::black)) return region; // clicked directly on a barrier

    QColor floodColor(255, 0, 0, 255);
    QImage floodMask = mask.copy();

    const int area = floodMask.width() * floodMask.height();
    int maxPixels = qMin(area - 4, 900000);
    if (maxPixels < 1) maxPixels = qMin(area, 900000);
    maxPixels = qMax(maxPixels, qMin(area, 1000));

    const int filled = enhancedFloodFill(floodMask, imagePoint, startColor, floodColor, maxPixels);
    if (filled <= 0) return region;

    if (touchesImageBorder(floodMask, floodColor)) {
        return region; // region not fully enclosed
    }

    QPainterPath maskPath = traceFilledRegionEnhanced(floodMask, floodColor);
    if (maskPath.isEmpty()) return region;

    QTransform back;
    back.translate(domainBounds.left(), domainBounds.top());
    back.scale(1.0 / scale, 1.0 / scale);
    maskPath = back.map(maskPath);
    maskPath.setFillRule(Qt::OddEvenFill);
    maskPath = smoothPath(maskPath, 2.0);

    region.outerBoundary = maskPath;
    region.bounds = maskPath.boundingRect();
    region.isValid = true;
    return region;
}

QRectF BucketFillTool::uniteSegmentBounds(const QList<PathSegmentEx>& segments) const
{
    QRectF bounds;
    for (const PathSegmentEx& seg : segments) {
        if (bounds.isNull()) bounds = seg.bounds;
        else bounds = bounds.united(seg.bounds);
    }
    return bounds;
}

bool BucketFillTool::touchesImageBorder(const QImage& image, const QColor& fillColor) const
{
    if (image.isNull()) return false;

    const int w = image.width();
    const int h = image.height();

    for (int x = 0; x < w; ++x) {
        if (getPixelColor(image, QPoint(x, 0)) == fillColor) return true;
        if (getPixelColor(image, QPoint(x, h - 1)) == fillColor) return true;
    }
    for (int y = 0; y < h; ++y) {
        if (getPixelColor(image, QPoint(0, y)) == fillColor) return true;
        if (getPixelColor(image, QPoint(w - 1, y)) == fillColor) return true;
    }
    return false;
}

// ===============================
// Path smoothing (Catmull-Rom)
// ===============================
QPainterPath BucketFillTool::smoothPath(const QPainterPath& path, qreal /*smoothingFactor*/)
{
    if (path.elementCount() < 4) return path;

    QList<QPointF> pts;
    pts.reserve(path.elementCount());
    for (int i = 0; i < path.elementCount(); ++i) {
        QPainterPath::Element e = path.elementAt(i);
        if (i == 0 || e.type != QPainterPath::MoveToElement)
            pts.append(QPointF(e.x, e.y));
    }
    if (pts.size() < 4) return path;

    QPainterPath smooth; smooth.moveTo(pts[0]);
    for (int i = 1; i < pts.size() - 2; ++i) {
        QPointF p0 = pts[qMax(0, i - 1)];
        QPointF p1 = pts[i];
        QPointF p2 = pts[i + 1];
        QPointF p3 = pts[i + 2];
        for (qreal t = 0; t <= 1.0; t += 0.1)
            smooth.lineTo(catmullRomInterpolate(p0, p1, p2, p3, t));
    }
    smooth.closeSubpath();
    return smooth;
}

QPointF BucketFillTool::catmullRomInterpolate(const QPointF& p0, const QPointF& p1,
    const QPointF& p2, const QPointF& p3, qreal t)
{
    const qreal t2 = t * t, t3 = t2 * t;
    QPointF r;
    r.setX(0.5 * ((2 * p1.x())
        + (-p0.x() + p2.x()) * t
        + (2 * p0.x() - 5 * p1.x() + 4 * p2.x() - p3.x()) * t2
        + (-p0.x() + 3 * p1.x() - 3 * p2.x() + p3.x()) * t3));
    r.setY(0.5 * ((2 * p1.y())
        + (-p0.y() + p2.y()) * t
        + (2 * p0.y() - 5 * p1.y() + 4 * p2.y() - p3.y()) * t2
        + (-p0.y() + 3 * p1.y() - 3 * p2.y() + p3.y()) * t3));
    return r;
}

// ===============================
// Region validation
// ===============================
bool BucketFillTool::isValidFillRegion(const ClosedRegion& region, const QRectF& canvasRect)
{
    if (!region.isValid || region.outerBoundary.isEmpty()) return false;

    const QRectF b = region.outerBoundary.boundingRect();
    const qreal regionArea = b.width() * b.height();
    const qreal canvasArea = canvasRect.width() * canvasRect.height();

    if (regionArea > canvasArea * 0.8) {
        qDebug() << "Region too large - likely canvas background";
        return false;
    }
    if (regionArea < 10.0) {
        qDebug() << "Region too small to fill";
        return false;
    }
    return true;
}

// ===============================
// Raster probe -> vector tracing (ARGB-safe)
// ===============================
void BucketFillTool::performEnhancedRasterFill(const QPointF& point)
{
    if (!m_canvas || !m_canvas->scene()) return;

    qreal fillSize = calculateAdaptiveFillSize(point);
    QRectF area(point.x() - fillSize / 2, point.y() - fillSize / 2, fillSize, fillSize);
    area = area.intersected(m_canvas->getCanvasRect());
    if (area.isEmpty()) return;

    const qreal scale = 3.0;
    QImage sceneImage = renderSceneToImage(area, scale); // ARGB32 (non-premultiplied)
    if (sceneImage.isNull()) return;

    const QPoint imagePoint((point.x() - area.left()) * scale,
        (point.y() - area.top()) * scale);
    if (!sceneImage.rect().contains(imagePoint)) return;

    const QColor target = getPixelColor(sceneImage, imagePoint);
    if (!shouldFill(target, m_fillColor)) return;

    QImage fillImage = sceneImage.copy();
    const int areaPixels = fillImage.width() * fillImage.height();
    int maxPixels = qMin(areaPixels - 4, 900000);
    if (maxPixels < 1) maxPixels = qMin(areaPixels, 900000);
    maxPixels = qMax(maxPixels, qMin(areaPixels, 1000));

    const int filled = enhancedFloodFill(fillImage, imagePoint, target, m_fillColor, maxPixels);
    if (filled < 10 || filled >= maxPixels) return;

    // Trace the filled region back into a vector path
    QPainterPath filledPath = traceFilledRegionEnhanced(fillImage, m_fillColor);
    if (filledPath.isEmpty()) return;

    // Map back to scene coordinates
    QTransform T;
    T.translate(area.x(), area.y());
    T.scale(1.0 / scale, 1.0 / scale);
    filledPath = T.map(filledPath);
    filledPath = smoothPath(filledPath, 2.0);

    if (QGraphicsPathItem* item = createFillItem(filledPath, m_fillColor)) {
        addFillToCanvas(item);
        qDebug() << "Enhanced raster fill completed successfully";
    }
}

qreal BucketFillTool::calculateAdaptiveFillSize(const QPointF& point)
{
    QList<PathSegmentEx> nearby = collectPathsInRadius(point, 100);
    if (nearby.isEmpty()) return 400.0;

    qreal sum = 0;
    for (const auto& s : nearby) sum += s.distanceToPoint;
    const qreal avg = sum / nearby.size();
    return qBound(200.0, avg * 4.0, 800.0);
}

bool BucketFillTool::shouldFill(const QColor& target, const QColor& fill)
{
    if (target.alpha() < 50) return false;

    const qreal dR = target.redF() - fill.redF();
    const qreal dG = target.greenF() - fill.greenF();
    const qreal dB = target.blueF() - fill.blueF();

    // Perceptual weighting
    const qreal dist = std::sqrt(0.3 * dR * dR + 0.59 * dG * dG + 0.11 * dB * dB);
    return dist > 0.1;
}

bool BucketFillTool::colorsMatch(const QColor& c1, const QColor& c2, int tolerance)
{
    const int dr = qAbs(c1.red() - c2.red());
    const int dg = qAbs(c1.green() - c2.green());
    const int db = qAbs(c1.blue() - c2.blue());
    const int da = qAbs(c1.alpha() - c2.alpha());
    return (dr + dg + db + da) <= tolerance * 3;
}

int BucketFillTool::enhancedFloodFill(QImage& image, const QPoint& start,
    const QColor& targetColor,
    const QColor& fillColor,
    int maxPixels)
{
    const QRect imageRect = image.rect();
    if (!imageRect.contains(start)) return 0;

    struct PixelNode { QPoint pos; int generation; };
    QQueue<PixelNode> queue;

    const int width = imageRect.width();
    const int height = imageRect.height();
    const int pixelCount = width * height;
    if (pixelCount <= 0) return 0;

    // 0 = untouched, 1 = queued, 2 = filled, 3 = rejected/edge
    std::vector<uint8_t> state(static_cast<size_t>(pixelCount), 0);
    std::vector<QPoint> edgePixels;
    edgePixels.reserve(static_cast<size_t>(std::min(pixelCount, maxPixels)));

    auto indexOf = [&](const QPoint& p) -> int {
        return p.y() * width + p.x();
    };

    auto tryEnqueue = [&](const QPoint& p, int generation) {
        if (!imageRect.contains(p)) return;
        const int idx = indexOf(p);
        uint8_t& cellState = state[static_cast<size_t>(idx)];
        if (cellState != 0) return;
        cellState = 1; // mark as queued
        queue.enqueue({ p, generation });
    };

    tryEnqueue(start, 0);

    int filledCount = 0;

    while (!queue.isEmpty() && filledCount < maxPixels) {
        PixelNode node = queue.dequeue();

        if (!imageRect.contains(node.pos)) continue;

        const int idx = indexOf(node.pos);
        uint8_t& cellState = state[static_cast<size_t>(idx)];
        if (cellState > 1) continue; // already processed

        const QColor cur = getPixelColor(image, node.pos);

        if (!colorsMatch(cur, targetColor, m_tolerance)) {
            cellState = 3; // processed non-target pixel
            if (node.generation > 0)
                edgePixels.push_back(node.pos);
            continue;
        }

        // Set pixel directly with non-premultiplied ARGB
        image.setPixel(node.pos, fillColor.rgba());
        cellState = 2;
        ++filledCount;

        const int nextGeneration = node.generation + 1;
        for (int i = 0; i < 8; ++i) {
            tryEnqueue(node.pos + DIRECTIONS[i], nextGeneration);
        }
    }

    // Simple edge cleanup for anti-aliased borders
    for (const QPoint& p : edgePixels) {
        cleanupEdgePixel(image, p, fillColor);
    }

    return filledCount;
}

void BucketFillTool::cleanupEdgePixel(QImage& image, const QPoint& pos, const QColor& fillColor)
{
    if (!image.rect().contains(pos)) return;

    int filledNeighbors = 0;
    for (int i = 0; i < 8; ++i) {
        const QPoint n = pos + DIRECTIONS[i];
        if (!image.rect().contains(n)) continue;
        const QColor c = getPixelColor(image, n);
        if (c == fillColor) ++filledNeighbors;
    }

    // If mostly surrounded by fill color, blend softly (non-premul)
    if (filledNeighbors >= 5) {
        QColor px = getPixelColor(image, pos);
        // Lightweight blend toward fillColor without premultiplying
        auto lerp = [](int a, int b, float t) { return int(a + (b - a) * t); };
        const float t = 0.5f;
        QColor mixed(
            lerp(px.red(), fillColor.red(), t),
            lerp(px.green(), fillColor.green(), t),
            lerp(px.blue(), fillColor.blue(), t),
            qMax(px.alpha(), fillColor.alpha())
        );
        image.setPixel(pos, mixed.rgba());
    }
}

// ===============================
// Raster -> vector tracing (marching squares-ish)
// ===============================
QPainterPath BucketFillTool::traceFilledRegionEnhanced(const QImage& img, const QColor& fillColor)
{
    const int w = img.width();
    const int h = img.height();
    if (w <= 1 || h <= 1) return QPainterPath();

    auto isFilled = [&](int x, int y) -> bool {
        if (x < 0 || y < 0 || x >= w || y >= h) return false;
        return getPixelColor(img, QPoint(x, y)) == fillColor;
        };

    auto isBoundary = [&](int x, int y) -> bool {
        if (!isFilled(x, y)) return false;
        for (int k = 0; k < 8; ++k) {
            const QPoint n(x + DIRECTIONS[k].x(), y + DIRECTIONS[k].y());
            if (!isFilled(n.x(), n.y())) return true;
        }
        return false;
        };

    std::vector<uint8_t> visited(w * h, 0);
    auto markVisited = [&](const QPoint& p) {
        if (p.x() < 0 || p.x() >= w || p.y() < 0 || p.y() >= h) return;
        visited[p.y() * w + p.x()] = 1;
        };
    auto boundaryVisited = [&](const QPoint& p) -> bool {
        if (p.x() < 0 || p.x() >= w || p.y() < 0 || p.y() >= h) return false;
        return visited[p.y() * w + p.x()] != 0;
        };

    auto turnLeft = [](int d) { return (d + 7) & 7; };
    auto turnRight = [](int d) { return (d + 1) & 7; };

    auto traceContour = [&](const QPoint& seed) -> QVector<QPointF> {
        QVector<QPointF> contour;
        QPoint start = seed;
        while (start.x() > 0 && isFilled(start.x() - 1, start.y())) start.rx()--;

        QPoint cur = start;
        QPoint startPt = start;
        int dir = 6; // North
        bool first = true;
        int guard = w * h * 12;

        do {
            contour.append(QPointF(cur.x() + 0.5, cur.y() + 0.5));
            markVisited(cur);

            int tryDir = turnLeft(dir);
            bool moved = false;

            for (int step = 0; step < 8; ++step) {
                const QPoint next = cur + DIRECTIONS[tryDir];
                if (isFilled(next.x(), next.y())) {
                    dir = tryDir;
                    cur = next;
                    moved = true;
                    break;
                }
                tryDir = turnRight(tryDir);
            }

            if (!moved) {
                const QPoint next = cur + DIRECTIONS[dir];
                if (!isFilled(next.x(), next.y())) break;
                cur = next;
            }

            if (--guard <= 0) break;
            if (!first && cur == startPt) break;
            first = false;
        } while (true);

        return contour;
    };

    QPainterPath path;
    path.setFillRule(Qt::OddEvenFill);

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            if (!isBoundary(x, y)) continue;
            QPoint p(x, y);
            if (boundaryVisited(p)) continue;

            QVector<QPointF> contour = traceContour(p);
            if (contour.size() < 3) continue;

            auto polygonArea = [](const QVector<QPointF>& pts) {
                qreal area = 0.0;
                const int count = pts.size();
                for (int i = 0; i < count; ++i) {
                    const QPointF& a = pts[i];
                    const QPointF& b = pts[(i + 1) % count];
                    area += (a.x() * b.y()) - (b.x() * a.y());
                }
                return area * 0.5;
            };

            if (polygonArea(contour) < 0) {
                std::reverse(contour.begin(), contour.end());
            }

            QPainterPath loop;
            loop.moveTo(contour[0]);
            for (int i = 1; i < contour.size(); ++i) loop.lineTo(contour[i]);
            loop.closeSubpath();
            path.addPath(loop);
        }
    }

    return path.simplified();
}

// ===============================
// Item creation & scene insertion
// ===============================
QGraphicsPathItem* BucketFillTool::createFillItem(const QPainterPath& fillPath, const QColor& color)
{
    if (fillPath.isEmpty()) return nullptr;

    auto* item = new QGraphicsPathItem(fillPath);
    item->setBrush(QBrush(color));
    item->setPen(Qt::NoPen);
    item->setFlag(QGraphicsItem::ItemIsSelectable, true);
    item->setFlag(QGraphicsItem::ItemIsMovable, true);

    // Keep Canvas opacity math stable: store individual opacity in data(0) and set item to 1.0.
    item->setData(0, 1.0);
    item->setOpacity(1.0);

    return item;
}

void BucketFillTool::addFillToCanvas(QGraphicsPathItem* item)
{
    if (!m_canvas || !item) return;

    // Simple path: add directly to the current layer; Canvas records frame state.
    m_canvas->addItemToCurrentLayer(item);
}


// ===============================
// Rendering helpers (ARGB-safe)
// ===============================
QImage BucketFillTool::renderSceneToImage(const QRectF& area, qreal scale) const
{
    if (!m_canvas || !m_canvas->scene()) return QImage();

    const QSize imgSize(qMax(1, int(area.width() * scale)),
        qMax(1, int(area.height() * scale)));

    QImage image(imgSize, QImage::Format_ARGB32); // non-premultiplied
    image.fill(Qt::transparent);

    QPainter p(&image);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    p.setCompositionMode(QPainter::CompositionMode_SourceOver);

    const QRectF sourceRect = area;
    const QRectF targetRect(0, 0, imgSize.width(), imgSize.height());

    m_canvas->scene()->render(&p, targetRect, sourceRect);
    p.end();
    return image;
}

QColor BucketFillTool::getPixelColor(const QImage& image, const QPoint& pos) const
{
    // Ensure bounds
    if (!image.rect().contains(pos)) return QColor(Qt::transparent);

    // QImage::pixel() returns non-premultiplied ARGB for Format_ARGB32
    const QRgb v = image.pixel(pos);
    return QColor::fromRgb(v);
}

// ===============================
// Convenience / small utilities
// ===============================
bool BucketFillTool::isPathClosed(const QPainterPath& path, qreal tol) const
{
    if (path.isEmpty()) return false;

    // Use first and last element positions as endpoints
    const QPainterPath::Element first = path.elementAt(0);
    const QPainterPath::Element last = path.elementAt(path.elementCount() - 1);

    const QPointF start(first.x, first.y);
    const QPointF end(last.x, last.y);

    // Many paths that were "closed" via closeSubpath() will still end at the start
    return QLineF(start, end).length() <= tol;
}


QPainterPath BucketFillTool::smoothContour(const QPainterPath& path, qreal smoothing)
{
    Q_UNUSED(smoothing);
    // Reuse smoothPath (Catmull-Rom)
    return smoothPath(path, 2.0);
}

QPainterPath BucketFillTool::traceFilledRegion(const QImage& img, const QColor& c)
{
    // For compatibility with existing code paths if they call this older name
    return traceFilledRegionEnhanced(img, c);
}

// ===============================
// Preview management (no-op safe guards)
// ===============================
void BucketFillTool::hideFillPreview()
{
    if (m_previewItem && m_canvas && m_canvas->scene()) {
        m_canvas->scene()->removeItem(m_previewItem);
        delete m_previewItem;
        m_previewItem = nullptr;
    }
}

// --- Cursor shown when the tool is active ---
QCursor BucketFillTool::getCursor() const
{
    // Simple crosshair works well; swap for a custom pixmap if you have one.
    return QCursor(Qt::CrossCursor);
}

// --- Optional hover preview of the region that would be filled ---
void BucketFillTool::mouseMoveEvent(QMouseEvent* event, const QPointF& scenePos)
{
    Q_UNUSED(event);
    if (!m_canvas || !m_canvas->scene()) return;

    // Only preview when inside canvas bounds and NOT dragging
    if (!m_canvas->getCanvasRect().contains(scenePos) || (event->buttons() & Qt::LeftButton)) {
        hideFillPreview();
        return;
    }

    // Vector-first preview: try to find the region under the cursor
    ClosedRegion region = findEnclosedRegionEnhanced(scenePos);
    if (region.isValid && !region.outerBoundary.isEmpty()
        && isValidFillRegion(region, m_canvas->getCanvasRect()))
    {
        showFillPreview(region.outerBoundary);
    }
    else {
        // No valid region -> hide preview
        hideFillPreview();
    }
}

// --- Nothing special needed on release for this tool ---
void BucketFillTool::mouseReleaseEvent(QMouseEvent* event, const QPointF& scenePos)
{
    Q_UNUSED(event);
    Q_UNUSED(scenePos);
    // No-op; click is handled in mousePressEvent(), but we ensure preview is cleared.
    // If you prefer to keep a preview while idle, remove this.
    // hideFillPreview();
}

// --- Lightweight preview helper (translucent path item) ---
void BucketFillTool::showFillPreview(const QPainterPath& path)
{
    if (!m_canvas || !m_canvas->scene() || path.isEmpty()) {
        hideFillPreview();
        return;
    }

    if (!m_previewItem) {
        m_previewItem = new QGraphicsPathItem(path);
        // Dashed edge, translucent fill; no ARGB premultiplication here—just alpha in the brush.
        m_previewItem->setPen(QPen(QColor(0, 0, 0, 90), 1.0, Qt::DashLine, Qt::RoundCap, Qt::RoundJoin));
        QColor c = m_fillColor; c.setAlpha(70);
        m_previewItem->setBrush(c);
        m_previewItem->setZValue(1e6); // keep above artwork; adjust if you sort Z elsewhere
        m_canvas->scene()->addItem(m_previewItem);
    }
    else {
        m_previewItem->setPath(path);
        QColor c = m_fillColor; c.setAlpha(70);
        m_previewItem->setBrush(c);
    }
}
