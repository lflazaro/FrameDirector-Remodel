#pragma once

#include <QtCore/Qt>

namespace GraphicsItemRoles {

// Role used to hint that an item should be positioned behind stroke content
// when it is inserted into a layer. This is primarily used by the bucket fill
// tool so that the generated fill does not cover the original drawing stroke.
inline constexpr int BucketFillBehindStrokeRole = Qt::UserRole + 501;

} // namespace GraphicsItemRoles

