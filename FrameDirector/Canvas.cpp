// Canvas.cpp - Fixed version with proper selection and blank keyframes

#include "Canvas.h"
#include "Tools/Tool.h"
#include "MainWindow.h"
#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QPainter>
#include <QRubberBand>
#include <QScrollBar>
#include <QApplication>
#include <QtMath>
#include <QDebug>

// LayerGraphicsGroup implementation - COMPLETELY REWRITTEN
LayerGraphicsGroup::LayerGraphicsGroup(int layerIndex, const QString& name)
    : QGraphicsItemGroup()
    , m_layerIndex(layerIndex)
    , m_layerName(name)
    , m_visible(true)
    , m_locked(false)
    , m_opacity(1.0)
{
    // FIXED: Make the group completely transparent to events
    setFlag(QGraphicsItem::ItemIsSelectable, false);
    setFlag(QGraphicsItem::ItemIsMovable, false);
    setFlag(QGraphicsItem::ItemHasNoContents, true);  // Group has no visual contents
    setHandlesChildEvents(false);  // Don't handle child events
    setOpacity(m_opacity);
    setVisible(m_visible);
}

void LayerGraphicsGroup::setLayerVisible(bool visible)
{
    m_visible = visible;
    setVisible(visible);
}

void LayerGraphicsGroup::setLayerLocked(bool locked)
{
    m_locked = locked;
    // Update all child items
    QList<QGraphicsItem*> children = childItems();
    for (QGraphicsItem* item : children) {
        item->setFlag(QGraphicsItem::ItemIsSelectable, !locked);
        item->setFlag(QGraphicsItem::ItemIsMovable, !locked);
    }
}

void LayerGraphicsGroup::setLayerOpacity(double opacity)
{
    m_opacity = qBound(0.0, opacity, 1.0);
    setOpacity(m_opacity);
}

// ALTERNATIVE: Don't use QGraphicsItemGroup for layers at all
// Instead, just track items per layer and handle visibility/locking manually
class LayerItemTracker
{
public:
    LayerItemTracker(int layerIndex, const QString& name)
        : m_layerIndex(layerIndex)
        , m_layerName(name)
        , m_visible(true)
        , m_locked(false)
        , m_opacity(1.0)
    {
    }

    void addItem(QGraphicsItem* item) {
        if (item && !m_items.contains(item)) {
            m_items.append(item);
            updateItemProperties(item);
        }
    }

    void removeItem(QGraphicsItem* item) {
        m_items.removeAll(item);
    }

    void setVisible(bool visible) {
        m_visible = visible;
        for (QGraphicsItem* item : m_items) {
            item->setVisible(visible);
        }
    }

    void setLocked(bool locked) {
        m_locked = locked;
        for (QGraphicsItem* item : m_items) {
            item->setFlag(QGraphicsItem::ItemIsSelectable, !locked);
            item->setFlag(QGraphicsItem::ItemIsMovable, !locked);
        }
    }

    void setOpacity(double opacity) {
        m_opacity = qBound(0.0, opacity, 1.0);
        for (QGraphicsItem* item : m_items) {
            item->setOpacity(m_opacity);
        }
    }

    QList<QGraphicsItem*> getItems() const { return m_items; }
    bool isVisible() const { return m_visible; }
    bool isLocked() const { return m_locked; }
    double getOpacity() const { return m_opacity; }
    int getLayerIndex() const { return m_layerIndex; }
    QString getLayerName() const { return m_layerName; }
    void setLayerName(const QString& name) { m_layerName = name; }

private:
    void updateItemProperties(QGraphicsItem* item) {
        item->setVisible(m_visible);
        item->setOpacity(m_opacity);
        item->setFlag(QGraphicsItem::ItemIsSelectable, !m_locked);
        item->setFlag(QGraphicsItem::ItemIsMovable, !m_locked);
        item->setZValue(m_layerIndex * 1000);  // Layer ordering
    }

    int m_layerIndex;
    QString m_layerName;
    bool m_visible;
    bool m_locked;
    double m_opacity;
    QList<QGraphicsItem*> m_items;
};

