#ifndef VECTORGRAPHICSITEM_H
#define VECTORGRAPHICSITEM_H

#include <QGraphicsItem>
#include <QPainter>
#include <QPen>
#include <QBrush>
#include <QStyleOptionGraphicsItem>
#include <QGraphicsSceneMouseEvent>
#include <QJsonObject>

class VectorGraphicsItem : public QGraphicsItem
{
public:
    enum ItemType {
        VectorPath,
        VectorRectangle,
        VectorEllipse,
        VectorLine,
        VectorText,
        VectorGroup
    };

    explicit VectorGraphicsItem(QGraphicsItem* parent = nullptr);
    virtual ~VectorGraphicsItem() = default;

    // QGraphicsItem interface
    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
    QPainterPath shape() const override;
    int type() const override;

    // Vector properties
    void setStroke(const QPen& pen);
    QPen getStroke() const;
    void setFill(const QBrush& brush);
    QBrush getFill() const;

    // Animation support
    void setAnimationFrame(int frame);
    int getAnimationFrame() const;
    void setKeyframe(bool keyframe);
    bool isKeyframe() const;

    // Serialization
    virtual QJsonObject toJson() const;
    virtual void fromJson(const QJsonObject& json);

    // Custom item type
    virtual ItemType getItemType() const = 0;

    // Selection handles
    void setShowSelectionHandles(bool show);
    bool showSelectionHandles() const;

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;

    virtual void drawSelectionHandles(QPainter* painter);
    QRectF getSelectionHandleRect(int handleIndex) const;
    int getHandleAtPoint(const QPointF& point) const;

    QPen m_stroke;
    QBrush m_fill;
    QRectF m_boundingRect;
    int m_animationFrame;
    bool m_isKeyframe;
    bool m_showSelectionHandles;

    // Selection handle state
    bool m_resizing;
    int m_resizeHandle;
    QPointF m_lastMousePos;
};

// Utility helper to duplicate vector graphics items
VectorGraphicsItem* cloneVectorGraphicsItem(const VectorGraphicsItem* item);

#endif
