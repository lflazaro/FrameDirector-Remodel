#include "BucketFillTool.h"
#include "MainWindow.h"
#include "Canvas.h"
#include "Commands/UndoCommands.h"
#include "third_party/clipper.h"
#include <QGraphicsScene>
#include <QGraphicsPathItem>
#include <QGraphicsRectItem>
#include <QGraphicsEllipseItem>
#include <QGraphicsLineItem>
#include <QBrush>
#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QPen>
#include <QTransform>
#include <QApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <QtMath>
#include <QQueue>
#include <QSet>
#include <QTimer>
#include <QDateTime>
#include <functional>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>
// BucketFillTool.cpp
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

namespace {
    constexpr double kClipperScale = 1024.0;
    constexpr double kClipperArcTolerance = kClipperScale * 0.25;
    constexpr int   kMaxTileItems = 220;
    constexpr int   kMaxPaths64 = 600;     // after clipping
    constexpr int   kMaxVerts64 = 120000;  // total vertices budget per compose
    constexpr qreal kMinTileR = 120.0;   // pixels
    constexpr qreal kReuseFrac = 0.15;    // cache reuse band

    using Clipper2Lib::ClipType;
    using Clipper2Lib::Clipper64;
    using Clipper2Lib::ClipperOffset;
    using Clipper2Lib::EndType;
    using Clipper2Lib::FillRule;
    using Clipper2Lib::JoinType;
    using Clipper2Lib::Path64;
    using Clipper2Lib::Paths64;
    using Clipper2Lib::Point64;
    using Clipper2Lib::PointInPolygonResult;
    using Clipper2Lib::PolyPath64;
    using Clipper2Lib::PolyTree64;

    Path64 qPolygonToPath64(const QPolygonF& polygon, double scale)
    {
        Path64 result;
        result.reserve(polygon.size());
        for (const QPointF& pt : polygon) {
            const auto x = static_cast<int64_t>(std::llround(pt.x() * scale));
            const auto y = static_cast<int64_t>(std::llround(pt.y() * scale));
            if (!result.empty() && result.back().x == x && result.back().y == y) {
                continue;
            }
            result.emplace_back(x, y);
        }

        if (result.size() > 1 && result.front() == result.back()) {
            result.pop_back();
        }

        if (result.size() < 3) {
            result.clear();
        }

        return result;
    }

    Paths64 painterPathToPaths64(const QPainterPath& path, double scale)
    {
        Paths64 paths;
        if (path.isEmpty()) {
            return paths;
        }

        QPainterPath copy = path;
        copy.setFillRule(Qt::WindingFill);
        const QList<QPolygonF> subpaths = copy.toSubpathPolygons(QTransform());
        for (const QPolygonF& polygon : subpaths) {
            Path64 clipperPath = qPolygonToPath64(polygon, scale);
            if (clipperPath.empty()) {
                continue;
            }

            paths.emplace_back(std::move(clipperPath));
        }

        return paths;
    }

    QPainterPath path64ToPainterPath(const Path64& path, double inverseScale)
    {
        QPainterPath painterPath;
        if (path.empty()) {
            return painterPath;
        }

        painterPath.setFillRule(Qt::WindingFill);
        painterPath.moveTo(path.front().x * inverseScale, path.front().y * inverseScale);
        for (size_t i = 1; i < path.size(); ++i) {
            painterPath.lineTo(path[i].x * inverseScale, path[i].y * inverseScale);
        }
        painterPath.closeSubpath();
        return painterPath;
    }

    void appendPolyPath(const PolyPath64* node, double inverseScale, bool invertOrientation, QPainterPath& out)
    {
        if (!node) {
            return;
        }

        Path64 polygon = node->Polygon();
        if (!polygon.empty()) {
            if (invertOrientation) {
                std::reverse(polygon.begin(), polygon.end());
            }
            out.addPath(path64ToPainterPath(polygon, inverseScale));
        }

        for (auto it = node->begin(); it != node->end(); ++it) {
            const PolyPath64* child = it->get();
            if (!child) {
                continue;
            }
            appendPolyPath(child, inverseScale, invertOrientation, out);
        }
    }