// Canvas implementation
Canvas::Canvas(MainWindow* parent)
    : QGraphicsView(parent)
    , m_mainWindow(parent)
    , m_scene(nullptr)
    , m_currentTool(nullptr)
    , m_canvasSize(1920, 1080)
    , m_backgroundRect(nullptr)
    , m_currentLayerIndex(0)
    , m_currentFrame(1)
    , m_zoomFactor(1.0)
    , m_gridVisible(true)
    , m_snapToGrid(false)
    , m_rulersVisible(false)
    , m_gridSize(20.0)
    , m_strokeColor(Qt::black)
    , m_fillColor(Qt::transparent)
    , m_strokeWidth(2.0)
    , m_dragging(false)
    , m_rubberBand(nullptr)
{
    setupScene();
    setupDefaultLayers();

    setRenderHint(QPainter::Antialiasing, true);
    setRenderHint(QPainter::SmoothPixmapTransform, true);
    setDragMode(QGraphicsView::NoDrag);
    setInteractive(true);

    // Set viewport update mode for better performance
    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);

    // Enable mouse tracking
    setMouseTracking(true);

    // Set background
    setBackgroundBrush(QBrush(QColor(48, 48, 48)));

    // Connect scene signals
    connect(m_scene, &QGraphicsScene::selectionChanged,
        this, &Canvas::onSceneSelectionChanged);

    // Make sure the canvas has focus to receive key events
    setFocusPolicy(Qt::StrongFocus);

    // Create initial keyframe
    createKeyframe(1);

    qDebug() << "Canvas created with size:" << m_canvasSize;
}

Canvas::~Canvas()
{
    if (m_rubberBand) {
        delete m_rubberBand;
    }
}

void Canvas::setupScene()
{
    m_scene = new QGraphicsScene(this);

    // Set canvas rectangle
    m_canvasRect = QRectF(0, 0, m_canvasSize.width(), m_canvasSize.height());

    // Set scene rect larger than canvas to allow objects outside canvas
    QRectF sceneRect = m_canvasRect.adjusted(-500, -500, 500, 500);
    m_scene->setSceneRect(sceneRect);

    // Set the scene background to dark gray
    m_scene->setBackgroundBrush(QBrush(QColor(64, 64, 64)));

    setScene(m_scene);

    qDebug() << "Scene set up with canvas rect:" << m_canvasRect;
}

void Canvas::setupDefaultLayers()
{
    // FIXED: Create placeholder "layers" - we track by Z-value, not actual groups
    m_layers.push_back(nullptr);  // Background layer placeholder
    m_layers.push_back(nullptr);  // Drawing layer placeholder

    // Create white background rectangle
    m_backgroundRect = new QGraphicsRectItem(m_canvasRect);
    m_backgroundRect->setPen(QPen(QColor(200, 200, 200), 1));
    m_backgroundRect->setBrush(QBrush(Qt::white));
    m_backgroundRect->setFlag(QGraphicsItem::ItemIsSelectable, false);
    m_backgroundRect->setFlag(QGraphicsItem::ItemIsMovable, false);
    m_backgroundRect->setZValue(-1000); // Behind everything
    m_scene->addItem(m_backgroundRect);

    // Set Layer 1 as current (not background)
    setCurrentLayer(1);
}

QSize Canvas::getCanvasSize() const
{
    return m_canvasSize;
}

QRectF Canvas::getCanvasRect() const
{
    return m_canvasRect;
}

void Canvas::setCanvasSize(const QSize& size)
{
    m_canvasSize = size;
    m_canvasRect = QRectF(0, 0, size.width(), size.height());

    // Update background rectangle
    if (m_backgroundRect) {
        m_backgroundRect->setRect(m_canvasRect);
    }

    // Update scene rect
    QRectF sceneRect = m_canvasRect.adjusted(-500, -500, 500, 500);
    m_scene->setSceneRect(sceneRect);

    viewport()->update();
}

int Canvas::addLayer(const QString& name)
{
    QString layerName = name.isEmpty() ? QString("Layer %1").arg(m_layers.size() + 1) : name;

    // FIXED: Just add a nullptr placeholder - we track items by Z-value
    m_layers.push_back(nullptr);

    qDebug() << "Added layer:" << layerName << "Index:" << (m_layers.size() - 1);

    emit layerChanged(m_layers.size() - 1);
    return m_layers.size() - 1;
}

