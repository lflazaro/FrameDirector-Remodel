// Canvas.cpp
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

// LayerGraphicsGroup implementation
LayerGraphicsGroup::LayerGraphicsGroup(int layerIndex, const QString& name)
    : QGraphicsItemGroup()
    , m_layerIndex(layerIndex)
    , m_layerName(name)
    , m_visible(true)
    , m_locked(false)
    , m_opacity(1.0)
{
    setFlag(QGraphicsItem::ItemIsSelectable, false);
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
    // Create background layer with white canvas
    addLayer("Background");

    // Create white background rectangle
    m_backgroundRect = new QGraphicsRectItem(m_canvasRect);
    m_backgroundRect->setPen(QPen(QColor(200, 200, 200), 1));
    m_backgroundRect->setBrush(QBrush(Qt::white));
    m_backgroundRect->setFlag(QGraphicsItem::ItemIsSelectable, false);
    m_backgroundRect->setFlag(QGraphicsItem::ItemIsMovable, false);
    m_backgroundRect->setZValue(-1000); // Behind everything

    // Add to background layer
    if (!m_layers.empty()) {
        m_layers[0]->addToGroup(m_backgroundRect);
    }
    else {
        m_scene->addItem(m_backgroundRect);
    }

    // Add default drawing layer
    addLayer("Layer 1");
    setCurrentLayer(1); // Set Layer 1 as current (not background)
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
    LayerGraphicsGroup* layer = new LayerGraphicsGroup(m_layers.size(), layerName);

    m_scene->addItem(layer);
    m_layers.push_back(layer);

    qDebug() << "Added layer:" << layerName << "Index:" << (m_layers.size() - 1);

    emit layerChanged(m_layers.size() - 1);
    return m_layers.size() - 1;
}