    QPainterPath polyPathToPainterPath(const PolyPath64* node, double inverseScale, bool invertOrientation = false)
    {
        QPainterPath result;
        result.setFillRule(Qt::WindingFill);
        if (!node) {
            return result;
        }

        appendPolyPath(node, inverseScale, invertOrientation, result);
        return result;
    }

    void appendPolygons(const PolyPath64* node, Paths64& out)
    {
        if (!node) {
            return;
        }

        const Path64& polygon = node->Polygon();
        if (!polygon.empty()) {
            out.push_back(polygon);
        }

        for (auto it = node->begin(); it != node->end(); ++it) {
            const PolyPath64* child = it->get();
            if (!child) {
                continue;
            }
            appendPolygons(child, out);
        }
    }

    Paths64 polyTreeToPaths(const PolyTree64& tree)
    {
        Paths64 result;
        for (auto it = tree.begin(); it != tree.end(); ++it) {
            const PolyPath64* child = it->get();
            if (!child) {
                continue;
            }
            appendPolygons(child, result);
        }
        return result;
    }

    void collectHolePaths(const PolyPath64* node, double inverseScale, QList<QPainterPath>& holes,
        bool skipCurrent = false, bool invertOrientation = false)
    {
        if (!node) {
            return;
        }

        const bool nodeIsHole = node->IsHole();
        const bool treatAsHole = invertOrientation ? !nodeIsHole : nodeIsHole;

        if (!skipCurrent && treatAsHole) {
            Path64 polygon = node->Polygon();
            if (!polygon.empty()) {
                if (invertOrientation) {
                    std::reverse(polygon.begin(), polygon.end());
                }
                holes.append(path64ToPainterPath(polygon, inverseScale));
            }
        }

        for (auto it = node->begin(); it != node->end(); ++it) {
            const PolyPath64* child = it->get();
            if (!child) {
                continue;
            }
            collectHolePaths(child, inverseScale, holes, false, invertOrientation);
        }
    }

    QPainterPath buildFacePath(const PolyPath64* node, double inverseScale)
    {
        QPainterPath result;
        if (!node) {
            return result;
        }

        result = path64ToPainterPath(node->Polygon(), inverseScale);
        result.setFillRule(Qt::WindingFill);
        if (result.isEmpty()) {
            return result;
        }

        for (auto it = node->begin(); it != node->end(); ++it) {
            const PolyPath64* child = it->get();
            if (!child) {
                continue;
            }

            QPainterPath childPath = buildFacePath(child, inverseScale);
            if (childPath.isEmpty()) {
                continue;
            }

            if (child->IsHole()) {
                result = result.subtracted(childPath);
            }
            else {
                result = result.united(childPath);
            }
        }

        result.setFillRule(Qt::WindingFill);
        return result;
    }

    // Find the PolyPath64 leaf (or deepest node) whose polygon contains the point.
    static const Clipper2Lib::PolyPath64*
        findContainingNode(const Clipper2Lib::PolyTree64* tree, const Clipper2Lib::Point64& pt)
    {
        for (auto it = tree->begin(); it != tree->end(); ++it) {
            const auto* node = it->get();
            // depth-first search
            std::function<const Clipper2Lib::PolyPath64* (const Clipper2Lib::PolyPath64*)> dfs =
                [&](const Clipper2Lib::PolyPath64* n) -> const Clipper2Lib::PolyPath64* {
                using Clipper2Lib::PointInPolygon;
                using Clipper2Lib::PointInPolygonResult;
                if (PointInPolygon(pt, n->Polygon()) == PointInPolygonResult::IsOutside) return nullptr;
                for (auto ci = n->begin(); ci != n->end(); ++ci) {
                    if (auto* child = dfs(ci->get())) return child;
                }
                return n; // deepest containing node
                };
            if (const auto* hit = dfs(node)) return hit;
        }
        return nullptr;
    }

    const PolyPath64* findNodeContainingPoint(const PolyTree64& tree, const QPointF& point, double scale)
    {
        const Point64 query(
            static_cast<int64_t>(std::llround(point.x() * scale)),
            static_cast<int64_t>(std::llround(point.y() * scale)));

        for (auto it = tree.begin(); it != tree.end(); ++it) {
            const PolyPath64* child = it->get();
            if (!child) {
                continue;
            }
            if (const PolyPath64* candidate = findContainingNode(child, query)) {
                return candidate;
            }
        }

        return nullptr;
    }

}

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

