#include "BucketFillTool.h"
#include "MainWindow.h"
#include "Canvas.h"
#include "Common/GraphicsItemRoles.h"
#include "Commands/UndoCommands.h"
#include <QGraphicsScene>
#include <QGraphicsPathItem>
#include <QGraphicsRectItem>
#include <QGraphicsEllipseItem>
#include <QGraphicsLineItem>
#include <QAbstractGraphicsShapeItem>
#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QPen>
#include <QApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <QtMath>
#include <QQueue>
#include <QSet>
#include <QVector>
#include <limits>
#include <vector>

namespace {

    qreal polygonArea(const QPolygonF& polygon)
    {
        const int count = polygon.size();
        if (count < 3) {
            return 0.0;
        }

        qreal area = 0.0;
        for (int i = 0; i < count; ++i) {
            const QPointF& p1 = polygon.at(i);
            const QPointF& p2 = polygon.at((i + 1) % count);
            area += (p1.x() * p2.y()) - (p2.x() * p1.y());
        }

        return qAbs(area) * 0.5;
    }

    QPolygonF smoothPolygonChaikin(const QPolygonF& polygon, int iterations, qreal weight)
    {
        if (polygon.size() < 3) {
            return polygon;
        }

        QPolygonF result = polygon;

        for (int iter = 0; iter < iterations; ++iter) {
            if (result.size() < 3) {
                break;
            }

            QPolygonF next;
            next.reserve(result.size() * 2);

            const int count = result.size();
            for (int i = 0; i < count; ++i) {
                const QPointF& current = result.at(i);
                const QPointF& nextPoint = result.at((i + 1) % count);

                const QPointF q = current * (1.0 - weight) + nextPoint * weight;
                const QPointF r = current * weight + nextPoint * (1.0 - weight);

                next.append(q);
                next.append(r);
            }

            result = next;
        }

        return result;
    }

}

// Direction vectors for 8-connected neighbors (Moore neighborhood)
const QPoint BucketFillTool::DIRECTIONS[8] = {
    QPoint(1, 0),   // 0: East
    QPoint(1, 1),   // 1: Southeast  
    QPoint(0, 1),   // 2: South
    QPoint(-1, 1),  // 3: Southwest
    QPoint(-1, 0),  // 4: West
    QPoint(-1, -1), // 5: Northwest
    QPoint(0, -1),  // 6: North
    QPoint(1, -1)   // 7: Northeast
};

BucketFillTool::BucketFillTool(MainWindow* mainWindow, QObject* parent)
    : Tool(mainWindow, parent)
    , m_fillColor(Qt::red)
    , m_tolerance(10)
    , m_fillMode(0)
    , m_searchRadius(100.0)
    , m_connectionTolerance(5.0)
    , m_debugMode(false)
    , m_cacheValid(false)
    , m_previewItem(nullptr)
{
    // Get initial fill color from canvas
    if (m_canvas) {
        m_fillColor = m_canvas->getFillColor();
    }

    qDebug() << "BucketFillTool created with color:" << m_fillColor.name();
}

void BucketFillTool::mousePressEvent(QMouseEvent* event, const QPointF& scenePos)
{
    if (!m_canvas || !m_canvas->scene()) return;

    if (event->button() == Qt::LeftButton) {
        // Get current fill color from canvas
        m_fillColor = m_canvas->getFillColor();

        QElapsedTimer timer;
        timer.start();

        qDebug() << "BucketFill: Starting fill at" << scenePos << "with color" << m_fillColor.name();

        hideFillPreview();

        // Check if we're clicking on canvas background
        QRectF canvasRect = m_canvas->getCanvasRect();
        if (!canvasRect.contains(scenePos)) {
            qDebug() << "BucketFill: Click outside canvas bounds, ignoring";
            return;
        }

        try {
            ClosedRegion region = findEnclosedRegion(scenePos);

            if ((!region.isValid || region.outerBoundary.isEmpty()) && m_fillMode == 1) {
                bool touchesEdge = false;
                region = floodFillRegionFromArea(canvasRect, scenePos, touchesEdge);
            }

            if (region.isValid && !region.outerBoundary.isEmpty()) {
                // Validate region size to prevent filling entire canvas unintentionally
                QRectF regionBounds = region.outerBoundary.boundingRect();
                QRectF canvasBounds = m_canvas->getCanvasRect();

                double regionArea = regionBounds.width() * regionBounds.height();
                double canvasArea = canvasBounds.width() * canvasBounds.height();

                if (m_fillMode == 0 && regionArea > canvasArea * 0.8) {
                    qDebug() << "BucketFill: Region too large, probably canvas background. Skipping.";
                    return;
                }

                QGraphicsPathItem* fillItem = createFillItem(region.outerBoundary, m_fillColor);
                if (fillItem) {
                    addFillToCanvas(fillItem);
                    qDebug() << "BucketFill: Fill completed in" << timer.elapsed() << "ms";
                }
                else {
                    qDebug() << "BucketFill: Failed to create fill item";
                }
            }
            else {
                qDebug() << "BucketFill: No enclosed region found for fill";
            }
        }
        catch (const std::exception& e) {
            qDebug() << "BucketFill: Exception occurred:" << e.what();
        }
    }
}

void BucketFillTool::mouseMoveEvent(QMouseEvent* event, const QPointF& scenePos)
{
    // Show preview only when not dragging and within canvas bounds
    if (event->buttons() == Qt::NoButton) {
        QRectF canvasRect = m_canvas ? m_canvas->getCanvasRect() : QRectF();
        if (!canvasRect.contains(scenePos)) {
            hideFillPreview();
            return;
        }

        try {
            if (m_fillMode == 0) {
                ClosedRegion region = findEnclosedRegion(scenePos);
                if (region.isValid && !region.outerBoundary.isEmpty()) {
                    // FIXED: Only show preview for reasonable-sized regions
                    QRectF regionBounds = region.outerBoundary.boundingRect();
                    QRectF canvasBounds = m_canvas->getCanvasRect();
                    double regionArea = regionBounds.width() * regionBounds.height();
                    double canvasArea = canvasBounds.width() * canvasBounds.height();

                    if (regionArea <= canvasArea * 0.8) {
                        showFillPreview(region.outerBoundary);
                    }
                    else {
                        hideFillPreview();
                    }
                }
                else {
                    hideFillPreview();
                }
            }
        }
        catch (...) {
            hideFillPreview();
        }
    }
}

void BucketFillTool::mouseReleaseEvent(QMouseEvent* event, const QPointF& scenePos)
{
    // Nothing specific needed for release
}

QCursor BucketFillTool::getCursor() const
{
    return QCursor(Qt::PointingHandCursor);
}

