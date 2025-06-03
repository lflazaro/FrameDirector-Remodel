// Canvas.cpp - Fixed version with stable layer system

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

// FIXED: Proper layer data structure instead of void pointers
struct LayerData {
    QString name;
    bool visible;
    bool locked;
    double opacity;
    QList<QGraphicsItem*> items;

    LayerData(const QString& layerName)
        : name(layerName), visible(true), locked(false), opacity(1.0) {
    }
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
    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    setMouseTracking(true);
    setBackgroundBrush(QBrush(QColor(48, 48, 48)));

    connect(m_scene, &QGraphicsScene::selectionChanged,
        this, &Canvas::onSceneSelectionChanged);

    setFocusPolicy(Qt::StrongFocus);
    createKeyframe(1);

    qDebug() << "Canvas created with size:" << m_canvasSize;
}

Canvas::~Canvas()
{
    // FIXED: Properly clean up layer data
    for (void* layerPtr : m_layers) {
        if (layerPtr) {
            delete static_cast<LayerData*>(layerPtr);
        }
    }

    if (m_rubberBand) {
        delete m_rubberBand;
    }
}

void Canvas::setupScene()
{
    m_scene = new QGraphicsScene(this);
    m_canvasRect = QRectF(0, 0, m_canvasSize.width(), m_canvasSize.height());
    QRectF sceneRect = m_canvasRect.adjusted(-500, -500, 500, 500);
    m_scene->setSceneRect(sceneRect);
    m_scene->setBackgroundBrush(QBrush(QColor(64, 64, 64)));
    setScene(m_scene);

    qDebug() << "Scene set up with canvas rect:" << m_canvasRect;
}

void Canvas::setupDefaultLayers()
{
    // FIXED: Create proper layer data structures
    m_layers.clear();

    // Create background layer
    LayerData* backgroundLayer = new LayerData("Background");
    m_layers.push_back(backgroundLayer);

    // Create drawing layer
    LayerData* drawingLayer = new LayerData("Layer 1");
    m_layers.push_back(drawingLayer);

    // Create white background rectangle
    m_backgroundRect = new QGraphicsRectItem(m_canvasRect);
    m_backgroundRect->setPen(QPen(QColor(200, 200, 200), 1));
    m_backgroundRect->setBrush(QBrush(Qt::white));
    m_backgroundRect->setFlag(QGraphicsItem::ItemIsSelectable, false);
    m_backgroundRect->setFlag(QGraphicsItem::ItemIsMovable, false);
    m_backgroundRect->setZValue(-1000);
    m_scene->addItem(m_backgroundRect);

    // Add background to background layer
    backgroundLayer->items.append(m_backgroundRect);

    // Set Layer 1 as current (not background)
    setCurrentLayer(1);
}

int Canvas::addLayer(const QString& name)
{
    QString layerName = name.isEmpty() ? QString("Layer %1").arg(m_layers.size()) : name;

    // FIXED: Create new layer data structure
    LayerData* newLayer = new LayerData(layerName);
    m_layers.push_back(newLayer);

    int newIndex = m_layers.size() - 1;

    qDebug() << "Added layer:" << layerName << "Index:" << newIndex;
    emit layerChanged(newIndex);
    return newIndex;
}

