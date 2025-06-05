// Canvas.cpp - Robustly fixed layer system with better state tracking

#include "Canvas.h"
#include "Tools/Tool.h"
#include "MainWindow.h"
#include "Tools/SelectionTool.h"
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

// ROBUST: Enhanced layer data structure with better state management
struct LayerData {
    QString name;
    QString uuid;  // Unique identifier to prevent mix-ups
    bool visible;
    bool locked;
    double opacity;
    QList<QGraphicsItem*> items;
    QHash<int, QList<QGraphicsItem*>> frameItems; // Per-frame item tracking
    QSet<QGraphicsItem*> allTimeItems; // All items ever added to this layer

    LayerData(const QString& layerName)
        : name(layerName), visible(true), locked(false), opacity(1.0) {
        // Generate unique ID to prevent layer confusion
        uuid = QString("layer_%1_%2").arg(layerName).arg(QDateTime::currentMSecsSinceEpoch());
    }

    void addItem(QGraphicsItem* item, int frame) {
        if (!item || allTimeItems.contains(item)) return;

        items.append(item);
        frameItems[frame].append(item);
        allTimeItems.insert(item);
    }

    void removeItem(QGraphicsItem* item) {
        if (!item) return;

        items.removeAll(item);
        allTimeItems.remove(item);

        // Remove from all frames
        for (auto& frameList : frameItems) {
            frameList.removeAll(item);
        }
    }

    bool containsItem(QGraphicsItem* item) const {
        return allTimeItems.contains(item);
    }

    void setFrameItems(int frame, const QList<QGraphicsItem*>& itemList) {
        frameItems[frame] = itemList;

        // Update allTimeItems to include all items from all frames
        for (QGraphicsItem* item : itemList) {
            if (item && !allTimeItems.contains(item)) {
                allTimeItems.insert(item);
            }
        }
    }

    QList<QGraphicsItem*> getFrameItems(int frame) const {
        return frameItems.value(frame, QList<QGraphicsItem*>());
    }

    void clearFrame(int frame) {
        frameItems.remove(frame);
    }

    void debugPrint() const {
        qDebug() << "Layer" << name << "UUID:" << uuid
            << "Items:" << items.size()
            << "Frames:" << frameItems.keys()
            << "AllTime:" << allTimeItems.size();
    }
};

// Canvas implementation with robust layer management
Canvas::Canvas(MainWindow* parent)
    : QGraphicsView(parent)
    , m_mainWindow(parent)
    , m_scene(nullptr)
    , m_destroying(false)
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
    , m_rubberBandActive(false)
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
    m_rubberBand = new QRubberBand(QRubberBand::Rectangle, this);
    m_rubberBand->hide();

    qDebug() << "Canvas created with robust layer system, size:" << m_canvasSize;
}

Canvas::~Canvas()
{
    qDebug() << "Canvas destructor called";

    // FIXED: Disconnect ALL signals immediately to prevent crashes during destruction
    disconnect();

    // FIXED: Disconnect scene signals specifically
    if (m_scene) {
        disconnect(m_scene, nullptr, this, nullptr);
    }

    // FIXED: Clear all data structures that reference graphics items WITHOUT emitting signals
    m_frameItems.clear();
    m_keyframes.clear();

    // Clear rubber band if it exists
    if (m_rubberBand) {
        delete m_rubberBand;
        m_rubberBand = nullptr;
    }

    // Clean up layer data structures
    for (void* layerPtr : m_layers) {
        if (layerPtr) {
            LayerData* layer = static_cast<LayerData*>(layerPtr);

            // Clear the item lists first (don't delete items, scene will handle that)
            layer->items.clear();
            layer->frameItems.clear();
            layer->allTimeItems.clear();

            // Delete the layer data structure
            delete layer;
        }
    }
    m_layers.clear();

    // FIXED: Don't call any methods that might emit signals
    // The scene and its items will be cleaned up by Qt automatically
    // when the QGraphicsView is destroyed

    qDebug() << "Canvas destructor completed";
}

void Canvas::setupScene()
{
    m_scene = new QGraphicsScene(this);
    m_canvasRect = QRectF(0, 0, m_canvasSize.width(), m_canvasSize.height());
    QRectF sceneRect = m_canvasRect.adjusted(-500, -500, 500, 500);
    m_scene->setSceneRect(sceneRect);
    m_scene->setBackgroundBrush(QBrush(QColor(64, 64, 64)));
    setScene(m_scene);

    // Create and lock the background rectangle
    if (!m_backgroundRect) {
        m_backgroundRect = new QGraphicsRectItem(m_canvasRect);
        m_backgroundRect->setBrush(QBrush(Qt::white));
        m_backgroundRect->setPen(QPen(Qt::black, 1));

        // Lock the background - make it non-selectable and non-movable
        m_backgroundRect->setFlag(QGraphicsItem::ItemIsSelectable, false);
        m_backgroundRect->setFlag(QGraphicsItem::ItemIsMovable, false);
        m_backgroundRect->setFlag(QGraphicsItem::ItemIsSelectable, false);

        // Put background at the bottom Z-order
        m_backgroundRect->setZValue(-1000);

        // Add a custom data flag to identify this as the background
        m_backgroundRect->setData(0, "background");

        m_scene->addItem(m_backgroundRect);
    }

    qDebug() << "Scene set up with canvas rect:" << m_canvasRect;
}
void Canvas::setupDefaultLayers()
{
    // ROBUST: Clear any existing layers completely
    for (void* layerPtr : m_layers) {
        if (layerPtr) {
            delete static_cast<LayerData*>(layerPtr);
        }
    }
    m_layers.clear();

    // Create background layer with unique identification
    LayerData* backgroundLayer = new LayerData("Background");
    m_layers.push_back(backgroundLayer);

    // Create drawing layer
    LayerData* drawingLayer = new LayerData("Layer 1");
    m_layers.push_back(drawingLayer);

    // REMOVED: Don't create another background rectangle here
    // The background rectangle is already created in setupScene()
    // Just add the existing background to the background layer
    if (m_backgroundRect) {
        backgroundLayer->addItem(m_backgroundRect, 1);
    }

    // Set Layer 1 as current (not background)
    setCurrentLayer(1);

    qDebug() << "Default layers created - Background UUID:" << backgroundLayer->uuid
        << "Drawing UUID:" << drawingLayer->uuid;
}

