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
#include <QLineF>
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

qreal averageEdgeLength(const QPolygonF& polygon)
{
    const int count = polygon.size();
    if (count < 2) {
        return 0.0;
    }

    qreal totalLength = 0.0;
    for (int i = 0; i < count; ++i) {
        const QPointF& current = polygon.at(i);
        const QPointF& next = polygon.at((i + 1) % count);
        totalLength += QLineF(current, next).length();
    }

    return (count > 0) ? totalLength / count : 0.0;
}

QPolygonF removeNearDuplicatePoints(const QPolygonF& polygon, qreal threshold)
{
    if (polygon.size() < 2) {
        return polygon;
    }

    const qreal epsilon = qMax<qreal>(threshold, 0.0001);
    QPolygonF result;
    result.reserve(polygon.size());

    for (const QPointF& point : polygon) {
        if (result.isEmpty() || QLineF(result.last(), point).length() > epsilon) {
            result.append(point);
        }
    }

    if (result.size() > 2 && QLineF(result.first(), result.last()).length() <= epsilon) {
        result.removeLast();
    }

    return result;
}

QPolygonF densifyPolygon(const QPolygonF& polygon, qreal maxSegmentLength)
{
    if (polygon.size() < 2 || maxSegmentLength <= 0.0) {
        return polygon;
    }

    QPolygonF result;
    const int count = polygon.size();
    result.reserve(count * 2);

    for (int i = 0; i < count; ++i) {
        const QPointF& current = polygon.at(i);
        const QPointF& next = polygon.at((i + 1) % count);

        result.append(current);

        QLineF edge(current, next);
        qreal length = edge.length();

        if (length > maxSegmentLength) {
            int subdivisions = qBound(0, static_cast<int>(qFloor(length / maxSegmentLength)), 64);
            for (int s = 1; s <= subdivisions; ++s) {
                qreal t = static_cast<qreal>(s) / (subdivisions + 1);
                result.append(current + (next - current) * t);
            }
        }
    }

    return result;
}

QPolygonF relaxPolygon(const QPolygonF& polygon, qreal factor, int iterations)
{
    if (polygon.size() < 3 || factor <= 0.0 || iterations <= 0) {
        return polygon;
    }

    QPolygonF result = polygon;
    const int count = polygon.size();

    for (int iter = 0; iter < iterations; ++iter) {
        QPolygonF next;
        next.reserve(count);

        for (int i = 0; i < count; ++i) {
            const QPointF& prev = result.at((i - 1 + count) % count);
            const QPointF& current = result.at(i);
            const QPointF& nextPoint = result.at((i + 1) % count);

            QPointF relaxed = current * (1.0 - factor) + (prev + nextPoint) * (factor * 0.5);
            next.append(relaxed);
        }

        result = next;
    }

    return result;
}

struct PathSmoothingData
{
    QPainterPath path;
    qreal averageEdgeLength = 0.0;
    int polygonCount = 0;
};