void Canvas::removeLayer(int layerIndex)
{
    if (layerIndex >= 0 && layerIndex < m_layers.size() && m_layers.size() > 1) {
        // FIXED: Delete all items in this layer (by Z-value range)
        QList<QGraphicsItem*> allItems = m_scene->items();
        QList<QGraphicsItem*> itemsToDelete;

        int layerZMin = layerIndex * 1000;
        int layerZMax = (layerIndex + 1) * 1000 - 1;

        for (QGraphicsItem* item : allItems) {
            if (item != m_backgroundRect &&
                item->zValue() >= layerZMin &&
                item->zValue() <= layerZMax) {
                itemsToDelete.append(item);
            }
        }

        for (QGraphicsItem* item : itemsToDelete) {
            m_scene->removeItem(item);
            delete item;
        }

        // Remove layer placeholder
        m_layers.erase(m_layers.begin() + layerIndex);

        // Adjust current layer
        if (m_currentLayerIndex >= m_layers.size()) {
            m_currentLayerIndex = m_layers.size() - 1;
        }
        if (m_currentLayerIndex < 0) {
            m_currentLayerIndex = 0;
        }

        // Update frame state after layer deletion
        storeCurrentFrameState();

        emit layerChanged(m_currentLayerIndex);
    }
}

void Canvas::setCurrentLayer(int layerIndex)
{
    if (layerIndex >= 0 && layerIndex < m_layers.size()) {
        m_currentLayerIndex = layerIndex;
        emit layerChanged(layerIndex);
        qDebug() << "Current layer set to:" << layerIndex;
    }
}

int Canvas::getCurrentLayer() const
{
    return m_currentLayerIndex;
}

int Canvas::getLayerCount() const
{
    return m_layers.size();
}

void Canvas::setLayerVisible(int layerIndex, bool visible)
{
    // Find all items in this layer (by Z-value range) and set visibility
    QList<QGraphicsItem*> allItems = m_scene->items();
    int layerZMin = layerIndex * 1000;
    int layerZMax = (layerIndex + 1) * 1000 - 1;

    for (QGraphicsItem* item : allItems) {
        if (item != m_backgroundRect &&
            item->zValue() >= layerZMin &&
            item->zValue() <= layerZMax) {
            item->setVisible(visible);
        }
    }

    // Update current frame state if we modified visibility
    storeCurrentFrameState();
}

void Canvas::setLayerLocked(int layerIndex, bool locked)
{
    // Find all items in this layer (by Z-value range) and set lock state
    QList<QGraphicsItem*> allItems = m_scene->items();
    int layerZMin = layerIndex * 1000;
    int layerZMax = (layerIndex + 1) * 1000 - 1;

    for (QGraphicsItem* item : allItems) {
        if (item != m_backgroundRect &&
            item->zValue() >= layerZMin &&
            item->zValue() <= layerZMax) {
            item->setFlag(QGraphicsItem::ItemIsSelectable, !locked);
            item->setFlag(QGraphicsItem::ItemIsMovable, !locked);
        }
    }
}

void Canvas::setLayerOpacity(int layerIndex, double opacity)
{
    // Find all items in this layer (by Z-value range) and set opacity
    QList<QGraphicsItem*> allItems = m_scene->items();
    int layerZMin = layerIndex * 1000;
    int layerZMax = (layerIndex + 1) * 1000 - 1;

    for (QGraphicsItem* item : allItems) {
        if (item != m_backgroundRect &&
            item->zValue() >= layerZMin &&
            item->zValue() <= layerZMax) {
            item->setOpacity(opacity);
        }
    }

    // Update current frame state to preserve opacity changes
    storeCurrentFrameState();
}

void Canvas::moveLayer(int fromIndex, int toIndex)
{
    if (fromIndex >= 0 && fromIndex < m_layers.size() &&
        toIndex >= 0 && toIndex < m_layers.size() &&
        fromIndex != toIndex) {

        // Get all items in both layers by Z-value ranges
        QList<QGraphicsItem*> allItems = m_scene->items();
        QList<QGraphicsItem*> fromItems, toItems;

        int fromZMin = fromIndex * 1000;
        int fromZMax = (fromIndex + 1) * 1000 - 1;
        int toZMin = toIndex * 1000;
        int toZMax = (toIndex + 1) * 1000 - 1;

        // Collect items from both layers
        for (QGraphicsItem* item : allItems) {
            if (item != m_backgroundRect) {
                if (item->zValue() >= fromZMin && item->zValue() <= fromZMax) {
                    fromItems.append(item);
                }
                else if (item->zValue() >= toZMin && item->zValue() <= toZMax) {
                    toItems.append(item);
                }
            }
        }

        // Swap Z-values: move "from" items to "to" layer Z-range
        for (QGraphicsItem* item : fromItems) {
            item->setZValue(toIndex * 1000 + (item->zValue() - fromZMin));
        }

        // Move "to" items to "from" layer Z-range  
        for (QGraphicsItem* item : toItems) {
            item->setZValue(fromIndex * 1000 + (item->zValue() - toZMin));
        }

        // Swap layer placeholders
        void* temp = m_layers[fromIndex];
        m_layers[fromIndex] = m_layers[toIndex];
        m_layers[toIndex] = temp;

        // Update current frame state and emit signal
        storeCurrentFrameState();
        emit layerChanged(toIndex);
    }
}