BucketFillTool::ClosedRegion BucketFillTool::findEnclosedRegion(const QPointF& point)
{
    ClosedRegion invalidRegion;
    invalidRegion.isValid = false;

    if (!m_canvas || !m_canvas->scene()) {
        return invalidRegion;
    }

    QRectF canvasRect = m_canvas->getCanvasRect();
    if (!canvasRect.contains(point)) {
        return invalidRegion;
    }

    const qreal maxCanvasDim = qMax(canvasRect.width(), canvasRect.height());
    const qreal safeMax = qMax<qreal>(maxCanvasDim, 1.0);
    const qreal initialRadius = qMin<qreal>(qMax<qreal>(m_searchRadius, 40.0), safeMax);

    QVector<qreal> radii;
    radii.append(initialRadius);

    qreal radius = initialRadius;
    const int maxSteps = 6;
    for (int i = 0; i < maxSteps && radius < maxCanvasDim; ++i) {
        radius = qMin(maxCanvasDim, radius * 1.6);
        if (qFuzzyCompare(radius, radii.last())) {
            break;
        }
        radii.append(radius);
        if (radius >= maxCanvasDim) {
            break;
        }
    }

    if (radii.isEmpty() || !qFuzzyCompare(radii.last(), maxCanvasDim)) {
        radii.append(maxCanvasDim);
    }

    ClosedRegion lastValidRegion;
    bool hasCandidate = false;
    bool lastTouchedEdge = false;

    for (qreal currentRadius : radii) {
        QRectF sampleRect(point.x() - currentRadius, point.y() - currentRadius,
            currentRadius * 2.0, currentRadius * 2.0);
        sampleRect = sampleRect.intersected(canvasRect);
        if (sampleRect.isEmpty()) {
            continue;
        }

        bool touchesEdge = false;
        ClosedRegion candidate = floodFillRegionFromArea(sampleRect, point, touchesEdge);
        if (!candidate.isValid || candidate.outerBoundary.isEmpty()) {
            continue;
        }

        const qreal canvasArea = canvasRect.width() * canvasRect.height();
        const qreal regionArea = candidate.bounds.width() * candidate.bounds.height();

        if (canvasArea > 0.0 && regionArea > canvasArea * 0.9 && m_fillMode == 0) {
            qDebug() << "BucketFill: Detected candidate covering most of the canvas; skipping.";
            continue;
        }

        lastValidRegion = candidate;
        hasCandidate = true;
        lastTouchedEdge = touchesEdge;

        if (!touchesEdge) {
            return candidate;
        }

        if (sampleRect == canvasRect) {
            if (m_fillMode == 1) {
                return candidate;
            }

            if (canvasArea <= 0.0 || regionArea < canvasArea * 0.85) {
                return candidate;
            }

            hasCandidate = false;
        }
    }

    if (!hasCandidate) {
        if (m_fillMode == 1) {
            bool touchesEdge = false;
            ClosedRegion canvasRegion = floodFillRegionFromArea(canvasRect, point, touchesEdge);
            if (canvasRegion.isValid && !canvasRegion.outerBoundary.isEmpty()) {
                return canvasRegion;
            }
        }

    }
    else if (!lastTouchedEdge || m_fillMode == 1) {
        return lastValidRegion;
    }

    qDebug() << "BucketFill: No enclosed region could be determined";
    return invalidRegion;
}

BucketFillTool::ClosedRegion BucketFillTool::floodFillRegionFromArea(const QRectF& area,
    const QPointF& scenePoint, bool& touchesEdge)
{
    ClosedRegion region;
    region.isValid = false;
    touchesEdge = false;

    if (!m_canvas || !m_canvas->scene()) {
        return region;
    }

    if (area.isEmpty()) {
        return region;
    }

    const qreal maxDimension = qMax(area.width(), area.height());
    if (maxDimension <= 0.5) {
        return region;
    }

    const qreal maxImageDim = 512.0;
    qreal scale = maxImageDim / maxDimension;
    scale = qBound<qreal>(0.1, scale, 3.0);

    QImage image = renderSceneToImage(area, scale, true);
    if (image.isNull() || image.width() <= 0 || image.height() <= 0) {
        return region;
    }

    QPoint imagePoint(
        qBound(0, static_cast<int>(qRound((scenePoint.x() - area.left()) * scale)), image.width() - 1),
        qBound(0, static_cast<int>(qRound((scenePoint.y() - area.top()) * scale)), image.height() - 1));

    const QRgb targetColor = image.pixel(imagePoint);

    const int width = image.width();
    const int height = image.height();
    const int totalPixels = width * height;
    if (totalPixels <= 0) {
        return region;
    }

    std::vector<uchar> mask(static_cast<size_t>(totalPixels), 0);
    QVector<QPoint> stack;
    stack.reserve(qMin(totalPixels, 65536));
    stack.append(imagePoint);

    int filledPixels = 0;
    int minX = width;
    int minY = height;
    int maxX = -1;
    int maxY = -1;

    while (!stack.isEmpty() && filledPixels < totalPixels) {
        QPoint p = stack.back();
        stack.pop_back();

        if (p.x() < 0 || p.x() >= width || p.y() < 0 || p.y() >= height) {
            continue;
        }

        const int index = p.y() * width + p.x();
        if (mask[static_cast<size_t>(index)] != 0) {
            continue;
        }

        const QRgb currentColor = image.pixel(p);
        if (!colorsSimilar(currentColor, targetColor)) {
            continue;
        }

        mask[static_cast<size_t>(index)] = 1;
        ++filledPixels;

        if (p.x() == 0 || p.x() == width - 1 || p.y() == 0 || p.y() == height - 1) {
            touchesEdge = true;
        }

        minX = qMin(minX, p.x());
        maxX = qMax(maxX, p.x());
        minY = qMin(minY, p.y());
        maxY = qMax(maxY, p.y());

        stack.append(QPoint(p.x() + 1, p.y()));
        stack.append(QPoint(p.x() - 1, p.y()));
        stack.append(QPoint(p.x(), p.y() + 1));
        stack.append(QPoint(p.x(), p.y() - 1));
    }

    if (filledPixels <= 0 || filledPixels >= totalPixels) {
        return region;
    }

    if (minX > maxX || minY > maxY) {
        return region;
    }

    QPainterPath rasterPath;
    for (int y = minY; y <= maxY; ++y) {
        const int rowOffset = y * width;
        int x = minX;
        while (x <= maxX) {
            while (x <= maxX && mask[static_cast<size_t>(rowOffset + x)] == 0) {
                ++x;
            }
            if (x > maxX) {
                break;
            }

            const int start = x;
            while (x <= maxX && mask[static_cast<size_t>(rowOffset + x)] != 0) {
                ++x;
            }
            const int end = x;

            rasterPath.addRect(QRectF(start, y, end - start, 1));
        }
    }

    if (rasterPath.isEmpty()) {
        return region;
    }

    QTransform transform;
    transform.translate(area.left(), area.top());
    const qreal invScale = 1.0 / scale;
    transform.scale(invScale, invScale);

    QPainterPath scenePath = transform.map(rasterPath);
    scenePath = scenePath.simplified();

    if (!scenePath.contains(scenePoint)) {
        return region;
    }

    QPainterPathStroker stroker;
    stroker.setCapStyle(Qt::SquareCap);
    stroker.setJoinStyle(Qt::RoundJoin);
    stroker.setWidth(qMax<qreal>(0.6, 1.4 / scale));
    scenePath = scenePath.united(stroker.createStroke(scenePath)).simplified();

    QPainterPath refinedPath = scenePath;

    QPainterPath chaikinPath;
    chaikinPath.setFillRule(refinedPath.fillRule());
    const QVector<QPolygonF> polygons = refinedPath.toFillPolygons();
    for (const QPolygonF& polygon : polygons) {
        if (polygon.size() < 3) {
            continue;
        }

        QPolygonF smoothedPolygon = smoothPolygonChaikin(polygon, 2, 0.25);
        if (smoothedPolygon.size() < 3) {
            continue;
        }

        QPainterPath smoothedPath;
        smoothedPath.addPolygon(smoothedPolygon);
        smoothedPath.closeSubpath();
        chaikinPath.addPath(smoothedPath);
    }

    if (!chaikinPath.isEmpty()) {
        QPainterPath candidate = chaikinPath.united(refinedPath).simplified();
        if (candidate.contains(scenePoint)) {
            refinedPath = candidate;
        }
    }

    QPainterPath curvedPath = smoothContour(refinedPath, 1.35);
    if (!curvedPath.isEmpty() && curvedPath.contains(scenePoint)) {
        refinedPath = curvedPath.simplified();
    }

    scenePath = refinedPath;

    region.outerBoundary = scenePath;
    region.bounds = scenePath.boundingRect();
    region.isValid = true;

    return region;
}

