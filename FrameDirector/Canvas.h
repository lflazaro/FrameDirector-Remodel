// Canvas.h
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

class Tool;
class VectorGraphicsItem;
class MainWindow;
class AnimationLayer;

// Custom layer item that holds all graphics for a layer
class LayerGraphicsGroup : public QGraphicsItemGroup
{
public:
    LayerGraphicsGroup(int layerIndex, const QString& name);

    int m_layerIndex;
    QString m_layerName;
    void setLayerVisible(bool visible);
    void setLayerLocked(bool locked);
    void setLayerOpacity(double opacity);

    bool isLayerVisible() const { return m_visible; }
    bool isLayerLocked() const { return m_locked; }
    double getLayerOpacity() const { return m_opacity; }
    int getLayerIndex() const { return m_layerIndex; }
    QString getLayerName() const { return m_layerName; }

    void setLayerName(const QString& name) { m_layerName = name; }

private:
    bool m_visible;
    bool m_locked;
    double m_opacity;
};

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
    LayerGraphicsGroup* getLayer(int index) const;
    void setLayerVisible(int layerIndex, bool visible);
    void setLayerLocked(int layerIndex, bool locked);
    void setLayerOpacity(int layerIndex, double opacity);
    void moveLayer(int fromIndex, int toIndex);

    // Frame management
    void setCurrentFrame(int frame);
    int getCurrentFrame() const;
    void saveFrameState(int frame);
    void loadFrameState(int frame);
    void createKeyframe(int frame);
    bool hasKeyframe(int frame) const;

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
    void setupScene();
    void setupDefaultLayers();
    void drawGrid(QPainter* painter, const QRectF& rect);
    void drawRulers(QPainter* painter);
    void drawCanvasBounds(QPainter* painter, const QRectF& rect);
    QPointF snapToGrid(const QPointF& point);
    void updateCursor();

    MainWindow* m_mainWindow;
    QGraphicsScene* m_scene;
    Tool* m_currentTool;

    // Canvas properties
    QSize m_canvasSize;
    QRectF m_canvasRect;
    QGraphicsRectItem* m_backgroundRect;

    // Layer management
    std::vector<LayerGraphicsGroup*> m_layers;
    int m_currentLayerIndex;

    // Frame and keyframe management
    int m_currentFrame;
    std::map<int, std::map<int, QList<QGraphicsItem*>>> m_frameStates; // frame -> layer -> items
    std::set<int> m_keyframes;

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
    QRubberBand* m_rubberBand;
    QPoint m_rubberBandOrigin;
};
#endif