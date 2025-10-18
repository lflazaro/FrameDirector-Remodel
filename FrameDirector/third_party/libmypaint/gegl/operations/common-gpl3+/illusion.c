/* This file is an image processing operation for GEGL
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * Author: Hirotsuna Mizuno <s1041150@u-aizu.ac.jp>
 *
 * GEGL port: Thomas Manni <thomas.manni@free.fr>
 *
 */

#include "config.h"
#include <glib/gi18n-lib.h>

#ifdef GEGL_PROPERTIES

enum_start (gegl_illusion_type)
  enum_value (GEGL_ILLUSION_TYPE_1, "type1", N_("Type 1"))
  enum_value (GEGL_ILLUSION_TYPE_2, "type2", N_("Type 2"))
enum_end (GeglIllusionType)

property_int  (division, _("Division"), 8)
  description (_("The number of divisions"))
  value_range (0, 64)
  ui_range    (0, 64)

property_enum (illusion_type, _("Illusion type"),
                GeglIllusionType, gegl_illusion_type,
                GEGL_ILLUSION_TYPE_1)
  description (_("Type of illusion"))

#else

#define GEGL_OP_FILTER
#define GEGL_OP_NAME     illusion
#define GEGL_OP_C_SOURCE illusion.c

#include "gegl-op.h"

static void prepare (GeglOperation *operation)
{
  const Babl *format = gegl_operation_get_source_format (operation, "input");
  const GeglRectangle *bb = gegl_operation_source_get_bounding_box (operation, "input");

  if (! format || ! babl_format_has_alpha (format))
    format = babl_format_with_space ("R'G'B' float", format);
  else
    format = babl_format_with_space ("R'G'B'A float", format);

  gegl_operation_set_format (operation, "input",  format);
  gegl_operation_set_format (operation, "output", format);

  if (bb && ! gegl_rectangle_is_infinite_plane (bb))
    {
      GeglProperties *o  = GEGL_PROPERTIES (operation);
      gdouble        *dx = g_new (gdouble, (4 * o->division + 1) * 2);
      gdouble        *dy = &dx[4 * o->division + 1];
      gdouble         offset = (gint) (sqrt (bb->width * bb->width + bb->height * bb->height) / 4);
      gint            i;

      g_object_set_data_full (G_OBJECT (operation), "free-me",
                              o->user_data = dx, g_free);

      for (i = -2 * o->division; i <= 2 * o->division; ++i)
        {
          gdouble a = G_PI / o->division * (i * 0.5 + 1.0);
          gdouble c = cos (a);
          gdouble s = sin (a);

          dx[i + 2 * o->division] = GEGL_FLOAT_IS_ZERO (c) ? 0.0 : c * offset;
          dy[i + 2 * o->division] = GEGL_FLOAT_IS_ZERO (s) ? 0.0 : s * offset;
        }
    }
}

static GeglRectangle
get_required_for_output (GeglOperation       *operation,
                         const gchar         *input_pad,
                         const GeglRectangle *roi)
{
  GeglRectangle *result = gegl_operation_source_get_bounding_box (operation,
                                                                  "input");

  /* Don't request an infinite plane */
  if (! result || gegl_rectangle_is_infinite_plane (result))
    return *roi;

  return *result;
}

static GeglRectangle
get_invalidated_by_change (GeglOperation       *operation,
                           const gchar         *input_pad,
                           const GeglRectangle *input_region)
{
  GeglRectangle *result = gegl_operation_source_get_bounding_box (operation,
                                                                  "input");

  /* Don't request an infinite plane */
  if (! result || gegl_rectangle_is_infinite_plane (result))
    return *input_region;

  return *result;
}