bool BucketFillTool::colorsSimilar(QRgb first, QRgb second) const
{
    // Handle fully transparent pixels
    int a1 = qAlpha(first);
    int a2 = qAlpha(second);

    // Both transparent = similar
    if (a1 < 10 && a2 < 10) {
        return true;
    }

    // One transparent, one opaque = different
    if ((a1 < 10) != (a2 < 10)) {
        return false;
    }

    // For semi-transparent pixels, use weighted comparison
    if (a1 < 255 || a2 < 255) {
        // Compare based on visual appearance (alpha-weighted)
        qreal r1 = qRed(first) * a1 / 255.0;
        qreal g1 = qGreen(first) * a1 / 255.0;
        qreal b1 = qBlue(first) * a1 / 255.0;

        qreal r2 = qRed(second) * a2 / 255.0;
        qreal g2 = qGreen(second) * a2 / 255.0;
        qreal b2 = qBlue(second) * a2 / 255.0;

        qreal diff = qAbs(r1 - r2) + qAbs(g1 - g2) + qAbs(b1 - b2) + qAbs(a1 - a2) * 0.5;

        return diff <= (12 + m_tolerance * 4);
    }

    // Both opaque - simple comparison
    const int diff = qAbs(qRed(first) - qRed(second)) +
        qAbs(qGreen(first) - qGreen(second)) +
        qAbs(qBlue(first) - qBlue(second));

    return diff <= (12 + m_tolerance * 4);
}

QList<BucketFillTool::PathSegment> BucketFillTool::collectNearbyPaths(const QPointF& center, qreal searchRadius)
{
    QList<PathSegment> segments;

    if (!m_canvas || !m_canvas->scene()) return segments;

    // FIXED: Limit search area to reasonable bounds
    QRectF canvasRect = m_canvas->getCanvasRect();
    QRectF searchRect(center.x() - searchRadius, center.y() - searchRadius,
        searchRadius * 2, searchRadius * 2);

    // Intersect with canvas bounds to prevent searching outside canvas
    searchRect = searchRect.intersected(canvasRect);

    // Get items in the search area, excluding background
    QList<QGraphicsItem*> items = m_canvas->scene()->items(searchRect, Qt::IntersectsItemBoundingRect);

    for (QGraphicsItem* item : items) {
        // FIXED: Skip the background rectangle (it has zValue -1000)
        if (item->zValue() <= -999) {
            continue;
        }

        if (item == m_previewItem) {
            continue;
        }

        PathSegment segment;
        segment.item = item;

        // Convert different item types to paths
        if (auto pathItem = qgraphicsitem_cast<QGraphicsPathItem*>(item)) {
            segment.path = pathItem->path();
            segment.bounds = pathItem->boundingRect();
        }
        else if (auto rectItem = qgraphicsitem_cast<QGraphicsRectItem*>(item)) {
            segment.path.addRect(rectItem->rect());
            segment.bounds = rectItem->boundingRect();
        }
        else if (auto ellipseItem = qgraphicsitem_cast<QGraphicsEllipseItem*>(item)) {
            segment.path.addEllipse(ellipseItem->rect());
            segment.bounds = ellipseItem->boundingRect();
        }
        else if (auto lineItem = qgraphicsitem_cast<QGraphicsLineItem*>(item)) {
            QLineF line = lineItem->line();
            segment.path.moveTo(line.p1());
            segment.path.lineTo(line.p2());
            segment.bounds = lineItem->boundingRect();
        }

        if (!segment.path.isEmpty()) {
            // Transform path to scene coordinates
            QTransform transform = item->sceneTransform();
            segment.path = transform.map(segment.path);
            segment.bounds = transform.mapRect(segment.bounds);

            segments.append(segment);
        }
    }

    qDebug() << "BucketFill: Collected" << segments.size() << "path segments";
    return segments;
}