int Canvas::addLayer(const QString& name)
{
    QString layerName = name.isEmpty() ? QString("Layer %1").arg(m_layers.size()) : name;

    // ROBUST: Create new layer with unique identification
    LayerData* newLayer = new LayerData(layerName);
    m_layers.push_back(newLayer);

    int newIndex = m_layers.size() - 1;

    qDebug() << "Added layer:" << layerName << "Index:" << newIndex << "UUID:" << newLayer->uuid;
    emit layerChanged(newIndex);
    return newIndex;
}

void Canvas::removeLayer(int layerIndex)
{
    if (layerIndex >= 0 && layerIndex < m_layers.size() && m_layers.size() > 1) {
        LayerData* layer = static_cast<LayerData*>(m_layers[layerIndex]);

        qDebug() << "Removing layer" << layerIndex << "UUID:" << layer->uuid;

        // ROBUST: Remove all items in this layer from scene AND all frame tracking
        for (QGraphicsItem* item : layer->allTimeItems) {
            if (item && item != m_backgroundRect) {
                // Remove from scene
                if (m_scene->items().contains(item)) {
                    m_scene->removeItem(item);
                }

                // Remove from ALL frame tracking in ALL layers (safety measure)
                for (auto& frameEntry : m_frameItems) {
                    frameEntry.second.removeAll(item);
                }

                delete item;
            }
        }

        // Delete layer data
        delete layer;
        m_layers.erase(m_layers.begin() + layerIndex);

        // ROBUST: Adjust current layer carefully
        if (m_currentLayerIndex >= m_layers.size()) {
            m_currentLayerIndex = m_layers.size() - 1;
        }
        if (m_currentLayerIndex < 0 && !m_layers.empty()) {
            m_currentLayerIndex = 0;
        }

        // ROBUST: Recalculate Z-values for all remaining layers
        updateAllLayerZValues();

        storeCurrentFrameState();
        emit layerChanged(m_currentLayerIndex);

        qDebug() << "Layer removed. Current layer now:" << m_currentLayerIndex
            << "Total layers:" << m_layers.size();
    }
}

void Canvas::setCurrentLayer(int layerIndex)
{
    if (layerIndex >= 0 && layerIndex < m_layers.size()) {
        LayerData* oldLayer = (m_currentLayerIndex >= 0 && m_currentLayerIndex < m_layers.size())
            ? static_cast<LayerData*>(m_layers[m_currentLayerIndex]) : nullptr;
        LayerData* newLayer = static_cast<LayerData*>(m_layers[layerIndex]);

        m_currentLayerIndex = layerIndex;

        qDebug() << "Current layer changed from" << (oldLayer ? oldLayer->uuid : "none")
            << "to" << newLayer->uuid << "index:" << layerIndex;

        emit layerChanged(layerIndex);
    }
}

void Canvas::setLayerVisible(int layerIndex, bool visible)
{
    if (layerIndex >= 0 && layerIndex < m_layers.size()) {
        LayerData* layer = static_cast<LayerData*>(m_layers[layerIndex]);
        layer->visible = visible;

        // ROBUST: Update visibility of items in current frame only
        QList<QGraphicsItem*> currentFrameItems = layer->getFrameItems(m_currentFrame);
        for (QGraphicsItem* item : currentFrameItems) {
            if (item && m_scene->items().contains(item)) {
                item->setVisible(visible);
            }
        }

        qDebug() << "Layer" << layerIndex << "UUID:" << layer->uuid << "visibility set to:" << visible;
    }
}

void Canvas::setLayerLocked(int layerIndex, bool locked)
{
    if (layerIndex >= 0 && layerIndex < m_layers.size()) {
        LayerData* layer = static_cast<LayerData*>(m_layers[layerIndex]);
        layer->locked = locked;

        // ROBUST: Update lock state of items in current frame only
        QList<QGraphicsItem*> currentFrameItems = layer->getFrameItems(m_currentFrame);
        for (QGraphicsItem* item : currentFrameItems) {
            if (item && m_scene->items().contains(item)) {
                item->setFlag(QGraphicsItem::ItemIsSelectable, !locked);
                item->setFlag(QGraphicsItem::ItemIsMovable, !locked);
            }
        }

        qDebug() << "Layer" << layerIndex << "UUID:" << layer->uuid << "locked state set to:" << locked;
    }
}

void Canvas::moveLayer(int fromIndex, int toIndex)
{
    if (fromIndex < 0 || fromIndex >= m_layers.size() ||
        toIndex < 0 || toIndex >= m_layers.size() || fromIndex == toIndex)
        return;

    void* layerPtr = m_layers[fromIndex];
    m_layers.erase(m_layers.begin() + fromIndex);
    m_layers.insert(m_layers.begin() + toIndex, layerPtr);

    if (m_currentLayerIndex == fromIndex)
        m_currentLayerIndex = toIndex;
    else if (m_currentLayerIndex > fromIndex && m_currentLayerIndex <= toIndex)
        m_currentLayerIndex--;
    else if (m_currentLayerIndex < fromIndex && m_currentLayerIndex >= toIndex)
        m_currentLayerIndex++;

    updateAllLayerZValues();
    emit layerChanged(m_currentLayerIndex);
}


void Canvas::setLayerOpacity(int layerIndex, double opacity)
{
    if (layerIndex >= 0 && layerIndex < m_layers.size()) {
        LayerData* layer = static_cast<LayerData*>(m_layers[layerIndex]);
        layer->opacity = qBound(0.0, opacity, 1.0);

        // ROBUST: Update opacity of items in current frame only, preserve individual opacity
        QList<QGraphicsItem*> currentFrameItems = layer->getFrameItems(m_currentFrame);
        for (QGraphicsItem* item : currentFrameItems) {
            if (item && m_scene->items().contains(item)) {
                // Get individual item opacity and combine with layer opacity
                double individualOpacity = item->data(0).toDouble(); // Store individual opacity in data(0)
                if (individualOpacity == 0.0) individualOpacity = 1.0; // Default if not set

                item->setOpacity(individualOpacity * layer->opacity);
            }
        }

        storeCurrentFrameState();
        qDebug() << "Layer" << layerIndex << "UUID:" << layer->uuid << "opacity set to:" << layer->opacity;
    }
}

