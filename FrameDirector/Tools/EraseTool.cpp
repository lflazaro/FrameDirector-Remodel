#include "EraseTool.h"
#include "../MainWindow.h"
#include "../Canvas.h"
#include "../Commands/UndoCommands.h"
#include <QGraphicsScene>
#include <QGraphicsPathItem>
#include <QUndoStack>
#include <QDebug>
#include <QtMath>

EraseTool::EraseTool(MainWindow* mainWindow, QObject* parent)
    : Tool(mainWindow, parent)
    , m_eraserSize(20.0)
    , m_eraserMode(0)
    , m_erasing(false)
    , m_previewCircle(nullptr)
{
    qDebug() << "EraseTool created";
}

void EraseTool::mousePressEvent(QMouseEvent* event, const QPointF& scenePos)
{
    if (!m_canvas || !m_canvas->scene()) return;

    int currentLayer = m_canvas->getCurrentLayer();
    int currentFrame = m_canvas->getCurrentFrame();

    // Check if drawing is allowed
    if (!canDrawOnCurrentFrame(m_canvas, currentLayer, currentFrame)) {
        return;
    }

    // Auto-convert extended frame to keyframe
    checkAutoConversion(m_canvas, currentLayer, currentFrame);
    if (event->button() == Qt::LeftButton) {
        m_erasing = true;
        m_lastErasePos = scenePos;
        m_currentOperations.clear();
        m_affectedItems.clear();

        hideErasePreview();
        performErase(scenePos);
    }
}

void EraseTool::mouseMoveEvent(QMouseEvent* event, const QPointF& scenePos)
{
    if (m_erasing && (event->buttons() & Qt::LeftButton)) {
        performErase(scenePos);
        m_lastErasePos = scenePos;
    }
    else {
        updateErasePreview(scenePos);
    }
}

void EraseTool::mouseReleaseEvent(QMouseEvent* event, const QPointF& scenePos)
{
    if (event->button() == Qt::LeftButton && m_erasing) {
        m_erasing = false;
        commitEraseOperations();

        if (m_canvas) {
            m_canvas->storeCurrentFrameState();
        }

        showErasePreview(scenePos);
    }
}

void EraseTool::performErase(const QPointF& position)
{
    if (!m_canvas || !m_canvas->scene()) return;

    if (m_eraserMode == 0) {
        vectorErase(position, m_eraserSize / 2.0);
    }
    else {
        objectErase(position);
    }
}

void EraseTool::vectorErase(const QPointF& position, double radius)
{
    // Get items that might intersect with eraser
    QRectF eraseRect(position.x() - radius, position.y() - radius, radius * 2, radius * 2);
    QList<QGraphicsItem*> nearbyItems = m_canvas->scene()->items(eraseRect, Qt::IntersectsItemBoundingRect);

    for (QGraphicsItem* item : nearbyItems) {
        // Skip inappropriate items
        if (!item || item->zValue() <= -999 ||
            !(item->flags() & QGraphicsItem::ItemIsSelectable) ||
            m_affectedItems.contains(item)) {
            continue;
        }

        // Only process path items (drawings/strokes)
        auto pathItem = qgraphicsitem_cast<QGraphicsPathItem*>(item);
        if (!pathItem) continue;

        QPainterPath originalPath = pathItem->path();
        if (originalPath.isEmpty()) continue;

        // Convert to scene coordinates
        QTransform transform = item->sceneTransform();
        QPainterPath scenePath = transform.map(originalPath);

        // Simple and efficient intersection test
        if (!quickIntersectionTest(scenePath, position, radius)) {
            continue;
        }

        // FIXED: Use enhanced path processing that preserves stroke
        QList<QPainterPath> newPaths = eraseFromStrokedPath(scenePath, pathItem->pen(), position, radius);

        // Handle results
        if (newPaths.isEmpty()) {
            // Complete erasure
            recordEraseOperation(item, nullptr, true);
            m_canvas->scene()->removeItem(item);
        }
        else {
            // Partial erasure - replace with new segments
            recordEraseOperation(item, nullptr, true);
            m_canvas->scene()->removeItem(item);

            QTransform invTransform = transform.inverted();

            for (const QPainterPath& newPath : newPaths) {
                QPainterPath localPath = invTransform.map(newPath);

                if (!localPath.isEmpty()) {
                    QGraphicsPathItem* newItem = new QGraphicsPathItem(localPath);
                    copyItemProperties(pathItem, newItem);
                    m_canvas->addItemToCurrentLayer(newItem);
                    recordEraseOperation(nullptr, newItem, false);
                }
            }
        }

        m_affectedItems.append(item);
    }
}