void Canvas::removeLayer(int layerIndex)
{
    if (layerIndex >= 0 && layerIndex < m_layers.size() && m_layers.size() > 1) {
        LayerData* layer = static_cast<LayerData*>(m_layers[layerIndex]);

        // Remove all items in this layer from scene (except background)
        for (QGraphicsItem* item : layer->items) {
            if (item != m_backgroundRect) {
                m_scene->removeItem(item);
                delete item;
            }
        }

        // Delete layer data
        delete layer;
        m_layers.erase(m_layers.begin() + layerIndex);

        // Adjust current layer
        if (m_currentLayerIndex >= m_layers.size()) {
            m_currentLayerIndex = m_layers.size() - 1;
        }
        if (m_currentLayerIndex < 0) {
            m_currentLayerIndex = 0;
        }

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
    if (layerIndex >= 0 && layerIndex < m_layers.size()) {
        LayerData* layer = static_cast<LayerData*>(m_layers[layerIndex]);
        layer->visible = visible;

        // Update visibility of all items in this layer
        for (QGraphicsItem* item : layer->items) {
            item->setVisible(visible);
        }

        storeCurrentFrameState();
        qDebug() << "Layer" << layerIndex << "visibility set to:" << visible;
    }
}

void Canvas::setLayerLocked(int layerIndex, bool locked)
{
    if (layerIndex >= 0 && layerIndex < m_layers.size()) {
        LayerData* layer = static_cast<LayerData*>(m_layers[layerIndex]);
        layer->locked = locked;

        // Update lock state of all items in this layer
        for (QGraphicsItem* item : layer->items) {
            item->setFlag(QGraphicsItem::ItemIsSelectable, !locked);
            item->setFlag(QGraphicsItem::ItemIsMovable, !locked);
        }

        qDebug() << "Layer" << layerIndex << "locked state set to:" << locked;
    }
}

void Canvas::setLayerOpacity(int layerIndex, double opacity)
{
    if (layerIndex >= 0 && layerIndex < m_layers.size()) {
        LayerData* layer = static_cast<LayerData*>(m_layers[layerIndex]);
        layer->opacity = qBound(0.0, opacity, 1.0);

        // FIXED: Update opacity of all items in this specific layer only
        for (QGraphicsItem* item : layer->items) {
            item->setOpacity(layer->opacity);
        }

        storeCurrentFrameState();
        qDebug() << "Layer" << layerIndex << "opacity set to:" << layer->opacity;
    }
}

void Canvas::moveLayer(int fromIndex, int toIndex)
{
    if (fromIndex >= 0 && fromIndex < m_layers.size() &&
        toIndex >= 0 && toIndex < m_layers.size() &&
        fromIndex != toIndex) {

        // Swap layer data
        void* temp = m_layers[fromIndex];
        m_layers[fromIndex] = m_layers[toIndex];
        m_layers[toIndex] = temp;

        // Update Z-values for proper rendering order
        updateAllLayerZValues();

        storeCurrentFrameState();
        emit layerChanged(toIndex);
    }
}

void Canvas::updateAllLayerZValues()
{
    // FIXED: Update Z-values to match layer order
    for (int i = 0; i < m_layers.size(); ++i) {
        LayerData* layer = static_cast<LayerData*>(m_layers[i]);
        int baseZValue = i * 1000;

        for (QGraphicsItem* item : layer->items) {
            if (item != m_backgroundRect) {
                item->setZValue(baseZValue + static_cast<int>(item->zValue()) % 1000);
            }
        }
    }
}

void Canvas::addItemToCurrentLayer(QGraphicsItem* item)
{
    if (item && m_currentLayerIndex >= 0 && m_currentLayerIndex < m_layers.size()) {
        if (!item->scene()) {
            m_scene->addItem(item);
        }

        LayerData* currentLayer = static_cast<LayerData*>(m_layers[m_currentLayerIndex]);

        // Add item to current layer
        currentLayer->items.append(item);

        // Set properties based on layer state
        item->setZValue(m_currentLayerIndex * 1000);
        item->setFlag(QGraphicsItem::ItemIsSelectable, !currentLayer->locked);
        item->setFlag(QGraphicsItem::ItemIsMovable, !currentLayer->locked);
        item->setVisible(currentLayer->visible);
        item->setOpacity(currentLayer->opacity);

        storeCurrentFrameState();

        qDebug() << "Added item to layer:" << m_currentLayerIndex
            << "Opacity:" << currentLayer->opacity
            << "Visible:" << currentLayer->visible;
    }
}

void Canvas::saveFrameState(int frame)
{
    QList<QGraphicsItem*> frameItems;

    // FIXED: Collect items from all layers, preserving layer association
    QList<QGraphicsItem*> allItems = m_scene->items();
    for (QGraphicsItem* item : allItems) {
        if (item != m_backgroundRect && item->isVisible()) {
            frameItems.append(item);
        }
    }

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

        // FIXED: Restore items and their layer properties
        QList<QGraphicsItem*> validItems;
        for (QGraphicsItem* item : frameItems) {
            if (item && m_scene->items().contains(item) && item != m_backgroundRect) {
                item->setVisible(true);

                // Restore layer-specific properties
                int layerIndex = getItemLayerIndex(item);
                if (layerIndex >= 0 && layerIndex < m_layers.size()) {
                    LayerData* layer = static_cast<LayerData*>(m_layers[layerIndex]);
                    item->setVisible(layer->visible);
                    item->setOpacity(layer->opacity);
                    item->setFlag(QGraphicsItem::ItemIsSelectable, !layer->locked);
                    item->setFlag(QGraphicsItem::ItemIsMovable, !layer->locked);
                }

                validItems.append(item);
            }
        }

        if (validItems.size() != frameItems.size()) {
            m_frameItems[frame] = validItems;
        }

        qDebug() << "Loaded frame state for frame:" << frame << "Showing" << validItems.size() << "valid items";
    }

    viewport()->update();
}

