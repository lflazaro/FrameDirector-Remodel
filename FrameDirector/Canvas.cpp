// Canvas.cpp - Robustly fixed layer system with better state tracking
// Unique layer IDs to prevent mix-ups

#include "Canvas.h"
#include "Tools/Tool.h"
#include "MainWindow.h"
#include "Tools/SelectionTool.h"
#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QGraphicsRectItem>
#include <QGraphicsEllipseItem>
#include <QGraphicsLineItem>
#include <QGraphicsPathItem>
#include <QGraphicsPixmapItem>
#include <QGraphicsTextItem>
#include <QGraphicsItemGroup>
#include <QGraphicsBlurEffect>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QPainter>
#include <QRubberBand>
#include <QScrollBar>
#include <QApplication>
#include <QtMath>
#include <QDebug>
#include <QJsonObject>
#include <QJsonArray>
#include <QBuffer>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QConicalGradient>

// ROBUST: Enhanced layer data structure with better state management
struct LayerData {
    QString name;
    QString uuid;  // Unique identifier to prevent mix-ups
    bool visible;
    bool locked;
    double opacity;
    QPainter::CompositionMode blendMode;
    QList<QGraphicsItem*> items;
    QHash<int, QList<QGraphicsItem*>> frameItems; // Per-frame item tracking
    QSet<QGraphicsItem*> allTimeItems; // All items ever added to this layer