bool EraseTool::quickIntersectionTest(const QPainterPath& path, const QPointF& center, double radius)
{
    // Quick bounding box test first
    QRectF pathBounds = path.boundingRect();
    QRectF eraseBounds(center.x() - radius, center.y() - radius, radius * 2, radius * 2);

    if (!pathBounds.intersects(eraseBounds)) {
        return false;
    }

    // Test key points on the path
    QList<qreal> testPoints = { 0.0, 0.25, 0.5, 0.75, 1.0 };
    for (qreal t : testPoints) {
        QPointF point = path.pointAtPercent(t);
        if (QLineF(center, point).length() <= radius) {
            return true;
        }
    }

    return false;
}

// FIXED: New method that preserves stroke width
QList<QPainterPath> EraseTool::eraseFromStrokedPath(const QPainterPath& path, const QPen& pen, const QPointF& center, double radius)
{
    QList<QPainterPath> resultPaths;

    // For stroked paths, we need to work with the actual stroke outline
    QPainterPathStroker stroker;
    stroker.setWidth(pen.widthF());
    stroker.setCapStyle(pen.capStyle());
    stroker.setJoinStyle(pen.joinStyle());

    QPainterPath strokedPath = stroker.createStroke(path);

    // Check if the eraser circle intersects the stroked path
    QPainterPath eraserPath;
    eraserPath.addEllipse(center, radius, radius);

    if (!strokedPath.intersects(eraserPath)) {
        // No intersection, return original path
        resultPaths.append(path);
        return resultPaths;
    }

    // If there's intersection, we need to split the path
    // This is a simplified approach that works well for most cases
    QList<QLineF> segments = pathToLineSegments(path);
    if (segments.isEmpty()) return resultPaths;

    // Process each segment
    QList<QLineF> survivingSegments;

    for (const QLineF& segment : segments) {
        QList<QLineF> segmentParts = eraseFromLineSegment(segment, center, radius);
        survivingSegments.append(segmentParts);
    }

    // Convert surviving segments back to paths
    if (!survivingSegments.isEmpty()) {
        resultPaths = lineSegmentsToPaths(survivingSegments);
    }

    return resultPaths;
}

QList<QPainterPath> EraseTool::eraseFromPath(const QPainterPath& path, const QPointF& center, double radius)
{
    QList<QPainterPath> resultPaths;

    // Convert path to simple line segments
    QList<QLineF> segments = pathToLineSegments(path);
    if (segments.isEmpty()) return resultPaths;

    // Process each segment
    QList<QLineF> survivingSegments;

    for (const QLineF& segment : segments) {
        QList<QLineF> segmentParts = eraseFromLineSegment(segment, center, radius);
        survivingSegments.append(segmentParts);
    }

    // Convert surviving segments back to paths
    if (!survivingSegments.isEmpty()) {
        resultPaths = lineSegmentsToPaths(survivingSegments);
    }

    return resultPaths;
}

QList<QLineF> EraseTool::pathToLineSegments(const QPainterPath& path)
{
    QList<QLineF> segments;

    QPointF currentPoint;
    bool hasCurrentPoint = false;

    for (int i = 0; i < path.elementCount(); ++i) {
        QPainterPath::Element element = path.elementAt(i);
        QPointF elementPoint(element.x, element.y);

        switch (element.type) {
        case QPainterPath::MoveToElement:
            currentPoint = elementPoint;
            hasCurrentPoint = true;
            break;

        case QPainterPath::LineToElement:
            if (hasCurrentPoint) {
                segments.append(QLineF(currentPoint, elementPoint));
                currentPoint = elementPoint;
            }
            break;

        case QPainterPath::CurveToElement:
            // Simplify curves to line segments
            if (hasCurrentPoint && i + 2 < path.elementCount()) {
                QPainterPath::Element cp1 = path.elementAt(i + 1);
                QPainterPath::Element cp2 = path.elementAt(i + 2);

                // Sample curve with a few points
                for (int j = 1; j <= 4; ++j) {
                    qreal t = static_cast<qreal>(j) / 4.0;
                    QPointF curvePoint = bezierPoint(currentPoint,
                        QPointF(element.x, element.y),
                        QPointF(cp1.x, cp1.y),
                        QPointF(cp2.x, cp2.y), t);
                    segments.append(QLineF(currentPoint, curvePoint));
                    currentPoint = curvePoint;
                }
                i += 2; // Skip control points
            }
            break;
        }
    }

    return segments;
}

QPointF EraseTool::bezierPoint(const QPointF& p0, const QPointF& p1, const QPointF& p2, const QPointF& p3, qreal t)
{
    qreal u = 1.0 - t;
    qreal tt = t * t;
    qreal uu = u * u;
    qreal uuu = uu * u;
    qreal ttt = tt * t;

    QPointF p = uuu * p0;
    p += 3 * uu * t * p1;
    p += 3 * u * tt * p2;
    p += ttt * p3;

    return p;
}

