// Canvas.h - Complete implementation with frame extension and layer-independent tweening support
#ifndef CANVAS_H
#define CANVAS_H

#include "Common/FrameTypes.h"
#include "Common/CommonIncludes.h"
#include <QGraphicsView>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QFont>
#include <QRubberBand>
#include <QTimer>
#include <QJsonObject>
#include <QJsonArray>
#include <set>
#include <optional>
#include <QHash>
#include <QSet>
#include <QPainter>

using namespace FrameDirector;

class VectorGraphicsItem;
class AnimationLayer;
class Tool;
class MainWindow;

// Forward declaration for layer data
struct LayerData;

class Canvas : public QGraphicsView
{
    Q_OBJECT

public:
    explicit Canvas(MainWindow* parent = nullptr);
    ~Canvas();

    // Scene management
    void clear();
    void selectAll();
    void clearSelection();
    bool hasSelection() const;
    int getSelectionCount() const;
    void deleteSelected();
    QList<QGraphicsItem*> getSelectedItems() const;
    std::optional<FrameData> getFrameData(int frame) const;
    QGraphicsRectItem* getBackgroundRect() const { return m_backgroundRect; }

    // Canvas size and background
    void setCanvasSize(const QSize& size);
    QSize getCanvasSize() const;
    QRectF getCanvasRect() const;
    void setBackgroundColor(const QColor& color);
    QColor getBackgroundColor() const;

    void convertCurrentExtendedFrameToKeyframe();

    // Layer management - Complete interface
    int addLayer(const QString& name = QString(), bool visible = true,
                 double opacity = 1.0,
                 QPainter::CompositionMode blendMode = QPainter::CompositionMode_SourceOver);  // Returns layer index
    void addLayerVoid(const QString& name = QString(), bool visible = true,
                      double opacity = 1.0,
                      QPainter::CompositionMode blendMode = QPainter::CompositionMode_SourceOver); // Void version for compatibility
    void removeLayer(int layerIndex);
    void setCurrentLayer(int layerIndex);
    int getCurrentLayer() const;
    QString getCurrentLayerName() const;
    int getLayerCount() const;
    void setLayerVisible(int layerIndex, bool visible);
    void setLayerVisibility(int index, bool visible); // Alternative signature
    void setLayerLocked(int layerIndex, bool locked);
    void setLayerOpacity(int layerIndex, double opacity);
    void setLayerOpacity(int index, int opacity); // Alternative signature
    void setLayerName(int index, const QString& name);
    bool isLayerVisible(int index) const;
    bool isLayerLocked(int index) const;
    QString getLayerName(int index) const;
    double getLayerOpacity(int index) const;
    void moveLayer(int fromIndex, int toIndex);
    int getItemLayerIndex(QGraphicsItem* item) const;

    // Frame management - Enhanced with full functionality
    void setCurrentFrame(int frame);
    int getCurrentFrame() const;
    void saveFrameState(int frame);  // Keep this method as requested
    void loadFrameState(int frame);

    // Enhanced frame creation methods
    void createKeyframe(int frame);              // Creates a keyframe with current content
    void createBlankKeyframe(int frame);         // Creates empty keyframe, breaks extension
    void createExtendedFrame(int frame);         // Creates frame extending from last keyframe
    void clearCurrentFrameContent();
    void copyFrame(int fromFrame, int toFrame);
    void deleteFrame(int frame);

    // Serialization
    QJsonObject toJson() const;
    bool fromJson(const QJsonObject& json);

    // Frame data helpers for undo/redo
    FrameData exportFrameData(int layerIndex, int frame);
    void importFrameData(int layerIndex, int frame, const FrameData& data);
    void removeKeyframe(int layerIndex, int frame);

    // Frame type queries and navigation
    bool hasKeyframe(int frame, int layerIndex) const;
    bool hasContent(int frame, int layerIndex) const;
    void storeCurrentFrameState();
    void clearFrameState();

