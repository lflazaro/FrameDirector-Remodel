/* This file is an image processing operation for GEGL
 *
 * GEGL is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * GEGL is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with GEGL; if not, see <https://www.gnu.org/licenses/>.
 *
 * Copyright 2006 Øyvind Kolås <pippin@gimp.org>
 */

#include "config.h"
#include <stdlib.h>
#include <glib/gi18n-lib.h>


#ifdef GEGL_PROPERTIES

property_file_path (path, _("File"), "/tmp/gegl-logo.svg")
    description (_("Path of file to load."))

#else

#define GEGL_OP_SOURCE
#define GEGL_OP_NAME     magick_load
#define GEGL_OP_C_SOURCE magick-load.c

#include "gegl-op.h"
#include <stdio.h>

static void
load_cache (GeglProperties *op_magick_load)
{
  gchar *convert;

  convert = g_find_program_in_path ("convert");

  if (convert && !op_magick_load->user_data)
    {
      gchar    *filename;
      GeglNode *graph, *sink, *loader;
      GeglBuffer *newbuf = NULL;

      /* ImageMagick backed fallback FIXME: make this robust.
       * maybe use pipes in a manner similar to the raw loader,
       * or at least use a properly unique filename  */
      char     *argv[4]  = {convert, NULL, NULL, NULL};

      filename = g_build_filename (g_get_tmp_dir (), "gegl-magick.png", NULL);

      argv[1] = g_strdup_printf ("%s[0]", op_magick_load->path);
      if (argv[1][0]=='-') /* clobber argument if path starts with a dash     */
        argv[1][0]='_';    /* which when controlling the file system leads
                              to being able to selection parameters for
                              imagemagic.  */
      argv[2] = filename;
      if (!g_spawn_sync (NULL, argv, NULL,
                         G_SPAWN_STDOUT_TO_DEV_NULL|G_SPAWN_STDERR_TO_DEV_NULL,
                         NULL, NULL, NULL, NULL, NULL, NULL))
        g_warning ("Error executing ImageMagick convert program");

      g_free (argv[1]);

      graph = gegl_node_new ();
      sink = gegl_node_new_child (graph,
                                 "operation", "gegl:buffer-sink",
                                 "buffer", &newbuf, NULL);
      loader = gegl_node_new_child (graph,
                                    "operation", "gegl:png-load",
                                    "path", filename, NULL);
      gegl_node_link_many (loader, sink, NULL);
      gegl_node_process (sink);
      op_magick_load->user_data = (gpointer) newbuf;
      g_object_unref (graph);
      g_free (filename);
    }

  g_free (convert);
}

static GeglRectangle
get_bounding_box (GeglOperation *operation)
{
  GeglRectangle result = {0,0,0,0};
  GeglProperties   *o = GEGL_PROPERTIES (operation);
  gint width, height;

  load_cache (o);

  g_object_get (o->user_data, "width", &width,
                               "height", &height, NULL);
  result.width  = width;
  result.height = height;
  return result;
}

static GeglRectangle
get_cached_region (GeglOperation *operation,
                   const GeglRectangle *roi)
{
  return get_bounding_box (operation);
}

static gboolean
process (GeglOperation         *operation,
         GeglOperationContext  *context,
         const gchar           *output_pad,
         const GeglRectangle   *result,
         gint                   level)
{
  GeglProperties *o = GEGL_PROPERTIES (operation);

  if (!o->user_data)
    return FALSE;
  /* overriding the predefined behavior */
  g_object_ref (o->user_data);
  gegl_operation_context_take_object (context, "output", G_OBJECT (o->user_data));
  return  TRUE;
}

static gboolean
gegl_magick_load_is_available (void)
{
  gchar    *convert;
  gboolean  found = FALSE;

  convert = g_find_program_in_path ("convert");
  found = (convert != NULL);
  g_free (convert);

  return found;
}

static void finalize (GObject *object)
{
  GeglOperation *op = (void*) object;
  GeglProperties *o = GEGL_PROPERTIES (op);

  g_clear_object (&o->user_data);

  G_OBJECT_CLASS (gegl_op_parent_class)->finalize (object);
}

static void
gegl_op_class_init (GeglOpClass *klass)
{
  GeglOperationClass       *operation_class;
  GObjectClass             *object_class;

  operation_class = GEGL_OPERATION_CLASS (klass);
  object_class    = G_OBJECT_CLASS (klass);

  object_class->finalize = finalize;

  operation_class->process = process;
  operation_class->get_bounding_box  = get_bounding_box;
  operation_class->get_cached_region = get_cached_region;;
  operation_class->is_available      = gegl_magick_load_is_available;

  gegl_operation_class_set_keys (operation_class,
        "name"       , "gegl:magick-load",
        "categories" , "hidden",
        "description",
        _("Image Magick wrapper using the png op."),
        NULL);
}

#endif