QPainterPath BucketFillTool::createClosedPath(const QList<PathSegment>& segments, const QPointF& seedPoint)
{
    if (segments.isEmpty()) return QPainterPath();

    // Start with the segment closest to the seed point
    QPainterPath result;
    qreal minDistance = std::numeric_limits<qreal>::max();
    int startIndex = -1;

    for (int i = 0; i < segments.size(); ++i) {
        QPointF closestPoint = segments[i].path.pointAtPercent(0.5);
        qreal distance = QLineF(seedPoint, closestPoint).length();
        if (distance < minDistance) {
            minDistance = distance;
            startIndex = i;
        }
    }

    if (startIndex == -1) return QPainterPath();

    result = segments[startIndex].path;
    QList<QPainterPath> remainingPaths;

    for (int i = 0; i < segments.size(); ++i) {
        if (i != startIndex) {
            remainingPaths.append(segments[i].path);
        }
    }

    // Try to connect other paths
    const qreal connectionTolerance = m_connectionTolerance * 2;
    bool foundConnection = true;

    while (foundConnection && !remainingPaths.isEmpty()) {
        foundConnection = false;
        QPointF resultEnd = result.pointAtPercent(1.0);

        for (int i = 0; i < remainingPaths.size(); ++i) {
            QPainterPath& path = remainingPaths[i];
            QPointF pathStart = path.pointAtPercent(0.0);
            QPointF pathEnd = path.pointAtPercent(1.0);

            if (QLineF(resultEnd, pathStart).length() <= connectionTolerance) {
                result.connectPath(path);
                remainingPaths.removeAt(i);
                foundConnection = true;
                break;
            }
            else if (QLineF(resultEnd, pathEnd).length() <= connectionTolerance) {
                QPainterPath reversedPath = path.toReversed();
                result.connectPath(reversedPath);
                remainingPaths.removeAt(i);
                foundConnection = true;
                break;
            }
        }
    }

    QPainterPath closedResult = closeOpenPath(result, connectionTolerance);
    if (isPathClosed(closedResult) && closedResult.contains(seedPoint)) {
        return closedResult;
    }

    return QPainterPath();
}

BucketFillTool::ClosedRegion BucketFillTool::buildClosedRegionFromSegments(const QList<PathSegment>& segments,
    const QPointF& seedPoint, qreal searchRadius)
{
    ClosedRegion region;
    region.isValid = false;

    if (segments.isEmpty()) {
        return region;
    }

    QRectF combinedBounds;
    bool hasBounds = false;

    for (const PathSegment& segment : segments) {
        if (!hasBounds) {
            combinedBounds = segment.bounds;
            hasBounds = true;
        }
        else {
            combinedBounds = combinedBounds.united(segment.bounds);
        }
    }

    if (!hasBounds || combinedBounds.isNull()) {
        return region;
    }

    if (!combinedBounds.contains(seedPoint)) {
        qreal adjust = qMax(searchRadius * 0.5, m_connectionTolerance * 8.0);
        combinedBounds = combinedBounds.united(QRectF(seedPoint.x() - adjust, seedPoint.y() - adjust, adjust * 2, adjust * 2));
    }

    qreal margin = qMax(qreal(12.0), qMax(searchRadius * 0.25, m_connectionTolerance * 6.0));
    combinedBounds.adjust(-margin, -margin, margin, margin);

    if (m_canvas) {
        QRectF canvasRect = m_canvas->getCanvasRect();
        combinedBounds = combinedBounds.intersected(canvasRect);
        if (!combinedBounds.contains(seedPoint)) {
            combinedBounds = canvasRect;
        }
    }

    if (combinedBounds.isEmpty()) {
        return region;
    }

    QPainterPath searchArea;
    searchArea.addRect(combinedBounds);

    QPainterPath obstacles;
    QPainterPathStroker stroker;
    stroker.setCapStyle(Qt::RoundCap);
    stroker.setJoinStyle(Qt::RoundJoin);

    qreal baseStrokeWidth = qMax(m_connectionTolerance * 2.0, 1.5);
    qreal maxStrokeWidth = baseStrokeWidth;

    for (const PathSegment& segment : segments) {
        if (segment.path.isEmpty()) {
            continue;
        }

        qreal strokeWidth = baseStrokeWidth;
        if (auto shapeItem = qgraphicsitem_cast<QAbstractGraphicsShapeItem*>(segment.item)) {
            strokeWidth = qMax(strokeWidth, shapeItem->pen().widthF() + m_connectionTolerance);
        }

        stroker.setWidth(strokeWidth);
        maxStrokeWidth = qMax(maxStrokeWidth, strokeWidth);

        QPainterPath thickPath = stroker.createStroke(segment.path);
        obstacles = obstacles.united(thickPath);

        if (auto shapeItem = qgraphicsitem_cast<QAbstractGraphicsShapeItem*>(segment.item)) {
            if (shapeItem->brush().style() != Qt::NoBrush && shapeItem->brush().color().alpha() > 0) {
                obstacles = obstacles.united(segment.path);
            }
        }
    }

    obstacles = obstacles.intersected(searchArea);

    QPainterPath available = searchArea.subtracted(obstacles);
    available = available.simplified();

    if (available.isEmpty()) {
        return region;
    }

    QList<QPolygonF> polygons = available.toFillPolygons();
    const qreal minArea = 4.0;
    QPainterPath candidatePath;

    for (const QPolygonF& polygon : polygons) {
        if (polygon.isEmpty()) {
            continue;
        }

        QPainterPath polygonPath;
        polygonPath.addPolygon(polygon);
        polygonPath.closeSubpath();

        if (polygonArea(polygon) < minArea) {
            continue;
        }

        if (polygonPath.contains(seedPoint)) {
            candidatePath = polygonPath;
            break;
        }
    }

    if (candidatePath.isEmpty()) {
        return region;
    }

    candidatePath = candidatePath.simplified();

    QRectF candidateRect = candidatePath.boundingRect();
    qreal boundaryTolerance = qMax(maxStrokeWidth * 1.5, m_connectionTolerance * 4.0);

    if (qAbs(candidateRect.left() - combinedBounds.left()) < boundaryTolerance ||
        qAbs(candidateRect.right() - combinedBounds.right()) < boundaryTolerance ||
        qAbs(candidateRect.top() - combinedBounds.top()) < boundaryTolerance ||
        qAbs(candidateRect.bottom() - combinedBounds.bottom()) < boundaryTolerance) {
        return region;
    }

    region.outerBoundary = candidatePath;
    region.bounds = candidateRect;
    region.isValid = true;

    return region;
}

