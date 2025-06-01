#ifndef ANIMATIONLAYER_H
#define ANIMATIONLAYER_H

#include <QString>
#include <QColor>
#include <QGraphicsItem>
#include <QPropertyAnimation>
#include <vector>
#include <memory>

class AnimationKeyframe;
class VectorGraphicsItem;

class AnimationLayer
{
public:
    explicit AnimationLayer(const QString& name);
    ~AnimationLayer();

    // Layer properties
    void setName(const QString& name);
    QString getName() const;

    void setVisible(bool visible);
    bool isVisible() const;

    void setLocked(bool locked);
    bool isLocked() const;

    void setOpacity(double opacity);
    double getOpacity() const;

    void setColor(const QColor& color);
    QColor getColor() const;

    // Items management
    void addItem(QGraphicsItem* item);
    void removeItem(QGraphicsItem* item);
    void clearItems();
    QList<QGraphicsItem*> getItems() const;

    // Keyframes
    void addKeyframe(int frame, std::unique_ptr<AnimationKeyframe> keyframe);
    void removeKeyframe(int frame);
    AnimationKeyframe* getKeyframe(int frame) const;
    bool hasKeyframe(int frame) const;
    std::vector<int> getKeyframeNumbers() const;

    // Animation
    void setCurrentFrame(int frame);
    int getCurrentFrame() const;
    void interpolateToFrame(int frame);

    // Onion skinning
    void setOnionSkinEnabled(bool enabled);
    bool isOnionSkinEnabled() const;
    void setOnionSkinFrames(int before, int after);
    void getOnionSkinFrames(int& before, int& after) const;

private:
    void updateItemsVisibility();
    void interpolateBetweenKeyframes(int fromFrame, int toFrame, int currentFrame);
    AnimationKeyframe* findPreviousKeyframe(int frame) const;
    AnimationKeyframe* findNextKeyframe(int frame) const;

    QString m_name;
    bool m_visible;
    bool m_locked;
    double m_opacity;
    QColor m_color;
    int m_currentFrame;

    QList<QGraphicsItem*> m_items;
    std::map<int, std::unique_ptr<AnimationKeyframe>> m_keyframes;

    // Onion skinning
    bool m_onionSkinEnabled;
    int m_onionSkinBefore;
    int m_onionSkinAfter;
};
#endif