void Canvas::addItemToCurrentLayer(QGraphicsItem* item)
{
    if (!item || m_currentLayerIndex < 0 || m_currentLayerIndex >= m_layers.size()) {
        qDebug() << "addItemToCurrentLayer: Invalid item or layer index";
        return;
    }

    // FIXED: Flash-like behavior - check if this is a tweened frame
    if (hasTweening(m_currentLayerIndex, m_currentFrame)) {
        // In Flash, you can only edit the last frame of a tween
        int tweenStart = -1;
        int tweenEnd = -1;

        // Find the tween span for this frame
        auto layerIt = m_layerFrameData.find(m_currentLayerIndex);
        if (layerIt != m_layerFrameData.end()) {
            auto frameIt = layerIt->second.find(m_currentFrame);
            if (frameIt != layerIt->second.end()) {
                tweenStart = frameIt->second.tweenStartFrame;
                tweenEnd = frameIt->second.tweenEndFrame;
            }
        }

        // Only allow editing the last frame of the tween (Flash behavior)
        if (m_currentFrame != tweenEnd) {
            QMessageBox::information(m_mainWindow, "Tweening Active",
                QString("Cannot edit frame %1. This frame is part of a tween.\n"
                    "You can only edit the last frame (%2) of a tween, or remove the tween first.")
                .arg(m_currentFrame).arg(tweenEnd));
            return;
        }
    }

    // Add to scene first
    if (!item->scene()) {
        m_scene->addItem(item);
    }

    LayerData* currentLayer = static_cast<LayerData*>(m_layers[m_currentLayerIndex]);

    // ROBUST: Check if item is already in another layer and remove it
    for (int i = 0; i < m_layers.size(); ++i) {
        if (i != m_currentLayerIndex) {
            LayerData* otherLayer = static_cast<LayerData*>(m_layers[i]);
            if (otherLayer->containsItem(item)) {
                qDebug() << "Warning: Moving item from layer" << i << "to layer" << m_currentLayerIndex;
                otherLayer->removeItem(item);
            }
        }
    }

    // Add item to current layer and frame
    currentLayer->addItem(item, m_currentFrame);

    // Set properties based on layer state
    item->setZValue(m_currentLayerIndex * 1000);
    item->setFlag(QGraphicsItem::ItemIsSelectable, !currentLayer->locked);
    item->setFlag(QGraphicsItem::ItemIsMovable, !currentLayer->locked);
    item->setVisible(currentLayer->visible);

    // ROBUST: Store individual opacity before applying layer opacity
    if (item->data(0).toDouble() == 0.0) {
        item->setData(0, item->opacity()); // Store original individual opacity
    }
    double individualOpacity = item->data(0).toDouble();
    item->setOpacity(individualOpacity * currentLayer->opacity);

    storeCurrentFrameState();

    qDebug() << "Added item to layer:" << m_currentLayerIndex
        << "Frame:" << m_currentFrame;
}

void Canvas::saveFrameState(int frame)
{
    if (frame < 1) return;

    QList<QGraphicsItem*> frameItems;

    // Collect all relevant items
    for (QGraphicsItem* item : m_scene->items()) {
        if (item != m_backgroundRect && item->zValue() > -999) {
            frameItems.append(item);
        }
    }

    m_frameItems[frame] = frameItems;

    // If this frame has data, preserve its type, otherwise mark as having content
    if (m_frameData.find(frame) == m_frameData.end()) {
        // New frame with content - make it a keyframe if it's not extending from another
        if (!frameItems.isEmpty()) {
            m_frameData[frame].type = FrameType::Keyframe;
            m_frameData[frame].sourceKeyframe = frame;
            m_keyframes.insert(frame);
        }
    }

    m_frameData[frame].items = frameItems;
}

// Existing loadFrameState method - enhanced but compatible
void Canvas::loadFrameState(int frame)
{
    // Clear current scene content (except background)
    QList<QGraphicsItem*> itemsToRemove;
    for (QGraphicsItem* item : m_scene->items()) {
        if (item != m_backgroundRect && item->zValue() > -999) {
            itemsToRemove.append(item);
        }
    }

    for (QGraphicsItem* item : itemsToRemove) {
        m_scene->removeItem(item);
        // Don't delete items here - they might be used in other frames
    }

    // Load frame content
    auto frameIt = m_frameItems.find(frame);
    if (frameIt != m_frameItems.end()) {
        for (QGraphicsItem* item : frameIt->second) {
            if (item && !m_scene->items().contains(item)) {
                m_scene->addItem(item);
            }
        }
    }
}

int Canvas::getItemLayerIndex(QGraphicsItem* item)
{
    if (!item) return -1;

    // ROBUST: Find which layer an item belongs to using UUID tracking
    for (int i = 0; i < m_layers.size(); ++i) {
        LayerData* layer = static_cast<LayerData*>(m_layers[i]);
        if (layer->containsItem(item)) {
            return i;
        }
    }

    qDebug() << "Warning: Item not found in any layer!";
    return -1; // Item not found in any layer
}

void Canvas::updateAllLayerZValues()
{
    // ROBUST: Update Z-values to match layer order and prevent Z-fighting
    for (int i = 0; i < m_layers.size(); ++i) {
        LayerData* layer = static_cast<LayerData*>(m_layers[i]);
        int baseZValue = i * 1000;

        for (QGraphicsItem* item : layer->allTimeItems) {
            if (item && item != m_backgroundRect && m_scene->items().contains(item)) {
                item->setZValue(baseZValue + (static_cast<int>(item->zValue()) % 1000));
            }
        }
    }
}