void Canvas::setCurrentFrame(int frame)
{
    if (frame != m_currentFrame && frame >= 1) {
        m_currentFrame = frame;

        // Load frame state (but don't save current state automatically)
        loadFrameState(frame);

        emit frameChanged(frame);
        qDebug() << "Current frame set to:" << frame;
    }
}

int Canvas::getCurrentFrame() const
{
    return m_currentFrame;
}

void Canvas::createKeyframe(int frame)
{
    m_keyframes.insert(frame);
    saveFrameState(frame);  // This will save all currently visible items
    emit keyframeCreated(frame);
    qDebug() << "Keyframe created at frame:" << frame;
}

void Canvas::createBlankKeyframe(int frame)
{
    // FIXED: Don't delete items, just clear the frame's item list
    m_frameItems[frame] = QList<QGraphicsItem*>();  // Empty list for this frame
    m_keyframes.insert(frame);

    // Hide all items since this frame should be blank
    clearFrameState();

    emit keyframeCreated(frame);
    qDebug() << "Blank keyframe created at frame:" << frame;
}

void Canvas::clearCurrentFrameContent()
{
    // FIXED: Don't delete items, just remove them from current frame
    if (m_currentFrame >= 1) {
        m_frameItems[m_currentFrame] = QList<QGraphicsItem*>();  // Clear current frame's item list
        clearFrameState();  // Hide all items
    }
    qDebug() << "Cleared current frame content";
}

bool Canvas::hasKeyframe(int frame) const
{
    return m_keyframes.find(frame) != m_keyframes.end();
}

void Canvas::saveFrameState(int frame)
{
    // FIXED: Save all scene items (except background) directly
    QList<QGraphicsItem*> frameItems;

    QList<QGraphicsItem*> allItems = m_scene->items();
    for (QGraphicsItem* item : allItems) {
        if (item != m_backgroundRect && item->isVisible()) {
            frameItems.append(item);
        }
    }

    // Store items directly per frame (no layer grouping needed)
    m_frameItems[frame] = frameItems;

    qDebug() << "Saved frame state for frame:" << frame << "Items:" << frameItems.size();
}

void Canvas::loadFrameState(int frame)
{
    // Hide all items first
    clearFrameState();

    auto frameIt = m_frameItems.find(frame);
    if (frameIt != m_frameItems.end()) {
        QList<QGraphicsItem*> frameItems = frameIt->second;

        // FIXED: Check if items are still valid before accessing them
        QList<QGraphicsItem*> validItems;
        for (QGraphicsItem* item : frameItems) {
            // Verify item still exists in scene before using it
            if (item && m_scene->items().contains(item) && item != m_backgroundRect) {
                item->setVisible(true);
                validItems.append(item);
            }
        }

        // Update frame state to only include valid items
        if (validItems.size() != frameItems.size()) {
            m_frameItems[frame] = validItems;
        }

        qDebug() << "Loaded frame state for frame:" << frame << "Showing" << validItems.size() << "valid items";
    }
    else {
        qDebug() << "No keyframe at frame:" << frame << "- frame is empty";
    }

    viewport()->update();
}

void Canvas::storeCurrentFrameState()
{
    if (m_currentFrame >= 1) {
        saveFrameState(m_currentFrame);
    }
}

void Canvas::clearFrameState()
{
    // FIXED: Hide all items except background
    QList<QGraphicsItem*> allItems = m_scene->items();
    for (QGraphicsItem* item : allItems) {
        if (item != m_backgroundRect) {
            item->setVisible(false);
        }
    }
}

void Canvas::addItemToCurrentLayer(QGraphicsItem* item)
{
    if (item && m_currentLayerIndex >= 0 && m_currentLayerIndex < m_layers.size()) {
        if (!item->scene()) {
            m_scene->addItem(item);
        }

        // Use Z-values for layering instead of groups
        item->setZValue(m_currentLayerIndex * 1000);
        item->setFlag(QGraphicsItem::ItemIsSelectable, true);
        item->setFlag(QGraphicsItem::ItemIsMovable, true);
        item->setVisible(true);  // Make sure item is visible

        // IMPORTANT: Store state after adding item so it gets saved to current frame
        storeCurrentFrameState();

        qDebug() << "Added item - Layer:" << m_currentLayerIndex
            << "Z-value:" << item->zValue()
            << "Selectable:" << (item->flags() & QGraphicsItem::ItemIsSelectable)
            << "Visible:" << item->isVisible();
    }
}


