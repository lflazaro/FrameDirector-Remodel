#include "BucketFillTool.h"
#include "MainWindow.h"
#include "Canvas.h"
#include "Commands/UndoCommands.h"
#include <QGraphicsScene>
#include <QGraphicsPathItem>
#include <QGraphicsPixmapItem>
#include <QPainter>
#include <QPainterPath>
#include <QApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <QtMath>

BucketFillTool::BucketFillTool(MainWindow* mainWindow, QObject* parent)
    : Tool(mainWindow, parent)
    , m_fillColor(Qt::red)
    , m_tolerance(10)
    , m_fillMode(0) // Vector fill by default
    , m_searchRadius(100.0)
    , m_connectionTolerance(5.0)
    , m_cacheValid(false)
    , m_previewItem(nullptr)
    , m_debugMode(false)
{
}

void BucketFillTool::mousePressEvent(QMouseEvent* event, const QPointF& scenePos)
{
    if (!m_canvas || event->button() != Qt::LeftButton) return;

    QElapsedTimer timer;
    timer.start();

    qDebug() << "BucketFill: Starting fill operation at" << scenePos;

    // Hide any existing preview
    hideFillPreview();

    try {
        if (m_fillMode == 0) {
            // Vector-based filling (preferred method)
            ClosedRegion region = findEnclosedRegion(scenePos);

            if (region.isValid && !region.outerBoundary.isEmpty()) {
                // Create fill item
                QGraphicsPathItem* fillItem = createFillItem(region.outerBoundary, m_fillColor);
                if (fillItem) {
                    auto itemsAtPoint = m_canvas->scene()->items(scenePos);
                    for (QGraphicsItem* it : itemsAtPoint) {
                        qDebug() << "  Item" << it << "Z =" << it->zValue()
                            << "type =" << it->type()
                            << "brush =" << (it->data(0).toString());
                        // (you can store e.g. “BrushStroke” in data(0) when you create strokes)
                    }
                    addFillToCanvas(fillItem);
                    qDebug() << "BucketFill: Vector fill completed in" << timer.elapsed() << "ms";
                }
                else {
                    qDebug() << "BucketFill: Failed to create fill item";
                }
            }
            else {
                qDebug() << "BucketFill: No enclosed region found, falling back to raster fill";
                // Fall back to raster filling
                performRasterFill(scenePos);
            }
        }
        else {
            // Raster-based filling
            performRasterFill(scenePos);
            qDebug() << "BucketFill: Raster fill completed in" << timer.elapsed() << "ms";
        }
    }
    catch (const std::exception& e) {
        qDebug() << "BucketFill: Exception occurred:" << e.what();
    }
}