    LayerData(const QString& layerName)
        : name(layerName), visible(true), locked(false), opacity(1.0),
          blendMode(QPainter::CompositionMode_SourceOver) {
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
    void clearFrame(int frame) {
        auto it = frameItems.find(frame);
        if (it != frameItems.end()) {
            it.value().clear();
        }
    }

    QList<QGraphicsItem*> getFrameItems(int frame) const {
        auto it = frameItems.find(frame);
        if (it != frameItems.end()) {
            return it.value();
        }
        return QList<QGraphicsItem*>();
    }

    void debugPrint() const {
        qDebug() << "Layer" << name << "UUID:" << uuid
            << "Items:" << items.size()
            << "Frames:" << frameItems.keys()
            << "AllTime:" << allTimeItems.size();
    }
    void removeItemFromAllFrames(QGraphicsItem* item) {
        if (!item) return;

        // Remove from main items list
        items.removeAll(item);
        allTimeItems.remove(item);

        // Remove from all frame-specific data
        for (auto& frameEntry : frameItems) {
            frameEntry.removeAll(item);
        }

        qDebug() << "Removed item from all frames in layer" << uuid;
    }

    void removeItemFromFrame(int frame, QGraphicsItem* item) {
        if (!item) return;

        auto it = frameItems.find(frame);
        if (it != frameItems.end()) {
            it.value().removeAll(item);
        }
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
    , m_backgroundColor(Qt::white)
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
    , m_onionSkinEnabled(false)
    , m_onionSkinBefore(1)
    , m_onionSkinAfter(1)
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
    m_interpolatedItems.clear();
}

Canvas::~Canvas()
{
    qDebug() << "Canvas destructor called";

    m_destroying = true;
    disconnect();

    if (m_scene) {
        disconnect(m_scene, nullptr, this, nullptr);
    }

    // CRITICAL: Clean up all layer-specific interpolated items
    cleanupInterpolatedItems();  // This cleans up all layers

    // Clear all data structures
    m_frameItems.clear();
    m_layerKeyframes.clear();
    m_layerFrameData.clear();
    m_layerInterpolatedItems.clear();
    m_layerShowingInterpolated.clear();

    // Clean up rubber band and layers...
    if (m_rubberBand) {
        delete m_rubberBand;
        m_rubberBand = nullptr;
    }

    for (void* layerPtr : m_layers) {
        if (layerPtr) {
            LayerData* layer = static_cast<LayerData*>(layerPtr);
            layer->items.clear();
            layer->frameItems.clear();
            layer->allTimeItems.clear();
            delete layer;
        }
    }
    m_layers.clear();

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

        // Identify this item as the background and preserve base opacity
        m_backgroundRect->setData(1, "background");
        m_backgroundRect->setData(0, 1.0);

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

    // Initialize frame 1 for both layers
    FrameData bgFrame;
    bgFrame.type = FrameType::Keyframe;
    if (m_backgroundRect) {
        bgFrame.items.append(m_backgroundRect);
    }
    m_layerFrameData[0][1] = bgFrame;
    m_layerKeyframes[0].insert(1);

    FrameData drawFrame;
    drawFrame.type = FrameType::Keyframe;
    m_layerFrameData[1][1] = drawFrame;
    m_layerKeyframes[1].insert(1);

    // Set Layer 1 as current (not background)
    setCurrentLayer(1);

    qDebug() << "Default layers created - Background UUID:" << backgroundLayer->uuid
        << "Drawing UUID:" << drawingLayer->uuid;
}


int Canvas::addLayer(const QString& name, bool visible, double opacity,
                     QPainter::CompositionMode blendMode)
{
    QString layerName = name.isEmpty() ? QString("Layer %1").arg(m_layers.size()) : name;

    // Save current frame state before adding layer
    if (m_currentLayerIndex >= 0) {
        storeCurrentFrameState();
    }

    // Create new layer
    LayerData* newLayer = new LayerData(layerName);
    newLayer->visible = visible;
    newLayer->opacity = qBound(0.0, opacity, 1.0);
    newLayer->blendMode = blendMode;
    m_layers.push_back(newLayer);

    int newIndex = m_layers.size() - 1;

    // Initialize layer-specific data structures (empty)
    m_layerFrameData[newIndex] = QHash<int, FrameData>();
    m_layerInterpolatedItems[newIndex] = QList<QGraphicsItem*>();
    m_layerShowingInterpolated[newIndex] = false;
    m_layerKeyframes[newIndex] = QSet<int>();

    // Every layer starts with an empty keyframe at frame 1
    FrameData frame1;
    frame1.type = FrameType::Keyframe;
    m_layerFrameData[newIndex][1] = frame1;
    m_layerKeyframes[newIndex].insert(1);

    qDebug() << "Added layer:" << layerName << "Index:" << newIndex << "UUID:" << newLayer->uuid;

    emit layerChanged(m_currentLayerIndex); // Refresh current layer display

    return newIndex;
}

void Canvas::addLayerVoid(const QString& name, bool visible, double opacity,
                          QPainter::CompositionMode blendMode)
{
    addLayer(name, visible, opacity, blendMode);
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

        m_layerFrameData.remove(layerIndex);
        m_layerInterpolatedItems.remove(layerIndex);
        m_layerShowingInterpolated.remove(layerIndex);
        m_layerKeyframes.remove(layerIndex);

        // Reindex remaining layer-specific data
        QHash<int, QHash<int, FrameData>> newFrameData;
        for (auto it = m_layerFrameData.begin(); it != m_layerFrameData.end(); ++it) {
            int idx = it.key();
            newFrameData[idx > layerIndex ? idx - 1 : idx] = it.value();
        }
        m_layerFrameData = newFrameData;

        QHash<int, QList<QGraphicsItem*>> newInterpolated;
        for (auto it = m_layerInterpolatedItems.begin(); it != m_layerInterpolatedItems.end(); ++it) {
            int idx = it.key();
            newInterpolated[idx > layerIndex ? idx - 1 : idx] = it.value();
        }
        m_layerInterpolatedItems = newInterpolated;

        QHash<int, bool> newShowing;
        for (auto it = m_layerShowingInterpolated.begin(); it != m_layerShowingInterpolated.end(); ++it) {
            int idx = it.key();
            newShowing[idx > layerIndex ? idx - 1 : idx] = it.value();
        }
        m_layerShowingInterpolated = newShowing;

        QHash<int, QSet<int>> newKeys;
        for (auto it = m_layerKeyframes.begin(); it != m_layerKeyframes.end(); ++it) {
            int idx = it.key();
            newKeys[idx > layerIndex ? idx - 1 : idx] = it.value();
        }
        m_layerKeyframes = newKeys;

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
    if (layerIndex >= 0 && layerIndex < m_layers.size() && layerIndex != m_currentLayerIndex) {

        qDebug() << "Switching from layer" << m_currentLayerIndex << "to layer" << layerIndex;

        // Save current frame state for current layer
        if (m_currentLayerIndex >= 0) {
            storeCurrentFrameState();
            cleanupInterpolatedItems(m_currentLayerIndex);
        }

        LayerData* oldLayer = (m_currentLayerIndex >= 0 && m_currentLayerIndex < m_layers.size())
            ? static_cast<LayerData*>(m_layers[m_currentLayerIndex]) : nullptr;
        LayerData* newLayer = static_cast<LayerData*>(m_layers[layerIndex]);

        m_currentLayerIndex = layerIndex;

        // Load the correct content for new layer on current frame
        loadFrameState(m_currentFrame);

        qDebug() << "Layer switch completed - from" << (oldLayer ? oldLayer->uuid : "none")
            << "to" << newLayer->uuid;

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

    // Determine appropriate Z-value to preserve relative ordering
    int baseZ = m_currentLayerIndex * 1000;
    int maxZ = -1;
    for (QGraphicsItem* existing : currentLayer->items) {
        int z = static_cast<int>(existing->zValue()) % 1000;
        if (z > maxZ) {
            maxZ = z;
        }
    }

    // Add item to current layer and frame
    currentLayer->addItem(item, m_currentFrame);

    // Set properties based on layer state
    item->setZValue(baseZ + maxZ + 1);
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
        << "UUID:" << currentLayer->uuid
        << "Frame:" << m_currentFrame
        << "Individual opacity:" << individualOpacity
        << "Final opacity:" << item->opacity();
}

void Canvas::saveFrameState(int frame)
{
    qDebug() << "Saving frame state for frame:" << frame << "layer:" << m_currentLayerIndex;

    // Collect current layer items from scene
    // Use ascending order so items are stored from back to front,
    // ensuring their stacking order is preserved when reloaded
    QList<QGraphicsItem*> currentLayerItems;
    for (QGraphicsItem* item : m_scene->items(Qt::AscendingOrder)) {
        if (item != m_backgroundRect &&
            getItemLayerIndex(item) == m_currentLayerIndex &&
            !m_onionSkinItems.contains(item)) {
            currentLayerItems.append(item);
        }
    }

    // Save ONLY to layer-specific storage
    auto& layerFrameData = m_layerFrameData[m_currentLayerIndex];
    auto& frameData = layerFrameData[frame];
    frameData.items = currentLayerItems;
    frameData.itemStates.clear();

    // Store item states for potential tweening
    for (QGraphicsItem* item : currentLayerItems) {
        if (item) {
            QVariantMap state;
            state["position"] = item->pos();
            state["rotation"] = item->rotation();
            state["scale"] = item->scale();
            state["opacity"] = item->opacity();
            if (auto blur = dynamic_cast<QGraphicsBlurEffect*>(item->graphicsEffect())) {
                state["blur"] = blur->blurRadius();
            } else {
                state["blur"] = 0.0;
            }
            frameData.itemStates[item] = state;
        }
    }

    // Update frame type if needed
    if (frameData.type == FrameType::Empty && !currentLayerItems.isEmpty()) {
        frameData.type = FrameType::Keyframe;
        m_layerKeyframes[m_currentLayerIndex].insert(frame);
    }

    // Update layer data structure
    if (m_currentLayerIndex >= 0 && m_currentLayerIndex < m_layers.size()) {
        LayerData* layer = static_cast<LayerData*>(m_layers[m_currentLayerIndex]);
        layer->setFrameItems(frame, currentLayerItems);
    }

    qDebug() << "Saved frame state - Layer" << m_currentLayerIndex << "items:" << currentLayerItems.size();
}


void Canvas::loadFrameState(int frame)
{
    qDebug() << "Loading frame state for frame:" << frame;

    // Remove any existing interpolated items before loading new state
    cleanupInterpolatedItems();

    clearOnionSkinItems();

    // Clear current items first
    QList<QGraphicsItem*> currentItems;
    for (QGraphicsItem* item : scene()->items()) {
        if (item != m_backgroundRect && item->zValue() > -999 && !item->parentItem()) {
            currentItems.append(item);
        }
    }

    for (QGraphicsItem* item : currentItems) {
        scene()->removeItem(item);
    }

    // Load items for each layer with VALIDATION
    for (int layerIndex = 0; layerIndex < m_layers.size(); ++layerIndex) {
        LayerData* layer = static_cast<LayerData*>(m_layers[layerIndex]);
        QList<QGraphicsItem*> layerFrameItems = layer->getFrameItems(frame);

        // Add items for this layer to scene with proper validation
        int order = 0;
        for (QGraphicsItem* item : layerFrameItems) {
            // CRITICAL: Validate item pointer before using it
            if (item && isValidItem(item)) {
                m_scene->addItem(item);
                // Ensure proper layer Z-ordering based on stored order
                item->setZValue(layerIndex * 1000 + order++);
                // Apply layer properties
                item->setVisible(layer->visible);
                item->setOpacity(item->data(0).toDouble() * layer->opacity);
                item->setFlag(QGraphicsItem::ItemIsSelectable, !layer->locked);
                item->setFlag(QGraphicsItem::ItemIsMovable, !layer->locked);
            }
            else {
                qDebug() << "Invalid item detected in frame" << frame << "layer" << layerIndex
                    << "- removing from data structures";
                // Remove invalid item from layer data
                layer->removeItemFromFrame(frame, item);
            }
        }
    }

    // After base items are loaded, generate interpolated items for tweened layers
    for (int layerIndex = 0; layerIndex < m_layers.size(); ++layerIndex) {
        if (!isFrameTweened(frame, layerIndex)) {
            continue;
        }

        int startFrame = frame;
        int endFrame = -1;

        if (getFrameType(frame, layerIndex) == FrameType::ExtendedFrame) {
            startFrame = getSourceKeyframe(frame, layerIndex);
        }

        if (hasFrameTweening(startFrame, layerIndex)) {
            endFrame = getTweeningEndFrame(startFrame, layerIndex);
        }

        if (startFrame != -1 && endFrame != -1 && frame > startFrame && frame < endFrame) {
            clearLayerFromScene(layerIndex);
            float t = static_cast<float>(frame - startFrame) / (endFrame - startFrame);
            QString easingType = getFrameTweeningEasing(startFrame, layerIndex);
            if (easingType == "ease-in") {
                t = t * t;  // Quadratic ease-in
            }
            else if (easingType == "ease-out") {
                t = 1 - (1 - t) * (1 - t);  // Quadratic ease-out
            }
            else if (easingType == "ease-in-out") {
                t = t < 0.5 ? 2 * t * t : 1 - 2 * (1 - t) * (1 - t);  // Quadratic ease-in-out
            }
            interpolateFrame(frame, startFrame, endFrame, t, layerIndex);
        }
    }

    applyOnionSkin(frame);

    qDebug() << "Frame state loaded successfully for frame:" << frame;
}

void Canvas::clearOnionSkinItems()
{
    if (!m_scene) {
        m_onionSkinItems.clear();
        return;
    }
    for (QGraphicsItem* item : m_onionSkinItems) {
        if (m_scene->items().contains(item)) {
            m_scene->removeItem(item);
        }
    }
    m_onionSkinItems.clear();
}

void Canvas::applyOnionSkin(int frame)
{
    clearOnionSkinItems();
    if (!m_onionSkinEnabled) return;

    const double baseOpacity = 0.4;

    // Previous frames
    for (int i = 1; i <= m_onionSkinBefore; ++i) {
        int f = frame - i;
        if (f < 1) break;
        double factor = baseOpacity * (m_onionSkinBefore - i + 1) / m_onionSkinBefore;
        for (int layerIndex = 0; layerIndex < m_layers.size(); ++layerIndex) {
            LayerData* layer = static_cast<LayerData*>(m_layers[layerIndex]);
            QList<QGraphicsItem*> items = layer->getFrameItems(f);
            for (QGraphicsItem* item : items) {
                if (!item || !isValidItem(item)) continue;
                m_scene->addItem(item);
                item->setOpacity(item->data(0).toDouble() * layer->opacity * factor);
                item->setZValue(layerIndex * 1000 - 500 - i);
                item->setFlag(QGraphicsItem::ItemIsSelectable, false);
                item->setFlag(QGraphicsItem::ItemIsMovable, false);
                item->setAcceptedMouseButtons(Qt::NoButton);
                m_onionSkinItems.insert(item);
            }
        }
    }

    // Next frames
    for (int i = 1; i <= m_onionSkinAfter; ++i) {
        int f = frame + i;
        double factor = baseOpacity * (m_onionSkinAfter - i + 1) / m_onionSkinAfter;
        for (int layerIndex = 0; layerIndex < m_layers.size(); ++layerIndex) {
            LayerData* layer = static_cast<LayerData*>(m_layers[layerIndex]);
            QList<QGraphicsItem*> items = layer->getFrameItems(f);
            for (QGraphicsItem* item : items) {
                if (!item || !isValidItem(item)) continue;
                m_scene->addItem(item);
                item->setOpacity(item->data(0).toDouble() * layer->opacity * factor);
                item->setZValue(layerIndex * 1000 - 500 + i);
                item->setFlag(QGraphicsItem::ItemIsSelectable, false);
                item->setFlag(QGraphicsItem::ItemIsMovable, false);
                item->setAcceptedMouseButtons(Qt::NoButton);
                m_onionSkinItems.insert(item);
            }
        }
    }
}


bool Canvas::isValidItem(QGraphicsItem* item) const
{
    if (!item) return false;

    // Check if the item pointer is valid by testing a simple property access
    try {
        // Try to access a basic property - this will crash if pointer is invalid
        item->type();
        return true;
    }
    catch (...) {
        return false;
    }
}

int Canvas::getItemLayerIndex(QGraphicsItem* item) const
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

    // Remove deleted items from every tracking structure
    for (QGraphicsItem* item : selectedItems) {
        removeItemFromAllFrames(item);
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
        m_layerKeyframes.clear();

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
    if (frame == m_currentFrame) return;

    // Clean up any existing interpolated items across all layers to avoid
    // residual drawings when jumping between frames or layers
    cleanupInterpolatedItems();

    m_currentFrame = frame;

    // Check if this frame is part of a tweened sequence on the current layer
    if (isFrameTweened(frame, m_currentLayerIndex)) {
        // Determine tween start and end frames for the active layer
        int startFrame = frame;
        int endFrame = -1;

        if (getFrameType(frame, m_currentLayerIndex) == FrameType::ExtendedFrame) {
            startFrame = getSourceKeyframe(frame, m_currentLayerIndex);
        }

        if (hasFrameTweening(startFrame, m_currentLayerIndex)) {
            endFrame = getTweeningEndFrame(startFrame, m_currentLayerIndex);
        }

        if (frame == startFrame || frame == endFrame) {
            // For actual keyframes simply load the saved state for all layers
            loadFrameState(frame);
        }
        else if (startFrame != -1 && endFrame != -1 && frame > startFrame && frame < endFrame) {
            // Load all layer content for this frame first, then replace the
            // current layer with interpolated items so other layers display
            // the correct frame content
            loadFrameState(frame);
            clearLayerFromScene(m_currentLayerIndex);
            float t = static_cast<float>(frame - startFrame) / (endFrame - startFrame);
            QString easingType = getFrameTweeningEasing(startFrame, m_currentLayerIndex);
            if (easingType == "ease-in") {
                t = t * t;  // Quadratic ease-in
            }
            else if (easingType == "ease-out") {
                t = 1 - (1 - t) * (1 - t);  // Quadratic ease-out
            }
            else if (easingType == "ease-in-out") {
                t = t < 0.5 ? 2 * t * t : 1 - 2 * (1 - t) * (1 - t);  // Quadratic ease-in-out
            }
            interpolateFrame(frame, startFrame, endFrame, t, m_currentLayerIndex);
        }

        emit frameChanged(frame);
        return;
    }

    // Normal frame loading
    loadFrameState(frame);
    emit frameChanged(frame);
}



void Canvas::cleanupInterpolatedItems(int layerIndex)
{
    if (!m_layerInterpolatedItems.contains(layerIndex) ||
        m_layerInterpolatedItems[layerIndex].isEmpty()) {
        return;
    }

    qDebug() << "Cleaning up" << m_layerInterpolatedItems[layerIndex].size()
             << "interpolated items for layer" << layerIndex;

    // Retrieve layer pointer if available for safety checks
    LayerData* layer =
        (layerIndex >= 0 && layerIndex < m_layers.size())
            ? static_cast<LayerData*>(m_layers[layerIndex])
            : nullptr;

    QList<QGraphicsItem*>& items = m_layerInterpolatedItems[layerIndex];
    for (QGraphicsItem* item : items) {
        if (!item) {
            continue;
        }

        // Determine whether the item is a temporary interpolated clone. If an
        // interpolated item has been promoted to a real layer item (for
        // example, when editing on a tweened frame), it will also appear in the
        // layer's data structures. Such items must not be deleted here.
        const bool isInterpolatedClone = item->data(999).toString() == "interpolated";
        const bool belongsToLayer = layer && layer->containsItem(item);

        if (scene()->items().contains(item)) {
            scene()->removeItem(item);
        }

        if (isInterpolatedClone && !belongsToLayer) {
            delete item;
        }
    }

    items.clear();
    m_layerShowingInterpolated[layerIndex] = false;
}


void Canvas::interpolateFrame(int currentFrame, int startFrame, int endFrame, float t, int layerIndex)
{
    // Ensure we don't leave stale interpolated items behind
    cleanupInterpolatedItems(layerIndex);

    m_layerShowingInterpolated[layerIndex] = true;

    // Get start and end frame data for specific layer
    auto& layerFrameData = m_layerFrameData[layerIndex];
    auto startIt = layerFrameData.find(startFrame);
    auto endIt = layerFrameData.find(endFrame);

    if (startIt == layerFrameData.end() || endIt == layerFrameData.end()) {
        m_layerShowingInterpolated[layerIndex] = false;
        return;
    }

    // Interpolate between keyframes on this layer only
    const QList<QGraphicsItem*>& startItems = startIt.value().items;
    const QList<QGraphicsItem*>& endItems = endIt.value().items;

    for (int i = 0; i < qMin(startItems.size(), endItems.size()); ++i) {
        QGraphicsItem* startItem = startItems[i];
        QGraphicsItem* endItem = endItems[i];

        if (!startItem || !endItem) continue;

        QGraphicsItem* interpolatedItem = cloneGraphicsItem(startItem);
        if (!interpolatedItem) continue;

        // Interpolate position using item centers to keep proper rotation pivot
        QPointF startCenter = startItem->mapToScene(startItem->boundingRect().center());
        QPointF endCenter = endItem->mapToScene(endItem->boundingRect().center());
        QPointF interpolatedCenter = startCenter + t * (endCenter - startCenter);

        // Set transform origin to item center and position accordingly
        QPointF origin = interpolatedItem->boundingRect().center();
        interpolatedItem->setTransformOriginPoint(origin);
        interpolatedItem->setPos(interpolatedCenter - origin);

        // Interpolate rotation
        qreal startRotation = startItem->rotation();
        qreal endRotation = endItem->rotation();
        qreal rotationDiff = endRotation - startRotation;
        if (rotationDiff > 180) rotationDiff -= 360;
        if (rotationDiff < -180) rotationDiff += 360;
        qreal interpolatedRotation = startRotation + t * rotationDiff;
        interpolatedItem->setRotation(interpolatedRotation);

        // Interpolate scaling
        QTransform startTransform = startItem->transform();
        QTransform endTransform = endItem->transform();
        qreal startScaleX = startTransform.m11();
        qreal startScaleY = startTransform.m22();
        qreal endScaleX = endTransform.m11();
        qreal endScaleY = endTransform.m22();
        qreal interpolatedScaleX = startScaleX + t * (endScaleX - startScaleX);
        qreal interpolatedScaleY = startScaleY + t * (endScaleY - startScaleY);
        QTransform interpolatedTransform;
        interpolatedTransform.scale(interpolatedScaleX, interpolatedScaleY);
        interpolatedItem->setTransform(interpolatedTransform);

        // Interpolate opacity
        qreal startOpacity = startItem->opacity();
       qreal endOpacity = endItem->opacity();
       qreal interpolatedOpacity = startOpacity + t * (endOpacity - startOpacity);
       interpolatedItem->setOpacity(interpolatedOpacity);

        // Interpolate blur
        qreal startBlur = 0;
        if (auto blur = dynamic_cast<QGraphicsBlurEffect*>(startItem->graphicsEffect()))
            startBlur = blur->blurRadius();
        qreal endBlur = 0;
        if (auto blur2 = dynamic_cast<QGraphicsBlurEffect*>(endItem->graphicsEffect()))
            endBlur = blur2->blurRadius();
        qreal interpolatedBlur = startBlur + t * (endBlur - startBlur);
        if (interpolatedBlur > 0) {
            QGraphicsBlurEffect* blurEffect = dynamic_cast<QGraphicsBlurEffect*>(interpolatedItem->graphicsEffect());
            if (!blurEffect) {
                blurEffect = new QGraphicsBlurEffect();
                interpolatedItem->setGraphicsEffect(blurEffect);
            }
            blurEffect->setBlurRadius(interpolatedBlur);
        } else if (interpolatedItem->graphicsEffect()) {
            interpolatedItem->setGraphicsEffect(nullptr);
        }

        // Preserve pen properties for paths
        if (auto startPath = qgraphicsitem_cast<QGraphicsPathItem*>(startItem)) {
            if (auto interpPath = qgraphicsitem_cast<QGraphicsPathItem*>(interpolatedItem)) {
                QPen startPen = startPath->pen();
                interpPath->setPen(startPen);
            }
        }

        // Mark interpolated items as non-selectable and set layer Z-value
        interpolatedItem->setFlag(QGraphicsItem::ItemIsSelectable, false);
        interpolatedItem->setFlag(QGraphicsItem::ItemIsMovable, false);
        interpolatedItem->setData(999, "interpolated");
        interpolatedItem->setData(998, layerIndex);  // Store layer index
        interpolatedItem->setZValue(layerIndex * 1000);  // Proper layer Z-ordering

        // Add to scene and track per layer
        scene()->addItem(interpolatedItem);
        m_layerInterpolatedItems[layerIndex].append(interpolatedItem);
    }

    qDebug() << "Created" << m_layerInterpolatedItems[layerIndex].size()
        << "interpolated items for layer" << layerIndex << "frame" << currentFrame;
}

int Canvas::getCurrentFrame() const { return m_currentFrame; }


void Canvas::createKeyframe(int frame)
{
    qDebug() << "Creating keyframe at frame" << frame << "on layer" << m_currentLayerIndex;

    // Clone current scene items that belong to current layer
    QList<QGraphicsItem*> clonedItems;
    for (QGraphicsItem* item : m_scene->items()) {
        if (item != m_backgroundRect && getItemLayerIndex(item) == m_currentLayerIndex) {
            QGraphicsItem* clonedItem = cloneGraphicsItem(item);
            if (clonedItem) {
                clonedItems.append(clonedItem);
            }
        }
    }

    // Store as keyframe ONLY in layer-specific data
    auto& layerFrameData = m_layerFrameData[m_currentLayerIndex];
    auto& frameData = layerFrameData[frame];
    frameData.type = FrameType::Keyframe;
    frameData.items = clonedItems;
    frameData.sourceKeyframe = -1;
    frameData.hasTweening = false;
    frameData.tweeningEndFrame = -1;
    frameData.easingType = "linear";

    // Update layer data structure
    if (m_currentLayerIndex >= 0 && m_currentLayerIndex < m_layers.size()) {
        LayerData* layer = static_cast<LayerData*>(m_layers[m_currentLayerIndex]);
        layer->setFrameItems(frame, clonedItems);
    }

    m_layerKeyframes[m_currentLayerIndex].insert(frame);

    qDebug() << "Keyframe created with" << clonedItems.size() << "items";
}


QGraphicsItem* Canvas::cloneGraphicsItem(QGraphicsItem* item)
{
    if (!item) return nullptr;

    QGraphicsItem* copy = nullptr;

    if (auto rectItem = qgraphicsitem_cast<QGraphicsRectItem*>(item)) {
        auto newRect = new QGraphicsRectItem(rectItem->rect());
        // FIX: Explicitly preserve all pen properties
        QPen originalPen = rectItem->pen();
        newRect->setPen(originalPen);  // This preserves width, color, style
        newRect->setBrush(rectItem->brush());
        copy = newRect;
    }
    else if (auto ellipseItem = qgraphicsitem_cast<QGraphicsEllipseItem*>(item)) {
        auto newEllipse = new QGraphicsEllipseItem(ellipseItem->rect());
        // FIX: Explicitly preserve all pen properties
        QPen originalPen = ellipseItem->pen();
        newEllipse->setPen(originalPen);
        newEllipse->setBrush(ellipseItem->brush());
        copy = newEllipse;
    }
    else if (auto lineItem = qgraphicsitem_cast<QGraphicsLineItem*>(item)) {
        auto newLine = new QGraphicsLineItem(lineItem->line());
        // FIX: Explicitly preserve all pen properties
        QPen originalPen = lineItem->pen();
        newLine->setPen(originalPen);
        copy = newLine;
    }
    else if (auto pathItem = qgraphicsitem_cast<QGraphicsPathItem*>(item)) {
        auto newPath = new QGraphicsPathItem(pathItem->path());
        // FIX: Explicitly preserve all pen properties for path items (brush strokes)
        QPen originalPen = pathItem->pen();
        qDebug() << "Cloning path item with pen width:" << originalPen.widthF();
        newPath->setPen(originalPen);  // This should preserve brush stroke width
        newPath->setBrush(pathItem->brush());
        copy = newPath;
    }
    else if (auto groupItem = qgraphicsitem_cast<QGraphicsItemGroup*>(item)) {
        auto newGroup = new QGraphicsItemGroup();
        for (QGraphicsItem* child : groupItem->childItems()) {
            if (QGraphicsItem* childClone = cloneGraphicsItem(child)) {
                newGroup->addToGroup(childClone);
            }
        }
        copy = newGroup;
    }
    else if (auto pixmapItem = qgraphicsitem_cast<QGraphicsPixmapItem*>(item)) {
        // Handle imported images
        auto newPixmap = new QGraphicsPixmapItem(pixmapItem->pixmap());
        newPixmap->setOffset(pixmapItem->offset());
        copy = newPixmap;
    }
    else if (auto textItem = qgraphicsitem_cast<QGraphicsTextItem*>(item)) {
        auto newText = new QGraphicsTextItem(textItem->toPlainText());
        newText->setFont(textItem->font());
        newText->setDefaultTextColor(textItem->defaultTextColor());
        copy = newText;
    }

    if (copy) {
        copy->setTransformOriginPoint(item->transformOriginPoint());
        copy->setTransform(item->transform());
        copy->setPos(item->pos());
        copy->setRotation(item->rotation());
        copy->setFlags(item->flags());
        copy->setZValue(item->zValue());
        copy->setOpacity(item->opacity());
        copy->setVisible(item->isVisible());
        copy->setEnabled(item->isEnabled());
        copy->setSelected(item->isSelected());
        copy->setData(0, item->opacity());  // Store original opacity as data
        if (auto blur = dynamic_cast<QGraphicsBlurEffect*>(item->graphicsEffect())) {
            auto newBlur = new QGraphicsBlurEffect();
            newBlur->setBlurRadius(blur->blurRadius());
            copy->setGraphicsEffect(newBlur);
        }
    }

    return copy;
}


void Canvas::clearLayerFromScene(int layerIndex)
{
    QList<QGraphicsItem*> itemsToRemove;
    for (QGraphicsItem* item : scene()->items()) {
        if (item != m_backgroundRect && getItemLayerIndex(item) == layerIndex) {
            itemsToRemove.append(item);
        }
    }

    for (QGraphicsItem* item : itemsToRemove) {
        scene()->removeItem(item);
        // Don't delete here if it might be referenced in frameData
    }
}

void Canvas::clearFrameItems(int frame)
{
    auto it = m_frameData.find(frame);
    if (it != m_frameData.end()) {
        it->second.items.clear();
        it->second.itemStates.clear();
    }

    m_frameItems[frame].clear();
}

void Canvas::createBlankKeyframe(int frame)
{
    if (m_currentLayerIndex < 0 || m_currentLayerIndex >= m_layers.size()) return;

    LayerData* layer = static_cast<LayerData*>(m_layers[m_currentLayerIndex]);
    QList<QGraphicsItem*> items;
    if (m_currentLayerIndex == 0 && m_backgroundRect) {
        items.append(m_backgroundRect);
    }
    layer->setFrameItems(frame, items);

    auto& frameData = m_layerFrameData[m_currentLayerIndex][frame];
    frameData.type = FrameType::Keyframe;
    frameData.items = items;
    frameData.sourceKeyframe = -1;
    frameData.hasTweening = false;
    frameData.tweeningEndFrame = -1;
    frameData.easingType = "linear";

    m_frameItems[frame] = QList<QGraphicsItem*>();
    m_layerKeyframes[m_currentLayerIndex].insert(frame);
    clearFrameState();
    emit keyframeCreated(frame);
    qDebug() << "Blank keyframe created at frame:" << frame;
}

bool Canvas::hasKeyframe(int frame, int layerIndex) const
{
    auto it = m_layerKeyframes.find(layerIndex);
    if (it == m_layerKeyframes.end()) {
        return false;
    }
    return it.value().contains(frame);
}


void Canvas::storeCurrentFrameState()
{
    // CRITICAL FIX: Never store interpolated frame state
    if (m_layerShowingInterpolated.value(m_currentLayerIndex, false)) {
        qDebug() << "Skipping frame state storage for interpolated frame" << m_currentFrame
            << "on layer" << m_currentLayerIndex;
        return;
    }

    if (m_currentFrame >= 1) {
        // Ensure interpolated items are cleaned up before storing
        cleanupInterpolatedItems(m_currentLayerIndex);
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

    // CRITICAL FIX: Only trigger drawing logic for actual drawing tools that create content
    if (m_currentTool && event->button() == Qt::LeftButton) {
        // Check if this is a tool that actually creates/modifies graphics content
        SelectionTool* selectionTool = dynamic_cast<SelectionTool*>(m_currentTool);

        // FIXED: Selection tool should NEVER trigger drawing logic or keyframe creation
        if (!selectionTool) {
            // For actual drawing tools, check permissions and convert frames if needed
            if (!canDrawOnCurrentFrame()) {
                qDebug() << "Drawing blocked on tweened frame";
                event->ignore();
                return;
            }
            onDrawingStarted(); // Auto-convert extended frames if needed
        }
    }

    // Handle rubber band selection for SelectionTool
    if (m_currentTool) {
        SelectionTool* selectionTool = dynamic_cast<SelectionTool*>(m_currentTool);
        if (selectionTool && event->button() == Qt::LeftButton) {
            QGraphicsItem* item = m_scene->itemAt(scenePos, transform());
            if (!item || item == m_backgroundRect) {
                m_rubberBandOrigin = event->pos();
                if (!m_rubberBand) {
                    m_rubberBand = new QRubberBand(QRubberBand::Rectangle, this);
                }
                m_rubberBand->setGeometry(QRect(m_rubberBandOrigin, QSize()));
                m_rubberBand->show();
                m_rubberBandActive = true;
                if (!(event->modifiers() & Qt::ControlModifier)) {
                    // FIXED: Suppress frame conversion during selection clearing
                    m_suppressFrameConversion = true;
                    m_scene->clearSelection();
                    m_suppressFrameConversion = false;
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
        m_suppressFrameConversion = true;
        for (QGraphicsItem* item : allItems) {
            if (item != m_backgroundRect &&
                (item->flags() & QGraphicsItem::ItemIsSelectable) &&
                item->data(999).toString() != "interpolated") { // Don't select interpolated items
                item->setSelected(true);
            }
        }
        m_suppressFrameConversion = false;
        emit selectionChanged();
    }
}


void Canvas::clearSelection()
{
    if (m_scene) {
        m_suppressFrameConversion = true;
        m_scene->clearSelection();
        m_suppressFrameConversion = false;
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

void Canvas::setOnionSkinEnabled(bool enabled)
{
    if (m_onionSkinEnabled == enabled)
        return;
    m_onionSkinEnabled = enabled;
    loadFrameState(m_currentFrame);
}

bool Canvas::isOnionSkinEnabled() const
{
    return m_onionSkinEnabled;
}

void Canvas::setOnionSkinRange(int before, int after)
{
    m_onionSkinBefore = qMax(0, before);
    m_onionSkinAfter = qMax(0, after);
    if (m_onionSkinEnabled)
        loadFrameState(m_currentFrame);
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
    if (!m_destroying && !m_suppressFrameConversion) {
        QList<QGraphicsItem*> selectedItems = m_scene->selectedItems();
        if (!selectedItems.isEmpty()) {
            int layerIdx = getItemLayerIndex(selectedItems.first());
            if (layerIdx >= 0 && layerIdx != m_currentLayerIndex) {
                setCurrentLayer(layerIdx);
            }
        }
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
        // Clear current selection so child items don't remain individually selected
        m_scene->clearSelection();

        QGraphicsItemGroup* group = m_scene->createItemGroup(selectedItems);
        group->setFlag(QGraphicsItem::ItemIsSelectable, true);
        group->setFlag(QGraphicsItem::ItemIsMovable, true);
        group->setHandlesChildEvents(false);

        // Remove individual items from tracking structures
        for (QGraphicsItem* child : selectedItems) {
            int layerIndex = getItemLayerIndex(child);
            if (layerIndex >= 0) {
                LayerData* layer = static_cast<LayerData*>(m_layers[layerIndex]);
                layer->removeItemFromAllFrames(child);

                if (m_layerFrameData.contains(layerIndex)) {
                    auto& layerFrameData = m_layerFrameData[layerIndex];
                    for (auto it = layerFrameData.begin(); it != layerFrameData.end(); ++it) {
                        it.value().items.removeAll(child);
                        it.value().itemStates.remove(child);
                    }
                }
            }

            for (auto& frameEntry : m_frameItems) {
                frameEntry.second.removeAll(child);
            }
        }

        // Add group as single item to current layer
        addItemToCurrentLayer(group);

        // Select the new group so subsequent operations affect it
        group->setSelected(true);

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
            QList<QGraphicsItem*> children = group->childItems();

            int layerIndex = getItemLayerIndex(group);
            if (layerIndex >= 0) {
                LayerData* layer = static_cast<LayerData*>(m_layers[layerIndex]);
                layer->removeItemFromAllFrames(group);

                if (m_layerFrameData.contains(layerIndex)) {
                    auto& layerFrameData = m_layerFrameData[layerIndex];
                    for (auto it = layerFrameData.begin(); it != layerFrameData.end(); ++it) {
                        it.value().items.removeAll(group);
                        it.value().itemStates.remove(group);
                    }
                }
            }

            for (auto& frameEntry : m_frameItems) {
                frameEntry.second.removeAll(group);
            }

            m_scene->destroyItemGroup(group);
            m_scene->clearSelection();

            for (QGraphicsItem* child : children) {
                addItemToCurrentLayer(child);
                child->setSelected(true);
            }
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
        QPointF sceneCenter = item->mapToScene(item->boundingRect().center());
        item->setTransformOriginPoint(item->boundingRect().center());
        item->setPos(sceneCenter - item->transformOriginPoint());
        item->setRotation(item->rotation() + angle);
    }
    storeCurrentFrameState();
}
void Canvas::createExtendedFrame(int frame)
{
    if (frame < 1) return;
    if (m_currentLayerIndex < 0 || m_currentLayerIndex >= m_layers.size()) return;
    qDebug() << "Creating extended frame at frame" << frame;

    int sourceKeyframe = getLastKeyframeBefore(frame, m_currentLayerIndex);
    if (sourceKeyframe == -1) {
        qDebug() << "No previous keyframe found, creating blank keyframe";
        createBlankKeyframe(frame);
        return;
    }
    qDebug() << "Source keyframe found at frame" << sourceKeyframe;

    LayerData* layer = static_cast<LayerData*>(m_layers[m_currentLayerIndex]);
    QList<QGraphicsItem*> sourceItems = layer->getFrameItems(sourceKeyframe);

    for (int f = sourceKeyframe + 1; f <= frame; ++f) {
        if (hasContent(f, m_currentLayerIndex)) {
            qDebug() << "Frame" << f << "already has content, skipping";
            continue;
        }
        qDebug() << "Creating extended frame data for frame" << f;

        FrameData& data = m_layerFrameData[m_currentLayerIndex][f];
        data.type = FrameType::ExtendedFrame;
        data.sourceKeyframe = sourceKeyframe;
        data.items = sourceItems;
        data.hasTweening = false;
        data.tweeningEndFrame = -1;
        data.easingType = "linear";
        layer->setFrameItems(f, sourceItems);

        emit frameExtended(sourceKeyframe, f);
    }

    setCurrentFrame(frame);
    qDebug() << "Extended frames created from" << sourceKeyframe + 1 << "to" << frame;
}


// ENHANCED: Also update copyItemsToFrame for better performance
void Canvas::copyItemsToFrame(int fromFrame, int toFrame)
{
    if (m_frameData.find(fromFrame) == m_frameData.end()) {
        qDebug() << "Source frame" << fromFrame << "has no data";
        return;
    }

    qDebug() << "Copying items from frame" << fromFrame << "to frame" << toFrame;

    // PERFORMANCE OPTIMIZATION: For extended frames, just reference the items
    // instead of duplicating them in memory
    if (m_frameItems.find(fromFrame) != m_frameItems.end()) {
        // Reference the same items (no scene manipulation needed)
        m_frameData[toFrame].items = m_frameItems[fromFrame];
        m_frameItems[toFrame] = m_frameItems[fromFrame];  // Update compatibility layer

        // Copy item states
        if (m_frameData.find(fromFrame) != m_frameData.end()) {
            m_frameData[toFrame].itemStates = m_frameData[fromFrame].itemStates;
        }

        qDebug() << "Referenced" << m_frameItems[fromFrame].size() << "items from frame" << fromFrame;
        return;
    }

    // FALLBACK: Original method for cases where we need actual duplication
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

    // Store for the target frame
    m_frameData[toFrame].items = sourceItems;
    m_frameItems[toFrame] = sourceItems;  // Update compatibility layer
}

// Add these methods to Canvas.cpp


bool Canvas::hasContent(int frame, int layerIndex) const
{
    if (m_layerFrameData.contains(layerIndex)) {
        const auto& layerFrameData = m_layerFrameData[layerIndex];
        if (layerFrameData.contains(frame)) {
            return !layerFrameData[frame].items.isEmpty();
        }
    }

    if (layerIndex >= 0 && layerIndex < m_layers.size()) {
        LayerData* layer = static_cast<LayerData*>(m_layers[layerIndex]);
        return !layer->getFrameItems(frame).isEmpty();
    }

    return false;
}

FrameType Canvas::getFrameType(int frame, int layerIndex) const
{
    if (!m_layerFrameData.contains(layerIndex)) return FrameType::Empty;

    const auto& layerFrameData = m_layerFrameData[layerIndex];
    auto it = layerFrameData.find(frame);
    if (it != layerFrameData.end()) {
        return it.value().type;
    }

    // Check if it's a keyframe
    if (hasKeyframe(frame, layerIndex)) {
        return FrameType::Keyframe;
    }

    return FrameType::Empty;
}


int Canvas::getSourceKeyframe(int frame, int layerIndex) const
{
    if (!m_layerFrameData.contains(layerIndex)) return -1;

    const auto& layerFrameData = m_layerFrameData[layerIndex];
    auto it = layerFrameData.find(frame);
    if (it != layerFrameData.end() && it.value().type == FrameType::ExtendedFrame) {
        return it.value().sourceKeyframe;
    }

    // If this frame is a keyframe, it's its own source
    if (hasKeyframe(frame, layerIndex)) {
        return frame;
    }

    return -1;
}

int Canvas::getLastKeyframeBefore(int frame, int layerIndex) const
{
    int lastKeyframe = -1;
    auto it = m_layerKeyframes.find(layerIndex);
    if (it != m_layerKeyframes.end()) {
        const QSet<int>& keys = it.value();
        for (int key : keys) {
            if (key < frame && key > lastKeyframe) {
                lastKeyframe = key;
            }
        }
    }

    if (m_layerFrameData.contains(layerIndex)) {
        const auto& layerFrameData = m_layerFrameData[layerIndex];
        for (auto iter = layerFrameData.begin(); iter != layerFrameData.end(); ++iter) {
            if (iter.key() < frame && iter.value().type == FrameType::Keyframe) {
                if (iter.key() > lastKeyframe) {
                    lastKeyframe = iter.key();
                }
            }
        }
    }

    return lastKeyframe;
}

int Canvas::getNextKeyframeAfter(int frame, int layerIndex) const
{
    int nextKeyframe = -1;
    auto it = m_layerKeyframes.find(layerIndex);
    if (it != m_layerKeyframes.end()) {
        const QSet<int>& keys = it.value();
        for (int key : keys) {
            if (key > frame) {
                if (nextKeyframe == -1 || key < nextKeyframe) {
                    nextKeyframe = key;
                }
            }
        }
    }

    if (m_layerFrameData.contains(layerIndex)) {
        const auto& layerFrameData = m_layerFrameData[layerIndex];
        for (auto iter = layerFrameData.begin(); iter != layerFrameData.end(); ++iter) {
            if (iter.key() > frame && iter.value().type == FrameType::Keyframe) {
                if (nextKeyframe == -1 || iter.key() < nextKeyframe) {
                    nextKeyframe = iter.key();
                }
            }
        }
    }

    return nextKeyframe;
}


void Canvas::clearCurrentFrameContent()
{
    qDebug() << "Clearing current frame content for frame:" << m_currentFrame;

    // Get items to remove from current layer only
    QList<QGraphicsItem*> itemsToRemove;
    if (m_currentLayerIndex >= 0 && m_currentLayerIndex < m_layers.size()) {
        LayerData* currentLayer = static_cast<LayerData*>(m_layers[m_currentLayerIndex]);
        QList<QGraphicsItem*> currentFrameItems = currentLayer->getFrameItems(m_currentFrame);

        for (QGraphicsItem* item : currentFrameItems) {
            if (item && item != m_backgroundRect) {
                itemsToRemove.append(item);
            }
        }
    }

    // Remove items from scene and delete them
    for (QGraphicsItem* item : itemsToRemove) {
        if (item && m_scene->items().contains(item)) {
            m_scene->removeItem(item);

            // CRITICAL: Remove item from ALL data structures before deletion
            removeItemFromAllFrames(item);

            delete item;
        }
    }

    // Clear frame data for current frame
    auto frameDataIt = m_frameData.find(m_currentFrame);
    if (frameDataIt != m_frameData.end()) {
        frameDataIt->second.items.clear();
        frameDataIt->second.itemStates.clear();
        frameDataIt->second.type = FrameType::Empty;
        frameDataIt->second.sourceKeyframe = -1;
    }

    // Clear from frameItems map
    m_frameItems[m_currentFrame].clear();

    // Remove from keyframes set if it was a keyframe
    m_layerKeyframes[m_currentLayerIndex].remove(m_currentFrame);

    // Update current layer data
    if (m_currentLayerIndex >= 0 && m_currentLayerIndex < m_layers.size()) {
        LayerData* currentLayer = static_cast<LayerData*>(m_layers[m_currentLayerIndex]);
        currentLayer->clearFrame(m_currentFrame);
    }

    emit frameChanged(m_currentFrame);
    qDebug() << "Frame content cleared successfully";
}

void Canvas::removeItemFromAllFrames(QGraphicsItem* item)
{
    if (!item) return;

    // Remove from all m_frameItems entries
    for (auto& frameEntry : m_frameItems) {
        frameEntry.second.removeAll(item);
    }

    // Remove from all m_frameData entries
    for (auto& frameEntry : m_frameData) {
        frameEntry.second.items.removeAll(item);
        frameEntry.second.itemStates.remove(item);
    }

    // Remove from all layer data
    for (int i = 0; i < m_layers.size(); ++i) {
        LayerData* layer = static_cast<LayerData*>(m_layers[i]);
        layer->removeItemFromAllFrames(item);
    }

    qDebug() << "Removed item from all frame data structures";
}

bool Canvas::hasFrameTweening(int frame, int layerIndex) const
{
    if (!m_layerFrameData.contains(layerIndex)) return false;

    const auto& layerFrameData = m_layerFrameData[layerIndex];
    auto it = layerFrameData.find(frame);
    if (it != layerFrameData.end()) {
        return it.value().hasTweening;
    }
    return false;
}


// NEW: Check if a frame is part of a tweened sequence
bool Canvas::isFrameTweened(int frame, int layerIndex) const
{
    if (!m_layerFrameData.contains(layerIndex)) return false;

    const auto& layerFrameData = m_layerFrameData[layerIndex];

    // Check if this frame itself has tweening
    auto it = layerFrameData.find(frame);
    if (it != layerFrameData.end() && it.value().hasTweening) {
        return true;
    }

    for (auto it = layerFrameData.begin(); it != layerFrameData.end(); ++it) {
        const FrameData& frameData = it.value();
        if (frameData.hasTweening &&
            it.key() < frame &&
            frame <= frameData.tweeningEndFrame) {
            return true;
        }
    }

    return false;
}

// NEW: Apply tweening from one keyframe to another
void Canvas::applyTweening(int startFrame, int endFrame, const QString& easingType)
{
    qDebug() << "Applying tweening from frame" << startFrame << "to" << endFrame
        << "on layer" << m_currentLayerIndex;

    // Validate frames
    if (startFrame >= endFrame || startFrame < 1) {
        qDebug() << "Invalid tweening range";
        return;
    }

    // Ensure both start and end frames are keyframes on current layer
    if (!hasKeyframe(startFrame, m_currentLayerIndex)) {
        qDebug() << "Start frame" << startFrame << "is not a keyframe, creating one";
        createKeyframe(startFrame);
    }

    if (!hasKeyframe(endFrame, m_currentLayerIndex)) {
        qDebug() << "End frame" << endFrame << "is not a keyframe, creating one";
        createKeyframe(endFrame);
    }

    // Set tweening data on start frame for current layer
    auto& layerFrameData = m_layerFrameData[m_currentLayerIndex];
    auto& startFrameData = layerFrameData[startFrame];
    startFrameData.hasTweening = true;
    startFrameData.tweeningEndFrame = endFrame;
    startFrameData.easingType = easingType;

    // Convert all frames between start and end to extended frames with tweening
    for (int frame = startFrame + 1; frame < endFrame; frame++) {
        auto& frameData = layerFrameData[frame];
        frameData.type = FrameType::ExtendedFrame;
        frameData.sourceKeyframe = startFrame;
        frameData.hasTweening = true;
        frameData.tweeningEndFrame = endFrame;
        frameData.easingType = easingType;

        // Clear any existing items in intermediate frames for this layer
        frameData.items.clear();
        frameData.itemStates.clear();
    }

    emit tweeningApplied(startFrame, endFrame);
    qDebug() << "Tweening applied successfully on layer" << m_currentLayerIndex;
}

// NEW: Remove tweening from a frame span
void Canvas::removeTweening(int startFrame)
{
    auto& layerFrameData = m_layerFrameData[m_currentLayerIndex];
    auto it = layerFrameData.find(startFrame);
    if (it == layerFrameData.end() || !it.value().hasTweening) {
        return;
    }

    int endFrame = it.value().tweeningEndFrame;
    qDebug() << "Removing tweening from frame" << startFrame << "to" << endFrame;

    // Remove tweening from start frame
    it.value().hasTweening = false;
    it.value().tweeningEndFrame = -1;
    it.value().easingType = "linear";

    // Convert intermediate frames back to regular extended frames
    for (int frame = startFrame + 1; frame < endFrame; frame++) {
        auto frameIt = layerFrameData.find(frame);
        if (frameIt != layerFrameData.end()) {
            frameIt.value().hasTweening = false;
            frameIt.value().tweeningEndFrame = -1;
            frameIt.value().easingType = "linear";
            // Keep them as extended frames but without tweening
        }
    }

    emit tweeningRemoved(startFrame);
}

// NEW: Check if drawing is allowed on current frame

bool Canvas::canDrawOnCurrentFrame() const
{
    // Allow drawing on empty frames
    FrameType frameType = getFrameType(m_currentFrame, m_currentLayerIndex);
    if (frameType == FrameType::Empty) {
        return true;
    }

    // Allow drawing on regular keyframes
    if (frameType == FrameType::Keyframe && !isFrameTweened(m_currentFrame, m_currentLayerIndex)) {
        return true;
    }

    // Allow drawing on extended frames without tweening (will auto-convert)
    if (frameType == FrameType::ExtendedFrame && !isFrameTweened(m_currentFrame, m_currentLayerIndex)) {
        return true;
    }

    // Allow drawing on the END frame of a tweening sequence
    if (!m_layerFrameData.contains(m_currentLayerIndex)) return false;

    const auto& layerFrameData = m_layerFrameData[m_currentLayerIndex];
    for (auto it = layerFrameData.begin(); it != layerFrameData.end(); ++it) {
        const FrameData& frameData = it.value();
        if (frameData.hasTweening && frameData.tweeningEndFrame == m_currentFrame) {
            return true;  // This is an end frame of tweening, allow drawing
        }
    }
    // Disallow drawing on tweened intermediate frames
    return false;
}

// NEW: Get easing type for a frame
QString Canvas::getFrameTweeningEasing(int frame, int layerIndex) const
{
    if (!m_layerFrameData.contains(layerIndex)) return "linear";

    const auto& layerFrameData = m_layerFrameData[layerIndex];
    auto it = layerFrameData.find(frame);
    if (it != layerFrameData.end()) {
        return it.value().easingType;
    }
    return "linear";
}


// NEW: Get the end frame of tweening for a start frame
int Canvas::getTweeningEndFrame(int frame, int layerIndex) const
{
    if (!m_layerFrameData.contains(layerIndex)) return -1;

    const auto& layerFrameData = m_layerFrameData[layerIndex];
    auto it = layerFrameData.find(frame);
    if (it != layerFrameData.end() && it.value().hasTweening) {
        return it.value().tweeningEndFrame;
    }
    return -1;
}

void Canvas::onItemAdded(QGraphicsItem* item)
{
    if (!item) return;

    // Check if we need to convert extended frame to keyframe
    if (shouldConvertExtendedFrame()) {
        convertCurrentExtendedFrameToKeyframe();
    }

    // Store item in current frame data
    storeCurrentFrameState();
}

// NEW: Check if current extended frame should be converted
bool Canvas::shouldConvertExtendedFrame() const
{
    FrameType frameType = getFrameType(m_currentFrame);

    // Convert extended frames without tweening when drawing occurs
    return (frameType == FrameType::ExtendedFrame && !isFrameTweened(m_currentFrame));
}

// NEW: Convert current extended frame to keyframe

void Canvas::convertCurrentExtendedFrameToKeyframe()
{
    if (getFrameType(m_currentFrame) != FrameType::ExtendedFrame) {
        return;
    }

    int sourceFrame = getSourceKeyframe(m_currentFrame);
    qDebug() << "Converting extended frame" << m_currentFrame << "to keyframe, source:" << sourceFrame;

    // Get current scene items that will be transformed
    QList<QGraphicsItem*> originalSceneItems;
    for (QGraphicsItem* item : scene()->items()) {
        if (item != m_backgroundRect && getItemLayerIndex(item) == m_currentLayerIndex) {
            originalSceneItems.append(item);
        }
    }

    // CRITICAL: Find ALL frames that reference these same objects
    QList<int> framesToUpdate;
    for (const auto& pair : m_frameData) {
        int frame = pair.first;
        const FrameData& frameData = pair.second;

        // Check if this frame references any of the original objects
        for (QGraphicsItem* originalItem : originalSceneItems) {
            if (frameData.items.contains(originalItem)) {
                framesToUpdate.append(frame);
                break;
            }
        }
    }

    qDebug() << "Frames sharing objects:" << framesToUpdate;

    // Create independent copies for each frame that shares objects
    for (int frame : framesToUpdate) {
        QList<QGraphicsItem*> independentCopies;

        for (QGraphicsItem* originalItem : originalSceneItems) {
            QGraphicsItem* copy = cloneGraphicsItem(originalItem);
            if (copy) {
                independentCopies.append(copy);
            }
        }

        // Update frame data with independent copies
        m_frameData[frame].items = independentCopies;
        m_frameItems[frame] = independentCopies;

        qDebug() << "Created" << independentCopies.size() << "independent copies for frame" << frame;
    }

    // Remove ALL original shared objects from scene
    for (QGraphicsItem* item : originalSceneItems) {
        scene()->removeItem(item);
    }

    // Add the independent copies for current frame to scene
    if (framesToUpdate.contains(m_currentFrame)) {
        auto& currentFrameData = m_frameData[m_currentFrame];
        for (QGraphicsItem* item : currentFrameData.items) {
            scene()->addItem(item);
        }
    }

    // Convert current frame to keyframe
    auto& frameData = m_frameData[m_currentFrame];
    frameData.type = FrameType::Keyframe;
    frameData.sourceKeyframe = -1;
    m_layerKeyframes[m_currentLayerIndex].insert(m_currentFrame);

    qDebug() << "Converted frame" << m_currentFrame << "to independent keyframe";
    emit frameChanged(m_currentFrame);
}

void Canvas::saveStateAfterTransform()
{
    // Force save current frame state
    QList<QGraphicsItem*> currentItems;
    for (QGraphicsItem* item : scene()->items()) {
        if (item != m_backgroundRect && getItemLayerIndex(item) == m_currentLayerIndex) {
            currentItems.append(item);
        }
    }

    auto& frameData = m_frameData[m_currentFrame];
    frameData.items = currentItems;
    m_frameItems[m_currentFrame] = currentItems;

    if (frameData.type != FrameType::ExtendedFrame) {
        frameData.type = FrameType::Keyframe;
        m_layerKeyframes[m_currentLayerIndex].insert(m_currentFrame);
    }

    qDebug() << "Force saved state for frame" << m_currentFrame << "with" << currentItems.size() << "items";
}

void Canvas::setBackgroundColor(const QColor& color) {
    m_backgroundColor = color;
    if (m_backgroundRect) {
        m_backgroundRect->setBrush(QBrush(color));
    }
}

QList<QGraphicsItem*> Canvas::duplicateItems(const QList<QGraphicsItem*>& items) {
    QList<QGraphicsItem*> duplicates;
    for (QGraphicsItem* item : items) {
        if (item) {
            QGraphicsItem* clone = cloneGraphicsItem(item);
            if (clone) {
                duplicates.append(clone);
            }
        }
    }
    return duplicates;
}

// FIX: Add method to ensure object independence during transformations
void Canvas::ensureObjectIndependence(QGraphicsItem* item)
{
    // This method is called before any transformation to ensure the object
    // being transformed doesn't affect other frames.

    if (!item) return;

    FrameType frameType = getFrameType(m_currentFrame);

    // If we're on an extended frame, convert it to keyframe first
    if (frameType == FrameType::ExtendedFrame) {
        convertCurrentExtendedFrameToKeyframe();
        return;
    }

    // If we're on a keyframe, ensure this item is unique to this frame
    if (frameType == FrameType::Keyframe) {
        // Check if this item appears in other frames
        bool itemSharedWithOtherFrames = false;

        for (const auto& pair : m_frameData) {
            int frame = pair.first;
            const FrameData& frameData = pair.second;

            if (frame != m_currentFrame && frameData.items.contains(item)) {
                itemSharedWithOtherFrames = true;
                break;
            }
        }

        if (itemSharedWithOtherFrames) {
            // Create a new independent copy for this frame
            QGraphicsItem* independentCopy = cloneGraphicsItem(item);
            if (independentCopy) {
                // Replace the shared item with the independent copy
                auto& currentFrameData = m_frameData[m_currentFrame];
                int itemIndex = currentFrameData.items.indexOf(item);
                if (itemIndex >= 0) {
                    currentFrameData.items[itemIndex] = independentCopy;
                    m_frameItems[m_currentFrame][itemIndex] = independentCopy;

                    // Replace in scene
                    m_scene->removeItem(item);
                    m_scene->addItem(independentCopy);

                    // Transfer selection state
                    if (item->isSelected()) {
                        independentCopy->setSelected(true);
                        item->setSelected(false);
                    }
                }
            }
        }
    }
}

void Canvas::onDrawingStarted()
{
    // Check if drawing is allowed
    if (!canDrawOnCurrentFrame()) {
        qDebug() << "Drawing not allowed on tweened frame" << m_currentFrame;
        return;
    }

    // Auto-convert extended frames to keyframes only when actually drawing
    if (shouldConvertExtendedFrame()) {
        convertCurrentExtendedFrameToKeyframe();
    }
}


// Also add the global cleanup method (calls all layers)
void Canvas::cleanupInterpolatedItems()
{
    for (int layerIndex = 0; layerIndex < m_layers.size(); ++layerIndex) {
        cleanupInterpolatedItems(layerIndex);
    }

    // Also clean up legacy interpolated items
    for (QGraphicsItem* item : m_interpolatedItems) {
        if (item && scene()->items().contains(item)) {
            scene()->removeItem(item);
            delete item;
        }
    }
    m_interpolatedItems.clear();
    m_isShowingInterpolatedFrame = false;
}

QList<QGraphicsItem*> Canvas::getFrameItems(int frame) const
{
    QList<QGraphicsItem*> allFrameItems;

    // Combine items from all layers for this frame
    for (int layerIndex = 0; layerIndex < m_layers.size(); ++layerIndex) {
        if (m_layerFrameData.contains(layerIndex)) {
            const auto& layerFrameData = m_layerFrameData[layerIndex];
            if (layerFrameData.contains(frame)) {
                allFrameItems.append(layerFrameData[frame].items);
            }
        }
    }

    return allFrameItems;
}


void Canvas::updateGlobalFrameItems(int frame)
{
    m_frameItems[frame] = getFrameItems(frame);
}


std::optional<FrameData> Canvas::getFrameData(int frame) const
{
    auto it = m_frameData.find(frame);
    if (it != m_frameData.end()) {
        return it->second;
    }

    // Check if frame has items in compatibility layer
    auto itemsIt = m_frameItems.find(frame);
    if (itemsIt != m_frameItems.end() && !itemsIt->second.isEmpty()) {
        FrameData data;
        data.items = itemsIt->second;
        data.type = hasKeyframe(frame) ? FrameType::Keyframe : FrameType::ExtendedFrame;
        data.sourceKeyframe = (data.type == FrameType::ExtendedFrame) ? getSourceKeyframe(frame) : -1;
        return data;
    }

    return std::nullopt;
}

double Canvas::getLayerOpacity(int index) const
{
    if (index >= 0 && index < m_layers.size()) {
        LayerData* layer = static_cast<LayerData*>(m_layers[index]);
        return layer->opacity;
    }
    return 1.0;
}

// -------- Serialization and frame data helpers --------

FrameData Canvas::exportFrameData(int layerIndex, int frame)
{
    FrameData result;
    auto layerIt = m_layerFrameData.find(layerIndex);
    if (layerIt != m_layerFrameData.end()) {
        auto frameIt = layerIt->find(frame);
        if (frameIt != layerIt->end()) {
            const FrameData& existing = frameIt.value();
            result = existing;
            result.items = duplicateItems(existing.items);
        }
    }
    return result;
}

void Canvas::removeKeyframe(int layerIndex, int frame)
{
    auto layerIt = m_layerFrameData.find(layerIndex);
    if (layerIt != m_layerFrameData.end()) {
        auto frameIt = layerIt->find(frame);
        if (frameIt != layerIt->end()) {
            for (QGraphicsItem* item : frameIt.value().items) {
                if (item) {
                    if (item->scene())
                        m_scene->removeItem(item);
                    delete item;
                }
            }
            layerIt->remove(frame);
        }
    }

    m_layerKeyframes[layerIndex].remove(frame);

    if (layerIndex >= 0 && layerIndex < m_layers.size()) {
        LayerData* layer = static_cast<LayerData*>(m_layers[layerIndex]);
        layer->clearFrame(frame);
    }
}

void Canvas::importFrameData(int layerIndex, int frame, const FrameData& data)
{
    removeKeyframe(layerIndex, frame);

    FrameData copy = data;
    copy.items = duplicateItems(data.items);

    for (QGraphicsItem* item : copy.items) {
        if (item && !item->scene())
            m_scene->addItem(item);
    }

    m_layerFrameData[layerIndex][frame] = copy;

    if (layerIndex >= 0 && layerIndex < m_layers.size()) {
        LayerData* layer = static_cast<LayerData*>(m_layers[layerIndex]);
        layer->setFrameItems(frame, copy.items);
    }

    if (copy.type == FrameType::Keyframe)
        m_layerKeyframes[layerIndex].insert(frame);
}

QJsonObject Canvas::serializeBrush(const QBrush& brush) const
{
    QJsonObject obj;
    obj["style"] = static_cast<int>(brush.style());
    if (brush.style() == Qt::SolidPattern) {
        obj["color"] = brush.color().name();
    } else if (brush.style() == Qt::LinearGradientPattern ||
               brush.style() == Qt::RadialGradientPattern ||
               brush.style() == Qt::ConicalGradientPattern) {
        const QGradient* grad = brush.gradient();
        if (grad) {
            obj["type"] = static_cast<int>(grad->type());
            obj["spread"] = static_cast<int>(grad->spread());
            QJsonArray stops;
            for (const auto& stop : grad->stops()) {
                QJsonObject s;
                s["pos"] = stop.first;
                s["color"] = stop.second.name();
                stops.append(s);
            }
            obj["stops"] = stops;

            if (grad->type() == QGradient::LinearGradient) {
                const QLinearGradient* lg = static_cast<const QLinearGradient*>(grad);
                obj["startX"] = lg->start().x();
                obj["startY"] = lg->start().y();
                obj["endX"] = lg->finalStop().x();
                obj["endY"] = lg->finalStop().y();
            } else if (grad->type() == QGradient::RadialGradient) {
                const QRadialGradient* rg = static_cast<const QRadialGradient*>(grad);
                obj["centerX"] = rg->center().x();
                obj["centerY"] = rg->center().y();
                obj["focalX"] = rg->focalPoint().x();
                obj["focalY"] = rg->focalPoint().y();
                obj["radius"] = rg->radius();
            } else if (grad->type() == QGradient::ConicalGradient) {
                const QConicalGradient* cg = static_cast<const QConicalGradient*>(grad);
                obj["centerX"] = cg->center().x();
                obj["centerY"] = cg->center().y();
                obj["angle"] = cg->angle();
            }
        }
    } else if (brush.style() != Qt::NoBrush) {
        obj["color"] = brush.color().name();
    }
    return obj;
}

QBrush Canvas::deserializeBrush(const QJsonObject& obj) const
{
    Qt::BrushStyle style = static_cast<Qt::BrushStyle>(obj["style"].toInt(static_cast<int>(Qt::NoBrush)));
    if (style == Qt::SolidPattern) {
        return QBrush(QColor(obj["color"].toString("#000000")));
    } else if (style == Qt::LinearGradientPattern ||
               style == Qt::RadialGradientPattern ||
               style == Qt::ConicalGradientPattern) {
        QGradient::Type type = static_cast<QGradient::Type>(obj["type"].toInt());
        QGradient* grad = nullptr;
        if (type == QGradient::LinearGradient) {
            QLinearGradient* lg = new QLinearGradient(obj["startX"].toDouble(), obj["startY"].toDouble(),
                                                      obj["endX"].toDouble(), obj["endY"].toDouble());
            grad = lg;
        } else if (type == QGradient::RadialGradient) {
            QRadialGradient* rg = new QRadialGradient(obj["centerX"].toDouble(), obj["centerY"].toDouble(),
                                                     obj["radius"].toDouble(), obj["focalX"].toDouble(),
                                                     obj["focalY"].toDouble());
            grad = rg;
        } else if (type == QGradient::ConicalGradient) {
            QConicalGradient* cg = new QConicalGradient(obj["centerX"].toDouble(), obj["centerY"].toDouble(),
                                                        obj["angle"].toDouble());
            grad = cg;
        }

        if (grad) {
            grad->setSpread(static_cast<QGradient::Spread>(obj["spread"].toInt()));
            QJsonArray stops = obj["stops"].toArray();
            for (const QJsonValue& v : stops) {
                QJsonObject s = v.toObject();
                grad->setColorAt(s["pos"].toDouble(), QColor(s["color"].toString("#000000")));
            }
            QBrush brush(*grad);
            delete grad;
            brush.setStyle(style);
            return brush;
        }
    } else if (style != Qt::NoBrush) {
        return QBrush(QColor(obj["color"].toString("#000000")));
    }
    return QBrush();
}

QJsonObject Canvas::serializeGraphicsItem(QGraphicsItem* item) const
{
    QJsonObject json;
    if (!item)
        return json;

    if (auto rectItem = qgraphicsitem_cast<QGraphicsRectItem*>(item)) {
        json["class"] = "rect";
        QRectF r = rectItem->rect();
        json["x"] = r.x();
        json["y"] = r.y();
        json["w"] = r.width();
        json["h"] = r.height();

        QPen pen = rectItem->pen();
        json["penColor"] = pen.color().name();
        json["penWidth"] = pen.widthF();
        json["penStyle"] = static_cast<int>(pen.style());

        json["brush"] = serializeBrush(rectItem->brush());
        if (item == m_backgroundRect) {
            json["isBackground"] = true;
        }
    }
    else if (auto ellipseItem = qgraphicsitem_cast<QGraphicsEllipseItem*>(item)) {
        json["class"] = "ellipse";
        QRectF r = ellipseItem->rect();
        json["x"] = r.x();
        json["y"] = r.y();
        json["w"] = r.width();
        json["h"] = r.height();

        QPen pen = ellipseItem->pen();
        json["penColor"] = pen.color().name();
        json["penWidth"] = pen.widthF();
        json["penStyle"] = static_cast<int>(pen.style());
        json["brush"] = serializeBrush(ellipseItem->brush());
    }
    else if (auto lineItem = qgraphicsitem_cast<QGraphicsLineItem*>(item)) {
        json["class"] = "line";
        QLineF l = lineItem->line();
        json["x1"] = l.x1();
        json["y1"] = l.y1();
        json["x2"] = l.x2();
        json["y2"] = l.y2();

        QPen pen = lineItem->pen();
        json["penColor"] = pen.color().name();
        json["penWidth"] = pen.widthF();
        json["penStyle"] = static_cast<int>(pen.style());
    }
    else if (auto pathItem = qgraphicsitem_cast<QGraphicsPathItem*>(item)) {
        json["class"] = "path";
        QPainterPath path = pathItem->path();
        QJsonArray points;
        for (int i = 0; i < path.elementCount(); ++i) {
            QPainterPath::Element e = path.elementAt(i);
            QJsonObject p; p["x"] = e.x; p["y"] = e.y; points.append(p);
        }
        json["points"] = points;

        QPen pen = pathItem->pen();
        json["penColor"] = pen.color().name();
        json["penWidth"] = pen.widthF();
        json["penStyle"] = static_cast<int>(pen.style());
        json["brush"] = serializeBrush(pathItem->brush());
    }
    else if (auto pixmapItem = qgraphicsitem_cast<QGraphicsPixmapItem*>(item)) {
        json["class"] = "pixmap";
        QPixmap pix = pixmapItem->pixmap();
        QByteArray bytes;
        QBuffer buffer(&bytes);
        buffer.open(QIODevice::WriteOnly);
        pix.save(&buffer, "PNG");
        json["data"] = QString::fromLatin1(bytes.toBase64());
    }
    else if (auto textItem = qgraphicsitem_cast<QGraphicsTextItem*>(item)) {
        json["class"] = "text";
        json["text"] = textItem->toPlainText();
        QFont f = textItem->font();
        json["fontFamily"] = f.family();
        json["fontPointSize"] = f.pointSizeF();
        json["fontBold"] = f.bold();
        json["fontItalic"] = f.italic();
        json["fontUnderline"] = f.underline();
        json["color"] = textItem->defaultTextColor().name();
    }

    json["posX"] = item->pos().x();
    json["posY"] = item->pos().y();
    json["originX"] = item->transformOriginPoint().x();
    json["originY"] = item->transformOriginPoint().y();
    json["rotation"] = item->rotation();
    json["scaleX"] = item->transform().m11();
    json["scaleY"] = item->transform().m22();
    // Store per-item opacity rather than the opacity already multiplied by
    // the layer opacity. The original opacity is kept in item->data(0).
    double baseOpacity = item->data(0).isValid() ? item->data(0).toDouble() : item->opacity();
    json["opacity"] = baseOpacity;
    json["zValue"] = item->zValue();
    json["visible"] = item->isVisible();
    if (auto blur = dynamic_cast<QGraphicsBlurEffect*>(item->graphicsEffect())) {
        json["blur"] = blur->blurRadius();
    } else {
        json["blur"] = 0.0;
    }
    return json;
}

QGraphicsItem* Canvas::deserializeGraphicsItem(const QJsonObject& json) const
{
    QString cls = json["class"].toString();
    QGraphicsItem* item = nullptr;
    if (cls == "rect") {
        QRectF r(json["x"].toDouble(), json["y"].toDouble(), json["w"].toDouble(), json["h"].toDouble());
        auto rectItem = new QGraphicsRectItem(r);
        QPen pen(QColor(json["penColor"].toString("#000000")));
        pen.setWidthF(json["penWidth"].toDouble(1.0));
        pen.setStyle(static_cast<Qt::PenStyle>(json["penStyle"].toInt(Qt::SolidLine)));
        rectItem->setPen(pen);
        rectItem->setBrush(deserializeBrush(json["brush"].toObject()));
        if (json["isBackground"].toBool(false)) {
            rectItem->setFlag(QGraphicsItem::ItemIsSelectable, false);
            rectItem->setFlag(QGraphicsItem::ItemIsMovable, false);
            rectItem->setZValue(-1000);
            rectItem->setData(1, "background");
        }
        item = rectItem;
    }
    else if (cls == "ellipse") {
        QRectF r(json["x"].toDouble(), json["y"].toDouble(), json["w"].toDouble(), json["h"].toDouble());
        auto ellipseItem = new QGraphicsEllipseItem(r);
        QPen pen(QColor(json["penColor"].toString("#000000")));
        pen.setWidthF(json["penWidth"].toDouble(1.0));
        pen.setStyle(static_cast<Qt::PenStyle>(json["penStyle"].toInt(Qt::SolidLine)));
        ellipseItem->setPen(pen);
        ellipseItem->setBrush(deserializeBrush(json["brush"].toObject()));
        item = ellipseItem;
    }
    else if (cls == "line") {
        QLineF l(json["x1"].toDouble(), json["y1"].toDouble(), json["x2"].toDouble(), json["y2"].toDouble());
        auto lineItem = new QGraphicsLineItem(l);
        QPen pen(QColor(json["penColor"].toString("#000000")));
        pen.setWidthF(json["penWidth"].toDouble(1.0));
        pen.setStyle(static_cast<Qt::PenStyle>(json["penStyle"].toInt(Qt::SolidLine)));
        lineItem->setPen(pen);
        item = lineItem;
    }
    else if (cls == "path") {
        QPainterPath path;
        QJsonArray points = json["points"].toArray();
        for (int i = 0; i < points.size(); ++i) {
            QJsonObject p = points[i].toObject();
            if (i == 0)
                path.moveTo(p["x"].toDouble(), p["y"].toDouble());
            else
                path.lineTo(p["x"].toDouble(), p["y"].toDouble());
        }
        auto pathItem = new QGraphicsPathItem(path);
        QPen pen(QColor(json["penColor"].toString("#000000")));
        pen.setWidthF(json["penWidth"].toDouble(1.0));
        pen.setStyle(static_cast<Qt::PenStyle>(json["penStyle"].toInt(Qt::SolidLine)));
        pathItem->setPen(pen);
        pathItem->setBrush(deserializeBrush(json["brush"].toObject()));
        item = pathItem;
    }
    else if (cls == "pixmap") {
        QByteArray bytes = QByteArray::fromBase64(json["data"].toString().toLatin1());
        QPixmap pix;
        pix.loadFromData(bytes, "PNG");
        item = new QGraphicsPixmapItem(pix);
    }
    else if (cls == "text") {
        auto textItem = new QGraphicsTextItem(json["text"].toString());
        QFont f;
        f.setFamily(json["fontFamily"].toString());
        f.setPointSizeF(json["fontPointSize"].toDouble());
        f.setBold(json["fontBold"].toBool());
        f.setItalic(json["fontItalic"].toBool());
        f.setUnderline(json["fontUnderline"].toBool());
        textItem->setFont(f);
        textItem->setDefaultTextColor(QColor(json["color"].toString("#000000")));
        item = textItem;
    }

    if (item) {
        QPointF origin(json["originX"].toDouble(item->boundingRect().center().x()),
                       json["originY"].toDouble(item->boundingRect().center().y()));
        item->setTransformOriginPoint(origin);
        QTransform transform;
        transform.scale(json["scaleX"].toDouble(1.0), json["scaleY"].toDouble(1.0));
        item->setTransform(transform);
        item->setPos(json["posX"].toDouble(), json["posY"].toDouble());
        item->setRotation(json["rotation"].toDouble());
        double baseOpacity = json["opacity"].toDouble(1.0);
        item->setOpacity(baseOpacity);
        item->setData(0, baseOpacity); // preserve individual opacity for layer scaling

        item->setZValue(json["zValue"].toDouble(0.0));
        item->setVisible(json["visible"].toBool(true));

        double blurRadius = json["blur"].toDouble(0.0);
        if (blurRadius > 0.0) {
            auto blur = new QGraphicsBlurEffect();
            blur->setBlurRadius(blurRadius);
            item->setGraphicsEffect(blur);
        }
    }

    return item;
}

QJsonObject Canvas::toJson() const
{
    QJsonObject json;
    json["width"] = m_canvasSize.width();
    json["height"] = m_canvasSize.height();

    QJsonArray layers;
    for (int i = 0; i < m_layers.size(); ++i) {
        LayerData* layer = static_cast<LayerData*>(m_layers[i]);
        QJsonObject layerJson;
        layerJson["name"] = layer->name;
        layerJson["visible"] = layer->visible;
        layerJson["locked"] = layer->locked;
        layerJson["opacity"] = layer->opacity;
        layerJson["blendMode"] = static_cast<int>(layer->blendMode);

        QJsonObject frames;
        auto layerFrames = m_layerFrameData.value(i);
        for (auto it = layerFrames.begin(); it != layerFrames.end(); ++it) {
            const FrameData& f = it.value();
            QJsonObject frameJson;
            frameJson["type"] = static_cast<int>(f.type);
            frameJson["source"] = f.sourceKeyframe;
            frameJson["hasTween"] = f.hasTweening;
            frameJson["tweenEnd"] = f.tweeningEndFrame;
            frameJson["easing"] = f.easingType;

            QJsonArray itemsArray;
            for (QGraphicsItem* item : f.items) {
                itemsArray.append(serializeGraphicsItem(item));
            }
            frameJson["items"] = itemsArray;
            frames[QString::number(it.key())] = frameJson;
        }

        layerJson["frames"] = frames;
        layers.append(layerJson);
    }

    json["layers"] = layers;
    json["currentFrame"] = m_currentFrame;
    json["currentLayer"] = m_currentLayerIndex;

    return json;
}

bool Canvas::fromJson(const QJsonObject& json)
{
    if (json.isEmpty())
        return false;

    setCanvasSize(QSize(json["width"].toInt(800), json["height"].toInt(600)));

    // Remove any existing background before rebuilding from JSON
    if (m_backgroundRect) {
        m_scene->removeItem(m_backgroundRect);
        delete m_backgroundRect;
        m_backgroundRect = nullptr;
    }

    clear();
    m_layerFrameData.clear();
    m_layers.clear();

    // Avoid storing stale state while reconstructing layers
    m_currentLayerIndex = -1;
    m_currentFrame = 1;

    QJsonArray layers = json["layers"].toArray();
    for (int i = 0; i < layers.size(); ++i) {
        QJsonObject layerJson = layers[i].toObject();
        QString name = layerJson["name"].toString(QString("Layer %1").arg(i + 1));
        bool visible = layerJson["visible"].toBool(true);
        double opacity = layerJson["opacity"].toDouble(1.0);
        QPainter::CompositionMode blendMode =
            static_cast<QPainter::CompositionMode>(layerJson["blendMode"].toInt(QPainter::CompositionMode_SourceOver));
        int idx = addLayer(name, visible, opacity, blendMode);
        if (idx == 0 && m_backgroundRect) {
            LayerData* bgLayer = static_cast<LayerData*>(m_layers[idx]);
            bgLayer->addItem(m_backgroundRect, 1);
        }
        setLayerVisible(idx, visible);
        setLayerLocked(idx, layerJson["locked"].toBool(false));
        setLayerOpacity(idx, opacity);

        QJsonObject frames = layerJson["frames"].toObject();
        for (auto it = frames.begin(); it != frames.end(); ++it) {
            int frame = it.key().toInt();
            QJsonObject frameJson = it.value().toObject();
            FrameData data;
            data.type = static_cast<FrameType>(frameJson["type"].toInt());
            data.sourceKeyframe = frameJson["source"].toInt(-1);
            data.hasTweening = frameJson["hasTween"].toBool(false);
            data.tweeningEndFrame = frameJson["tweenEnd"].toInt(-1);
            data.easingType = frameJson["easing"].toString("linear");

            QJsonArray itemsArray = frameJson["items"].toArray();
            for (const QJsonValue& v : itemsArray) {
                QJsonObject itemObj = v.toObject();
                QGraphicsItem* item = deserializeGraphicsItem(itemObj);
                if (item) {
                    if (itemObj["isBackground"].toBool(false) && qgraphicsitem_cast<QGraphicsRectItem*>(item)) {
                        m_backgroundRect = static_cast<QGraphicsRectItem*>(item);
                        m_backgroundRect->setData(1, "background");
                        m_backgroundRect->setData(0, m_backgroundRect->opacity());
                    }
                    data.items.append(item);
                }
            }

            m_layerFrameData[idx][frame] = data;
            LayerData* layer = static_cast<LayerData*>(m_layers[idx]);
            layer->setFrameItems(frame, data.items);

            if (data.type == FrameType::Keyframe)
                m_layerKeyframes[idx].insert(frame);
        }

        // Ensure each layer has a frame 1
        if (!m_layerFrameData[idx].contains(1)) {
            FrameData defaultFrame;
            defaultFrame.type = FrameType::Keyframe;
            LayerData* layer = static_cast<LayerData*>(m_layers[idx]);
            layer->setFrameItems(1, QList<QGraphicsItem*>());
            m_layerFrameData[idx][1] = defaultFrame;
            m_layerKeyframes[idx].insert(1);
        }
    }

    // Ensure a background rectangle exists even if not provided
    if (!m_backgroundRect) {
        m_backgroundRect = new QGraphicsRectItem(m_canvasRect);
        m_backgroundRect->setBrush(QBrush(Qt::white));
        m_backgroundRect->setPen(QPen(Qt::black, 1));
        m_backgroundRect->setFlag(QGraphicsItem::ItemIsSelectable, false);
        m_backgroundRect->setFlag(QGraphicsItem::ItemIsMovable, false);
        m_backgroundRect->setZValue(-1000);
        m_backgroundRect->setData(1, "background");
        m_backgroundRect->setData(0, 1.0);
        if (!m_layers.empty()) {
            LayerData* bgLayer = static_cast<LayerData*>(m_layers[0]);
            bgLayer->addItem(m_backgroundRect, 1);
            m_layerFrameData[0][1].items.append(m_backgroundRect);
        }
    }

    m_currentFrame = json["currentFrame"].toInt(1);
    m_currentLayerIndex = json["currentLayer"].toInt(0);

    loadFrameState(m_currentFrame);

    return true;
}


QString Canvas::getLayerName(int index) const
{
    if (index >= 0 && index < m_layers.size()) {
        LayerData* layer = static_cast<LayerData*>(m_layers[index]);
        if (layer) return layer->name;
    }
    qDebug() << "getLayerName: invalid layer index" << index;
    return QString();
}

bool Canvas::isLayerVisible(int index) const
{
    if (index >= 0 && index < m_layers.size()) {
        LayerData* layer = static_cast<LayerData*>(m_layers[index]);
        if (layer) return layer->visible;
    }
    qDebug() << "isLayerVisible: invalid layer index" << index;
    return false;
}

bool Canvas::isLayerLocked(int index) const
{
    if (index >= 0 && index < m_layers.size()) {
        LayerData* layer = static_cast<LayerData*>(m_layers[index]);
        if (layer) return layer->locked;
    }
    qDebug() << "isLayerLocked: invalid layer index" << index;
    return false;
}