void Canvas::deleteSelected()
{
    if (!m_scene) return;

    QList<QGraphicsItem*> selectedItems = m_scene->selectedItems();
    if (selectedItems.isEmpty()) return;

    qDebug() << "Deleting" << selectedItems.size() << "selected items";

    // Remove deleted items from layer tracking AND frame states
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

        // Remove from scene (let scene handle actual deletion)
        m_scene->removeItem(item);
    }

    storeCurrentFrameState();
    emit selectionChanged();

    qDebug() << "deleteSelected completed";
}

void Canvas::clear()
{
    if (m_scene) {
        qDebug() << "Canvas::clear() called";

        // FIXED: Clear data structures first before removing items
        m_frameItems.clear();
        m_keyframes.clear();

        // Clear all layer data except background
        for (int i = m_layers.size() - 1; i >= 0; --i) {
            LayerData* layer = static_cast<LayerData*>(m_layers[i]);

            // Remove all items except background from layer tracking
            QList<QGraphicsItem*> itemsToRemove;
            for (QGraphicsItem* item : layer->items) {
                if (item != m_backgroundRect) {
                    itemsToRemove.append(item);
                }
            }

            // Remove items from layer tracking first
            for (QGraphicsItem* item : itemsToRemove) {
                layer->items.removeAll(item);
            }

            // Delete layer data (except background layer)
            if (i > 0) {
                delete layer;
                m_layers.erase(m_layers.begin() + i);
            }
        }

        // FIXED: Let the scene handle item deletion
        // Get all items except background
        QList<QGraphicsItem*> allItems = m_scene->items();
        for (QGraphicsItem* item : allItems) {
            if (item != m_backgroundRect) {
                m_scene->removeItem(item);
                // Let the scene delete the items automatically
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

        qDebug() << "Canvas::clear() completed";
    }
}

// [All other Canvas methods remain the same as in the original...]

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

        // FIXED: Properly handle tweened frames
        for (int layer = 0; layer < getLayerCount(); ++layer) {
            if (hasTweening(layer, frame)) {
                calculateTweenedFrame(layer, frame);
            }
        }

        loadFrameState(frame);
        emit frameChanged(frame);
        qDebug() << "Current frame set to:" << frame;
    }
}

int Canvas::getCurrentFrame() const { return m_currentFrame; }


void Canvas::createKeyframe(int frame)
{
    if (frame < 1) return;

    qDebug() << "Creating keyframe at frame" << frame;

    // Capture current scene state as a keyframe
    captureCurrentStateAsKeyframe(frame);

    // Mark as keyframe
    m_keyframes.insert(frame);
    m_frameData[frame].type = FrameType::Keyframe;
    m_frameData[frame].sourceKeyframe = frame; // Points to itself

    // Store current items
    QList<QGraphicsItem*> currentItems;
    for (QGraphicsItem* item : m_scene->items()) {
        if (item->zValue() > -999 && (item->flags() & QGraphicsItem::ItemIsSelectable)) {
            currentItems.append(item);
        }
    }
    m_frameData[frame].items = currentItems;

    storeCurrentFrameState();
    emit keyframeCreated(frame);

    qDebug() << "Keyframe created at frame" << frame << "with" << currentItems.size() << "items";
}

void Canvas::createBlankKeyframe(int frame)
{
    if (frame < 1) return;

    qDebug() << "Creating blank keyframe at frame" << frame;

    // Clear current content
    QList<QGraphicsItem*> itemsToRemove;
    for (QGraphicsItem* item : m_scene->items()) {
        if (item != m_backgroundRect && item->zValue() > -999) {
            itemsToRemove.append(item);
        }
    }

    for (QGraphicsItem* item : itemsToRemove) {
        m_scene->removeItem(item);
        delete item;
    }

    // Mark as blank keyframe
    m_keyframes.insert(frame);
    m_frameData[frame].type = FrameType::Keyframe;
    m_frameData[frame].sourceKeyframe = frame;
    m_frameData[frame].items.clear();

    storeCurrentFrameState();
    emit keyframeCreated(frame);

    qDebug() << "Blank keyframe created at frame" << frame;
}


void Canvas::createExtendedFrame(int frame)
{
    if (frame < 1) return;

    qDebug() << "Creating extended frame at frame" << frame;

    // Find the last keyframe before this frame
    int sourceKeyframe = getLastKeyframeBefore(frame);
    if (sourceKeyframe == -1) {
        // No previous keyframe, create as blank keyframe instead
        qDebug() << "No previous keyframe found, creating blank keyframe";
        createBlankKeyframe(frame);
        return;
    }

    qDebug() << "Source keyframe found at frame" << sourceKeyframe;

    // AUTOMATIC SPAN CALCULATION: Fill the gap between source keyframe and target frame
    for (int f = sourceKeyframe + 1; f <= frame; f++) {
        // Skip if frame already has content on this layer
        if (hasContent(f, getCurrentLayer())) {
            qDebug() << "Frame" << f << "already has content on current layer, skipping";
            continue;
        }

        qDebug() << "Creating extended frame data for frame" << f;

        // Create extended frame data
        m_frameData[f].type = FrameType::ExtendedFrame;
        m_frameData[f].sourceKeyframe = sourceKeyframe;

        // PERFORMANCE FIX: Reference the source frame items for both data structures
        // This ensures compatibility with loadFrameState()
        if (m_frameItems.find(sourceKeyframe) != m_frameItems.end()) {
            // Reference the same item list (no duplication)
            m_frameData[f].items = m_frameItems[sourceKeyframe];
            m_frameItems[f] = m_frameItems[sourceKeyframe];  // CRITICAL: Update compatibility layer
        }

        // Copy item states for potential tweening
        if (m_frameData.find(sourceKeyframe) != m_frameData.end()) {
            m_frameData[f].itemStates = m_frameData[sourceKeyframe].itemStates;
        }
    }

    // Load the target frame to display the content
    setCurrentFrame(frame);

    qDebug() << "Extended frames created from" << sourceKeyframe + 1 << "to" << frame;

    // Single emit for the entire span
    emit frameExtended(sourceKeyframe, frame);
}
// Canvas.cpp - Implementation of clearCurrentFrameContent method