    // Layer-aware tweening and navigation
    FrameType getFrameType(int frame, int layerIndex) const;
    int getSourceKeyframe(int frame, int layerIndex) const;      // For extended frames
    int getLastKeyframeBefore(int frame, int layerIndex) const;  // Find previous keyframe
    int getNextKeyframeAfter(int frame, int layerIndex) const;   // Find next keyframe
    bool hasFrameTweening(int frame, int layerIndex) const;
    bool isFrameTweened(int frame, int layerIndex) const;
    void applyTweening(int startFrame, int endFrame, const QString& easingType = "linear");
    void removeTweening(int startFrame);
    bool canDrawOnCurrentFrame() const;
    QString getFrameTweeningEasing(int frame, int layerIndex) const;
    int getTweeningEndFrame(int frame, int layerIndex) const;
    void interpolateFrame(int frame, int startFrame, int endFrame, float t, int layerIndex);

    // Convenience wrappers for current layer
    bool hasKeyframe(int frame) const { return hasKeyframe(frame, m_currentLayerIndex); }
    bool hasContent(int frame) const { return hasContent(frame, m_currentLayerIndex); }
    FrameType getFrameType(int frame) const { return getFrameType(frame, m_currentLayerIndex); }
    int getSourceKeyframe(int frame) const { return getSourceKeyframe(frame, m_currentLayerIndex); }
    int getLastKeyframeBefore(int frame) const { return getLastKeyframeBefore(frame, m_currentLayerIndex); }
    int getNextKeyframeAfter(int frame) const { return getNextKeyframeAfter(frame, m_currentLayerIndex); }
    bool hasFrameTweening(int frame) const { return hasFrameTweening(frame, m_currentLayerIndex); }
    bool isFrameTweened(int frame) const { return isFrameTweened(frame, m_currentLayerIndex); }
    QString getFrameTweeningEasing(int frame) const { return getFrameTweeningEasing(frame, m_currentLayerIndex); }
    int getTweeningEndFrame(int frame) const { return getTweeningEndFrame(frame, m_currentLayerIndex); }

    // Onion skinning
    void setOnionSkinEnabled(bool enabled);
    bool isOnionSkinEnabled() const;
    void setOnionSkinRange(int before, int after);

    // Tools and drawing
    void setCurrentTool(Tool* tool);
    Tool* getCurrentTool() const;

    // Drawing properties
    void setStrokeColor(const QColor& color);
    void setFillColor(const QColor& color);
    void setStrokeWidth(double width);
    QColor getStrokeColor() const;
    QColor getFillColor() const;
    double getStrokeWidth() const;
    void clearLayerFromScene(int layerIndex);
    void ensureObjectIndependence(QGraphicsItem* item);
    void saveStateAfterTransform();

    // Zoom and view
    void zoomIn();
    void zoomOut();
    void zoomToFit();
    void setZoomFactor(double factor);
    double getZoomFactor() const;

    // Grid and guides - Complete interface
    void setGridVisible(bool visible);
    void setShowGrid(bool show); // Alternative signature
    void setSnapToGrid(bool snap);
    void setRulersVisible(bool visible);
    void setGridSize(int size);
    bool isGridVisible() const;
    bool isSnapToGrid() const;
    bool areRulersVisible() const;
    int getGridSize() const;

    // Object operations
    void groupSelectedItems();
    void groupSelection(); // Alternative name
    void ungroupSelectedItems();
    void ungroupSelection(); // Alternative name
    void alignSelectedItems(int alignment);
    void bringSelectedToFront();
    void bringSelectedForward();
    void sendSelectedBackward();
    void sendSelectedToBack();
    void flipSelectedHorizontal();
    void updateGlobalFrameItems(int frame);
    void flipSelectedVertical();
    void rotateSelected(double angle);

    QList<QGraphicsItem*> getFrameItems(int frame) const;

    // Item management
    void addItemToCurrentLayer(QGraphicsItem* item);
    void addItemWithUndo(QGraphicsItem* item);
    void removeItemWithUndo(QGraphicsItem* item);

    // Layer compatibility methods
    QList<QGraphicsItem*> getCurrentLayerItems() const;
    QList<QGraphicsItem*> getLayerItems(int layerIndex) const;

    void removeItemFromAllFrames(QGraphicsItem* item);
    bool isValidItem(QGraphicsItem* item) const;

signals:
    void selectionChanged();
    void mousePositionChanged(QPointF position);
    void zoomChanged(double factor);
    void layerChanged(int layerIndex);
    void frameChanged(int frame);
    void keyframeCreated(int frame);
    void frameExtended(int fromFrame, int toFrame);
    void canvasResized(const QSize& size);