QList<QLineF> EraseTool::eraseFromLineSegment(const QLineF& segment, const QPointF& center, double radius)
{
    QList<QLineF> result;

    QPointF start = segment.p1();
    QPointF end = segment.p2();

    bool startInside = QLineF(center, start).length() <= radius;
    bool endInside = QLineF(center, end).length() <= radius;

    if (!startInside && !endInside) {
        // Check if segment passes through circle
        QPointF closest = closestPointOnSegment(segment, center);
        if (QLineF(center, closest).length() <= radius) {
            // Segment intersects circle - find intersection points
            QList<QPointF> intersections = lineCircleIntersection(segment, center, radius);

            if (intersections.size() >= 2) {
                // Split into two segments
                QLineF firstPart(start, intersections[0]);
                QLineF secondPart(intersections[1], end);

                if (firstPart.length() > 1.0) result.append(firstPart);
                if (secondPart.length() > 1.0) result.append(secondPart);
            }
            else {
                // Keep whole segment
                result.append(segment);
            }
        }
        else {
            // No intersection
            result.append(segment);
        }
    }
    else if (!startInside && endInside) {
        // Start outside, end inside
        QList<QPointF> intersections = lineCircleIntersection(segment, center, radius);
        if (!intersections.isEmpty()) {
            QLineF survivingPart(start, intersections[0]);
            if (survivingPart.length() > 1.0) {
                result.append(survivingPart);
            }
        }
    }
    else if (startInside && !endInside) {
        // Start inside, end outside
        QList<QPointF> intersections = lineCircleIntersection(segment, center, radius);
        if (!intersections.isEmpty()) {
            QLineF survivingPart(intersections.last(), end);
            if (survivingPart.length() > 1.0) {
                result.append(survivingPart);
            }
        }
    }
    // If both inside, segment is completely erased (return empty list)

    return result;
}

QPointF EraseTool::closestPointOnSegment(const QLineF& segment, const QPointF& point)
{
    QPointF start = segment.p1();
    QPointF end = segment.p2();
    QPointF direction = end - start;

    if (direction.isNull()) return start;

    qreal length = QLineF(start, end).length();
    QPointF unitDirection = direction / length;

    QPointF toPoint = point - start;
    qreal projection = QPointF::dotProduct(toPoint, unitDirection);
    projection = qMax(0.0, qMin(length, projection));

    return start + projection * unitDirection;
}

QList<QPointF> EraseTool::lineCircleIntersection(const QLineF& line, const QPointF& center, double radius)
{
    QList<QPointF> intersections;

    QPointF start = line.p1();
    QPointF end = line.p2();
    QPointF direction = end - start;

    if (direction.isNull()) return intersections;

    QPointF toCenter = center - start;

    qreal a = QPointF::dotProduct(direction, direction);
    qreal b = 2.0 * QPointF::dotProduct(direction, -toCenter);
    qreal c = QPointF::dotProduct(toCenter, toCenter) - radius * radius;

    qreal discriminant = b * b - 4 * a * c;

    if (discriminant >= 0) {
        qreal sqrt_d = qSqrt(discriminant);
        qreal t1 = (-b - sqrt_d) / (2 * a);
        qreal t2 = (-b + sqrt_d) / (2 * a);

        if (t1 >= 0 && t1 <= 1) {
            intersections.append(start + t1 * direction);
        }
        if (t2 >= 0 && t2 <= 1 && qAbs(t2 - t1) > 0.01) {
            intersections.append(start + t2 * direction);
        }
    }

    return intersections;
}

QList<QPainterPath> EraseTool::lineSegmentsToPaths(const QList<QLineF>& segments)
{
    QList<QPainterPath> paths;

    if (segments.isEmpty()) return paths;

    // Group connected segments into paths
    QList<QLineF> remaining = segments;

    while (!remaining.isEmpty()) {
        QPainterPath path;
        QLineF current = remaining.takeFirst();

        path.moveTo(current.p1());
        path.lineTo(current.p2());

        QPointF lastPoint = current.p2();

        // Try to connect more segments
        bool foundConnection = true;
        while (foundConnection && !remaining.isEmpty()) {
            foundConnection = false;

            for (int i = 0; i < remaining.size(); ++i) {
                QLineF candidate = remaining[i];

                if (QLineF(lastPoint, candidate.p1()).length() < 2.0) {
                    // Connect forward
                    path.lineTo(candidate.p2());
                    lastPoint = candidate.p2();
                    remaining.removeAt(i);
                    foundConnection = true;
                    break;
                }
                else if (QLineF(lastPoint, candidate.p2()).length() < 2.0) {
                    // Connect backward
                    path.lineTo(candidate.p1());
                    lastPoint = candidate.p1();
                    remaining.removeAt(i);
                    foundConnection = true;
                    break;
                }
            }
        }

        if (!path.isEmpty()) {
            paths.append(path);
        }
    }

    return paths;
}