int Canvas::getItemLayerIndex(QGraphicsItem* item)
{
    // FIXED: Find which layer an item belongs to
    for (int i = 0; i < m_layers.size(); ++i) {
        LayerData* layer = static_cast<LayerData*>(m_layers[i]);
        if (layer->items.contains(item)) {
            return i;
        }
    }
    return -1; // Item not found in any layer
}

void Canvas::deleteSelected()
{
    if (!m_scene) return;

    QList<QGraphicsItem*> selectedItems = m_scene->selectedItems();

    // FIXED: Remove deleted items from layer tracking AND frame states
    for (QGraphicsItem* item : selectedItems) {
        // Remove from layer
        int layerIndex = getItemLayerIndex(item);
        if (layerIndex >= 0) {
            LayerData* layer = static_cast<LayerData*>(m_layers[layerIndex]);
            layer->items.removeAll(item);
        }

        // Remove item from all frames that reference it
        for (auto& frameEntry : m_frameItems) {
            frameEntry.second.removeAll(item);
        }

        // Then delete the item
        m_scene->removeItem(item);
        delete item;
    }

    storeCurrentFrameState();
    emit selectionChanged();
}

// FIXED: Override clear to properly handle layers
void Canvas::clear()
{
    if (m_scene) {
        m_frameItems.clear();
        m_keyframes.clear();

        // Clear all layer data except background
        for (int i = m_layers.size() - 1; i >= 0; --i) {
            LayerData* layer = static_cast<LayerData*>(m_layers[i]);

            // Remove all items except background
            QList<QGraphicsItem*> itemsToDelete;
            for (QGraphicsItem* item : layer->items) {
                if (item != m_backgroundRect) {
                    itemsToDelete.append(item);
                }
            }

            for (QGraphicsItem* item : itemsToDelete) {
                layer->items.removeAll(item);
                m_scene->removeItem(item);
                delete item;
            }

            // Delete layer data (except background layer)
            if (i > 0) {
                delete layer;
                m_layers.erase(m_layers.begin() + i);
            }
        }

        // Reset to have background + one drawing layer
        if (m_layers.size() == 1) {
            LayerData* drawingLayer = new LayerData("Layer 1");
            m_layers.push_back(drawingLayer);
        }

        setCurrentLayer(1);
        createKeyframe(1);
        emit selectionChanged();
    }
}

// ... [Include all other existing methods that weren't changed] ...

void Canvas::setCanvasSize(const QSize& size)
{
    m_canvasSize = size;
    m_canvasRect = QRectF(0, 0, size.width(), size.height());

    if (m_backgroundRect) {
        m_backgroundRect->setRect(m_canvasRect);
    }

    QRectF sceneRect = m_canvasRect.adjusted(-500, -500, 500, 500);
    m_scene->setSceneRect(sceneRect);
    viewport()->update();
}

QSize Canvas::getCanvasSize() const { return m_canvasSize; }
QRectF Canvas::getCanvasRect() const { return m_canvasRect; }

void Canvas::setCurrentFrame(int frame)
{
    if (frame != m_currentFrame && frame >= 1) {
        m_currentFrame = frame;
        loadFrameState(frame);
        emit frameChanged(frame);
        qDebug() << "Current frame set to:" << frame;
    }
}

int Canvas::getCurrentFrame() const { return m_currentFrame; }

void Canvas::createKeyframe(int frame)
{
    m_keyframes.insert(frame);
    saveFrameState(frame);
    emit keyframeCreated(frame);
    qDebug() << "Keyframe created at frame:" << frame;
}

