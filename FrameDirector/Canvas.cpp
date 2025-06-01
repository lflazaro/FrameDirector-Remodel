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

Canvas::Canvas(MainWindow* parent)
    : QGraphicsView(parent)
    , m_mainWindow(parent)
    , m_scene(nullptr)
    , m_currentTool(nullptr)
    , m_zoomFactor(1.0)
    , m_gridVisible(true)
    , m_snapToGrid(false)
    , m_rulersVisible(false)
    , m_gridSize(20.0)
    , m_strokeColor(Qt::black)
    , m_fillColor(Qt::transparent)
    , m_strokeWidth(2.0)
    , m_currentFrame(1)
    , m_dragging(false)
    , m_rubberBand(nullptr)
{
    setupScene();

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

    // Set scene background color
    if (m_scene) {
        m_scene->setBackgroundBrush(QBrush(QColor(64, 64, 64)));
    }

    // Connect scene signals
    connect(m_scene, &QGraphicsScene::selectionChanged,
        this, &Canvas::onSceneSelectionChanged);

    // Make sure the canvas has focus to receive key events
    setFocusPolicy(Qt::StrongFocus);

    qDebug() << "Canvas created with scene:" << m_scene;
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

    // Set a reasonable scene size
    m_scene->setSceneRect(-1000, -1000, 2000, 2000);

    // Set the scene background
    m_scene->setBackgroundBrush(QBrush(QColor(64, 64, 64)));

    setScene(m_scene);

    qDebug() << "Scene set up with rect:" << m_scene->sceneRect();
}

void Canvas::clear()
{
    if (m_scene) {
        m_scene->clear();
        emit selectionChanged();
    }
}

void Canvas::selectAll()
{
    if (m_scene) {
        QPainterPath path;
        path.addRect(m_scene->itemsBoundingRect());

        // Select all selectable items
        QList<QGraphicsItem*> allItems = m_scene->items();
        for (QGraphicsItem* item : allItems) {
            if (item->flags() & QGraphicsItem::ItemIsSelectable) {
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

void Canvas::deleteSelected()
{
    if (!m_scene) return;

    QList<QGraphicsItem*> selectedItems = m_scene->selectedItems();
    for (QGraphicsItem* item : selectedItems) {
        m_scene->removeItem(item);
        delete item;
    }
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

    QRectF itemsRect = m_scene->itemsBoundingRect();
    if (!itemsRect.isEmpty()) {
        fitInView(itemsRect, Qt::KeepAspectRatio);
        m_zoomFactor = transform().m11();
        emit zoomChanged(m_zoomFactor);
    }
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
}

void Canvas::bringSelectedToFront()
{
    if (!m_scene) return;

    QList<QGraphicsItem*> selectedItems = m_scene->selectedItems();
    qreal maxZ = 0;

    // Find the highest Z value
    for (QGraphicsItem* item : m_scene->items()) {
        if (item->zValue() > maxZ) {
            maxZ = item->zValue();
        }
    }

    // Move selected items to front
    for (QGraphicsItem* item : selectedItems) {
        item->setZValue(maxZ + 1);
        maxZ += 1;
    }
}

void Canvas::bringSelectedForward()
{
    if (!m_scene) return;

    QList<QGraphicsItem*> selectedItems = m_scene->selectedItems();
    for (QGraphicsItem* item : selectedItems) {
        item->setZValue(item->zValue() + 1);
    }
}

void Canvas::sendSelectedBackward()
{
    if (!m_scene) return;

    QList<QGraphicsItem*> selectedItems = m_scene->selectedItems();
    for (QGraphicsItem* item : selectedItems) {
        item->setZValue(item->zValue() - 1);
    }
}

void Canvas::sendSelectedToBack()
{
    if (!m_scene) return;

    QList<QGraphicsItem*> selectedItems = m_scene->selectedItems();
    qreal minZ = 0;

    // Find the lowest Z value
    for (QGraphicsItem* item : m_scene->items()) {
        if (item->zValue() < minZ) {
            minZ = item->zValue();
        }
    }

    // Move selected items to back
    for (QGraphicsItem* item : selectedItems) {
        item->setZValue(minZ - 1);
        minZ -= 1;
    }
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
}

void Canvas::setCurrentFrame(int frame)
{
    m_currentFrame = frame;
    // Update display based on current frame
    viewport()->update();
}

int Canvas::getCurrentFrame() const
{
    return m_currentFrame;
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
        // Default selection behavior
        if (event->button() == Qt::LeftButton) {
            QGraphicsItem* item = m_scene->itemAt(scenePos, transform());
            if (!item) {
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

            // Use Qt 6 compatible selection method
            QPainterPath path;
            path.addPolygon(selectionArea);

            // Iterate through items and select manually
            QList<QGraphicsItem*> itemsInArea = m_scene->items(path, Qt::IntersectsItemShape);
            for (QGraphicsItem* item : itemsInArea) {
                if (item->flags() & QGraphicsItem::ItemIsSelectable) {
                    item->setSelected(true);
                }
            }

            m_rubberBand->hide();
        }
        QGraphicsView::mouseReleaseEvent(event);
    }

    emit mousePositionChanged(scenePos);
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

void Canvas::drawBackground(QPainter* painter, const QRectF& rect)
{
    // Draw the background color first
    QGraphicsView::drawBackground(painter, rect);

    if (m_gridVisible) {
        drawGrid(painter, rect);
    }
}

void Canvas::drawForeground(QPainter* painter, const QRectF& rect)
{
    QGraphicsView::drawForeground(painter, rect);

    if (m_rulersVisible) {
        drawRulers(painter);
    }
}

void Canvas::drawGrid(QPainter* painter, const QRectF& rect)
{
    painter->save();

    QPen gridPen(QColor(96, 96, 96), 0.5);
    painter->setPen(gridPen);

    double left = int(rect.left()) - (int(rect.left()) % int(m_gridSize));
    double top = int(rect.top()) - (int(rect.top()) % int(m_gridSize));

    QVector<QLineF> lines;

    // Vertical lines
    for (double x = left; x < rect.right(); x += m_gridSize) {
        lines.append(QLineF(x, rect.top(), x, rect.bottom()));
    }

    // Horizontal lines
    for (double y = top; y < rect.bottom(); y += m_gridSize) {
        lines.append(QLineF(rect.left(), y, rect.right(), y));
    }

    painter->drawLines(lines);
    painter->restore();
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

void Canvas::onSceneSelectionChanged()
{
    emit selectionChanged();
}