static gboolean
process (GeglOperation       *operation,
         GeglBuffer          *input,
         GeglBuffer          *output,
         const GeglRectangle *result,
         gint                 level)
{
  GeglProperties      *o  = GEGL_PROPERTIES (operation);
  const GeglRectangle *bb = gegl_operation_source_get_bounding_box (operation, "input");
  GeglBufferIterator *iter;
  GeglSampler        *sampler;

  gint         x, y, xx, yy, b;
  gint         width, height, components, angle;
  gdouble      radius, cx, cy;
  gdouble      center_x;
  gdouble      center_y;
  gdouble      scale;
  gboolean     has_alpha;
  gfloat       alpha, alpha1, alpha2;
  gfloat      *in_pixel2;
  const gdouble *dx = o->user_data;
  const gdouble *dy = &dx[4 * o->division + 1];

  const Babl *format = gegl_operation_get_format (operation, "output");
  has_alpha  = babl_format_has_alpha (format);

  if (has_alpha)
    components = 4;
  else
    components = 3;

  in_pixel2 = g_new (float, components);

  iter = gegl_buffer_iterator_new (output, result, level, format,
                                   GEGL_ACCESS_WRITE, GEGL_ABYSS_NONE, 2);

  gegl_buffer_iterator_add (iter, input, result, level, format,
                            GEGL_ACCESS_READ, GEGL_ABYSS_NONE);

  sampler = gegl_buffer_sampler_new_at_level (input, format,
                                              GEGL_SAMPLER_NEAREST, level);

  width = bb->width;
  height = bb->height;

  center_x = width / 2.0;
  center_y = height / 2.0;
  scale = sqrt (width * width + height * height) / 2;

  while (gegl_buffer_iterator_next (iter))
    {
       gfloat  *out_pixel = iter->items[0].data;
       gfloat  *in_pixel1 = iter->items[1].data;

       for (y = iter->items[0].roi.y; y < iter->items[0].roi.y + iter->items[0].roi.height; ++y)
         for (x = iter->items[0].roi.x; x < iter->items[0].roi.x + iter->items[0].roi.width; ++x)
           {
              cy = ((gdouble) y - center_y) / scale;
              cx = ((gdouble) x - center_x) / scale;

              angle = floor (atan2 (cy, cx) * o->division / G_PI_2 +
                             GEGL_FLOAT_EPSILON);
              radius = sqrt ((gdouble) (cx * cx + cy * cy));

              if (o->illusion_type == GEGL_ILLUSION_TYPE_1)
                {
                   xx = x - dx [2 * o->division + angle];
                   yy = y - dy [2 * o->division + angle];
                }
              else                          /* GEGL_ILLUSION_TYPE_2 */
                {
                   xx = x - dy [2 * o->division + angle];
                   yy = y - dx [2 * o->division + angle];
                }

                gegl_sampler_get (sampler, xx, yy, NULL,
                                  in_pixel2, GEGL_ABYSS_CLAMP);

                if (has_alpha)
                  {
                     alpha1 = in_pixel1[3];
                     alpha2 = in_pixel2[3];
                     alpha  = (1 - radius) * alpha1 + radius * alpha2;

                     if ((out_pixel[3] = (alpha / 2)))
                       {
                         for (b = 0; b < 3; b++)
                           out_pixel[b] = ((1 - radius) * in_pixel1[b] * alpha1 +
                                           radius * in_pixel2[b] * alpha2) / alpha;
                       }
                  }
                else
                 {
                   for (b = 0; b < 3; b++)
                     out_pixel[b] = (1 - radius) * in_pixel1[b] + radius * in_pixel2[b];
                 }

                 out_pixel += components;
                 in_pixel1 += components;
           }
    }

  g_free (in_pixel2);

  g_object_unref (sampler);

  return  TRUE;
}

static gboolean
operation_process (GeglOperation        *operation,
                   GeglOperationContext *context,
                   const gchar          *output_prop,
                   const GeglRectangle  *result,
                   gint                  level)
{
  GeglOperationClass  *operation_class;

  const GeglRectangle *in_rect =
    gegl_operation_source_get_bounding_box (operation, "input");

  operation_class = GEGL_OPERATION_CLASS (gegl_op_parent_class);

  if (in_rect && gegl_rectangle_is_infinite_plane (in_rect))
    {
      gpointer in = gegl_operation_context_get_object (context, "input");
      gegl_operation_context_take_object (context, "output",
                                          g_object_ref (G_OBJECT (in)));
      return TRUE;
    }

  return operation_class->process (operation, context, output_prop, result,
                                   gegl_operation_context_get_level (context));
}

static void
gegl_op_class_init (GeglOpClass *klass)
{
  GeglOperationClass       *operation_class;
  GeglOperationFilterClass *filter_class;
  gchar                    *composition = 
    "<?xml version='1.0' encoding='UTF-8'?>"
    "<gegl>"
    "  <node operation='gegl:crop' width='200' height='200'/>"
    "  <node operation='gegl:illusion'/>"
    "  <node operation='gegl:load' path='standard-input.png'/>"
    "</gegl>";

  operation_class = GEGL_OPERATION_CLASS (klass);
  filter_class    = GEGL_OPERATION_FILTER_CLASS (klass);

  filter_class->process                      = process;
  operation_class->prepare                   = prepare;
  operation_class->process                   = operation_process;
  operation_class->get_invalidated_by_change = get_invalidated_by_change;
  operation_class->get_required_for_output   = get_required_for_output;
  operation_class->opencl_support            = FALSE;
  operation_class->threaded                  = FALSE;

  gegl_operation_class_set_keys (operation_class,
      "name",          "gegl:illusion",
      "title",          _("Illusion"),
      "categories",     "map",
      "license",        "GPL3+",
      "reference-hash", "8a578729f9beb4e3fb35021995caae70",
      "reference-composition", composition,
      "description", _("Superimpose many altered copies of the image."),
      NULL);
}

#endif
