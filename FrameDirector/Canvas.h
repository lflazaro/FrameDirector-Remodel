// Canvas.h - Complete implementation with frame extension and tweening support
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
#include <memory>
#include <vector>
#include <map>
#include <set>

class Tool;
class VectorGraphicsItem;
class MainWindow;
class AnimationLayer;

// Enhanced frame type tracking
enum class FrameType {
    Empty,        // No content, no keyframe
    Keyframe,     // Contains unique content/state
    ExtendedFrame // Extends from previous keyframe
};

struct FrameData {
    FrameType type;
    int sourceKeyframe;  // For extended frames, which keyframe they extend from
    QList<QGraphicsItem*> items;
    QMap<QGraphicsItem*, QVariant> itemStates; // Store item states for tweening

    // Tweening support
    bool hasTweening;          // Whether this frame span has tweening applied
    int tweeningEndFrame;      // The end frame of the tween (if this is start frame)
    QString easingType;        // Easing curve type ("linear", "ease-in", "ease-out", etc.)

    FrameData() : type(FrameType::Empty), sourceKeyframe(-1), hasTweening(false), tweeningEndFrame(-1), easingType("linear") {}
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
    void performInterpolation(int currentFrame, int startFrame, int endFrame);
    QList<QGraphicsItem*> getSelectedItems() const;

    // Canvas size and background
    void setCanvasSize(const QSize& size);
    QSize getCanvasSize() const;
    QRectF getCanvasRect() const;
    void setBackgroundColor(const QColor& color);
    QColor getBackgroundColor() const;

    void convertCurrentExtendedFrameToKeyframe();

    // Layer management - Complete interface
    int addLayer(const QString& name = QString());  // Returns layer index
    void addLayerVoid(const QString& name = QString()); // Void version for compatibility
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
    int getLayerOpacity(int index) const;
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

    // Frame type queries and navigation
    bool hasKeyframe(int frame) const;
    bool hasContent(int frame) const;
    FrameType getFrameType(int frame) const;
    int getSourceKeyframe(int frame) const;      // For extended frames
    int getLastKeyframeBefore(int frame) const;  // Find previous keyframe
    int getNextKeyframeAfter(int frame) const;   // Find next keyframe
    QList<int> getFrameSpan(int keyframe) const; // Get all frames extending from keyframe
    void storeCurrentFrameState();
    void clearFrameState();

    // Tweening functionality
    bool hasFrameTweening(int frame) const;
    bool isFrameTweened(int frame) const;
    void applyTweening(int startFrame, int endFrame, const QString& easingType = "linear");
    void removeTweening(int startFrame);
    bool canDrawOnCurrentFrame() const;
    QString getFrameTweeningEasing(int frame) const;
    int getTweeningEndFrame(int frame) const;
    void interpolateFrame(int frame, int startFrame, int endFrame, float t);

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
    void flipSelectedVertical();
    void rotateSelected(double angle);
    void cleanupInterpolatedItems();

    // Item management
    void addItemToCurrentLayer(QGraphicsItem* item);
    void addItemWithUndo(QGraphicsItem* item);
    void removeItemWithUndo(QGraphicsItem* item);

    // Layer compatibility methods
    QList<QGraphicsItem*> getCurrentLayerItems() const;
    QList<QGraphicsItem*> getLayerItems(int layerIndex) const;

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
    // Drawing and rendering
    void drawGrid(QPainter* painter, const QRectF& rect);
    void drawGrid(QPainter* painter); // Alternative signature
    void drawRulers(QPainter* painter);
    void drawCanvasBounds(QPainter* painter, const QRectF& rect);
    void drawBackground(QPainter* painter);
    QList<QGraphicsItem*> m_interpolatedItems;
    bool m_isShowingInterpolatedFrame = false;
    bool m_suppressFrameConversion = false;

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

    // Frame and keyframe management with extension support
    int m_currentFrame;
    std::map<int, FrameData> m_frameData;        // Enhanced frame tracking
    std::map<int, QList<QGraphicsItem*>> m_frameItems; // Keep for compatibility
    std::set<int> m_keyframes;                   // Track keyframes specifically

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
};

#endif // CANVAS_H