BucketFillTool::ClosedRegion
BucketFillTool::composeRegionFromSegments_Tiled(const QList<PathSegment>& segments,
    const QPointF& seedPoint,
    qreal gapPadding)
{
    ClosedRegion out; out.isValid = false;
    if (!m_canvas) return out;

    // Tile around seed (clamped to canvas)
    const qreal R = qMax<qreal>(m_searchRadius, kMinTileR);
    QRectF tile(seedPoint.x() - R, seedPoint.y() - R, 2 * R, 2 * R);
    tile = tile.intersected(m_canvas->getCanvasRect());

    // Cache reuse
    if (m_compCacheValid) {
        const qreal dx = seedPoint.x() - m_compCacheCenter.x();
        const qreal dy = seedPoint.y() - m_compCacheCenter.y();
        if (m_compCacheRadius == R && (dx * dx + dy * dy) <= (R * kReuseFrac) * (R * kReuseFrac)) {
            if (!m_compCachedUnion.isEmpty() && m_compCachedUnion.contains(seedPoint)) {
                out.outerBoundary = m_compCachedUnion; out.bounds = out.outerBoundary.boundingRect(); out.isValid = true; return out;
            }
        }
    }

    // (Optional) tiny debounce for mouseMove previews
    if (QApplication::mouseButtons() == Qt::NoButton) {
        qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (now - m_lastComposeMs < 12) { return out; }  // skip this frame; keeps UI fluid
        m_lastComposeMs = now;
    }

    // === Clip each segment to the tile BEFORE converting to Paths64 ===
    QPainterPath tilePath; tilePath.addRect(tile);
    QList<QPainterPath> clippedSegments; clippedSegments.reserve(segments.size());

    // Keep nearest items first and cap
    QList<PathSegment> filtered = segments;
    std::sort(filtered.begin(), filtered.end(), [&](const PathSegment& a, const PathSegment& b) {
        return QLineF(seedPoint, a.bounds.center()).length() < QLineF(seedPoint, b.bounds.center()).length();
        });
    if (filtered.size() > kMaxTileItems) filtered = filtered.mid(0, kMaxTileItems);

    for (const PathSegment& seg : filtered) {
        const QPainterPath& g = seg.geometry.isEmpty() ? seg.path : seg.geometry;
        if (g.isEmpty()) continue;
        if (!g.boundingRect().intersects(tile)) continue;
        QPainterPath clipped = g.intersected(tilePath); // cheap box clip in Qt
        if (!clipped.isEmpty()) {
            clipped.setFillRule(Qt::WindingFill);
            clippedSegments.push_back(clipped);
        }
    }
    if (clippedSegments.isEmpty()) return out;

    // Convert clipped geometry -> Paths64 with vertex budget
    Paths64 subjectPaths; subjectPaths.reserve(qMin(kMaxPaths64, clippedSegments.size()));
    int vertBudget = kMaxVerts64;

    for (const QPainterPath& cp : clippedSegments) {
        const QList<QPolygonF> polys = cp.toFillPolygons(); // flattened polygons in tile coords
        for (const QPolygonF& poly : polys) {
            auto p64 = qPolygonToPath64(poly, kClipperScale);
            if (p64.empty()) continue;
            if (p64.size() > vertBudget) { vertBudget = -1; break; }
            vertBudget -= int(p64.size());
            subjectPaths.push_back(std::move(p64));
            if (int(subjectPaths.size()) >= kMaxPaths64 || vertBudget <= 0) break;
        }
        if (vertBudget <= 0 || int(subjectPaths.size()) >= kMaxPaths64) break;
    }

    if (subjectPaths.empty() || vertBudget <= 0) {
        // Too complex  bail (let legacy/raster handle)
        return out;
    }

    // (A) optional dilation to close tiny gaps
    Paths64 dilated = subjectPaths;
    if (gapPadding > 0.0) {
        Clipper2Lib::ClipperOffset off;
        off.AddPaths(subjectPaths, JoinType::Round, EndType::Polygon);
        Paths64 padded; off.Execute(gapPadding * kClipperScale, padded);
        if (!padded.empty()) dilated = std::move(padded);
    }

    // (B) single UNION
    Clipper2Lib::PolyTree64 uniTree;
    {
        Clipper64 c;
        c.AddSubject(dilated);
        c.Execute(ClipType::Union, FillRule::NonZero, uniTree);
    }
    if (uniTree.Count() == 0) return out;

    // (C) optional shrink back
    Paths64 finalPaths = polyTreeToPaths(uniTree);
    if (gapPadding > 0.0 && !finalPaths.empty()) {
        Clipper2Lib::ClipperOffset shrink;
        shrink.AddPaths(finalPaths, JoinType::Round, EndType::Polygon);
        Paths64 shrunk; shrink.Execute(-gapPadding * kClipperScale, shrunk);
        if (!shrunk.empty()) finalPaths = std::move(shrunk);
    }

    // 1) Build the tile rectangle in Clipper space
    Path64 tileRect64;
    tileRect64.reserve(4);
    tileRect64.emplace_back((int64_t)std::llround(tile.left() * kClipperScale),
        (int64_t)std::llround(tile.top() * kClipperScale));
    tileRect64.emplace_back((int64_t)std::llround(tile.right() * kClipperScale),
        (int64_t)std::llround(tile.top() * kClipperScale));
    tileRect64.emplace_back((int64_t)std::llround(tile.right() * kClipperScale),
        (int64_t)std::llround(tile.bottom() * kClipperScale));
    tileRect64.emplace_back((int64_t)std::llround(tile.left() * kClipperScale),
        (int64_t)std::llround(tile.bottom() * kClipperScale));

    // 2) FREE = tileRect - finalPaths (finalPaths is the union of barriers)
    PolyTree64 freeTree;
    {
        Clipper64 c;
        c.AddSubject(Paths64{ tileRect64 });
        c.AddClip(finalPaths);
        c.Execute(ClipType::Difference, FillRule::NonZero, freeTree);
    }
    if (freeTree.Count() == 0) return out;  // no free faces in tile
    using namespace Clipper2Lib;
    const double inv = 1.0 / kClipperScale;

    const PolyPath64* faceNode = findNodeContainingPoint(freeTree, seedPoint, kClipperScale);
    if (!faceNode) return out;

    // Clipper marks alternating nodes as holes. If we accidentally landed inside a
    // hole, walk up to the nearest non-hole ancestor so we always return a face.
    const PolyPath64* current = faceNode;
    while (current && current->IsHole()) {
        current = current->Parent();
    }
    if (!current) return out;

    QPainterPath facePath = buildFacePath(current, inv);
    if (facePath.isEmpty()) return out;

    QList<QPainterPath> holePaths;
    collectHolePaths(current, inv, holePaths, true);

    if (!facePath.contains(seedPoint)) return out;

    m_compCachedUnion = facePath;
    m_compCacheCenter = seedPoint;
    m_compCacheRadius = R;
    m_compCacheValid = true;

    out.outerBoundary = facePath;
    out.innerHoles = holePaths;
    out.bounds = facePath.boundingRect();
    out.isValid = true;
    return out;
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
            if (m_fillMode == 0) {
                // Vector-based filling (preferred method)
                ClosedRegion region = findEnclosedRegion(scenePos);

                if (region.isValid && !region.outerBoundary.isEmpty()) {
                    // Validate region size to prevent filling entire canvas
                    QRectF regionBounds = region.outerBoundary.boundingRect();
                    QRectF canvasBounds = m_canvas->getCanvasRect();

                    // Don't fill if region covers more than 80% of canvas
                    double regionArea = regionBounds.width() * regionBounds.height();
                    double canvasArea = canvasBounds.width() * canvasBounds.height();

                    if (regionArea > canvasArea * 0.8) {
                        qDebug() << "BucketFill: Region too large, probably canvas background. Skipping.";
                        return;
                    }

                    QGraphicsPathItem* fillItem = createFillItem(region.outerBoundary, m_fillColor);
                    if (fillItem) {
                        addFillToCanvas(fillItem);
                        qDebug() << "BucketFill: Vector fill completed in" << timer.elapsed() << "ms";
                    }
                    else {
                        qDebug() << "BucketFill: Failed to create vector fill item";
                    }
                }
                else {
                    qDebug() << "BucketFill: No enclosed region found, using raster fill";
                    performRasterFill(scenePos);
                    qDebug() << "BucketFill: Fallback raster fill completed in" << timer.elapsed() << "ms";
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
    ClosedRegion region; region.isValid = false;
    if (!m_canvas) return region;

    const QRectF canvas = m_canvas->getCanvasRect();
    if (!canvas.contains(point)) return region;

    // 0) Fast path: if the point is already inside a visible filled geometry nearby, return it.
    QList<PathSegment> nearby = collectNearbyPaths(point, m_searchRadius);
    for (const PathSegment& seg : nearby) {
        if (seg.hasFill && !seg.geometry.isEmpty() && seg.geometry.contains(point)) {
            region.outerBoundary = seg.geometry; region.outerBoundary.setFillRule(Qt::WindingFill);
            region.bounds = region.outerBoundary.boundingRect(); region.isValid = true;
            return region;
        }
    }

    // 1) Use Clipper-based tiled composition (offset->union->shrink) with caps & cache
    ClosedRegion composed = composeRegionFromSegments_Tiled(nearby, point, m_connectionTolerance);
    if (composed.isValid) return composed;

    // 2) Legacy fallbacks (cheap)  keep them for resilience
    QPainterPath closedPath = createClosedPath(nearby, point);
    if (!closedPath.isEmpty() && closedPath.contains(point)) {
        region.outerBoundary = closedPath; region.bounds = closedPath.boundingRect(); region.isValid = true; return region;
    }

    QPainterPath unionPath;
    for (const PathSegment& seg : nearby) {
        const QPainterPath& c = seg.geometry.isEmpty() ? seg.path : seg.geometry;
        unionPath = unionPath.united(c);
    }
    if (!unionPath.isEmpty() && unionPath.contains(point)) {
        region.outerBoundary = unionPath; region.bounds = unionPath.boundingRect(); region.isValid = true; return region;
    }

    return region; // invalid -> caller may fall back to raster on click
}

QList<BucketFillTool::PathSegment>
BucketFillTool::collectNearbyPaths(const QPointF& center, qreal searchRadius)
{
    QList<PathSegment> segments;
    if (!m_canvas || !m_canvas->scene()) return segments;

    QRectF canvasRect = m_canvas->getCanvasRect();
    QRectF searchRect(center.x() - searchRadius, center.y() - searchRadius, searchRadius * 2, searchRadius * 2);
    searchRect = searchRect.intersected(canvasRect);

    QList<QGraphicsItem*> items = m_canvas->scene()->items(searchRect, Qt::IntersectsItemBoundingRect);

    // Nearest-first; hard cap
    std::sort(items.begin(), items.end(), [&](QGraphicsItem* a, QGraphicsItem* b) {
        return QLineF(center, a->sceneBoundingRect().center()).length()
            < QLineF(center, b->sceneBoundingRect().center()).length();
        });
    if (items.size() > 600) items = items.mid(0, 600);

    for (QGraphicsItem* item : items) {
        if (!item || item->zValue() <= -999) continue;

        // NEW: never treat the preview overlay as geometry
        if (item == m_previewItem) continue;

        PathSegment seg; seg.item = item;

        QPainterPath localPath, geometry; QRectF localBounds;

        if (auto p = qgraphicsitem_cast<QGraphicsPathItem*>(item)) {
            localPath = p->path(); localBounds = p->boundingRect();
            const QPen pen = p->pen();
            seg.strokeWidth = qMax(0.1, pen.widthF());
            seg.hasStroke = (pen.style() != Qt::NoPen) && seg.strokeWidth > 0.0;
            seg.hasFill = (p->brush().style() != Qt::NoBrush);
            seg.isClosed = isPathClosed(localPath);
            if (seg.hasFill) geometry.addPath(localPath);
            if (seg.hasStroke) { QPainterPathStroker s; s.setWidth(seg.strokeWidth); s.setCapStyle(pen.capStyle()); s.setJoinStyle(pen.joinStyle()); s.setMiterLimit(pen.miterLimit()); geometry.addPath(s.createStroke(localPath)); }
        }
        else if (auto r = qgraphicsitem_cast<QGraphicsRectItem*>(item)) {
            localPath.addRect(r->rect()); localBounds = r->boundingRect();
            const QPen pen = r->pen();
            seg.strokeWidth = qMax(0.1, pen.widthF());
            seg.hasStroke = (pen.style() != Qt::NoPen) && seg.strokeWidth > 0.0;
            seg.hasFill = (r->brush().style() != Qt::NoBrush);
            seg.isClosed = true;
            if (seg.hasFill) geometry.addPath(localPath);
            if (seg.hasStroke) { QPainterPathStroker s; s.setWidth(seg.strokeWidth); s.setCapStyle(pen.capStyle()); s.setJoinStyle(pen.joinStyle()); s.setMiterLimit(pen.miterLimit()); geometry.addPath(s.createStroke(localPath)); }
        }
        else if (auto e = qgraphicsitem_cast<QGraphicsEllipseItem*>(item)) {
            localPath.addEllipse(e->rect()); localBounds = e->boundingRect();
            const QPen pen = e->pen();
            seg.strokeWidth = qMax(0.1, pen.widthF());
            seg.hasStroke = (pen.style() != Qt::NoPen) && seg.strokeWidth > 0.0;
            seg.hasFill = (e->brush().style() != Qt::NoBrush);
            seg.isClosed = true;
            if (seg.hasFill) geometry.addPath(localPath);
            if (seg.hasStroke) { QPainterPathStroker s; s.setWidth(seg.strokeWidth); s.setCapStyle(pen.capStyle()); s.setJoinStyle(pen.joinStyle()); s.setMiterLimit(pen.miterLimit()); geometry.addPath(s.createStroke(localPath)); }
        }
        else if (auto l = qgraphicsitem_cast<QGraphicsLineItem*>(item)) {
            const QPen pen = l->pen();
            QLineF ln = l->line(); localBounds = l->boundingRect();
            localPath.moveTo(ln.p1()); localPath.lineTo(ln.p2());
            seg.strokeWidth = qMax(0.1, pen.widthF());
            seg.hasStroke = (pen.style() != Qt::NoPen) && seg.strokeWidth > 0.0;
            seg.hasFill = false; seg.isClosed = false;
            if (seg.hasStroke) { QPainterPathStroker s; s.setWidth(seg.strokeWidth); s.setCapStyle(pen.capStyle()); s.setJoinStyle(pen.joinStyle()); s.setMiterLimit(pen.miterLimit()); geometry.addPath(s.createStroke(localPath)); }
        }

        if (localPath.isEmpty() && geometry.isEmpty()) continue;
        if (geometry.isEmpty()) geometry = localPath;

        QTransform xf = item->sceneTransform();
        if (!localPath.isEmpty()) seg.path = xf.map(localPath);
        if (!geometry.isEmpty()) { geometry.setFillRule(Qt::WindingFill); seg.geometry = xf.map(geometry); seg.geometry.setFillRule(Qt::WindingFill); }
        seg.bounds = xf.mapRect(localBounds);

        if (!seg.geometry.isEmpty()) segments.append(seg);
    }

    return segments;
}

BucketFillTool::ClosedRegion BucketFillTool::composeRegionFromSegments(
    const QList<PathSegment>& segments, const QPointF& seedPoint, qreal gapPadding)
{
    ClosedRegion region;
    region.isValid = false;

    Paths64 subjectPaths;
    for (const PathSegment& segment : segments) {
        if (segment.geometry.isEmpty()) {
            continue;
        }
        Paths64 geomPaths = painterPathToPaths64(segment.geometry, kClipperScale);
        subjectPaths.insert(subjectPaths.end(), geomPaths.begin(), geomPaths.end());
    }

    if (subjectPaths.empty()) {
        return region;
    }

    Paths64 expandedPaths = subjectPaths;
    if (gapPadding > 0.0) {
        ClipperOffset offset;
        offset.ArcTolerance(kClipperArcTolerance);
        offset.AddPaths(subjectPaths, JoinType::Round, EndType::Polygon);
        Paths64 padded;
        offset.Execute(gapPadding * kClipperScale, padded);
        if (!padded.empty()) {
            expandedPaths = std::move(padded);
        }
    }

    PolyTree64 unionTree;
    {
        Clipper64 clipper;
        clipper.AddSubject(expandedPaths);
        clipper.Execute(ClipType::Union, FillRule::NonZero, unionTree);
    }

    if (unionTree.Count() == 0) {
        return region;
    }

    Paths64 unionPaths = polyTreeToPaths(unionTree);
    Paths64 finalPaths = unionPaths;

    if (gapPadding > 0.0 && !unionPaths.empty()) {
        ClipperOffset shrink;
        shrink.ArcTolerance(kClipperArcTolerance);
        shrink.AddPaths(unionPaths, JoinType::Round, EndType::Polygon);
        Paths64 shrunk;
        shrink.Execute(-gapPadding * kClipperScale, shrunk);
        if (!shrunk.empty()) {
            finalPaths = std::move(shrunk);
        }
    }

    PolyTree64 finalTree;
    {
        Clipper64 clipper;
        clipper.AddSubject(finalPaths);
        clipper.Execute(ClipType::Union, FillRule::NonZero, finalTree);
    }

    if (finalTree.Count() == 0) {
        return region;
    }

    const PolyPath64* containingNode = findNodeContainingPoint(finalTree, seedPoint, kClipperScale);
    if (!containingNode) {
        return region;
    }

    const double inverseScale = 1.0 / kClipperScale;
    const bool invertOrientation = containingNode->IsHole();
    QPainterPath fillPath = polyPathToPainterPath(containingNode, inverseScale, invertOrientation);
    fillPath.setFillRule(Qt::WindingFill);

    if (fillPath.isEmpty() || !fillPath.contains(seedPoint)) {
        return region;
    }

    region.outerBoundary = fillPath;
    region.bounds = fillPath.boundingRect();
    region.innerHoles.clear();
    collectHolePaths(containingNode, inverseScale, region.innerHoles, true, invertOrientation);
    region.isValid = true;
    return region;
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

// Create stroked outlines of all provided segments and merge them into a single filled path.
// This is purely vector-based and avoids raster blending or ARGB multiplication.
QPainterPath BucketFillTool::strokeAndMergePaths(const QList<PathSegment>& segments, qreal strokeWidth)
{
    QPainterPath merged;
    if (segments.isEmpty()) return merged;

    QPainterPathStroker stroker;
    stroker.setWidth(strokeWidth);
    stroker.setJoinStyle(Qt::RoundJoin);
    stroker.setCapStyle(Qt::RoundCap);

    for (const PathSegment& seg : segments) {
        const QPainterPath& candidate = seg.geometry.isEmpty() ? seg.path : seg.geometry;
        if (candidate.isEmpty()) continue;

        if (!seg.geometry.isEmpty()) {
            merged = merged.united(candidate);
        }
        else {
            QPainterPath stroked = stroker.createStroke(candidate);
            merged = merged.united(stroked);
        }
    }

    return merged;
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

    // Reasonable fill area
    qreal size = 200;
    QRectF fillArea(point.x() - size / 2, point.y() - size / 2, size, size);

    // Make sure fill area is within canvas bounds
    QRectF canvasRect = m_canvas->getCanvasRect();
    fillArea = fillArea.intersected(canvasRect);

    if (fillArea.isEmpty()) {
        qDebug() << "BucketFill: Fill area outside canvas bounds";
        return;
    }

    // Render scene to image with proper resolution
    QImage sceneImage = renderSceneToImage(fillArea, 2.0);

    if (sceneImage.isNull()) {
        qDebug() << "BucketFill: Failed to render scene to image";
        return;
    }

    // Convert scene point to image coordinates
    QPointF relativePoint = point - fillArea.topLeft();
    QPoint imagePoint(relativePoint.x() * 2, relativePoint.y() * 2);

    if (!sceneImage.rect().contains(imagePoint)) {
        qDebug() << "BucketFill: Click point outside rendered area";
        return;
    }

    // Get target color at click point
    QColor targetColor = getPixelColor(sceneImage, imagePoint);
    qDebug() << "BucketFill: Target color:" << targetColor.name() << "Fill color:" << m_fillColor.name();

    // Check if colors are different enough
    if (qAbs(targetColor.red() - m_fillColor.red()) <= m_tolerance / 2 &&
        qAbs(targetColor.green() - m_fillColor.green()) <= m_tolerance / 2 &&
        qAbs(targetColor.blue() - m_fillColor.blue()) <= m_tolerance / 2) {
        qDebug() << "BucketFill: Target color too similar to fill color";
        return;
    }

    // Check if we're trying to fill transparent background
    if (targetColor.alpha() < 50) {
        qDebug() << "BucketFill: Not filling transparent background";
        return;
    }

    // Create a copy for flood fill
    QImage fillImage = sceneImage.copy();

    // Perform flood fill with size limit
    int filledPixels = floodFillImageLimited(fillImage, imagePoint, targetColor, m_fillColor, 8000);

    qDebug() << "BucketFill: Filled" << filledPixels << "pixels";

    if (filledPixels == 0) {
        qDebug() << "BucketFill: No pixels were filled";
        return;
    }

    if (filledPixels > 6000) {
        qDebug() << "BucketFill: Fill area too large (" << filledPixels << " pixels), aborting";
        return;
    }

    // RESTORED: Trace the filled region with proper contour tracing
    QPainterPath filledPath = traceFilledRegion(fillImage, m_fillColor);

    if (!filledPath.isEmpty()) {
        // Transform path back to scene coordinates
        QTransform transform;
        transform.translate(fillArea.x(), fillArea.y());
        transform.scale(0.5, 0.5); // Scale back from 2x
        filledPath = transform.map(filledPath);

        // Smooth the contour and create fill item
        filledPath = smoothContour(filledPath, 1.5);
        QGraphicsPathItem* fillItem = createFillItem(filledPath, m_fillColor);
        if (fillItem) {
            addFillToCanvas(fillItem);
            qDebug() << "BucketFill: Successfully added contour-traced fill to canvas";
        }
    }
    else {
        qDebug() << "BucketFill: Failed to trace filled region";
    }
}

QImage BucketFillTool::renderSceneToImage(const QRectF& region, qreal scale)
{
    if (!m_canvas || !m_canvas->scene()) return QImage();

    QSize imageSize(region.width() * scale, region.height() * scale);
    QImage image(imageSize, QImage::Format_ARGB32);
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

    int maxPoints = qMin(1000, image.width() + image.height()); // Reasonable limit

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

    qDebug() << "BucketFill: Created fill item with color" << color.name();
    return fillItem;
}

void BucketFillTool::addFillToCanvas(QGraphicsPathItem* fillItem)
{
    if (!fillItem || !m_canvas) return;

    if (m_mainWindow && m_mainWindow->m_undoStack) {
        DrawCommand* command = new DrawCommand(m_canvas, fillItem);
        m_mainWindow->m_undoStack->push(command);

        QTimer::singleShot(0, [fillItem]() {
            fillItem->setZValue(-100);
            });

        qDebug() << "BucketFill: Added fill to canvas with undo support";
    }
    else {
        addItemToCanvas(fillItem);
        fillItem->setZValue(-100);
        qDebug() << "BucketFill: Added fill directly to canvas";
    }
}

void BucketFillTool::showFillPreview(const QPainterPath& path)
{
    if (!m_canvas || !m_canvas->scene() || path.isEmpty()) {
        hideFillPreview();
        return;
    }

    if (!m_previewItem) {
        m_previewItem = new QGraphicsPathItem(path);
        QColor previewColor = m_fillColor; previewColor.setAlpha(100);
        m_previewItem->setBrush(QBrush(previewColor));
        m_previewItem->setPen(QPen(m_fillColor, 1, Qt::DashLine));
        m_previewItem->setFlag(QGraphicsItem::ItemIsSelectable, false);
        m_previewItem->setFlag(QGraphicsItem::ItemIsMovable, false);
        m_previewItem->setAcceptedMouseButtons(Qt::NoButton);
        m_previewItem->setZValue(1000);
        m_canvas->scene()->addItem(m_previewItem);
    }
    else {
        m_previewItem->setPath(path);   // reuse instead of add/remove
        m_previewItem->setVisible(true);
    }
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
        const QPainterPath& candidate = seg.geometry.isEmpty() ? seg.path : seg.geometry;
        result = result.united(candidate);
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
        const QPainterPath& candidate = seg.geometry.isEmpty() ? seg.path : seg.geometry;
        if (candidate.contains(point)) return true;
    }
    return false;
}

QPainterPath BucketFillTool::createBoundingShape(const QList<PathSegment>& segments)
{
    QPainterPath result;
    for (const PathSegment& seg : segments) {
        const QPainterPath& candidate = seg.geometry.isEmpty() ? seg.path : seg.geometry;
        result = result.united(candidate);
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