QList<QGraphicsItem*> Canvas::getSelectedItems() const
{
    return m_scene ? m_scene->selectedItems() : QList<QGraphicsItem*>();
}


void Canvas::clear()
{
    if (m_scene) {
        // FIXED: Clear frame tracking before deleting items
        m_frameItems.clear();
        m_keyframes.clear();

        // Delete all items except background
        QList<QGraphicsItem*> allItems = m_scene->items();
        QList<QGraphicsItem*> itemsToDelete;

        for (QGraphicsItem* item : allItems) {
            if (item != m_backgroundRect) {
                itemsToDelete.append(item);
            }
        }

        for (QGraphicsItem* item : itemsToDelete) {
            m_scene->removeItem(item);
            delete item;
        }

        // Create initial keyframe
        createKeyframe(1);
        emit selectionChanged();
    }
}


void Canvas::selectAll()
{
    if (m_scene) {
        // FIXED: Select all selectable items (not just in current layer)
        QList<QGraphicsItem*> allItems = m_scene->items();
        for (QGraphicsItem* item : allItems) {
            if (item != m_backgroundRect &&
                (item->flags() & QGraphicsItem::ItemIsSelectable)) {
                item->setSelected(true);
                qDebug() << "Selected item:" << item;
            }
        }
        emit selectionChanged();
    }
}

void Canvas::clearSelection()
{
    if (m_scene) {
        m_scene->clearSelection();
        emit selectionChanged();
    }
}

bool Canvas::hasSelection() const
{
    return m_scene && !m_scene->selectedItems().isEmpty();
}

int Canvas::getSelectionCount() const
{
    return m_scene ? m_scene->selectedItems().count() : 0;
}

void Canvas::deleteSelected()
{
    if (!m_scene) return;

    QList<QGraphicsItem*> selectedItems = m_scene->selectedItems();

    // FIXED: Remove deleted items from ALL frame states
    for (QGraphicsItem* item : selectedItems) {
        // Remove item from all frames that reference it
        for (auto& frameEntry : m_frameItems) {
            frameEntry.second.removeAll(item);
        }

        // Then delete the item
        m_scene->removeItem(item);
        delete item;
    }

    // Update current frame state
    storeCurrentFrameState();
    emit selectionChanged();
}


void Canvas::setCurrentTool(Tool* tool)
{
    m_currentTool = tool;
    updateCursor();
    qDebug() << "Tool set to:" << tool;
}

Tool* Canvas::getCurrentTool() const
{
    return m_currentTool;
}

void Canvas::setStrokeColor(const QColor& color)
{
    m_strokeColor = color;
    qDebug() << "Stroke color set to:" << color;
}

void Canvas::setFillColor(const QColor& color)
{
    m_fillColor = color;
    qDebug() << "Fill color set to:" << color;
}

void Canvas::setStrokeWidth(double width)
{
    m_strokeWidth = width;
}

QColor Canvas::getStrokeColor() const
{
    return m_strokeColor;
}

QColor Canvas::getFillColor() const
{
    return m_fillColor;
}

double Canvas::getStrokeWidth() const
{
    return m_strokeWidth;
}

void Canvas::mousePressEvent(QMouseEvent* event)
{
    if (!m_scene) {
        QGraphicsView::mousePressEvent(event);
        return;
    }

    QPointF scenePos = mapToScene(event->pos());

    if (m_snapToGrid) {
        scenePos = snapToGrid(scenePos);
    }

    qDebug() << "Mouse press at scene pos:" << scenePos << "Tool:" << m_currentTool;

    if (m_currentTool) {
        m_currentTool->mousePressEvent(event, scenePos);
    }
    else {
        // FIXED: Improved selection behavior
        if (event->button() == Qt::LeftButton) {
            QGraphicsItem* item = m_scene->itemAt(scenePos, transform());

            // Debug selection
            if (item && item != m_backgroundRect) {
                qDebug() << "Clicked item:" << item << "Selectable:" << (item->flags() & QGraphicsItem::ItemIsSelectable);
            }

            if (!item || item == m_backgroundRect) {
                // Start rubber band selection
                m_rubberBandOrigin = event->pos();
                if (!m_rubberBand) {
                    m_rubberBand = new QRubberBand(QRubberBand::Rectangle, this);
                }
                m_rubberBand->setGeometry(QRect(m_rubberBandOrigin, QSize()));
                m_rubberBand->show();
            }
        }
        QGraphicsView::mousePressEvent(event);
    }

    m_lastMousePos = scenePos;
    emit mousePositionChanged(scenePos);
}