BucketFillTool::ClosedRegion BucketFillTool::buildClosedRegionUsingRaster(const QList<PathSegment>& segments,
    const QPointF& seedPoint, qreal searchRadius)
{
    ClosedRegion region;
    region.isValid = false;

    if (!m_canvas || !m_canvas->scene()) {
        return region;
    }

    QRectF bounds;
    bool hasBounds = false;

    for (const PathSegment& segment : segments) {
        if (!hasBounds) {
            bounds = segment.bounds;
            hasBounds = true;
        }
        else {
            bounds = bounds.united(segment.bounds);
        }
    }

    if (!hasBounds || bounds.isNull()) {
        qreal fallbackRadius = qMax(searchRadius, qreal(48.0));
        bounds = QRectF(seedPoint.x() - fallbackRadius, seedPoint.y() - fallbackRadius,
            fallbackRadius * 2.0, fallbackRadius * 2.0);
        hasBounds = true;
    }

    if (!bounds.contains(seedPoint)) {
        qreal adjust = qMax(searchRadius * 0.5, m_connectionTolerance * 8.0);
        QRectF seedBounds(seedPoint.x() - adjust, seedPoint.y() - adjust, adjust * 2.0, adjust * 2.0);
        bounds = bounds.united(seedBounds);
    }

    qreal margin = qMax(qreal(18.0), qMax(searchRadius * 0.3, m_connectionTolerance * 6.0));
    bounds.adjust(-margin, -margin, margin, margin);

    QRectF canvasRect = m_canvas->getCanvasRect();
    bounds = bounds.intersected(canvasRect);

    if (bounds.isEmpty()) {
        return region;
    }

    qreal area = bounds.width() * bounds.height();
    if (area <= 0.0) {
        return region;
    }

    const qreal maxPixels = 450000.0;
    qreal scale = 3.0;
    qreal scaledArea = area * scale * scale;

    if (scaledArea > maxPixels) {
        scale = qSqrt(maxPixels / area);
    }

    const qreal minScale = 0.75;
    const qreal maxScale = 5.0;
    scale = qBound(minScale, scale, maxScale);

    if (area * scale * scale > maxPixels) {
        qreal adjustedScale = qSqrt(maxPixels / area);
        scale = qBound(qreal(0.4), adjustedScale, maxScale);
    }

    if (scale <= 0.0) {
        return region;
    }

    if (area * scale * scale > maxPixels) {
        return region;
    }

    QImage sceneImage = renderSceneToImage(bounds, scale, true);
    if (sceneImage.isNull() || sceneImage.width() <= 0 || sceneImage.height() <= 0) {
        return region;
    }

    QPointF relativePoint = seedPoint - bounds.topLeft();
    QPoint imagePoint(qRound(relativePoint.x() * scale), qRound(relativePoint.y() * scale));

    if (!sceneImage.rect().contains(imagePoint)) {
        return region;
    }

    QColor targetColor = getPixelColor(sceneImage, imagePoint);
    if (!targetColor.isValid()) {
        return region;
    }

    QImage fillImage = sceneImage.copy();
    QColor traceColor(255, 0, 255, 255);
    if (traceColor == targetColor) {
        traceColor = QColor(0, 255, 0, 255);
    }

    int totalPixels = sceneImage.width() * sceneImage.height();
    int maxFillPixels = qMin(totalPixels, static_cast<int>(maxPixels));
    if (maxFillPixels <= 0) {
        return region;
    }

    int filledPixels = floodFillImageLimited(fillImage, imagePoint, targetColor, traceColor, maxFillPixels);
    if (filledPixels <= 0 || filledPixels >= maxFillPixels) {
        return region;
    }

    if (filledPixels > totalPixels * 0.85) {
        return region;
    }

    QPainterPath tracedPath = traceFilledRegion(fillImage, traceColor);
    if (tracedPath.isEmpty()) {
        return region;
    }

    QTransform transform;
    transform.translate(bounds.left(), bounds.top());
    qreal invScale = 1.0 / scale;
    transform.scale(invScale, invScale);
    tracedPath = transform.map(tracedPath);
    tracedPath = tracedPath.simplified();
    tracedPath = smoothContour(tracedPath, 1.25);

    if (!tracedPath.contains(seedPoint)) {
        QPainterPath simplified = tracedPath.simplified();
        if (!simplified.contains(seedPoint)) {
            return region;
        }
    }

    QRectF pathBounds = tracedPath.boundingRect();

    qreal boundaryMargin = qMax(qreal(6.0), m_connectionTolerance * 2.0);
    if (qAbs(pathBounds.left() - bounds.left()) < boundaryMargin ||
        qAbs(pathBounds.right() - bounds.right()) < boundaryMargin ||
        qAbs(pathBounds.top() - bounds.top()) < boundaryMargin ||
        qAbs(pathBounds.bottom() - bounds.bottom()) < boundaryMargin) {
        return region;
    }

    QRectF canvasBounds = m_canvas->getCanvasRect();
    if (!canvasBounds.isEmpty()) {
        qreal pathArea = pathBounds.width() * pathBounds.height();
        qreal canvasArea = canvasBounds.width() * canvasBounds.height();
        if (canvasArea > 0.0 && pathArea > canvasArea * 0.9) {
            return region;
        }
    }

    region.outerBoundary = tracedPath;
    region.bounds = pathBounds;
    region.isValid = true;

    return region;
}

bool BucketFillTool::isPathClosed(const QPainterPath& path, qreal tolerance)
{
    if (path.elementCount() < 3) return false;

    QPointF start = path.pointAtPercent(0.0);
    QPointF end = path.pointAtPercent(1.0);

    return QLineF(start, end).length() <= tolerance;
}

QPainterPath BucketFillTool::closeOpenPath(const QPainterPath& path, qreal tolerance)
{
    if (path.isEmpty()) return path;

    QPainterPath closedPath = path;
    QPointF start = path.pointAtPercent(0.0);
    QPointF end = path.pointAtPercent(1.0);

    if (QLineF(start, end).length() <= tolerance * 3) {
        closedPath.lineTo(start);
        closedPath.closeSubpath();
    }

    return closedPath;
}

QPainterPath BucketFillTool::connectPathsByProximity(const QList<PathSegment>& segments, qreal maxDistance)
{
    if (segments.isEmpty()) return QPainterPath();

    QPainterPath result;
    QList<QPainterPath> paths;

    for (const PathSegment& segment : segments) {
        paths.append(segment.path);
    }

    if (paths.isEmpty()) return result;

    result = paths.first();
    QList<QPainterPath> remaining = paths.mid(1);

    bool madeConnection = true;
    while (madeConnection && !remaining.isEmpty()) {
        madeConnection = false;

        for (int i = 0; i < remaining.size(); ++i) {
            QPainterPath& candidatePath = remaining[i];

            QList<QPair<QPointF, QPointF>> connectionPairs;
            connectionPairs.append({ result.pointAtPercent(1.0), candidatePath.pointAtPercent(0.0) });
            connectionPairs.append({ result.pointAtPercent(1.0), candidatePath.pointAtPercent(1.0) });

            for (const auto& pair : connectionPairs) {
                if (QLineF(pair.first, pair.second).length() <= maxDistance) {
                    if (pair.second == candidatePath.pointAtPercent(1.0)) {
                        candidatePath = candidatePath.toReversed();
                    }

                    qreal gap = QLineF(pair.first, pair.second).length();
                    if (gap > 0.1) {
                        result.lineTo(pair.second);
                    }

                    result.connectPath(candidatePath);
                    remaining.removeAt(i);
                    madeConnection = true;
                    break;
                }
            }

            if (madeConnection) break;
        }
    }

    return closeOpenPath(result, maxDistance);
}