void Canvas::clearCurrentFrameContent()
{
    qDebug() << "Clearing current frame content at frame" << m_currentFrame;

    // Get all items that should be cleared (exclude background and UI elements)
    QList<QGraphicsItem*> itemsToRemove;
    for (QGraphicsItem* item : m_scene->items()) {
        if (item != m_backgroundRect &&
            item->zValue() > -999 &&
            (item->flags() & QGraphicsItem::ItemIsSelectable)) {
            itemsToRemove.append(item);
        }
    }

    // Remove items from scene and delete them
    for (QGraphicsItem* item : itemsToRemove) {
        m_scene->removeItem(item);
        delete item;
    }

    // Update frame data to reflect empty frame
    if (m_frameData.find(m_currentFrame) != m_frameData.end()) {
        m_frameData[m_currentFrame].items.clear();
        m_frameData[m_currentFrame].itemStates.clear();

        // If this was an extended frame, convert it to empty
        if (m_frameData[m_currentFrame].type == FrameType::ExtendedFrame) {
            m_frameData[m_currentFrame].type = FrameType::Empty;
            m_frameData[m_currentFrame].sourceKeyframe = -1;
        }
        // If this was a keyframe, keep it as keyframe but empty
        else if (m_frameData[m_currentFrame].type == FrameType::Keyframe) {
            // Keep as keyframe but with no content
            m_keyframes.erase(m_currentFrame); // Remove from keyframes set if empty
        }
    }

    // Clear frame items for compatibility
    m_frameItems[m_currentFrame].clear();

    // Store the cleared state
    storeCurrentFrameState();

    qDebug() << "Frame" << m_currentFrame << "content cleared," << itemsToRemove.size() << "items removed";
}
void Canvas::copyItemsToFrame(int fromFrame, int toFrame)
{
    if (m_frameData.find(fromFrame) == m_frameData.end()) {
        qDebug() << "Source frame" << fromFrame << "has no data";
        return;
    }

    // Clear current scene
    QList<QGraphicsItem*> currentItems;
    for (QGraphicsItem* item : m_scene->items()) {
        if (item != m_backgroundRect && item->zValue() > -999) {
            currentItems.append(item);
        }
    }

    for (QGraphicsItem* item : currentItems) {
        m_scene->removeItem(item);
        delete item;
    }

    // Load and duplicate items from source frame
    loadFrameState(fromFrame);

    QList<QGraphicsItem*> sourceItems;
    for (QGraphicsItem* item : m_scene->items()) {
        if (item != m_backgroundRect && item->zValue() > -999) {
            sourceItems.append(item);
        }
    }

    // The items are now loaded, store them for the target frame
    m_frameData[toFrame].items = sourceItems;
}

void Canvas::captureCurrentStateAsKeyframe(int frame)
{
    // Store current item states for potential tweening
    QMap<QGraphicsItem*, QVariant> itemStates;

    for (QGraphicsItem* item : m_scene->items()) {
        if (item != m_backgroundRect && item->zValue() > -999) {
            QVariantMap state;
            state["position"] = item->pos();
            state["rotation"] = item->rotation();
            state["scale"] = item->scale();
            state["opacity"] = item->opacity();
            state["visible"] = item->isVisible();
            state["zValue"] = item->zValue();
            state["transform"] = item->transform();

            // Store type-specific properties
            if (auto rectItem = qgraphicsitem_cast<QGraphicsRectItem*>(item)) {
                state["rect"] = rectItem->rect();
                state["pen"] = QVariant::fromValue(rectItem->pen());
                state["brush"] = QVariant::fromValue(rectItem->brush());
            }
            else if (auto ellipseItem = qgraphicsitem_cast<QGraphicsEllipseItem*>(item)) {
                state["rect"] = ellipseItem->rect();
                state["pen"] = QVariant::fromValue(ellipseItem->pen());
                state["brush"] = QVariant::fromValue(ellipseItem->brush());
            }
            else if (auto lineItem = qgraphicsitem_cast<QGraphicsLineItem*>(item)) {
                state["line"] = lineItem->line();
                state["pen"] = QVariant::fromValue(lineItem->pen());
            }
            else if (auto pathItem = qgraphicsitem_cast<QGraphicsPathItem*>(item)) {
                state["path"] = QVariant::fromValue(pathItem->path());
                state["pen"] = QVariant::fromValue(pathItem->pen());
                state["brush"] = QVariant::fromValue(pathItem->brush());
            }
            else if (auto textItem = qgraphicsitem_cast<QGraphicsTextItem*>(item)) {
                state["text"] = textItem->toPlainText();
                state["font"] = textItem->font();
                state["color"] = textItem->defaultTextColor();
            }

            itemStates[item] = state;
        }
    }

    m_frameData[frame].itemStates = itemStates;
}

FrameType Canvas::getFrameType(int frame, int layer) const
{
    auto it = m_frameData.find(frame);
    if (it != m_frameData.end()) {
        return it->second.type;
    }
    return FrameType::Empty;
}

int Canvas::getSourceKeyframe(int frame) const
{
    auto it = m_frameData.find(frame);
    if (it != m_frameData.end()) {
        return it->second.sourceKeyframe;
    }
    return -1;
}

int Canvas::getLastKeyframeBefore(int frame) const
{
    int lastKeyframe = -1;

    for (int keyframe : m_keyframes) {
        if (keyframe < frame && keyframe > lastKeyframe) {
            lastKeyframe = keyframe;
        }
    }

    return lastKeyframe;
}

int Canvas::getNextKeyframeAfter(int frame) const
{
    int nextKeyframe = -1;

    for (int keyframe : m_keyframes) {
        if (keyframe > frame && (nextKeyframe == -1 || keyframe < nextKeyframe)) {
            nextKeyframe = keyframe;
        }
    }

    return nextKeyframe;
}

QList<int> Canvas::getFrameSpan(int keyframe) const
{
    QList<int> span;

    if (m_keyframes.find(keyframe) == m_keyframes.end()) {
        return span; // Not a keyframe
    }

    span.append(keyframe);

    // Find all extended frames that reference this keyframe
    for (const auto& pair : m_frameData) {
        if (pair.second.type == FrameType::ExtendedFrame &&
            pair.second.sourceKeyframe == keyframe) {
            span.append(pair.first);
        }
    }

    std::sort(span.begin(), span.end());
    return span;
}