void Canvas::mouseMoveEvent(QMouseEvent* event)
{
    if (!m_scene) {
        QGraphicsView::mouseMoveEvent(event);
        return;
    }

    QPointF scenePos = mapToScene(event->pos());

    if (m_snapToGrid) {
        scenePos = snapToGrid(scenePos);
    }

    if (m_currentTool) {
        m_currentTool->mouseMoveEvent(event, scenePos);
    }
    else {
        // Handle rubber band selection
        if (m_rubberBand && m_rubberBand->isVisible()) {
            QRect rect = QRect(m_rubberBandOrigin, event->pos()).normalized();
            m_rubberBand->setGeometry(rect);
        }
        QGraphicsView::mouseMoveEvent(event);
    }

    m_lastMousePos = scenePos;
    emit mousePositionChanged(scenePos);
}

void Canvas::mouseReleaseEvent(QMouseEvent* event)
{
    if (!m_scene) {
        QGraphicsView::mouseReleaseEvent(event);
        return;
    }

    QPointF scenePos = mapToScene(event->pos());

    if (m_snapToGrid) {
        scenePos = snapToGrid(scenePos);
    }

    if (m_currentTool) {
        m_currentTool->mouseReleaseEvent(event, scenePos);
    }
    else {
        // Handle rubber band selection
        if (m_rubberBand && m_rubberBand->isVisible()) {
            QRect rect = m_rubberBand->geometry();
            QPolygonF selectionArea = mapToScene(rect);

            if (!(event->modifiers() & Qt::ControlModifier)) {
                m_scene->clearSelection();
            }

            // Select all selectable items in selection area
            QPainterPath path;
            path.addPolygon(selectionArea);

            QList<QGraphicsItem*> allItems = m_scene->items();
            for (QGraphicsItem* item : allItems) {
                if (item != m_backgroundRect &&
                    (item->flags() & QGraphicsItem::ItemIsSelectable) &&
                    path.intersects(item->sceneBoundingRect())) {
                    item->setSelected(true);
                    qDebug() << "Rubber band selected item:" << item;
                }
            }

            m_rubberBand->hide();
        }
        QGraphicsView::mouseReleaseEvent(event);
    }

    emit mousePositionChanged(scenePos);
}

// ... [Rest of the methods remain the same - drawBackground, zoom, alignment, etc.] ...

void Canvas::drawBackground(QPainter* painter, const QRectF& rect)
{
    QGraphicsView::drawBackground(painter, rect);
    drawCanvasBounds(painter, rect);
    if (m_gridVisible) {
        drawGrid(painter, rect);
    }
}

void Canvas::drawCanvasBounds(QPainter* painter, const QRectF& rect)
{
    painter->save();
    QPen borderPen(QColor(150, 150, 150), 2);
    painter->setPen(borderPen);
    painter->drawRect(m_canvasRect);
    painter->setPen(QPen(QColor(30, 30, 30), 1));
    painter->drawRect(m_canvasRect.adjusted(3, 3, 3, 3));
    painter->restore();
}

void Canvas::drawGrid(QPainter* painter, const QRectF& rect)
{
    painter->save();
    QPen gridPen(QColor(96, 96, 96), 0.5);
    painter->setPen(gridPen);
    QRectF gridRect = rect.intersected(m_canvasRect);
    double left = int(gridRect.left()) - (int(gridRect.left()) % int(m_gridSize));
    double top = int(gridRect.top()) - (int(gridRect.top()) % int(m_gridSize));
    QVector<QLineF> lines;
    for (double x = left; x < gridRect.right(); x += m_gridSize) {
        if (x >= m_canvasRect.left() && x <= m_canvasRect.right()) {
            lines.append(QLineF(x, gridRect.top(), x, gridRect.bottom()));
        }
    }
    for (double y = top; y < gridRect.bottom(); y += m_gridSize) {
        if (y >= m_canvasRect.top() && y <= m_canvasRect.bottom()) {
            lines.append(QLineF(gridRect.left(), y, gridRect.right(), y));
        }
    }
    painter->drawLines(lines);
    painter->restore();
}