void Canvas::createBlankKeyframe(int frame)
{
    m_frameItems[frame] = QList<QGraphicsItem*>();
    m_keyframes.insert(frame);
    clearFrameState();
    emit keyframeCreated(frame);
    qDebug() << "Blank keyframe created at frame:" << frame;
}

void Canvas::clearCurrentFrameContent()
{
    if (m_currentFrame >= 1) {
        m_frameItems[m_currentFrame] = QList<QGraphicsItem*>();
        clearFrameState();
    }
    qDebug() << "Cleared current frame content";
}

bool Canvas::hasKeyframe(int frame) const
{
    return m_keyframes.find(frame) != m_keyframes.end();
}

void Canvas::storeCurrentFrameState()
{
    if (m_currentFrame >= 1) {
        saveFrameState(m_currentFrame);
    }
}

void Canvas::clearFrameState()
{
    QList<QGraphicsItem*> allItems = m_scene->items();
    for (QGraphicsItem* item : allItems) {
        if (item != m_backgroundRect) {
            item->setVisible(false);
        }
    }
}

QList<QGraphicsItem*> Canvas::getSelectedItems() const
{
    return m_scene ? m_scene->selectedItems() : QList<QGraphicsItem*>();
}

void Canvas::selectAll()
{
    if (m_scene) {
        QList<QGraphicsItem*> allItems = m_scene->items();
        for (QGraphicsItem* item : allItems) {
            if (item != m_backgroundRect &&
                (item->flags() & QGraphicsItem::ItemIsSelectable)) {
                item->setSelected(true);
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

void Canvas::setCurrentTool(Tool* tool)
{
    m_currentTool = tool;
    updateCursor();
    qDebug() << "Tool set to:" << tool;
}

Tool* Canvas::getCurrentTool() const { return m_currentTool; }

void Canvas::setStrokeColor(const QColor& color) { m_strokeColor = color; }
void Canvas::setFillColor(const QColor& color) { m_fillColor = color; }
void Canvas::setStrokeWidth(double width) { m_strokeWidth = width; }
QColor Canvas::getStrokeColor() const { return m_strokeColor; }
QColor Canvas::getFillColor() const { return m_fillColor; }
double Canvas::getStrokeWidth() const { return m_strokeWidth; }

// [Include all mouse event handlers and other methods that remain unchanged]
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

    if (m_currentTool) {
        m_currentTool->mousePressEvent(event, scenePos);
    }
    else {
        if (event->button() == Qt::LeftButton) {
            QGraphicsItem* item = m_scene->itemAt(scenePos, transform());

            if (!item || item == m_backgroundRect) {
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
        if (m_rubberBand && m_rubberBand->isVisible()) {
            QRect rect = m_rubberBand->geometry();
            QPolygonF selectionArea = mapToScene(rect);

            if (!(event->modifiers() & Qt::ControlModifier)) {
                m_scene->clearSelection();
            }

            QPainterPath path;
            path.addPolygon(selectionArea);

            QList<QGraphicsItem*> allItems = m_scene->items();
            for (QGraphicsItem* item : allItems) {
                if (item != m_backgroundRect &&
                    (item->flags() & QGraphicsItem::ItemIsSelectable) &&
                    path.intersects(item->sceneBoundingRect())) {
                    item->setSelected(true);
                }
            }

            m_rubberBand->hide();
        }
        QGraphicsView::mouseReleaseEvent(event);
    }

    emit mousePositionChanged(scenePos);
}

// Include all remaining methods (zoom, alignment, drawing, etc.)
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

double Canvas::getZoomFactor() const { return m_zoomFactor; }

void Canvas::setGridVisible(bool visible) { m_gridVisible = visible; viewport()->update(); }
void Canvas::setSnapToGrid(bool snap) { m_snapToGrid = snap; }
void Canvas::setRulersVisible(bool visible) { m_rulersVisible = visible; viewport()->update(); }
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
        bool wasInteractive = isInteractive();
        setInteractive(false);
        QGraphicsView::wheelEvent(event);
        setInteractive(wasInteractive);
    }
}

void Canvas::keyPressEvent(QKeyEvent* event)
{
    if (m_currentTool) {
        m_currentTool->keyPressEvent(event);
    }
    QGraphicsView::keyPressEvent(event);
}

void Canvas::paintEvent(QPaintEvent* event) { QGraphicsView::paintEvent(event); }

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