void BucketFillTool::performRasterFill(const QPointF& point)
{
    if (!m_canvas || !m_canvas->scene()) return;

    qreal searchRadius = qMax(m_searchRadius * 1.5, qreal(120.0));
    QList<PathSegment> nearbySegments = collectNearbyPaths(point, searchRadius);

    ClosedRegion reconstructed = buildClosedRegionUsingRaster(nearbySegments, point, searchRadius);
    if (reconstructed.isValid && !reconstructed.outerBoundary.isEmpty()) {
        QGraphicsPathItem* fillItem = createFillItem(reconstructed.outerBoundary, m_fillColor);
        if (fillItem) {
            addFillToCanvas(fillItem);
            qDebug() << "BucketFill: Raster fill via reconstructed region";
        }
        return;
    }

    if (nearbySegments.isEmpty()) {
        qDebug() << "BucketFill: Raster fill aborted - no nearby segments";
        return;
    }

    QRectF canvasRect = m_canvas->getCanvasRect();
    qreal size = qMax(qreal(200.0), searchRadius);
    QRectF fillArea(point.x() - size / 2, point.y() - size / 2, size, size);
    fillArea = fillArea.intersected(canvasRect);

    if (fillArea.isEmpty()) {
        qDebug() << "BucketFill: Fill area outside canvas bounds";
        return;
    }

    qreal maxDimension = qMax(fillArea.width(), fillArea.height());
    qreal scale = 2.0;
    if (maxDimension * scale > 512.0) {
        scale = qMax(qreal(1.0), 512.0 / maxDimension);
    }

    QImage sceneImage = renderSceneToImage(fillArea, scale, true);

    if (sceneImage.isNull()) {
        qDebug() << "BucketFill: Failed to render scene to image";
        return;
    }

    QPointF relativePoint = point - fillArea.topLeft();
    QPoint imagePoint(qRound(relativePoint.x() * scale), qRound(relativePoint.y() * scale));

    if (!sceneImage.rect().contains(imagePoint)) {
        qDebug() << "BucketFill: Click point outside rendered area";
        return;
    }

    QColor targetColor = getPixelColor(sceneImage, imagePoint);

    if (!targetColor.isValid()) {
        qDebug() << "BucketFill: Invalid target color for raster fill";
        return;
    }

    QImage fillImage = sceneImage.copy();
    QColor traceColor(255, 0, 255, 255);
    if (traceColor == targetColor) {
        traceColor = QColor(0, 255, 0, 255);
    }

    int totalPixels = sceneImage.width() * sceneImage.height();
    int maxFillPixels = qMin(totalPixels, 150000);
    int filledPixels = floodFillImageLimited(fillImage, imagePoint, targetColor, traceColor, maxFillPixels);

    qDebug() << "BucketFill: Fallback raster fill filled" << filledPixels << "pixels";

    if (filledPixels <= 0 || filledPixels >= maxFillPixels) {
        qDebug() << "BucketFill: Raster fill fallback could not determine region";
        return;
    }

    if (filledPixels > totalPixels * 0.85) {
        qDebug() << "BucketFill: Raster fill fallback detected excessive area";
        return;
    }

    QPainterPath filledPath = traceFilledRegion(fillImage, traceColor);

    if (!filledPath.isEmpty()) {
        QTransform transform;
        transform.translate(fillArea.x(), fillArea.y());
        qreal invScale = 1.0 / scale;
        transform.scale(invScale, invScale);
        filledPath = transform.map(filledPath);

        filledPath = smoothContour(filledPath, 1.4);
        QGraphicsPathItem* fillItem = createFillItem(filledPath, m_fillColor);
        if (fillItem) {
            addFillToCanvas(fillItem);
            qDebug() << "BucketFill: Successfully added contour-traced fallback fill";
        }
    }
    else {
        qDebug() << "BucketFill: Fallback raster tracing failed";
    }
}

QImage BucketFillTool::renderSceneToImage(const QRectF& region, qreal scale, bool antialiased)
{
    if (!m_canvas || !m_canvas->scene()) return QImage();

    int w = qMax(1, int(qCeil(region.width() * scale)));
    int h = qMax(1, int(qCeil(region.height() * scale)));

    // CRITICAL: Use non-premultiplied format
    QImage img(QSize(w, h), QImage::Format_ARGB32);
    img.fill(Qt::transparent);

    QPainter p(&img);

    // Option 1: Disable antialiasing entirely for fill detection
    // This gives the most stable results but slightly jagged previews
    if (!antialiased) {
        p.setRenderHint(QPainter::Antialiasing, false);
        p.setRenderHint(QPainter::TextAntialiasing, false);
        p.setRenderHint(QPainter::SmoothPixmapTransform, false);
    }

    p.scale(scale, scale);
    p.translate(-region.topLeft());
    m_canvas->scene()->render(&p, QRectF(0, 0, region.width(), region.height()), region);
    p.end();

    return img;
}

QColor BucketFillTool::getPixelColor(const QImage& image, const QPoint& point)
{
    if (point.x() >= 0 && point.x() < image.width() &&
        point.y() >= 0 && point.y() < image.height()) {
        return QColor(image.pixel(point));
    }
    return QColor();
}

int BucketFillTool::floodFillImageLimited(QImage& image, const QPoint& startPoint,
    const QColor& targetColor, const QColor& fillColor,
    int maxPixels)
{
    if (targetColor == fillColor) return 0;
    if (!image.rect().contains(startPoint)) return 0;

    const int width = image.width();
    const int height = image.height();

    // Use a binary mask for visited pixels
    std::vector<bool> visited(width * height, false);
    QQueue<QPoint> pointQueue;
    int filledCount = 0;

    // Determine if we're filling transparent or opaque areas
    bool fillingTransparent = targetColor.alpha() < 128;

    pointQueue.enqueue(startPoint);

    while (!pointQueue.isEmpty() && filledCount < maxPixels) {
        QPoint current = pointQueue.dequeue();

        if (current.x() < 0 || current.x() >= width ||
            current.y() < 0 || current.y() >= height) {
            continue;
        }

        int index = current.y() * width + current.x();
        if (visited[index]) continue;

        QRgb currentPixel = image.pixel(current);
        bool currentTransparent = qAlpha(currentPixel) < 128;

        // Binary decision: same fill type or not
        if (fillingTransparent != currentTransparent) continue;

        // Fill and mark visited
        image.setPixel(current, fillColor.rgba());
        visited[index] = true;
        filledCount++;

        // Add neighbors
        const QPoint neighbors[4] = {
            QPoint(current.x() + 1, current.y()),
            QPoint(current.x() - 1, current.y()),
            QPoint(current.x(), current.y() + 1),
            QPoint(current.x(), current.y() - 1)
        };

        for (const QPoint& neighbor : neighbors) {
            if (neighbor.x() >= 0 && neighbor.x() < width &&
                neighbor.y() >= 0 && neighbor.y() < height) {
                int nIndex = neighbor.y() * width + neighbor.x();
                if (!visited[nIndex]) {
                    pointQueue.enqueue(neighbor);
                }
            }
        }
    }

    return filledCount;
}