void Canvas::onSceneSelectionChanged()
{
    emit selectionChanged();
}

// [Include all other methods like zoomIn, zoomOut, alignment methods, etc. - they remain unchanged]

void Canvas::zoomIn()
{
    double scaleFactor = 1.25;
    scale(scaleFactor, scaleFactor);
    m_zoomFactor *= scaleFactor;
    emit zoomChanged(m_zoomFactor);
}

void Canvas::zoomOut()
{
    double scaleFactor = 1.0 / 1.25;
    scale(scaleFactor, scaleFactor);
    m_zoomFactor *= scaleFactor;
    emit zoomChanged(m_zoomFactor);
}

void Canvas::zoomToFit()
{
    if (!m_scene) return;
    fitInView(m_canvasRect, Qt::KeepAspectRatio);
    m_zoomFactor = transform().m11();
    emit zoomChanged(m_zoomFactor);
}

void Canvas::setZoomFactor(double factor)
{
    resetTransform();
    scale(factor, factor);
    m_zoomFactor = factor;
    emit zoomChanged(m_zoomFactor);
}

double Canvas::getZoomFactor() const
{
    return m_zoomFactor;
}

void Canvas::setGridVisible(bool visible)
{
    m_gridVisible = visible;
    viewport()->update();
}

void Canvas::setSnapToGrid(bool snap)
{
    m_snapToGrid = snap;
}

void Canvas::setRulersVisible(bool visible)
{
    m_rulersVisible = visible;
    viewport()->update();
}

bool Canvas::isGridVisible() const { return m_gridVisible; }
bool Canvas::isSnapToGrid() const { return m_snapToGrid; }
bool Canvas::areRulersVisible() const { return m_rulersVisible; }

void Canvas::groupSelectedItems()
{
    if (!m_scene) return;
    QList<QGraphicsItem*> selectedItems = m_scene->selectedItems();
    if (selectedItems.count() > 1) {
        QGraphicsItemGroup* group = m_scene->createItemGroup(selectedItems);
        group->setFlag(QGraphicsItem::ItemIsSelectable, true);
        group->setFlag(QGraphicsItem::ItemIsMovable, true);
        addItemToCurrentLayer(group);
        emit selectionChanged();
    }
}

void Canvas::ungroupSelectedItems()
{
    if (!m_scene) return;
    QList<QGraphicsItem*> selectedItems = m_scene->selectedItems();
    for (QGraphicsItem* item : selectedItems) {
        QGraphicsItemGroup* group = qgraphicsitem_cast<QGraphicsItemGroup*>(item);
        if (group) {
            m_scene->destroyItemGroup(group);
        }
    }
    emit selectionChanged();
}

void Canvas::alignSelectedItems(int alignment)
{
    if (!m_scene) return;
    QList<QGraphicsItem*> selectedItems = m_scene->selectedItems();
    if (selectedItems.count() < 2) return;

    QRectF boundingRect = selectedItems.first()->sceneBoundingRect();
    for (QGraphicsItem* item : selectedItems) {
        boundingRect = boundingRect.united(item->sceneBoundingRect());
    }

    for (QGraphicsItem* item : selectedItems) {
        QRectF itemRect = item->sceneBoundingRect();
        QPointF newPos = item->scenePos();

        switch (alignment) {
        case 0: newPos.setX(boundingRect.left() - itemRect.left() + item->scenePos().x()); break;
        case 1: newPos.setX(boundingRect.center().x() - itemRect.center().x() + item->scenePos().x()); break;
        case 2: newPos.setX(boundingRect.right() - itemRect.right() + item->scenePos().x()); break;
        case 3: newPos.setY(boundingRect.top() - itemRect.top() + item->scenePos().y()); break;
        case 4: newPos.setY(boundingRect.center().y() - itemRect.center().y() + item->scenePos().y()); break;
        case 5: newPos.setY(boundingRect.bottom() - itemRect.bottom() + item->scenePos().y()); break;
        }
        item->setPos(newPos);
    }
    storeCurrentFrameState();
}

void Canvas::bringSelectedToFront()
{
    if (!m_scene) return;
    QList<QGraphicsItem*> selectedItems = m_scene->selectedItems();
    qreal maxZ = 0;
    for (QGraphicsItem* item : m_scene->items()) {
        if (item->zValue() > maxZ) maxZ = item->zValue();
    }
    for (QGraphicsItem* item : selectedItems) {
        item->setZValue(maxZ + 1);
        maxZ += 1;
    }
    storeCurrentFrameState();
}