bool Canvas::hasContent(int frame, int layer) const
{
    auto layerIt = m_layerFrameData.find(layer);
    if (layerIt != m_layerFrameData.end()) {
        auto frameIt = layerIt->second.find(frame);
        if (frameIt != layerIt->second.end()) {
            return frameIt->second.type != FrameType::Empty;
        }
    }
    return false;
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

// [Include all other existing methods that weren't changed...]

// [Mouse events, zoom methods, alignment methods, etc. remain the same...]
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
        // Check if we're using SelectionTool and clicking on empty space
        SelectionTool* selectionTool = dynamic_cast<SelectionTool*>(m_currentTool);
        if (selectionTool && event->button() == Qt::LeftButton) {
            QGraphicsItem* item = m_scene->itemAt(scenePos, transform());
            if (!item || item == m_backgroundRect) {
                // Start rubber band selection
                m_rubberBandOrigin = event->pos();
                if (!m_rubberBand) {
                    m_rubberBand = new QRubberBand(QRubberBand::Rectangle, this);
                }
                m_rubberBand->setGeometry(QRect(m_rubberBandOrigin, QSize()));
                m_rubberBand->show();
                m_rubberBandActive = true;

                // Clear selection if not holding Ctrl
                if (!(event->modifiers() & Qt::ControlModifier)) {
                    m_scene->clearSelection();
                }
            }
        }

        m_currentTool->mousePressEvent(event, scenePos);
    }
    else {
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

    // Handle rubber band selection
    if (m_rubberBandActive && m_rubberBand && m_rubberBand->isVisible()) {
        QRect rect = QRect(m_rubberBandOrigin, event->pos()).normalized();
        m_rubberBand->setGeometry(rect);
    }

    if (m_currentTool) {
        m_currentTool->mouseMoveEvent(event, scenePos);
    }
    else {
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

    // Handle rubber band selection completion
    if (m_rubberBandActive && m_rubberBand && m_rubberBand->isVisible() && event->button() == Qt::LeftButton) {
        QRect rect = m_rubberBand->geometry();
        QPolygonF selectionArea = mapToScene(rect);

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
        m_rubberBandActive = false;
    }

    if (m_currentTool) {
        m_currentTool->mouseReleaseEvent(event, scenePos);
    }
    else {
        QGraphicsView::mouseReleaseEvent(event);
    }

    emit mousePositionChanged(scenePos);
}

// [Include all other methods like drawing, zoom, etc...]

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
int Canvas::getCurrentLayer() const { return m_currentLayerIndex; }
int Canvas::getLayerCount() const { return m_layers.size(); }

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
    if (!m_destroying) {
        emit selectionChanged();
    }
}

void Canvas::zoomIn()
{
    double scaleFactor = 1.25;
    scale(scaleFactor, scaleFactor);
    m_zoomFactor *= scaleFactor;
    if (!m_destroying) {
        emit zoomChanged(m_zoomFactor);
    }
}

void Canvas::zoomOut()
{
    double scaleFactor = 1.0 / 1.25;
    scale(scaleFactor, scaleFactor);
    m_zoomFactor *= scaleFactor;
    if (!m_destroying) {
        emit zoomChanged(m_zoomFactor);
    }
}

void Canvas::zoomToFit()
{
    if (!m_scene) return;
    fitInView(m_canvasRect, Qt::KeepAspectRatio);
    m_zoomFactor = transform().m11();
    if (!m_destroying) {
        emit zoomChanged(m_zoomFactor);
    }
}

void Canvas::setZoomFactor(double factor)
{
    resetTransform();
    scale(factor, factor);
    m_zoomFactor = factor;
    if (!m_destroying) {
        emit zoomChanged(m_zoomFactor);
    }
}

double Canvas::getZoomFactor() const { return m_zoomFactor; }

void Canvas::setGridVisible(bool visible) { m_gridVisible = visible; viewport()->update(); }
void Canvas::setSnapToGrid(bool snap) { m_snapToGrid = snap; }
void Canvas::setRulersVisible(bool visible) { m_rulersVisible = visible; viewport()->update(); }
bool Canvas::isGridVisible() const { return m_gridVisible; }
bool Canvas::isSnapToGrid() const { return m_snapToGrid; }
bool Canvas::areRulersVisible() const { return m_rulersVisible; }

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
        if (!m_destroying) {
            emit zoomChanged(m_zoomFactor);
        }
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

// [Include all other alignment and object manipulation methods...]

