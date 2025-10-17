#pragma once

#include <QtCore/Qt>

namespace GraphicsItemRoles {

// Role used to hint that an item should be positioned behind stroke content
// when it is inserted into a layer. This is primarily used by the bucket fill
// tool so that the generated fill does not cover the original drawing stroke.
inline constexpr int BucketFillBehindStrokeRole = Qt::UserRole + 501;

// Role used to tag pixmap items that originate from the raster editor export.
// Stores a stable session identifier so that subsequent exports can detect and
// replace the previous frame content.
inline constexpr int RasterSessionIdRole = Qt::UserRole + 551;

// Records the 1-based project frame index a raster export belongs to. This
// allows us to selectively replace only the matching frame when the user
// re-exports from the raster editor.
inline constexpr int RasterFrameIndexRole = Qt::UserRole + 552;

// Stores a serialized representation of the raster document (in compact JSON
// form) so that project serialization can preserve the layered raster data and
// restore it on load.
inline constexpr int RasterDocumentJsonRole = Qt::UserRole + 553;

} // namespace GraphicsItemRoles