void EraseTool::objectErase(const QPointF& position)
{
    QGraphicsItem* itemToDelete = m_canvas->scene()->itemAt(position, m_canvas->transform());

    if (itemToDelete && itemToDelete->zValue() > -999 &&
        (itemToDelete->flags() & QGraphicsItem::ItemIsSelectable)) {

        recordEraseOperation(itemToDelete, nullptr, true);
        m_canvas->scene()->removeItem(itemToDelete);
        m_affectedItems.append(itemToDelete);
    }
}

void EraseTool::copyItemProperties(QGraphicsItem* source, QGraphicsItem* target)
{
    if (!source || !target) return;

    target->setPos(source->pos());
    target->setRotation(source->rotation());
    target->setScale(source->scale());
    target->setTransform(source->transform());
    target->setOpacity(source->opacity());
    target->setVisible(source->isVisible());
    target->setZValue(source->zValue());
    target->setFlags(source->flags());
    target->setSelected(false);
    target->setData(0, source->data(0));

    // FIXED: Properly copy pen and brush for path items
    auto sourcePathItem = qgraphicsitem_cast<QGraphicsPathItem*>(source);
    auto targetPathItem = qgraphicsitem_cast<QGraphicsPathItem*>(target);

    if (sourcePathItem && targetPathItem) {
        targetPathItem->setPen(sourcePathItem->pen());
        targetPathItem->setBrush(sourcePathItem->brush());
    }
}

void EraseTool::recordEraseOperation(QGraphicsItem* original, QGraphicsItem* newItem, bool deleted)
{
    EraseOperation op;
    op.originalItem = original;
    op.newItem = newItem;
    op.itemDeleted = deleted;
    m_currentOperations.append(op);
}

void EraseTool::commitEraseOperations()
{
    if (m_currentOperations.isEmpty()) return;

    if (m_mainWindow && m_mainWindow->m_undoStack) {
        m_mainWindow->m_undoStack->beginMacro("Erase");

        for (const EraseOperation& op : m_currentOperations) {
            if (op.itemDeleted && op.originalItem) {
                RemoveItemCommand* removeCmd = new RemoveItemCommand(m_canvas, { op.originalItem });
                m_mainWindow->m_undoStack->push(removeCmd);
            }
            if (op.newItem) {
                AddItemCommand* addCmd = new AddItemCommand(m_canvas, op.newItem);
                m_mainWindow->m_undoStack->push(addCmd);
            }
        }

        m_mainWindow->m_undoStack->endMacro();
    }

    m_currentOperations.clear();
}

void EraseTool::updateErasePreview(const QPointF& position)
{
    if (m_erasing) {
        hideErasePreview();
        return;
    }
    showErasePreview(position);
}

void EraseTool::showErasePreview(const QPointF& position)
{
    if (!m_canvas || !m_canvas->scene()) return;

    hideErasePreview();

    m_previewCircle = new QGraphicsEllipseItem();
    double radius = m_eraserSize / 2.0;
    m_previewCircle->setRect(position.x() - radius, position.y() - radius,
        m_eraserSize, m_eraserSize);

    QPen previewPen(QColor(255, 120, 120, 150), 1);
    m_previewCircle->setPen(previewPen);
    m_previewCircle->setBrush(QBrush(QColor(255, 120, 120, 30)));
    m_previewCircle->setFlag(QGraphicsItem::ItemIsSelectable, false);
    m_previewCircle->setFlag(QGraphicsItem::ItemIsMovable, false);
    m_previewCircle->setZValue(10000);

    m_canvas->scene()->addItem(m_previewCircle);
}

void EraseTool::hideErasePreview()
{
    if (m_previewCircle && m_canvas && m_canvas->scene()) {
        m_canvas->scene()->removeItem(m_previewCircle);
        delete m_previewCircle;
        m_previewCircle = nullptr;
    }
}

// FIXED: Add cleanup method
void EraseTool::cleanup()
{
    hideErasePreview();
    m_erasing = false;
    m_currentOperations.clear();
    m_affectedItems.clear();
}

QCursor EraseTool::getCursor() const
{
    return Qt::CrossCursor;
}

void EraseTool::setEraserSize(double size)
{
    m_eraserSize = qMax(5.0, qMin(100.0, size));
}

void EraseTool::setEraserMode(int mode)
{
    m_eraserMode = qBound(0, mode, 1);
}

double EraseTool::getEraserSize() const
{
    return m_eraserSize;
}

int EraseTool::getEraserMode() const
{
    return m_eraserMode;
}