void Canvas::removeLayer(int layerIndex)
{
    if (layerIndex >= 0 && layerIndex < m_layers.size() && m_layers.size() > 1) {
        LayerGraphicsGroup* layer = m_layers[layerIndex];

        // Remove all items from the layer
        QList<QGraphicsItem*> children = layer->childItems();
        for (QGraphicsItem* item : children) {
            layer->removeFromGroup(item);
            m_scene->removeItem(item);
            delete item;
        }

        // Remove layer from scene and vector
        m_scene->removeItem(layer);
        m_layers.erase(m_layers.begin() + layerIndex);
        delete layer;

        // Update layer indices
        for (int i = layerIndex; i < m_layers.size(); ++i) {
            m_layers[i]->m_layerIndex = i;
        }

        // Adjust current layer
        if (m_currentLayerIndex >= m_layers.size()) {
            m_currentLayerIndex = m_layers.size() - 1;
        }
        if (m_currentLayerIndex < 0) {
            m_currentLayerIndex = 0;
        }

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

LayerGraphicsGroup* Canvas::getLayer(int index) const
{
    if (index >= 0 && index < m_layers.size()) {
        return m_layers[index];
    }
    return nullptr;
}

void Canvas::setLayerVisible(int layerIndex, bool visible)
{
    LayerGraphicsGroup* layer = getLayer(layerIndex);
    if (layer) {
        layer->setLayerVisible(visible);
    }
}

void Canvas::setLayerLocked(int layerIndex, bool locked)
{
    LayerGraphicsGroup* layer = getLayer(layerIndex);
    if (layer) {
        layer->setLayerLocked(locked);
    }
}

void Canvas::setLayerOpacity(int layerIndex, double opacity)
{
    LayerGraphicsGroup* layer = getLayer(layerIndex);
    if (layer) {
        layer->setLayerOpacity(opacity);
    }
}

void Canvas::moveLayer(int fromIndex, int toIndex)
{
    if (fromIndex >= 0 && fromIndex < m_layers.size() &&
        toIndex >= 0 && toIndex < m_layers.size() &&
        fromIndex != toIndex) {

        LayerGraphicsGroup* layer = m_layers[fromIndex];
        m_layers.erase(m_layers.begin() + fromIndex);
        m_layers.insert(m_layers.begin() + toIndex, layer);

        // Update layer indices and Z values
        for (int i = 0; i < m_layers.size(); ++i) {
            m_layers[i]->m_layerIndex = i;
            m_layers[i]->setZValue(i);
        }

        emit layerChanged(toIndex);
    }
}

void Canvas::setCurrentFrame(int frame)
{
    if (frame != m_currentFrame && frame >= 1) {
        // Save current frame state
        storeCurrentFrameState();

        m_currentFrame = frame;

        // Load frame state
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
    saveFrameState(frame);
    emit keyframeCreated(frame);
    qDebug() << "Keyframe created at frame:" << frame;
}

bool Canvas::hasKeyframe(int frame) const
{
    return m_keyframes.find(frame) != m_keyframes.end();
}

void Canvas::saveFrameState(int frame)
{
    std::map<int, QList<QGraphicsItem*>> layerStates;

    for (int i = 0; i < m_layers.size(); ++i) {
        LayerGraphicsGroup* layer = m_layers[i];
        QList<QGraphicsItem*> layerItems = layer->childItems();
        layerStates[i] = layerItems;
    }

    m_frameStates[frame] = layerStates;
    qDebug() << "Saved frame state for frame:" << frame;
}

void Canvas::loadFrameState(int frame)
{
    // Clear current state first
    clearFrameState();

    auto frameIt = m_frameStates.find(frame);
    if (frameIt != m_frameStates.end()) {
        // Restore frame state
        for (auto& layerState : frameIt->second) {
            int layerIndex = layerState.first;
            QList<QGraphicsItem*> items = layerState.second;

            LayerGraphicsGroup* layer = getLayer(layerIndex);
            if (layer) {
                for (QGraphicsItem* item : items) {
                    if (item && item != m_backgroundRect) { // Don't hide background
                        item->setVisible(true);
                        if (!layer->childItems().contains(item)) {
                            layer->addToGroup(item);
                        }
                    }
                }
            }
        }
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
    // Hide all items except background
    for (LayerGraphicsGroup* layer : m_layers) {
        QList<QGraphicsItem*> children = layer->childItems();
        for (QGraphicsItem* item : children) {
            if (item != m_backgroundRect) {
                item->setVisible(false);
            }
        }
    }
}

void Canvas::addItemToCurrentLayer(QGraphicsItem* item)
{
    if (item && m_currentLayerIndex >= 0 && m_currentLayerIndex < m_layers.size()) {
        LayerGraphicsGroup* currentLayer = m_layers[m_currentLayerIndex];
        if (currentLayer) {
            // Add to scene first if not already added
            if (!item->scene()) {
                m_scene->addItem(item);
            }
            // Then add to layer group
            currentLayer->addToGroup(item);

            // Store in current frame
            storeCurrentFrameState();

            qDebug() << "Added item to layer:" << m_currentLayerIndex;
        }
    }
}

QList<QGraphicsItem*> Canvas::getSelectedItems() const
{
    return m_scene ? m_scene->selectedItems() : QList<QGraphicsItem*>();
}

void Canvas::clear()
{
    if (m_scene) {
        // Clear all layers except background
        for (int i = 1; i < m_layers.size(); ++i) { // Start from 1 to skip background
            LayerGraphicsGroup* layer = m_layers[i];
            QList<QGraphicsItem*> children = layer->childItems();
            for (QGraphicsItem* item : children) {
                layer->removeFromGroup(item);
                m_scene->removeItem(item);
                delete item;
            }
        }

        // Clear frame states and keyframes
        m_frameStates.clear();
        m_keyframes.clear();

        // Create initial keyframe
        createKeyframe(1);

        emit selectionChanged();
    }
}

void Canvas::selectAll()
{
    if (m_scene) {
        // Select all selectable items in current layer
        LayerGraphicsGroup* currentLayer = getLayer(m_currentLayerIndex);
        if (currentLayer) {
            QList<QGraphicsItem*> children = currentLayer->childItems();
            for (QGraphicsItem* item : children) {
                if (item->flags() & QGraphicsItem::ItemIsSelectable) {
                    item->setSelected(true);
                }
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
    for (QGraphicsItem* item : selectedItems) {
        // Find which layer contains this item
        for (LayerGraphicsGroup* layer : m_layers) {
            if (layer->childItems().contains(item)) {
                layer->removeFromGroup(item);
                break;
            }
        }
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

// ... [Previous zoom, grid, alignment methods remain the same] ...

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
        // Default selection behavior
        if (event->button() == Qt::LeftButton) {
            QGraphicsItem* item = m_scene->itemAt(scenePos, transform());
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

            // Select items in current layer only
            LayerGraphicsGroup* currentLayer = getLayer(m_currentLayerIndex);
            if (currentLayer) {
                QPainterPath path;
                path.addPolygon(selectionArea);

                QList<QGraphicsItem*> layerItems = currentLayer->childItems();
                for (QGraphicsItem* item : layerItems) {
                    if (item->flags() & QGraphicsItem::ItemIsSelectable &&
                        path.intersects(item->sceneBoundingRect())) {
                        item->setSelected(true);
                    }
                }
            }

            m_rubberBand->hide();
        }
        QGraphicsView::mouseReleaseEvent(event);
    }

    emit mousePositionChanged(scenePos);
}

void Canvas::drawBackground(QPainter* painter, const QRectF& rect)
{
    // Draw the background color first
    QGraphicsView::drawBackground(painter, rect);

    // Draw canvas bounds
    drawCanvasBounds(painter, rect);

    if (m_gridVisible) {
        drawGrid(painter, rect);
    }
}

void Canvas::drawCanvasBounds(QPainter* painter, const QRectF& rect)
{
    painter->save();

    // Draw canvas border
    QPen borderPen(QColor(150, 150, 150), 2);
    painter->setPen(borderPen);
    painter->drawRect(m_canvasRect);

    // Draw shadow effect
    painter->setPen(QPen(QColor(30, 30, 30), 1));
    painter->drawRect(m_canvasRect.adjusted(3, 3, 3, 3));

    painter->restore();
}

void Canvas::drawGrid(QPainter* painter, const QRectF& rect)
{
    painter->save();

    QPen gridPen(QColor(96, 96, 96), 0.5);
    painter->setPen(gridPen);

    // Only draw grid within canvas bounds
    QRectF gridRect = rect.intersected(m_canvasRect);

    double left = int(gridRect.left()) - (int(gridRect.left()) % int(m_gridSize));
    double top = int(gridRect.top()) - (int(gridRect.top()) % int(m_gridSize));

    QVector<QLineF> lines;

    // Vertical lines
    for (double x = left; x < gridRect.right(); x += m_gridSize) {
        if (x >= m_canvasRect.left() && x <= m_canvasRect.right()) {
            lines.append(QLineF(x, gridRect.top(), x, gridRect.bottom()));
        }
    }

    // Horizontal lines
    for (double y = top; y < gridRect.bottom(); y += m_gridSize) {
        if (y >= m_canvasRect.top() && y <= m_canvasRect.bottom()) {
            lines.append(QLineF(gridRect.left(), y, gridRect.right(), y));
        }
    }

    painter->drawLines(lines);
    painter->restore();
}

// ... [Rest of the zoom, alignment, and utility methods remain similar] ...

void Canvas::onSceneSelectionChanged()
{
    emit selectionChanged();
}

// Implement remaining methods with layer awareness...
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

bool Canvas::isGridVisible() const
{
    return m_gridVisible;
}

bool Canvas::isSnapToGrid() const
{
    return m_snapToGrid;
}

bool Canvas::areRulersVisible() const
{
    return m_rulersVisible;
}

void Canvas::groupSelectedItems()
{
    if (!m_scene) return;

    QList<QGraphicsItem*> selectedItems = m_scene->selectedItems();
    if (selectedItems.count() > 1) {
        QGraphicsItemGroup* group = m_scene->createItemGroup(selectedItems);
        group->setFlag(QGraphicsItem::ItemIsSelectable, true);
        group->setFlag(QGraphicsItem::ItemIsMovable, true);

        // Add group to current layer
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
        case 0: // AlignLeft
            newPos.setX(boundingRect.left() - itemRect.left() + item->scenePos().x());
            break;
        case 1: // AlignCenter
            newPos.setX(boundingRect.center().x() - itemRect.center().x() + item->scenePos().x());
            break;
        case 2: // AlignRight
            newPos.setX(boundingRect.right() - itemRect.right() + item->scenePos().x());
            break;
        case 3: // AlignTop
            newPos.setY(boundingRect.top() - itemRect.top() + item->scenePos().y());
            break;
        case 4: // AlignMiddle
            newPos.setY(boundingRect.center().y() - itemRect.center().y() + item->scenePos().y());
            break;
        case 5: // AlignBottom
            newPos.setY(boundingRect.bottom() - itemRect.bottom() + item->scenePos().y());
            break;
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

    // Find the highest Z value in current layer
    LayerGraphicsGroup* currentLayer = getLayer(m_currentLayerIndex);
    if (currentLayer) {
        QList<QGraphicsItem*> layerItems = currentLayer->childItems();
        for (QGraphicsItem* item : layerItems) {
            if (item->zValue() > maxZ) {
                maxZ = item->zValue();
            }
        }
    }

    // Move selected items to front
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

    // Find the lowest Z value in current layer
    LayerGraphicsGroup* currentLayer = getLayer(m_currentLayerIndex);
    if (currentLayer) {
        QList<QGraphicsItem*> layerItems = currentLayer->childItems();
        for (QGraphicsItem* item : layerItems) {
            if (item->zValue() < minZ) {
                minZ = item->zValue();
            }
        }
    }

    // Move selected items to back
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
    if (!m_snapToGrid) {
        return point;
    }

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
        // Zoom with Ctrl+Wheel
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
    // Implementation for rulers
    painter->save();

    QPen rulerPen(QColor(200, 200, 200));
    painter->setPen(rulerPen);
    painter->setFont(QFont("Arial", 8));

    // Draw ruler backgrounds
    QRectF viewRect = mapToScene(viewport()->rect()).boundingRect();

    // Top ruler
    painter->fillRect(0, 0, viewport()->width(), 20, QColor(80, 80, 80));

    // Left ruler  
    painter->fillRect(0, 20, 20, viewport()->height() - 20, QColor(80, 80, 80));

    painter->restore();
}