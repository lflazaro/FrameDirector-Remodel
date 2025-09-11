#ifndef ANIMATIONKEYFRAME_H
#define ANIMATIONKEYFRAME_H

#include <QPointF>
#include <QSizeF>
#include <QColor>
#include <QTransform>
#include <QGraphicsItem>
#include <QEasingCurve>
#include <vector>
#include <memory>

class AnimationKeyframe
{
public:
    struct ItemState {
        QPointF position;
        QSizeF size;
        double rotation;
        QPointF transformOrigin;
        QPointF scale;
        double opacity;
        double blurRadius;
        QColor strokeColor;
        QColor fillColor;
        double strokeWidth;
        QTransform transform;
        bool visible;
    };

    explicit AnimationKeyframe(int frame);
    ~AnimationKeyframe();

    // Frame properties
    int getFrame() const;
    void setFrame(int frame);

    // Item states
    void captureItemState(QGraphicsItem* item);
    void applyItemState(QGraphicsItem* item) const;
    bool hasItemState(QGraphicsItem* item) const;
    void removeItemState(QGraphicsItem* item);

    // Interpolation
    void setEasing(QEasingCurve::Type easing);
    QEasingCurve::Type getEasing() const;

    static void interpolateBetween(const AnimationKeyframe* from,
        const AnimationKeyframe* to,
        double t,
        QGraphicsItem* item);

    // Keyframe types
    enum KeyframeType {
        Linear,
        Hold,
        Ease
    };

    void setType(KeyframeType type);
    KeyframeType getType() const;

    // Selection
    void setSelected(bool selected);
    bool isSelected() const;

private:
    static double interpolateValue(double from, double to, double t);
    static QPointF interpolatePoint(const QPointF& from, const QPointF& to, double t);
    static QColor interpolateColor(const QColor& from, const QColor& to, double t);

    int m_frame;
    KeyframeType m_type;
    QEasingCurve::Type m_easing;
    bool m_selected;

    std::map<QGraphicsItem*, ItemState> m_itemStates;
};
#endif