// Keep the standard flood fill for backwards compatibility
void BucketFillTool::floodFillImage(QImage& image, const QPoint& startPoint,
    const QColor& targetColor, const QColor& fillColor)
{
    floodFillImageLimited(image, startPoint, targetColor, fillColor, 50000);
}


QPainterPath BucketFillTool::traceFilledRegion(const QImage& image, const QColor& fillColor)
{
    // 1. Find a filled start point
    QPoint startPoint = findStartPoint(image, fillColor);
    if (startPoint.x() == -1) {
        return QPainterPath(); // nothing to trace
    }

    // 2. Use improved marching-squares border walk to get raw contour
    QVector<QPointF> contour = traceContour(image, startPoint, fillColor);

    if (contour.size() < 3) {
        return QPainterPath(); // not enough points to form a region
    }

    // 3. Simplify the contour using Ramer–Douglas–Peucker
    contour = ramerDouglasPeucker(contour, 1.5);
    // epsilon ~1.5 pixels gives a good balance
    // Increase epsilon for smoother, looser outlines

    // 4. Build QPainterPath from simplified contour
    QPainterPath path;
    path.moveTo(contour.first());
    for (int i = 1; i < contour.size(); ++i) {
        path.lineTo(contour[i]);
    }
    path.closeSubpath();

    return path;
}

QPoint BucketFillTool::findStartPoint(const QImage& image, const QColor& fillColor)
{
    // Find the topmost, leftmost filled pixel
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (getPixelColor(image, QPoint(x, y)) == fillColor) {
                return QPoint(x, y);
            }
        }
    }
    return QPoint(-1, -1);
}

// Helper to check if a pixel is inside bounds & filled

inline bool isFilled(const QImage& mask, int x, int y) {
    if (x < 0 || y < 0 || x >= mask.width() || y >= mask.height())
        return false;
    return qAlpha(mask.pixel(x, y)) > 0; // Or mask.pixelColor(x,y).alpha() > 0
}

QVector<QPointF> BucketFillTool::traceContour(const QImage& mask, const QPoint& start, const QColor& fillColor)
{
    QVector<QPointF> contour;
    const int w = mask.width();
    const int h = mask.height();

    auto isFilled = [&](int x, int y) {
        if (x < 0 || y < 0 || x >= w || y >= h) return false;
        return mask.pixelColor(x, y) == fillColor;
        };

    // Marching Squares directions: right, down, left, up
    const QPoint dirs[4] = { {1,0}, {0,1}, {-1,0}, {0,-1} };

    QPoint pos = start;
    int dir = 0; // initial direction: right
    do {
        // Sub-pixel point at pixel edge
        contour.append(QPointF(pos.x() + 0.5, pos.y() + 0.5));

        // Build the 4-bit square mask
        int bits = 0;
        if (isFilled(pos.x(), pos.y())) bits |= 1; // top-left
        if (isFilled(pos.x() + 1, pos.y())) bits |= 2; // top-right
        if (isFilled(pos.x() + 1, pos.y() + 1)) bits |= 4; // bottom-right
        if (isFilled(pos.x(), pos.y() + 1)) bits |= 8; // bottom-left

        // Determine new direction based on configuration
        switch (bits) {
        case 1: case 5: case 13: dir = 3; break; // up
        case 8: case 10: case 11: dir = 1; break; // down
        case 4: case 12: case 14: dir = 0; break; // right
        case 2: case 3: case 7: dir = 2; break;   // left
        default: dir = (dir + 1) % 4; break;      // fallback turn
        }

        pos += dirs[dir];

    } while (pos != start && contour.size() < (w * h * 4)); // safety cutoff

    return contour;
}


// Recursive helper for RDP
void BucketFillTool::rdpSimplify(const QVector<QPointF>& points, double epsilon,
    int startIdx, int endIdx, QVector<bool>& keep)
{
    double maxDist = 0.0;
    int index = startIdx;

    const QPointF& start = points[startIdx];
    const QPointF& end = points[endIdx];

    // Line segment vector
    const double dx = end.x() - start.x();
    const double dy = end.y() - start.y();
    const double lenSq = dx * dx + dy * dy;

    for (int i = startIdx + 1; i < endIdx; ++i) {
        // Perpendicular distance from point to line
        double num = fabs(dy * points[i].x() - dx * points[i].y() + end.x() * start.y() - end.y() * start.x());
        double dist = num / sqrt(lenSq);
        if (dist > maxDist) {
            index = i;
            maxDist = dist;
        }
    }

    if (maxDist > epsilon) {
        keep[index] = true;
        rdpSimplify(points, epsilon, startIdx, index, keep);
        rdpSimplify(points, epsilon, index, endIdx, keep);
    }
}

QVector<QPointF> BucketFillTool::ramerDouglasPeucker(const QVector<QPointF>& points, double epsilon)
{
    if (points.size() < 3) return points;

    QVector<bool> keep(points.size(), false);
    keep[0] = keep[points.size() - 1] = true;

    rdpSimplify(points, epsilon, 0, points.size() - 1, keep);

    QVector<QPointF> result;
    for (int i = 0; i < points.size(); ++i)
        if (keep[i]) result.append(points[i]);

    return result;
}


QPainterPath BucketFillTool::pointsToPath(const QList<QPoint>& points)
{
    QPainterPath path;

    if (points.isEmpty()) return path;

    path.moveTo(points.first());

    for (int i = 1; i < points.size(); ++i) {
        path.lineTo(points[i]);
    }

    path.closeSubpath();
    return path;
}

QPainterPath BucketFillTool::smoothContour(const QPainterPath& roughPath, qreal smoothing)
{
    if (roughPath.elementCount() < 4) return roughPath;

    QPainterPath smoothPath;
    QList<QPointF> points;

    for (int i = 0; i < roughPath.elementCount(); ++i) {
        QPainterPath::Element elem = roughPath.elementAt(i);
        points.append(QPointF(elem.x, elem.y));
    }

    if (points.size() < 4) return roughPath;

    smoothPath.moveTo(points.first());

    for (int i = 1; i < points.size() - 2; ++i) {
        QPointF p0 = points[i - 1];
        QPointF p1 = points[i];
        QPointF p2 = points[i + 1];
        QPointF p3 = (i + 2 < points.size()) ? points[i + 2] : points.first();

        QPointF cp1 = p1 + (p2 - p0) / (6.0 * smoothing);
        QPointF cp2 = p2 - (p3 - p1) / (6.0 * smoothing);

        smoothPath.cubicTo(cp1, cp2, p2);
    }

    smoothPath.closeSubpath();
    return smoothPath;
}

