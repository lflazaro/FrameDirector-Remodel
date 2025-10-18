/* This file is part of GEGL.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <https://www.gnu.org/licenses/>.
 *
 * Copyright 2006-2008 Øyvind Kolås <pippin@gimp.org>
 */

#ifndef __GEGL_BUFFER_PRIVATE_H__
#define __GEGL_BUFFER_PRIVATE_H__

#include "gegl-buffer-types.h"
#include "gegl-buffer.h"
#include "gegl-tile-handler.h"
#include "gegl-buffer-iterator.h"

#define GEGL_BUFFER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GEGL_TYPE_BUFFER, GeglBufferClass))
#define GEGL_IS_BUFFER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GEGL_TYPE_BUFFER))
#define GEGL_BUFFER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GEGL_TYPE_BUFFER, GeglBufferClass))

typedef struct _GeglBufferClass GeglBufferClass;

struct _GeglBuffer
{
  GeglTileHandler   parent_instance; /* which is a GeglTileHandler which has a
                                        source field which is used for chaining
                                        sub buffers with their anchestors */

  GeglRectangle     extent;        /* the dimensions of the buffer */

  const Babl       *format;  /* the pixel format used for pixels in this
                                buffer */
  const Babl  *soft_format;  /* the format the buffer pretends to be, might
                                be different from format */

  gint              shift_x; /* The relative offset of origins compared with */
  gint              shift_y; /* anchestral tile_storage buffer, during       */
                             /* construction relative to immediate source    */

  GeglRectangle     abyss;
  gboolean          abyss_tracks_extent; /* specifies whether the abyss rectangle
                                            should track any modifications to the
                                            extent rectangle */

  GeglTileStorage  *tile_storage;

  gint              tile_width;
  gint              tile_height;
  gchar            *path;

  gint              lock_count;

  gpointer         *alloc_stack_trace; /* Stack trace for allocation,
                                          useful for debugging */
  gint              alloc_stack_size;

  gint              changed_signal_connections; /* to avoid firing changed signals
                                                   with no listeners */
  gint              changed_signal_freeze_count;
  GeglRectangle     changed_signal_accumulator;

  GeglTileBackend  *backend;

  gboolean          initialized;
};

struct _GeglBufferClass
{
  GeglTileHandlerClass parent_class;
};


gint              gegl_buffer_leaks       (void);

void              gegl_buffer_stats       (void);

void              gegl_tile_cache_init    (void);

void              gegl_tile_cache_destroy (void);

void              gegl_tile_backend_swap_cleanup (void);

GeglTileBackend * gegl_buffer_backend     (GeglBuffer *buffer);
GeglTileBackend * gegl_buffer_backend2    (GeglBuffer *buffer); /* non-cached */

gboolean          gegl_buffer_is_shared   (GeglBuffer *buffer);

#define GEGL_BUFFER_DISABLE_LOCKS 1

#ifdef GEGL_BUFFER_DISABLE_LOCKS
#define           gegl_buffer_try_lock(a)   (TRUE)
#define           gegl_buffer_lock(a)       do{}while(0)
#define           gegl_buffer_unlock(a)       do{}while(0)
#else
gboolean          gegl_buffer_try_lock    (GeglBuffer *buffer);
gboolean          gegl_buffer_lock        (GeglBuffer *buffer);
gboolean          gegl_buffer_unlock      (GeglBuffer *buffer);
#endif

void              gegl_buffer_set_unlocked (GeglBuffer          *buffer,
                                            const GeglRectangle *rect,
                                            gint                 level,
                                            const Babl          *format,
                                            const void          *src,
                                            gint                 rowstride);

void              gegl_buffer_set_unlocked_no_notify (GeglBuffer          *buffer,
                                                      const GeglRectangle *rect,
                                                      gint                 level,
                                                      const Babl          *format,
                                                      const void          *src,
                                                      gint                 rowstride);

void              gegl_buffer_get_unlocked (GeglBuffer          *buffer,
                                            gdouble              scale,
                                            const GeglRectangle *rect,
                                            const Babl          *format,
                                            gpointer             dest_buf,
                                            gint                 rowstride,
                                            GeglAbyssPolicy      repeat_mode);

GeglBuffer *      gegl_buffer_new_ram     (const GeglRectangle *extent,
                                           const Babl          *format);

void              gegl_buffer_emit_changed_signal (GeglBuffer *buffer,
                                                   const GeglRectangle *rect);

/* the instance size of a GeglTile is a bit large, and should if possible be
 * trimmed down
 */
struct _GeglTile
{
 /* GObject          parent_instance;*/
  gint             ref_count;
  guchar          *data;        /* actual pixel data for tile, a linear buffer*/
  gint             size;        /* The size of the linear buffer */

