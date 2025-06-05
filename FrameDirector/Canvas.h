// Canvas.h - Enhanced with frame extension support
#ifndef CANVAS_H
#define CANVAS_H

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QGraphicsRectItem>
#include <QGraphicsEllipseItem>
#include <QGraphicsLineItem>
#include <QGraphicsTextItem>
#include <QGraphicsPathItem>
#include <QGraphicsItemGroup>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QPainter>
#include <QPen>
#include <QBrush>
#include <QColor>
#include <QFont>
#include <QRubberBand>
#include <QTimer>
#include <QEasingCurve>  // FIXED: Add missing include
#include <memory>
#include <vector>
#include <map>
#include <set>

class Tool;
class VectorGraphicsItem;
class MainWindow;
class AnimationLayer;

// Enhanced frame type tracking with tweening support
enum class FrameType {
    Empty,        // No content, no keyframe
    Keyframe,     // Contains unique content/state
    ExtendedFrame, // Extends from previous keyframe
    TweenedFrame  // Part of a tweened span
};

// Tweening types (Flash-style)
enum class TweenType {
    None,         // No tweening
    Motion,       // Position, rotation, scale tweening
    Shape,        // Morphing between shapes (future)
    Classic       // Traditional Flash-style motion tween
};

// Layer-specific frame data
struct LayerFrameData {
    FrameType type;
    int sourceKeyframe;                    // For extended/tweened frames
    QList<QGraphicsItem*> items;
    QMap<QGraphicsItem*, QVariant> itemStates;

    // NEW: Tweening properties (per layer)
    TweenType tweenType;
    bool hasTweening;
    int tweenStartFrame;
    int tweenEndFrame;
    QEasingCurve::Type easingType;  // FIXED: Now QEasingCurve is properly included

    LayerFrameData() : type(FrameType::Empty), sourceKeyframe(-1),
        tweenType(TweenType::None), hasTweening(false),
        tweenStartFrame(-1), tweenEndFrame(-1),
        easingType(QEasingCurve::Linear) {  // FIXED: Now QEasingCurve::Linear is accessible
    }
};

struct FrameData {
    FrameType type;
    int sourceKeyframe;  // For extended frames, which keyframe they extend from
    QList<QGraphicsItem*> items;
    QMap<QGraphicsItem*, QVariant> itemStates; // Store item states for tweening

    FrameData() : type(FrameType::Empty), sourceKeyframe(-1) {}
};

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

    // Canvas size and background
    void setCanvasSize(const QSize& size);
    QSize getCanvasSize() const;
    QRectF getCanvasRect() const;

    // Layer management
    int addLayer(const QString& name = QString());
    void removeLayer(int layerIndex);
    void setCurrentLayer(int layerIndex);
    int getCurrentLayer() const;
    int getLayerCount() const;
    void setLayerVisible(int layerIndex, bool visible);
    void setLayerLocked(int layerIndex, bool locked);
    void setLayerOpacity(int layerIndex, double opacity);
    void moveLayer(int fromIndex, int toIndex);

    // ENHANCED: Frame management with proper keyframe/frame distinction
    void setCurrentFrame(int frame);
    int getCurrentFrame() const;
    void saveFrameState(int frame);  // Keep this method as requested
    void loadFrameState(int frame);

    // NEW: Enhanced frame creation methods
    void createKeyframe(int frame);              // Creates a keyframe with current content
    void createBlankKeyframe(int frame);         // Creates empty keyframe, breaks extension
    void createExtendedFrame(int frame);         // Creates frame extending from last keyframe
    void clearCurrentFrameContent();

    void applyTweening(int layer, int startFrame, int endFrame, TweenType type = TweenType::Motion);
    void removeTweening(int layer, int startFrame, int endFrame);
    bool hasTweening(int layer, int frame) const;
    TweenType getTweenType(int layer, int frame) const;
    QList<int> getTweeningFrames(int layer, int startFrame, int endFrame) const;

    void convertExtendedFrameToKeyframe(int frame, int layer);
    bool canDrawOnFrame(int frame, int layer) const;  // Check if drawing is allowed

    // NEW: Frame type queries
    bool hasKeyframe(int frame) const;
    bool hasContent(int frame, int layer) const;
    FrameType getFrameType(int frame, int layer) const;
    int getSourceKeyframe(int frame) const;      // For extended frames
    int getLastKeyframeBefore(int frame) const;  // Find previous keyframe
    int getNextKeyframeAfter(int frame) const;   // Find next keyframe
    QList<int> getFrameSpan(int keyframe) const; // Get all frames extending from keyframe
    bool isExtendedFrame(int frame, int layer) const;
    bool isTweenedFrame(int frame, int layer) const;

    // Tools
    void setCurrentTool(Tool* tool);
    Tool* getCurrentTool() const;

    // Zoom and view
    void zoomIn();
    void zoomOut();
    void zoomToFit();
    void setZoomFactor(double factor);
    double getZoomFactor() const;

    // Grid and guides
    void setGridVisible(bool visible);
    void setSnapToGrid(bool snap);
    void setRulersVisible(bool visible);
    bool isGridVisible() const;
    bool isSnapToGrid() const;
    bool areRulersVisible() const;

    // Object operations
    void groupSelectedItems();
    void ungroupSelectedItems();
    void alignSelectedItems(int alignment);
    void bringSelectedToFront();
    void bringSelectedForward();
    void sendSelectedBackward();
    void sendSelectedToBack();
    void flipSelectedHorizontal();
    void flipSelectedVertical();
    void rotateSelected(double angle);
    void storeCurrentFrameState();
    void clearFrameState();

    // Drawing properties
    void setStrokeColor(const QColor& color);
    void setFillColor(const QColor& color);
    void setStrokeWidth(double width);
    QColor getStrokeColor() const;
    QColor getFillColor() const;
    double getStrokeWidth() const;

    // Item management
    void addItemToCurrentLayer(QGraphicsItem* item);