PathSmoothingData buildSmoothPath(const QPainterPath& sourcePath, qreal baseSpacing)
{
    PathSmoothingData data;
    data.path.setFillRule(sourcePath.fillRule());

    if (sourcePath.isEmpty() || baseSpacing <= 0.0) {
        return data;
    }

    const qreal spacing = qMax<qreal>(baseSpacing, 0.5);
    const QVector<QPolygonF> polygons = sourcePath.toFillPolygons();

    for (const QPolygonF& polygon : polygons) {
        if (polygon.size() < 3) {
            continue;
        }

        QPolygonF cleaned = removeNearDuplicatePoints(polygon, spacing * 0.2);
        if (cleaned.size() < 3) {
            continue;
        }

        qreal averageEdge = averageEdgeLength(cleaned);
        if (!qIsFinite(averageEdge) || averageEdge <= 0.0) {
            averageEdge = spacing;
        }

        qreal segmentLength = qBound<qreal>(spacing * 0.8, averageEdge, spacing * 3.5);
        QPolygonF densified = densifyPolygon(cleaned, segmentLength);

        int iterations = (averageEdge <= spacing * 1.4) ? 3 : 2;
        qreal weight = (averageEdge <= spacing * 1.2) ? 0.36 : 0.32;
        QPolygonF smoothed = smoothPolygonChaikin(densified, iterations, weight);

        qreal relaxFactor = (averageEdge <= spacing * 1.6) ? 0.26 : 0.2;
        smoothed = relaxPolygon(smoothed, relaxFactor, 1);
        smoothed = removeNearDuplicatePoints(smoothed, spacing * 0.15);

        if (smoothed.size() < 3) {
            continue;
        }

        QPainterPath polyPath;
        polyPath.addPolygon(smoothed);
        polyPath.closeSubpath();
        data.path.addPath(polyPath);

        data.averageEdgeLength += averageEdge;
        ++data.polygonCount;
    }

    data.path = data.path.simplified();
    return data;
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
            ClosedRegion region = findEnclosedRegion(scenePos, false);

            if ((!region.isValid || region.outerBoundary.isEmpty()) && m_fillMode == 1) {
                bool touchesEdge = false;
                region = floodFillRegionFromArea(canvasRect, scenePos, touchesEdge, false);
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
                ClosedRegion region = findEnclosedRegion(scenePos, true);
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

BucketFillTool::ClosedRegion BucketFillTool::findEnclosedRegion(const QPointF& point, bool forPreview)
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
    const int maxSteps = forPreview ? 3 : 6;
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
        ClosedRegion candidate = floodFillRegionFromArea(sampleRect, point, touchesEdge, forPreview);
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
            if (forPreview) {
                break;
            }
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
        if (forPreview) {
            return invalidRegion;
        }
        if (m_fillMode == 1) {
            bool touchesEdge = false;
            ClosedRegion canvasRegion = floodFillRegionFromArea(canvasRect, point, touchesEdge, forPreview);
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
    const QPointF& scenePoint, bool& touchesEdge, bool forPreview)
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

    const qreal maxImageDim = forPreview ? 256.0 : 512.0;
    qreal scale = maxImageDim / maxDimension;
    scale = qBound<qreal>(0.1, scale, 3.0);

    QImage image = renderSceneToImage(area, scale);
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
    stack.reserve(qMin(totalPixels, forPreview ? 32768 : 65536));
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

    QPainterPath clipPath;
    clipPath.addRect(area);

    const qreal pixelStep = (scale > 0.0) ? (1.0 / scale) : 1.0;
    const qreal smoothingPixel = qBound<qreal>(0.5, pixelStep, 2.4);

    QPainterPathStroker expansionStroker;
    expansionStroker.setCapStyle(Qt::RoundCap);
    expansionStroker.setJoinStyle(Qt::RoundJoin);
    expansionStroker.setWidth(qBound<qreal>(0.65, smoothingPixel * 1.6, 1.9));
    QPainterPath refinedPath = scenePath.united(expansionStroker.createStroke(scenePath)).simplified();
    refinedPath = refinedPath.intersected(clipPath);

    if (!forPreview) {
        PathSmoothingData smoothingData = buildSmoothPath(refinedPath, smoothingPixel);
        if (!smoothingData.path.isEmpty()) {
            QPainterPath candidate = smoothingData.path.united(refinedPath).simplified();
            candidate = candidate.intersected(clipPath);
            if (candidate.contains(scenePoint)) {
                refinedPath = candidate;
            }
        }

        if (smoothingData.polygonCount > 0) {
            qreal averageEdge = smoothingData.averageEdgeLength / smoothingData.polygonCount;
            qreal roundWidth = qBound<qreal>(0.6, qMax(averageEdge, smoothingPixel) * 1.05, 1.7);
            QPainterPathStroker rounder;
            rounder.setCapStyle(Qt::RoundCap);
            rounder.setJoinStyle(Qt::RoundJoin);
            rounder.setWidth(roundWidth);
            QPainterPath candidate = refinedPath.united(rounder.createStroke(refinedPath)).simplified();
            candidate = candidate.intersected(clipPath);
            if (candidate.contains(scenePoint)) {
                refinedPath = candidate;
            }
        }

        qreal contourSpacing = qBound<qreal>(0.55, smoothingPixel * 1.3, 1.8);
        QPainterPath curvedPath = smoothContour(refinedPath, contourSpacing);
        if (!curvedPath.isEmpty() && curvedPath.contains(scenePoint)) {
            refinedPath = curvedPath.simplified();
        }
    }

    refinedPath = refinedPath.intersected(clipPath);

    region.outerBoundary = refinedPath;
    region.bounds = refinedPath.boundingRect();
    region.isValid = true;

    return region;
}

bool BucketFillTool::colorsSimilar(QRgb first, QRgb second) const
{
    if (qAlpha(first) < 10 && qAlpha(second) < 10) {
        return true;
    }

    const int diff = qAbs(qRed(first) - qRed(second)) +
        qAbs(qGreen(first) - qGreen(second)) +
        qAbs(qBlue(first) - qBlue(second)) +
        qAbs(qAlpha(first) - qAlpha(second));

    const int baseTolerance = 12;
    const int multiplier = 4;
    return diff <= baseTolerance + m_tolerance * multiplier;
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

    QImage sceneImage = renderSceneToImage(bounds, scale);
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

    QImage sceneImage = renderSceneToImage(fillArea, scale);

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

QImage BucketFillTool::renderSceneToImage(const QRectF& region, qreal scale)
{
    if (!m_canvas || !m_canvas->scene()) return QImage();

    int width = qMax(1, static_cast<int>(qCeil(region.width() * scale)));
    int height = qMax(1, static_cast<int>(qCeil(region.height() * scale)));
    QImage image(QSize(width, height), QImage::Format_ARGB32);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, false); // Sharp edges for flood fill
    painter.scale(scale, scale);
    painter.translate(-region.topLeft());

    // FIXED: Use scene's built-in render method - much safer
    m_canvas->scene()->render(&painter, QRectF(0, 0, region.width(), region.height()), region);

    painter.end();
    return image;
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
    const QColor& targetColor, const QColor& fillColor, int maxPixels)
{
    if (targetColor == fillColor) return 0;
    if (!image.rect().contains(startPoint)) return 0;

    QQueue<QPoint> pointQueue;
    QSet<QPoint> visited;
    int filledCount = 0;

    pointQueue.enqueue(startPoint);

    while (!pointQueue.isEmpty() && filledCount < maxPixels) {
        QPoint current = pointQueue.dequeue();

        if (visited.contains(current)) continue;
        if (!image.rect().contains(current)) continue;

        QColor currentColor = getPixelColor(image, current);

        // FIXED: Stricter color matching
        int colorDiff = qAbs(currentColor.red() - targetColor.red()) +
            qAbs(currentColor.green() - targetColor.green()) +
            qAbs(currentColor.blue() - targetColor.blue()) +
            qAbs(currentColor.alpha() - targetColor.alpha());

        if (colorDiff > m_tolerance * 2) continue; // Stricter tolerance

        // Fill this pixel
        image.setPixel(current, fillColor.rgba());
        visited.insert(current);
        filledCount++;

        // Add 4-connected neighbors only
        QList<QPoint> neighbors = {
            QPoint(current.x() + 1, current.y()),
            QPoint(current.x() - 1, current.y()),
            QPoint(current.x(), current.y() + 1),
            QPoint(current.x(), current.y() - 1)
        };

        for (const QPoint& neighbor : neighbors) {
            if (!visited.contains(neighbor)) {
                pointQueue.enqueue(neighbor);
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
    QPoint startPoint = findStartPoint(image, fillColor);
    if (startPoint.x() == -1) {
        return QPainterPath();
    }

    return traceContour(image, startPoint, fillColor);
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

QPainterPath BucketFillTool::traceContour(const QImage& image, const QPoint& startPoint, const QColor& fillColor)
{
    QList<QPoint> contourPoints;
    QPoint current = startPoint;
    QPoint first = startPoint;
    int direction = 0;

    int maxPoints = qBound(1000, qMax(image.width(), image.height()) * 8, 20000);

    do {
        contourPoints.append(current);

        bool foundNext = false;
        int searchDir = (direction + 6) % 8;

        for (int i = 0; i < 8; ++i) {
            QPoint neighbor = current + DIRECTIONS[searchDir];

            if (image.rect().contains(neighbor) &&
                getPixelColor(image, neighbor) == fillColor) {
                current = neighbor;
                direction = searchDir;
                foundNext = true;
                break;
            }

            searchDir = (searchDir + 1) % 8;
        }

        if (!foundNext || contourPoints.size() >= maxPoints) {
            break;
        }

    } while (current != first || contourPoints.size() < 3);

    return pointsToPath(contourPoints);
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
    if (roughPath.isEmpty() || roughPath.elementCount() < 4) {
        return roughPath;
    }

    QPainterPath simplified = roughPath.simplified();

    qreal baseSpacing = qBound<qreal>(0.5, smoothing, 2.4);

    QPainterPathStroker stroker;
    stroker.setCapStyle(Qt::RoundCap);
    stroker.setJoinStyle(Qt::RoundJoin);
    stroker.setWidth(qBound<qreal>(0.55, baseSpacing * 1.8, 1.8));
    simplified = simplified.united(stroker.createStroke(simplified)).simplified();

    PathSmoothingData data = buildSmoothPath(simplified, baseSpacing);
    if (data.path.isEmpty()) {
        return simplified;
    }

    QPainterPath result = data.path.united(simplified).simplified();

    if (data.polygonCount > 0) {
        qreal averageEdge = data.averageEdgeLength / data.polygonCount;
        qreal extraWidth = qBound<qreal>(0.5, qMax(averageEdge, baseSpacing) * 0.9, 1.5);
        QPainterPathStroker rounder;
        rounder.setCapStyle(Qt::RoundCap);
        rounder.setJoinStyle(Qt::RoundJoin);
        rounder.setWidth(extraWidth);
        result = result.united(rounder.createStroke(result)).simplified();
    }

    return result;
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