void Canvas::bringSelectedForward()
{
    if (!m_scene) return;
    QList<QGraphicsItem*> selectedItems = m_scene->selectedItems();
    for (QGraphicsItem* item : selectedItems) {
        item->setZValue(item->zValue() + 1);
    }
    storeCurrentFrameState();
}

void Canvas::sendSelectedBackward()
{
    if (!m_scene) return;
    QList<QGraphicsItem*> selectedItems = m_scene->selectedItems();
    for (QGraphicsItem* item : selectedItems) {
        item->setZValue(item->zValue() - 1);
    }
    storeCurrentFrameState();
}

void Canvas::sendSelectedToBack()
{
    if (!m_scene) return;
    QList<QGraphicsItem*> selectedItems = m_scene->selectedItems();
    qreal minZ = 0;
    for (QGraphicsItem* item : m_scene->items()) {
        if (item->zValue() < minZ) minZ = item->zValue();
    }
    for (QGraphicsItem* item : selectedItems) {
        item->setZValue(minZ - 1);
        minZ -= 1;
    }
    storeCurrentFrameState();
}

void Canvas::flipSelectedHorizontal()
{
    if (!m_scene) return;
    QList<QGraphicsItem*> selectedItems = m_scene->selectedItems();
    for (QGraphicsItem* item : selectedItems) {
        QTransform transform = item->transform();
        transform.scale(-1, 1);
        item->setTransform(transform);
    }
    storeCurrentFrameState();
}

void Canvas::flipSelectedVertical()
{
    if (!m_scene) return;
    QList<QGraphicsItem*> selectedItems = m_scene->selectedItems();
    for (QGraphicsItem* item : selectedItems) {
        QTransform transform = item->transform();
        transform.scale(1, -1);
        item->setTransform(transform);
    }
    storeCurrentFrameState();
}

void Canvas::rotateSelected(double angle)
{
    if (!m_scene) return;
    QList<QGraphicsItem*> selectedItems = m_scene->selectedItems();
    for (QGraphicsItem* item : selectedItems) {
        QPointF center = item->boundingRect().center();
        item->setTransformOriginPoint(center);
        item->setRotation(item->rotation() + angle);
    }
    storeCurrentFrameState();
}

QPointF Canvas::snapToGrid(const QPointF& point)
{
    if (!m_snapToGrid) return point;
    double x = qRound(point.x() / m_gridSize) * m_gridSize;
    double y = qRound(point.y() / m_gridSize) * m_gridSize;
    return QPointF(x, y);
}

void Canvas::updateCursor()
{
    if (m_currentTool) {
        setCursor(m_currentTool->getCursor());
    }
    else {
        setCursor(Qt::ArrowCursor);
    }
}

void Canvas::wheelEvent(QWheelEvent* event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        const double scaleFactor = 1.15;
        if (event->angleDelta().y() > 0) {
            scale(scaleFactor, scaleFactor);
            m_zoomFactor *= scaleFactor;
        }
        else {
            scale(1.0 / scaleFactor, 1.0 / scaleFactor);
            m_zoomFactor /= scaleFactor;
        }
        emit zoomChanged(m_zoomFactor);
    }
    else {
        QGraphicsView::wheelEvent(event);
    }
}

void Canvas::keyPressEvent(QKeyEvent* event)
{
    if (m_currentTool) {
        m_currentTool->keyPressEvent(event);
    }
    QGraphicsView::keyPressEvent(event);
}

void Canvas::paintEvent(QPaintEvent* event)
{
    QGraphicsView::paintEvent(event);
}

void Canvas::drawForeground(QPainter* painter, const QRectF& rect)
{
    QGraphicsView::drawForeground(painter, rect);
    if (m_rulersVisible) {
        drawRulers(painter);
    }
}

void Canvas::drawRulers(QPainter* painter)
{
    painter->save();
    QPen rulerPen(QColor(200, 200, 200));
    painter->setPen(rulerPen);
    painter->setFont(QFont("Arial", 8));
    QRectF viewRect = mapToScene(viewport()->rect()).boundingRect();
    painter->fillRect(0, 0, viewport()->width(), 20, QColor(80, 80, 80));
    painter->fillRect(0, 20, 20, viewport()->height() - 20, QColor(80, 80, 80));
    painter->restore();
}