signals:
    void selectionChanged();
    void mousePositionChanged(QPointF position);
    void zoomChanged(double factor);
    void layerChanged(int layerIndex);
    void frameChanged(int frame);
    void keyframeCreated(int frame);
    void frameExtended(int fromFrame, int toFrame);  // NEW: Signal for frame extensions
    void tweeningApplied(int layer, int startFrame, int endFrame, TweenType type);
    void tweeningRemoved(int layer, int startFrame, int endFrame);
    void frameAutoConverted(int frame, int layer);  // When extended frame becomes keyframe

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
    std::map<int, std::map<int, LayerFrameData>> m_layerFrameData;  // [layer][frame] = data
    void calculateTweenedFrame(int layer, int frame);
    void interpolateItemsAtFrame(int layer, int frame, double t);
    QVariant interpolateItemState(const QVariant& fromState, const QVariant& toState, double t);
    void checkAndConvertExtendedFrame(int frame, int layer);
    void updateToolsAvailability();  // Update UI based on current frame/layer state
    QRubberBand* m_rubberBand;
    QPoint m_rubberBandOrigin;
    bool m_rubberBandActive;
    void setupScene();
    void setupDefaultLayers();
    void drawGrid(QPainter* painter, const QRectF& rect);
    void drawRulers(QPainter* painter);
    void drawCanvasBounds(QPainter* painter, const QRectF& rect);
    QPointF snapToGrid(const QPointF& point);
    void updateCursor();
    bool m_destroying;

    // ENHANCED: Frame management helpers
    void updateAllLayerZValues();
    int getItemLayerIndex(QGraphicsItem* item);
    void copyItemsToFrame(int fromFrame, int toFrame);
    void captureCurrentStateAsKeyframe(int frame);
    QList<QGraphicsItem*> duplicateItems(const QList<QGraphicsItem*>& items);

    MainWindow* m_mainWindow;
    QGraphicsScene* m_scene;
    Tool* m_currentTool;

    // Canvas properties
    QSize m_canvasSize;
    QRectF m_canvasRect;
    QGraphicsRectItem* m_backgroundRect;

    // Layer management using LayerData structures
    std::vector<void*> m_layers;  // Contains LayerData* pointers
    int m_currentLayerIndex;

    // ENHANCED: Frame and keyframe management with extension support
    int m_currentFrame;
    std::map<int, FrameData> m_frameData;        // Enhanced frame tracking
    std::map<int, QList<QGraphicsItem*>> m_frameItems; // Keep for compatibility
    std::set<int> m_keyframes;                   // Track keyframes specifically

    // View properties
    double m_zoomFactor;
    bool m_gridVisible;
    bool m_snapToGrid;
    bool m_rulersVisible;
    double m_gridSize;

    // Drawing properties
    QColor m_strokeColor;
    QColor m_fillColor;
    double m_strokeWidth;

    // Interaction state
    bool m_dragging;
    QPointF m_lastMousePos;
};

#endif // CANVAS_H