QGraphicsPathItem* BucketFillTool::createFillItem(const QPainterPath& fillPath, const QColor& color)
{
    if (fillPath.isEmpty()) return nullptr;

    QGraphicsPathItem* fillItem = new QGraphicsPathItem(fillPath);
    fillItem->setBrush(QBrush(color));
    fillItem->setPen(QPen(Qt::NoPen));
    fillItem->setFlag(QGraphicsItem::ItemIsSelectable, true);
    fillItem->setFlag(QGraphicsItem::ItemIsMovable, true);
    fillItem->setData(GraphicsItemRoles::BucketFillBehindStrokeRole, true);

    qDebug() << "BucketFill: Created fill item with color" << color.name();
    return fillItem;
}

void BucketFillTool::addFillToCanvas(QGraphicsPathItem* fillItem)
{
    if (!fillItem || !m_canvas) return;

    const qreal fillZValue = -500.0;
    fillItem->setZValue(fillZValue);

    if (m_mainWindow && m_mainWindow->m_undoStack) {
        DrawCommand* command = new DrawCommand(m_canvas, fillItem);
        m_mainWindow->m_undoStack->push(command);

        qDebug() << "BucketFill: Added fill to canvas with undo support";
    }
    else {
        addItemToCanvas(fillItem);
        qDebug() << "BucketFill: Added fill directly to canvas";
    }
}

void BucketFillTool::showFillPreview(const QPainterPath& path)
{
    hideFillPreview();

    if (path.isEmpty() || !m_canvas || !m_canvas->scene()) return;

    m_previewItem = new QGraphicsPathItem(path);
    QColor previewColor = m_fillColor;
    previewColor.setAlpha(100);
    m_previewItem->setBrush(QBrush(previewColor));
    m_previewItem->setPen(QPen(m_fillColor, 1, Qt::DashLine));
    m_previewItem->setFlag(QGraphicsItem::ItemIsSelectable, false);
    m_previewItem->setFlag(QGraphicsItem::ItemIsMovable, false);
    m_previewItem->setZValue(1000);

    m_canvas->scene()->addItem(m_previewItem);
}

void BucketFillTool::hideFillPreview()
{
    if (m_previewItem && m_canvas && m_canvas->scene()) {
        m_canvas->scene()->removeItem(m_previewItem);
        delete m_previewItem;
        m_previewItem = nullptr;
    }
}

// Settings methods
void BucketFillTool::setFillColor(const QColor& color)
{
    m_fillColor = color;
    qDebug() << "BucketFill: Fill color set to" << color.name();
}

void BucketFillTool::setTolerance(int tolerance)
{
    m_tolerance = qBound(0, tolerance, 100);
}

void BucketFillTool::setFillMode(int mode)
{
    m_fillMode = qBound(0, mode, 1);
}

void BucketFillTool::setSearchRadius(qreal radius)
{
    m_searchRadius = qMax(10.0, radius);
}

void BucketFillTool::setConnectionTolerance(qreal tolerance)
{
    m_connectionTolerance = qMax(1.0, tolerance);
}

void BucketFillTool::setDebugMode(bool enabled)
{
    m_debugMode = enabled;
}

QColor BucketFillTool::getFillColor() const { return m_fillColor; }
int BucketFillTool::getTolerance() const { return m_tolerance; }
int BucketFillTool::getFillMode() const { return m_fillMode; }
qreal BucketFillTool::getSearchRadius() const { return m_searchRadius; }
qreal BucketFillTool::getConnectionTolerance() const { return m_connectionTolerance; }
bool BucketFillTool::isDebugMode() const { return m_debugMode; }

// Placeholder methods for interface compatibility
bool BucketFillTool::pathsIntersect(const QPainterPath& path1, const QPainterPath& path2, qreal tolerance)
{
    return path1.intersects(path2);
}

QList<QPointF> BucketFillTool::findPathIntersections(const QPainterPath& path1, const QPainterPath& path2)
{
    return QList<QPointF>();
}

QPainterPath BucketFillTool::mergeIntersectingPaths(const QList<PathSegment>& segments)
{
    QPainterPath result;
    for (const PathSegment& seg : segments) {
        result = result.united(seg.path);
    }
    return result;
}

QPainterPath BucketFillTool::detectShape(const QPointF& point)
{
    return QPainterPath();
}

bool BucketFillTool::isPointInsideEnclosedArea(const QPointF& point, const QList<PathSegment>& paths)
{
    for (const PathSegment& seg : paths) {
        if (seg.path.contains(point)) return true;
    }
    return false;
}

QPainterPath BucketFillTool::createBoundingShape(const QList<PathSegment>& segments)
{
    QPainterPath result;
    for (const PathSegment& seg : segments) {
        result = result.united(seg.path);
    }
    return result;
}

QList<BucketFillTool::PathSegment> BucketFillTool::optimizePathSegments(const QList<PathSegment>& segments)
{
    return segments;
}

QPainterPath BucketFillTool::simplifyPath(const QPainterPath& path, qreal tolerance)
{
    return path;
}

void BucketFillTool::cacheNearbyItems(const QRectF& region)
{
    m_cachedRegion = region;
    m_cachedPaths = collectNearbyPaths(region.center(), region.width() / 2);
    m_cacheValid = true;
}

void BucketFillTool::clearCache()
{
    m_cachedPaths.clear();
    m_cacheValid = false;
}

void BucketFillTool::debugDrawPaths(const QList<PathSegment>& segments) {}
void BucketFillTool::debugDrawIntersections(const QList<QPointF>& intersections) {}
void BucketFillTool::debugDrawContour(const QList<QPoint>& contour) {}

QList<QPoint> BucketFillTool::getNeighbors8(const QPoint& point)
{
    QList<QPoint> neighbors;
    for (int i = 0; i < 8; ++i) {
        neighbors.append(point + DIRECTIONS[i]);
    }
    return neighbors;
}

QPoint BucketFillTool::getNextNeighbor(const QPoint& current, int direction)
{
    return current + DIRECTIONS[direction % 8];
}

int BucketFillTool::getDirection(const QPoint& from, const QPoint& to)
{
    QPoint diff = to - from;
    for (int i = 0; i < 8; ++i) {
        if (DIRECTIONS[i] == diff) {
            return i;
        }
    }
    return 0;
}
