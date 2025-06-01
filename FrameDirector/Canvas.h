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

class Tool;
class VectorGraphicsItem;
class MainWindow;

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

    // Animation
    void setCurrentFrame(int frame);
    int getCurrentFrame() const;

    // Drawing properties
    void setStrokeColor(const QColor& color);
    void setFillColor(const QColor& color);
    void setStrokeWidth(double width);
    QColor getStrokeColor() const;
    QColor getFillColor() const;
    double getStrokeWidth() const;

signals:
    void selectionChanged();
    void mousePositionChanged(QPointF position);
    void zoomChanged(double factor);

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
    void drawGrid(QPainter* painter, const QRectF& rect);
    void drawRulers(QPainter* painter);
    QPointF snapToGrid(const QPointF& point);
    void updateCursor();

    MainWindow* m_mainWindow;
    QGraphicsScene* m_scene;
    Tool* m_currentTool;

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

    // Animation
    int m_currentFrame;

    // Interaction state
    bool m_dragging;
    QPointF m_lastMousePos;
    QRubberBand* m_rubberBand;
    QPoint m_rubberBandOrigin;
};
#endif