#include "AnimationLayer.h"
#include "AnimationKeyframe.h"
#include <QGraphicsItem>
#include <algorithm>

AnimationLayer::AnimationLayer(const QString& name)
    : m_name(name)
    , m_visible(true)
    , m_locked(false)
    , m_opacity(1.0)
    , m_color(Qt::blue)
    , m_currentFrame(1)
    , m_onionSkinEnabled(false)
    , m_onionSkinBefore(1)
    , m_onionSkinAfter(1)
{
}

AnimationLayer::~AnimationLayer()
{
}

void AnimationLayer::setName(const QString& name)
{
    m_name = name;
}

QString AnimationLayer::getName() const
{
    return m_name;
}

void AnimationLayer::setVisible(bool visible)
{
    m_visible = visible;
    updateItemsVisibility();
}

bool AnimationLayer::isVisible() const
{
    return m_visible;
}

void AnimationLayer::setLocked(bool locked)
{
    m_locked = locked;
    for (QGraphicsItem* item : m_items) {
        item->setFlag(QGraphicsItem::ItemIsSelectable, !locked);
        item->setFlag(QGraphicsItem::ItemIsMovable, !locked);
    }
}

bool AnimationLayer::isLocked() const
{
    return m_locked;
}

void AnimationLayer::setOpacity(double opacity)
{
    m_opacity = qBound(0.0, opacity, 1.0);
    for (QGraphicsItem* item : m_items) {
        item->setOpacity(m_opacity);
    }
}

double AnimationLayer::getOpacity() const
{
    return m_opacity;
}

void AnimationLayer::setColor(const QColor& color)
{
    m_color = color;
}

QColor AnimationLayer::getColor() const
{
    return m_color;
}

void AnimationLayer::addItem(QGraphicsItem* item)
{
    if (item && !m_items.contains(item)) {
        m_items.append(item);
        item->setOpacity(m_opacity);
        item->setVisible(m_visible);
        item->setFlag(QGraphicsItem::ItemIsSelectable, !m_locked);
        item->setFlag(QGraphicsItem::ItemIsMovable, !m_locked);
    }
}

void AnimationLayer::removeItem(QGraphicsItem* item)
{
    m_items.removeAll(item);
}

void AnimationLayer::clearItems()
{
    m_items.clear();
}

QList<QGraphicsItem*> AnimationLayer::getItems() const
{
    return m_items;
}

void AnimationLayer::addKeyframe(int frame, std::unique_ptr<AnimationKeyframe> keyframe)
{
    if (frame > 0) {
        m_keyframes[frame] = std::move(keyframe);
    }
}

void AnimationLayer::removeKeyframe(int frame)
{
    auto it = m_keyframes.find(frame);
    if (it != m_keyframes.end()) {
        m_keyframes.erase(it);
    }
}

AnimationKeyframe* AnimationLayer::getKeyframe(int frame) const
{
    auto it = m_keyframes.find(frame);
    return (it != m_keyframes.end()) ? it->second.get() : nullptr;
}

bool AnimationLayer::hasKeyframe(int frame) const
{
    return m_keyframes.find(frame) != m_keyframes.end();
}

std::vector<int> AnimationLayer::getKeyframeNumbers() const
{
    std::vector<int> frames;
    for (const auto& pair : m_keyframes) {
        frames.push_back(pair.first);
    }
    return frames;
}

void AnimationLayer::setCurrentFrame(int frame)
{
    if (frame != m_currentFrame && frame > 0) {
        m_currentFrame = frame;
        interpolateToFrame(frame);
    }
}

int AnimationLayer::getCurrentFrame() const
{
    return m_currentFrame;
}

void AnimationLayer::interpolateToFrame(int frame)
{
    // Find surrounding keyframes
    AnimationKeyframe* prevKeyframe = findPreviousKeyframe(frame);
    AnimationKeyframe* nextKeyframe = findNextKeyframe(frame);

    if (prevKeyframe && nextKeyframe && prevKeyframe != nextKeyframe) {
        // Interpolate between keyframes
        interpolateBetweenKeyframes(prevKeyframe->getFrame(), nextKeyframe->getFrame(), frame);
    }
    else if (prevKeyframe) {
        // Apply previous keyframe state
        for (QGraphicsItem* item : m_items) {
            prevKeyframe->applyItemState(item);
        }
    }
}

void AnimationLayer::updateItemsVisibility()
{
    for (QGraphicsItem* item : m_items) {
        item->setVisible(m_visible);
    }
}

void AnimationLayer::interpolateBetweenKeyframes(int fromFrame, int toFrame, int currentFrame)
{
    AnimationKeyframe* fromKeyframe = getKeyframe(fromFrame);
    AnimationKeyframe* toKeyframe = getKeyframe(toFrame);

    if (!fromKeyframe || !toKeyframe) return;

    double t = static_cast<double>(currentFrame - fromFrame) / (toFrame - fromFrame);

    for (QGraphicsItem* item : m_items) {
        if (fromKeyframe->hasItemState(item) && toKeyframe->hasItemState(item)) {
            AnimationKeyframe::interpolateBetween(fromKeyframe, toKeyframe, t, item);
        }
    }
}

AnimationKeyframe* AnimationLayer::findPreviousKeyframe(int frame) const
{
    AnimationKeyframe* prev = nullptr;
    int prevFrame = -1;

    for (const auto& pair : m_keyframes) {
        if (pair.first <= frame && pair.first > prevFrame) {
            prev = pair.second.get();
            prevFrame = pair.first;
        }
    }

    return prev;
}

AnimationKeyframe* AnimationLayer::findNextKeyframe(int frame) const
{
    AnimationKeyframe* next = nullptr;
    int nextFrame = INT_MAX;

    for (const auto& pair : m_keyframes) {
        if (pair.first >= frame && pair.first < nextFrame) {
            next = pair.second.get();
            nextFrame = pair.first;
        }
    }

    return next;
}