void BucketFillTool::mouseMoveEvent(QMouseEvent* event, const QPointF& scenePos)
{
    // Show preview of what would be filled
    if (event->buttons() == Qt::NoButton) {
        try {
            ClosedRegion region = findEnclosedRegion(scenePos);
            if (region.isValid && !region.outerBoundary.isEmpty()) {
                showFillPreview(region.outerBoundary);
            }
            else {
                hideFillPreview();
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
    // Return a bucket cursor or crosshair
    return Qt::CrossCursor;
}

BucketFillTool::ClosedRegion BucketFillTool::findEnclosedRegion(const QPointF& point)
{
    ClosedRegion region;
    region.isValid = false;

    // Step 1: Collect nearby path items
    QList<PathSegment> nearbyPaths = collectNearbyPaths(point, m_searchRadius);

    if (nearbyPaths.isEmpty()) {
        qDebug() << "BucketFill: No nearby paths found";
        return region;
    }

    qDebug() << "BucketFill: Found" << nearbyPaths.size() << "nearby path segments";

    // Step 2: Check if point is already inside a closed path
    for (const PathSegment& segment : nearbyPaths) {
        if (isPathClosed(segment.path) && segment.path.contains(point)) {
            region.outerBoundary = segment.path;
            region.bounds = segment.bounds;
            region.isValid = true;
            qDebug() << "BucketFill: Point is inside an existing closed path";
            return region;
        }
    }

    // Step 3: Try to create a closed path from multiple segments
    QPainterPath closedPath = createClosedPath(nearbyPaths, point);

    if (!closedPath.isEmpty() && closedPath.contains(point)) {
        region.outerBoundary = closedPath;
        region.bounds = closedPath.boundingRect();
        region.isValid = true;
        qDebug() << "BucketFill: Created closed path from multiple segments";
        return region;
    }

    // Step 4: Try connecting paths by proximity
    QPainterPath connectedPath = connectPathsByProximity(nearbyPaths, m_connectionTolerance);

    if (!connectedPath.isEmpty() && connectedPath.contains(point)) {
        region.outerBoundary = connectedPath;
        region.bounds = connectedPath.boundingRect();
        region.isValid = true;
        qDebug() << "BucketFill: Created path by connecting nearby segments";
        return region;
    }

    qDebug() << "BucketFill: Could not find or create enclosed region";
    return region;
}

QList<BucketFillTool::PathSegment> BucketFillTool::collectNearbyPaths(const QPointF& center, qreal searchRadius)
{
    QList<PathSegment> segments;

    if (!m_canvas || !m_canvas->scene()) return segments;

    // Define search area
    QRectF searchRect(center.x() - searchRadius, center.y() - searchRadius,
        searchRadius * 2, searchRadius * 2);

    // Get all items in the search area
    QList<QGraphicsItem*> items = m_canvas->scene()->items(searchRect, Qt::IntersectsItemBoundingRect);

    for (QGraphicsItem* item : items) {
        if (auto pathItem = qgraphicsitem_cast<QGraphicsPathItem*>(item)) {
            PathSegment segment;
            segment.path = pathItem->path();
            segment.item = pathItem;
            segment.bounds = pathItem->boundingRect();

            // Transform path to scene coordinates
            if (!pathItem->transform().isIdentity()) {
                segment.path = pathItem->transform().map(segment.path);
            }
            segment.path.translate(pathItem->pos());

            segments.append(segment);
        }
    }

    return segments;
}

QPainterPath BucketFillTool::createClosedPath(const QList<PathSegment>& segments, const QPointF& seedPoint)
{
    if (segments.isEmpty()) return QPainterPath();

    // Try to find segments that can form a closed shape
    QList<QPainterPath> candidatePaths;

    for (const PathSegment& segment : segments) {
        candidatePaths.append(segment.path);
    }

    // Start with the path closest to the seed point
    QPainterPath result;
    qreal minDistance = std::numeric_limits<qreal>::max();
    int startIndex = -1;

    for (int i = 0; i < candidatePaths.size(); ++i) {
        QPointF closestPoint = candidatePaths[i].pointAtPercent(0.5); // Middle of path
        qreal distance = QLineF(seedPoint, closestPoint).length();
        if (distance < minDistance) {
            minDistance = distance;
            startIndex = i;
        }
    }

    if (startIndex == -1) return QPainterPath();

    result = candidatePaths[startIndex];
    QList<QPainterPath> remainingPaths = candidatePaths;
    remainingPaths.removeAt(startIndex);

    // Try to connect other paths to form a closed shape
    const qreal connectionTolerance = m_connectionTolerance * 2; // Be more generous
    bool foundConnection = true;

    while (foundConnection && !remainingPaths.isEmpty()) {
        foundConnection = false;
        QPointF resultEnd = result.pointAtPercent(1.0);

        for (int i = 0; i < remainingPaths.size(); ++i) {
            QPainterPath& path = remainingPaths[i];
            QPointF pathStart = path.pointAtPercent(0.0);
            QPointF pathEnd = path.pointAtPercent(1.0);

            // Check if we can connect to the start of this path
            if (QLineF(resultEnd, pathStart).length() <= connectionTolerance) {
                result.connectPath(path);
                remainingPaths.removeAt(i);
                foundConnection = true;
                break;
            }
            // Check if we can connect to the end of this path (reversed)
            else if (QLineF(resultEnd, pathEnd).length() <= connectionTolerance) {
                QPainterPath reversedPath = path.toReversed();
                result.connectPath(reversedPath);
                remainingPaths.removeAt(i);
                foundConnection = true;
                break;
            }
        }
    }

    // Check if we have a closed path or if we can close it
    if (isPathClosed(result)) {
        return result;
    }
    else {
        QPainterPath closedResult = closeOpenPath(result, connectionTolerance);
        if (isPathClosed(closedResult) && closedResult.contains(seedPoint)) {
            return closedResult;
        }
    }

    return QPainterPath();
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

    if (QLineF(start, end).length() <= tolerance * 3) { // Allow slightly larger gap for closing
        closedPath.lineTo(start); // Close the path
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

    // Start with the first path
    result = paths.first();
    QList<QPainterPath> remaining = paths.mid(1);

    // Keep connecting paths until no more connections can be made
    bool madeConnection = true;
    while (madeConnection && !remaining.isEmpty()) {
        madeConnection = false;

        for (int i = 0; i < remaining.size(); ++i) {
            QPainterPath& candidatePath = remaining[i];

            // Check various connection points
            QList<QPair<QPointF, QPointF>> connectionPairs;

            // End of result to start of candidate
            connectionPairs.append({ result.pointAtPercent(1.0), candidatePath.pointAtPercent(0.0) });
            // End of result to end of candidate (reversed)
            connectionPairs.append({ result.pointAtPercent(1.0), candidatePath.pointAtPercent(1.0) });

            for (const auto& pair : connectionPairs) {
                if (QLineF(pair.first, pair.second).length() <= maxDistance) {
                    // Make the connection
                    if (pair.second == candidatePath.pointAtPercent(1.0)) {
                        // Connect to end, so reverse the candidate path
                        candidatePath = candidatePath.toReversed();
                    }

                    // Add a connecting line if there's a gap
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

    // Try to close the final path
    return closeOpenPath(result, maxDistance);
}

void BucketFillTool::performRasterFill(const QPointF& point)
{
    if (!m_canvas || !m_canvas->scene()) return;

    // Define a reasonable area around the click point for raster filling
    qreal size = 200; // 200x200 pixel area
    QRectF fillArea(point.x() - size / 2, point.y() - size / 2, size, size);

    // Render the scene area to an image
    QImage sceneImage = renderSceneToImage(fillArea, 2.0); // 2x scale for better quality

    if (sceneImage.isNull()) {
        qDebug() << "BucketFill: Failed to render scene to image";
        return;
    }

    // Convert the scene point to image coordinates
    QPointF relativePoint = point - fillArea.topLeft();
    QPoint imagePoint(relativePoint.x() * 2, relativePoint.y() * 2); // Account for 2x scale

    if (!sceneImage.rect().contains(imagePoint)) {
        qDebug() << "BucketFill: Click point is outside rendered area";
        return;
    }

    // Get the target color at the click point
    QColor targetColor = getPixelColor(sceneImage, imagePoint);

    // Don't fill if target color is the same as fill color
    if (targetColor == m_fillColor) {
        qDebug() << "BucketFill: Target color same as fill color, no action needed";
        return;
    }

    // Perform flood fill on the image
    floodFillImage(sceneImage, imagePoint, targetColor, m_fillColor);

    // Trace the filled region back to a vector path
    QPainterPath filledPath = traceFilledRegion(sceneImage, m_fillColor);

    if (!filledPath.isEmpty()) {
        // Transform the path back to scene coordinates
        QTransform transform;
        transform.translate(fillArea.x(), fillArea.y());
        transform.scale(0.5, 0.5); // Scale back down from 2x
        filledPath = transform.map(filledPath);

        // Create and add the fill item
        QGraphicsPathItem* fillItem = createFillItem(filledPath, m_fillColor);
        if (fillItem) {
            addFillToCanvas(fillItem);
        }
    }
}

QImage BucketFillTool::renderSceneToImage(const QRectF& region, qreal scale)
{
    if (!m_canvas || !m_canvas->scene()) return QImage();

    QSize imageSize(region.width() * scale, region.height() * scale);
    QImage image(imageSize, QImage::Format_ARGB32);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.scale(scale, scale);
    painter.translate(-region.topLeft());

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

void BucketFillTool::floodFillImage(QImage& image, const QPoint& startPoint, const QColor& targetColor, const QColor& fillColor)
{
    if (targetColor == fillColor) return;
    if (!image.rect().contains(startPoint)) return;

    QQueue<QPoint> pointQueue;
    QSet<QPoint> visited;

    pointQueue.enqueue(startPoint);

    while (!pointQueue.isEmpty()) {
        QPoint current = pointQueue.dequeue();

        if (visited.contains(current)) continue;
        if (!image.rect().contains(current)) continue;

        QColor currentColor = getPixelColor(image, current);

        // Check if color matches target within tolerance
        int colorDiff = qAbs(currentColor.red() - targetColor.red()) +
            qAbs(currentColor.green() - targetColor.green()) +
            qAbs(currentColor.blue() - targetColor.blue()) +
            qAbs(currentColor.alpha() - targetColor.alpha());

        if (colorDiff > m_tolerance * 4) continue; // 4 channels

        // Fill this pixel
        image.setPixel(current, fillColor.rgba());
        visited.insert(current);

        // Add neighboring pixels to queue
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
}

QPainterPath BucketFillTool::traceFilledRegion(const QImage& image, const QColor& fillColor)
{
    // This is a simplified contour tracing algorithm
    // For production use, consider implementing Moore neighborhood tracing

    QPainterPath path;

    // Find the first filled pixel
    QPoint startPoint(-1, -1);
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (getPixelColor(image, QPoint(x, y)) == fillColor) {
                startPoint = QPoint(x, y);
                break;
            }
        }
        if (startPoint.x() != -1) break;
    }

    if (startPoint.x() == -1) return path; // No filled pixels found

    // Create a simple rectangular approximation for now
    // In a full implementation, you'd want to trace the actual contour
    QRect boundingRect(startPoint, startPoint);

    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (getPixelColor(image, QPoint(x, y)) == fillColor) {
                boundingRect = boundingRect.united(QRect(x, y, 1, 1));
            }
        }
    }

    path.addRect(boundingRect);
    return path;
}

QGraphicsPathItem* BucketFillTool::createFillItem(const QPainterPath& fillPath, const QColor& color)
{
    if (fillPath.isEmpty()) return nullptr;

    QGraphicsPathItem* fillItem = new QGraphicsPathItem(fillPath);
    fillItem->setBrush(QBrush(color));
    fillItem->setPen(QPen(Qt::NoPen)); // No outline
    fillItem->setFlag(QGraphicsItem::ItemIsSelectable, true);
    fillItem->setFlag(QGraphicsItem::ItemIsMovable, true);
    return fillItem;
}

void BucketFillTool::addFillToCanvas(QGraphicsPathItem* fillItem)
{
    if (!fillItem || !m_canvas) return;

    // Add with undo command
    DrawCommand* command = new DrawCommand(m_canvas, fillItem);
    m_mainWindow->m_undoStack->push(command);
    QTimer::singleShot(0, [fillItem]() {
        fillItem->setZValue(-1000);
        });
}

void BucketFillTool::showFillPreview(const QPainterPath& path)
{
    hideFillPreview();

    if (path.isEmpty() || !m_canvas || !m_canvas->scene()) return;

    m_previewItem = new QGraphicsPathItem(path);
    QColor previewColor = m_fillColor;
    previewColor.setAlpha(128); // Semi-transparent
    m_previewItem->setBrush(QBrush(previewColor));
    m_previewItem->setPen(QPen(m_fillColor, 1, Qt::DashLine));
    m_previewItem->setFlag(QGraphicsItem::ItemIsSelectable, false);
    m_previewItem->setFlag(QGraphicsItem::ItemIsMovable, false);

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

void BucketFillTool::setFillColor(const QColor& color)
{
    m_fillColor = color;
}

void BucketFillTool::setTolerance(int tolerance)
{
    m_tolerance = qBound(0, tolerance, 255);
}

void BucketFillTool::setFillMode(int mode)
{
    m_fillMode = qBound(0, mode, 1);
}