  GeglTileStorage *tile_storage; /* the buffer from which this tile was
                                  * retrieved needed for the tile to be able to
                                  * store itself back (for instance when it is
                                  * unreffed for the last time)
                                  */
  gint             x, y, z;


  guint            rev;         /* this tile revision */
  guint            stored_rev;  /* what revision was we when we from tile_storage?
                                   (currently set to 1 when loaded from disk */

  gint             lock_count;       /* number of outstanding write locks */
  gint             read_lock_count;  /* number of outstanding read locks */
  guint            is_zero_tile:1;   /* whether the tile data is fully zeroed
                                      * (allowing for false negatives, but not
                                      * false positives)
                                      */
  guint            is_global_tile:1; /* whether the tile data is global (and
                                      * therefore can never be owned by a
                                      * single mutable tile)
                                      */
  guint            keep_identity:1;  /* maintain data pointer identity, rather
                                      * than data content only
                                      */

  gint             clone_state; /* tile clone/unclone state & spinlock */
  gint            *n_clones;    /* an array of two atomic counters, shared
                                 * among all tiles sharing the same data.
                                 * the first counter indicates the number of
                                 * tiles sharing the data, and the second
                                 * counter indicates how many of those tiles
                                 * are in the cache.
                                 */

  gint             clones;       // data storage for n_clones
  gint             cached_clones;//

  guint64          damage;

  /* called when the tile is about to be destroyed */
  GDestroyNotify   destroy_notify;
  gpointer         destroy_notify_data;

  /* called when the tile has been unlocked which typically means tile
   * data has changed
   */
  GeglTileCallback unlock_notify;
  gpointer         unlock_notify_data;
};

gboolean gegl_tile_needs_store    (GeglTile *tile);
void     gegl_tile_unlock_no_void (GeglTile *tile);
gboolean gegl_tile_damage         (GeglTile *tile,
                                   guint64   damage);

void _gegl_buffer_drop_hot_tile (GeglBuffer *buffer);

GeglRectangle _gegl_get_required_for_scale (const GeglRectangle *roi,
                                            gdouble              scale);

gboolean gegl_buffer_scan_compatible (GeglBuffer *bufferA,
                                      gint        xA,
                                      gint        yA,
                                      GeglBuffer *bufferB,
                                      gint        xB,
                                      gint        yB);


extern void (*gegl_tile_handler_cache_ext_flush) (void *tile_handler_cache, const GeglRectangle *rect);
extern void (*gegl_buffer_ext_flush) (GeglBuffer *buffer, const GeglRectangle *rect);
extern void (*gegl_buffer_ext_invalidate) (GeglBuffer *buffer, const GeglRectangle *rect);


extern void (*gegl_resample_bilinear) (guchar *dest_buf,
                                       const guchar *source_buf,
                                       const GeglRectangle *dst_rect,
                                       const GeglRectangle *src_rect,
                                       gint                 s_rowstride,
                                       gdouble              scale,
                                       const Babl          *format,
                                       gint                 d_rowstride);

extern void (*gegl_resample_boxfilter)(guchar *dest_buf,
                                       const guchar *source_buf,
                                       const GeglRectangle *dst_rect,
                                       const GeglRectangle *src_rect,
                                       gint                 s_rowstride,
                                       gdouble              scale,
                                       const Babl          *format,
                                       gint                 d_rowstride);

extern void (*gegl_resample_nearest)(guchar *dest_buf,
                                     const guchar *source_buf,
                                     const GeglRectangle *dst_rect,
                                     const GeglRectangle *src_rect,
                                     gint                 s_rowstride,
                                     gdouble              scale,
                                     const gint           bpp,
                                     gint                 d_rowstride);

extern void (*gegl_downscale_2x2) (const Babl *format,
                                   gint        src_width,
                                   gint        src_height,
                                   guchar     *src_data,
                                   gint        src_rowstride,
                                   guchar     *dst_data,
                                   gint        dst_rowstride);


#ifndef __GEGL_TILE_H__
#define gegl_tile_get_data(tile)  ((tile)->data)
#endif

#define gegl_tile_n_clones(tile)         (&(tile)->n_clones[0])
#define gegl_tile_n_cached_clones(tile)  (&(tile)->n_clones[1])

/* computes the positive integer remainder (also for negative dividends)
 */
#define GEGL_REMAINDER(dividend, divisor) \
                   (((dividend) < 0) ? \
                    (divisor) - 1 - ((-((dividend) + 1)) % (divisor)) : \
                    (dividend) % (divisor))

#define gegl_tile_offset(coordinate, stride) GEGL_REMAINDER((coordinate), (stride))

/* helper function to compute tile indices and offsets for coordinates
 * based on a tile stride (tile_width or tile_height)
 */
#define gegl_tile_indice(coordinate,stride) \
  (((coordinate) >= 0)?\
      (coordinate) / (stride):\
      ((((coordinate) + 1) /(stride)) - 1))

#endif
