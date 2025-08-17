#ifndef FRAMEDIRECTOR_FRAMETYPES_H
#define FRAMEDIRECTOR_FRAMETYPES_H

#include <QGraphicsItem>
#include <QVariant>
#include <QString>
#include <QList>
#include <QMap>

namespace FrameDirector {

// Enhanced frame type tracking
enum class FrameType {
    Empty,        // No content, no keyframe
    Keyframe,     // Contains unique content/state
    ExtendedFrame // Extends from previous keyframe
};

struct FrameData {
    FrameType type = FrameType::Empty;
    int sourceKeyframe = -1;  // For extended frames, which keyframe they extend from
    QList<QGraphicsItem*> items;
    QMap<QGraphicsItem*, QVariant> itemStates; // Store item states for tweening

    // Tweening support
    bool hasTweening = false;          // Whether this frame span has tweening applied
    int tweeningEndFrame = -1;      // The end frame of the tween (if this is start frame)
    QString easingType = "linear";        // Easing curve type ("linear", "ease-in", "ease-out", etc.)
};

} // namespace FrameDirector

using FrameType = FrameDirector::FrameType;
using FrameData = FrameDirector::FrameData;

#endif // FRAMEDIRECTOR_FRAMETYPES_H