void Canvas::groupSelectedItems()
{
    if (!m_scene) return;
    QList<QGraphicsItem*> selectedItems = m_scene->selectedItems();
    if (selectedItems.count() > 1) {
        QGraphicsItemGroup* group = m_scene->createItemGroup(selectedItems);
        group->setFlag(QGraphicsItem::ItemIsSelectable, true);
        group->setFlag(QGraphicsItem::ItemIsMovable, true);
        addItemToCurrentLayer(group);
        if (!m_destroying) {
            emit selectionChanged();
        }
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
    if (!m_destroying) {
        emit selectionChanged();
    }
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


void Canvas::convertExtendedFrameToKeyframe(int frame, int layer)
{
    if (!isExtendedFrame(frame, layer)) return;

    qDebug() << "Auto-converting extended frame to keyframe at frame" << frame << "layer" << layer;

    // Get the current frame data
    auto& frameData = m_layerFrameData[layer][frame];

    // Capture current scene state for this layer
    QList<QGraphicsItem*> layerItems;
    for (QGraphicsItem* item : m_scene->items()) {
        if (getItemLayerIndex(item) == layer &&
            item != m_backgroundRect &&
            item->zValue() > -999) {
            layerItems.append(item);
        }
    }

    // Convert to keyframe
    frameData.type = FrameType::Keyframe;
    frameData.sourceKeyframe = frame;  // Points to itself
    frameData.items = layerItems;

    // Capture item states
    QMap<QGraphicsItem*, QVariant> itemStates;
    for (QGraphicsItem* item : layerItems) {
        // Capture complete state for tweening
        QVariantMap state;
        state["position"] = item->pos();
        state["rotation"] = item->rotation();
        state["scale"] = item->scale();
        state["opacity"] = item->opacity();
        state["visible"] = item->isVisible();
        state["zValue"] = item->zValue();
        state["transform"] = QVariant::fromValue(item->transform());

        // Capture type-specific properties
        if (auto rectItem = qgraphicsitem_cast<QGraphicsRectItem*>(item)) {
            state["rect"] = rectItem->rect();
            state["pen"] = QVariant::fromValue(rectItem->pen());
            state["brush"] = QVariant::fromValue(rectItem->brush());
        }
        else if (auto ellipseItem = qgraphicsitem_cast<QGraphicsEllipseItem*>(item)) {
            state["rect"] = ellipseItem->rect();
            state["pen"] = QVariant::fromValue(ellipseItem->pen());
            state["brush"] = QVariant::fromValue(ellipseItem->brush());
        }
        else if (auto lineItem = qgraphicsitem_cast<QGraphicsLineItem*>(item)) {
            state["line"] = lineItem->line();
            state["pen"] = QVariant::fromValue(lineItem->pen());
        }
        else if (auto pathItem = qgraphicsitem_cast<QGraphicsPathItem*>(item)) {
            state["path"] = QVariant::fromValue(pathItem->path());
            state["pen"] = QVariant::fromValue(pathItem->pen());
            state["brush"] = QVariant::fromValue(pathItem->brush());
        }
        else if (auto textItem = qgraphicsitem_cast<QGraphicsTextItem*>(item)) {
            state["text"] = textItem->toPlainText();
            state["font"] = textItem->font();
            state["color"] = textItem->defaultTextColor();
        }

        itemStates[item] = state;
    }
    frameData.itemStates = itemStates;

    // Add to keyframes set
    m_keyframes.insert(frame);

    // Update compatibility layer
    m_frameItems[frame] = layerItems;
    storeCurrentFrameState();

    emit frameAutoConverted(frame, layer);

    qDebug() << "Extended frame auto-converted to keyframe with" << layerItems.size() << "items";
}

bool Canvas::canDrawOnFrame(int frame, int layer) const
{
    // Can't draw on tweened frames unless it's the last frame of the tween
    if (hasTweening(layer, frame)) {
        auto layerIt = m_layerFrameData.find(layer);
        if (layerIt != m_layerFrameData.end()) {
            auto frameIt = layerIt->second.find(frame);
            if (frameIt != layerIt->second.end()) {
                // In Flash, you can only edit the last frame of a tween
                return frame == frameIt->second.tweenEndFrame;
            }
        }
        return false;
    }

    // Can draw on keyframes and empty frames
    // Extended frames will be auto-converted when drawn on
    return true;
}

// NEW: Layer-specific tweening application
void Canvas::applyTweening(int layer, int startFrame, int endFrame, TweenType type)
{
    if (startFrame >= endFrame || !hasKeyframe(startFrame) || !hasKeyframe(endFrame)) {
        qDebug() << "Cannot apply tweening: invalid range or missing keyframes";
        qDebug() << "Start frame" << startFrame << "has keyframe:" << hasKeyframe(startFrame);
        qDebug() << "End frame" << endFrame << "has keyframe:" << hasKeyframe(endFrame);
        return;
    }

    qDebug() << "Applying" << static_cast<int>(type) << "tweening to layer" << layer
        << "from frame" << startFrame << "to" << endFrame;

    // Ensure layer data exists
    auto& layerData = m_layerFrameData[layer];

    // First, capture the state of start and end keyframes
    captureCurrentStateAsKeyframe(startFrame);
    captureCurrentStateAsKeyframe(endFrame);

    // Mark start and end frames as having tweening
    layerData[startFrame].hasTweening = true;
    layerData[startFrame].tweenType = type;
    layerData[startFrame].tweenStartFrame = startFrame;
    layerData[startFrame].tweenEndFrame = endFrame;
    layerData[startFrame].easingType = QEasingCurve::Linear;

    layerData[endFrame].hasTweening = true;
    layerData[endFrame].tweenType = type;
    layerData[endFrame].tweenStartFrame = startFrame;
    layerData[endFrame].tweenEndFrame = endFrame;
    layerData[endFrame].easingType = QEasingCurve::Linear;

    // Mark all frames in between as tweened
    for (int frame = startFrame + 1; frame < endFrame; ++frame) {
        auto& frameData = layerData[frame];
        frameData.type = FrameType::TweenedFrame;
        frameData.tweenType = type;
        frameData.hasTweening = true;
        frameData.tweenStartFrame = startFrame;
        frameData.tweenEndFrame = endFrame;
        frameData.easingType = QEasingCurve::Linear;
        frameData.sourceKeyframe = startFrame;

        // Ensure the frame has the extended frame items from the start
        if (frameData.items.isEmpty() && m_frameItems.find(startFrame) != m_frameItems.end()) {
            frameData.items = m_frameItems[startFrame];
            frameData.itemStates = layerData[startFrame].itemStates;
        }
    }

    // Update compatibility layer
    for (int frame = startFrame; frame <= endFrame; ++frame) {
        if (m_frameItems.find(frame) == m_frameItems.end() &&
            m_frameItems.find(startFrame) != m_frameItems.end()) {
            m_frameItems[frame] = m_frameItems[startFrame];
        }
    }

    storeCurrentFrameState();
    emit tweeningApplied(layer, startFrame, endFrame, type);

    // Refresh current frame if it's in the tweened range
    if (m_currentFrame >= startFrame && m_currentFrame <= endFrame) {
        calculateTweenedFrame(layer, m_currentFrame);
        viewport()->update();
    }

    qDebug() << "Tweening applied successfully";
}

// NEW: Calculate interpolated frame content
void Canvas::calculateTweenedFrame(int layer, int frame)
{
    if (!isTweenedFrame(frame, layer)) return;

    auto layerIt = m_layerFrameData.find(layer);
    if (layerIt == m_layerFrameData.end()) return;

    auto frameIt = layerIt->second.find(frame);
    if (frameIt == layerIt->second.end()) return;

    const auto& frameData = frameIt->second;
    int startFrame = frameData.tweenStartFrame;
    int endFrame = frameData.tweenEndFrame;

    if (startFrame == -1 || endFrame == -1) return;

    // Calculate interpolation factor (0.0 to 1.0)
    double t = static_cast<double>(frame - startFrame) / (endFrame - startFrame);

    // Apply easing curve
    QEasingCurve easingCurve(frameData.easingType);
    t = easingCurve.valueForProgress(t);

    qDebug() << "Calculating tweened frame" << frame << "with t=" << t
        << "between frames" << startFrame << "and" << endFrame;

    // Update items for this frame based on interpolation
    interpolateItemsAtFrame(layer, frame, t);
}

// FIXED: Enhanced interpolateItemsAtFrame method
void Canvas::interpolateItemsAtFrame(int layer, int frame, double t)
{
    auto layerIt = m_layerFrameData.find(layer);
    if (layerIt == m_layerFrameData.end()) return;

    auto frameIt = layerIt->second.find(frame);
    if (frameIt == layerIt->second.end()) return;

    const auto& frameData = frameIt->second;
    int startFrame = frameData.tweenStartFrame;
    int endFrame = frameData.tweenEndFrame;

    // Get start and end frame data
    auto startFrameIt = layerIt->second.find(startFrame);
    auto endFrameIt = layerIt->second.find(endFrame);

    if (startFrameIt == layerIt->second.end() || endFrameIt == layerIt->second.end()) {
        return;
    }

    const auto& startFrameData = startFrameIt->second;
    const auto& endFrameData = endFrameIt->second;

    // Only update items that belong to this layer and are currently in the scene
    for (QGraphicsItem* item : m_scene->items()) {
        if (getItemLayerIndex(item) != layer) continue;

        // Check if item exists in both start and end frames
        if (!startFrameData.itemStates.contains(item) ||
            !endFrameData.itemStates.contains(item)) continue;

        QVariantMap startState = startFrameData.itemStates[item].toMap();
        QVariantMap endState = endFrameData.itemStates[item].toMap();

        // Interpolate position
        QPointF startPos = startState["position"].toPointF();
        QPointF endPos = endState["position"].toPointF();
        QPointF newPos = startPos + (endPos - startPos) * t;
        item->setPos(newPos);

        // Interpolate rotation
        double startRot = startState["rotation"].toDouble();
        double endRot = endState["rotation"].toDouble();
        double newRot = startRot + (endRot - startRot) * t;
        item->setRotation(newRot);

        // Interpolate scale
        double startScale = startState["scale"].toDouble();
        double endScale = endState["scale"].toDouble();
        double newScale = startScale + (endScale - startScale) * t;
        item->setScale(newScale);

        // Interpolate opacity
        double startOpacity = startState["opacity"].toDouble();
        double endOpacity = endState["opacity"].toDouble();
        double newOpacity = startOpacity + (endOpacity - startOpacity) * t;
        item->setOpacity(newOpacity);

        qDebug() << "Interpolated item at t=" << t << "pos:" << newPos << "rotation:" << newRot;
    }
}

bool Canvas::hasTweening(int layer, int frame) const
{
    auto layerIt = m_layerFrameData.find(layer);
    if (layerIt != m_layerFrameData.end()) {
        auto frameIt = layerIt->second.find(frame);
        if (frameIt != layerIt->second.end()) {
            return frameIt->second.hasTweening;
        }
    }
    return false;
}


TweenType Canvas::getTweenType(int layer, int frame) const
{
    auto layerIt = m_layerFrameData.find(layer);
    if (layerIt != m_layerFrameData.end()) {
        auto frameIt = layerIt->second.find(frame);
        if (frameIt != layerIt->second.end()) {
            return frameIt->second.tweenType;
        }
    }
    return TweenType::None;
}

QList<int> Canvas::getTweeningFrames(int layer, int startFrame, int endFrame) const
{
    QList<int> frames;
    auto layerIt = m_layerFrameData.find(layer);
    if (layerIt != m_layerFrameData.end()) {
        for (int frame = startFrame; frame <= endFrame; ++frame) {
            auto frameIt = layerIt->second.find(frame);
            if (frameIt != layerIt->second.end() && frameIt->second.hasTweening) {
                frames.append(frame);
            }
        }
    }
    return frames;
}

void Canvas::removeTweening(int layer, int startFrame, int endFrame)
{
    qDebug() << "Removing tweening from layer" << layer << "frames" << startFrame << "to" << endFrame;

    auto& layerData = m_layerFrameData[layer];

    // Remove tweening from all frames in the range
    for (int frame = startFrame; frame <= endFrame; ++frame) {
        auto& frameData = layerData[frame];
        frameData.hasTweening = false;
        frameData.tweenType = TweenType::None;
        frameData.tweenStartFrame = -1;
        frameData.tweenEndFrame = -1;
        frameData.easingType = QEasingCurve::Linear;
    }

    // Convert tweened frames back to extended frames
    for (int frame = startFrame + 1; frame < endFrame; ++frame) {
        auto& frameData = layerData[frame];
        if (frameData.type == FrameType::TweenedFrame) {
            frameData.type = FrameType::ExtendedFrame;
            frameData.sourceKeyframe = startFrame;
        }
    }

    storeCurrentFrameState();
    emit tweeningRemoved(layer, startFrame, endFrame);

    qDebug() << "Tweening removed successfully";
}

// FIXED: Make sure isExtendedFrame method exists
bool Canvas::isExtendedFrame(int frame, int layer) const
{
    auto layerIt = m_layerFrameData.find(layer);
    if (layerIt != m_layerFrameData.end()) {
        auto frameIt = layerIt->second.find(frame);
        if (frameIt != layerIt->second.end()) {
            return frameIt->second.type == FrameType::ExtendedFrame;
        }
    }
    return false;
}

// FIXED: Make sure isTweenedFrame method exists  
bool Canvas::isTweenedFrame(int frame, int layer) const
{
    return hasTweening(layer, frame);
}