    // Tweening signals
    void tweeningApplied(int startFrame, int endFrame);
    void tweeningRemoved(int frame);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void drawBackground(QPainter* painter, const QRectF& rect) override;
    void drawForeground(QPainter* painter, const QRectF& rect) override;

private slots:
    void onSceneSelectionChanged();

private:
    // Scene and UI setup
    void setupScene();
    void setupDefaultLayers();
    void updateSceneRect();
    QGraphicsItem* cloneGraphicsItem(QGraphicsItem* item);
    void clearFrameItems(int frame);
    QJsonObject serializeBrush(const QBrush& brush) const;
    QBrush deserializeBrush(const QJsonObject& json) const;
    QJsonObject serializeGraphicsItem(QGraphicsItem* item) const;
    QGraphicsItem* deserializeGraphicsItem(const QJsonObject& json) const;

    void applyOnionSkin(int frame);
    void clearOnionSkinItems();

    // Drawing and rendering
    void drawGrid(QPainter* painter, const QRectF& rect);
    void drawGrid(QPainter* painter); // Alternative signature
    void drawRulers(QPainter* painter);
    void drawCanvasBounds(QPainter* painter, const QRectF& rect);
    void drawBackground(QPainter* painter);

    // ENHANCED: Layer-aware tweening and interpolation methods
    void cleanupInterpolatedItems();  // Global cleanup (all layers)
    void cleanupInterpolatedItems(int layerIndex);  // Layer-specific cleanup

    // Utility functions
    QPointF snapToGrid(const QPointF& point);
    void updateCursor();
    void updateAllLayerZValues();

    // Frame management helpers
    void copyItemsToFrame(int fromFrame, int toFrame);
    void captureCurrentStateAsKeyframe(int frame);
    QList<QGraphicsItem*> duplicateItems(const QList<QGraphicsItem*>& items);

    // Drawing operation detection for auto-conversion
    void onDrawingStarted();
    void onItemAdded(QGraphicsItem* item);
    bool shouldConvertExtendedFrame() const;

    // Core components
    MainWindow* m_mainWindow;
    QGraphicsScene* m_scene;
    Tool* m_currentTool;

    // Canvas properties
    QSize m_canvasSize;
    QRectF m_canvasRect;
    QGraphicsRectItem* m_backgroundRect;
    QColor m_backgroundColor;

    // Layer management using LayerData structures
    std::vector<void*> m_layers;  // Contains LayerData* pointers
    std::vector<LayerData> m_layersData; // Alternative storage
    int m_currentLayerIndex;

    // ENHANCED: Layer-specific tweening and animation data
    QHash<int, QHash<int, FrameData>> m_layerFrameData;  // layerIndex -> frame -> FrameData
    QHash<int, QList<QGraphicsItem*>> m_layerInterpolatedItems;  // layerIndex -> interpolated items
    QHash<int, bool> m_layerShowingInterpolated;  // layerIndex -> isShowingInterpolated flag

    // Frame and keyframe management with extension support (legacy - for compatibility)
    int m_currentFrame;
    std::map<int, FrameData> m_frameData;        // Enhanced frame tracking (legacy global)
    std::map<int, QList<QGraphicsItem*>> m_frameItems; // Keep for compatibility
    QHash<int, QSet<int>> m_layerKeyframes;      // layerIndex -> keyframe set
    QList<QGraphicsItem*> m_interpolatedItems;  // Legacy global interpolated items

    // Tweening state flags
    bool m_isShowingInterpolatedFrame = false;  // Legacy global flag
    bool m_suppressFrameConversion = false;     // Prevents unwanted frame conversions

    // View properties
    double m_zoomFactor;
    bool m_gridVisible;
    bool m_showGrid; // Alternative property
    bool m_snapToGrid;
    bool m_rulersVisible;
    double m_gridSize;
    int m_gridSizeInt; // Alternative property

    // Drawing properties
    QColor m_strokeColor;
    QColor m_fillColor;
    double m_strokeWidth;

    // Interaction state
    bool m_dragging;
    bool m_destroying;
    QPointF m_lastMousePos;
    QRubberBand* m_rubberBand;
    QPoint m_rubberBandOrigin;
    bool m_rubberBandActive;
    bool m_panning;
    QPoint m_lastPanPoint;

    // Onion skin state
    bool m_onionSkinEnabled;
    int m_onionSkinBefore;
    int m_onionSkinAfter;
    QSet<QGraphicsItem*> m_onionSkinItems;
};

#endif // CANVAS_H