/*
 * This program is free software; you can redistribute it and/or modify
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
 * along with this program; if not, see <https://www.gnu.org/licenses/>.
 *
 * Copyright (C) 2013 Daniel Sabo
 * Copyright (C) 2016, 2017 Red Hat, Inc.
 */

#include "config.h"
#include <string.h>
#include <stdio.h>

#include "gegl.h"
#include "gegl-plugin.h"

static void
already_connected_invalidated (gpointer user_data)
{
  gboolean *result = (gboolean *) user_data;
  *result = FALSE;
}

static gboolean
test_node_already_connected (void)
{
  gboolean result = TRUE;
  GeglNode *ptn, *sink, *src;

  ptn  = gegl_node_new ();
  src  = gegl_node_new_child (ptn,
                              "operation", "gegl:color",
                              NULL);
  sink = gegl_node_new_child (ptn,
                              "operation", "gegl:nop",
                              NULL);
  gegl_node_link (src, sink);

  g_signal_connect_swapped (sink, "invalidated", G_CALLBACK (already_connected_invalidated), &result);
  gegl_node_link (src, sink);

  g_object_unref (ptn);
  return result;
}

static void
dont_invalidate_source_invalidated (gpointer user_data)
{
  gboolean *result = (gboolean *) user_data;
  *result = FALSE;
}

static gboolean
test_node_dont_invalidate_source (void)
{
  gboolean result = TRUE;
  GeglNode *ptn, *sink, *src;

  ptn  = gegl_node_new ();
  src  = gegl_node_new_child (ptn,
                              "operation", "gegl:color",
                              NULL);
  sink = gegl_node_new_child (ptn,
                              "operation", "gegl:nop",
                              NULL);

  g_signal_connect_swapped (src, "invalidated", G_CALLBACK (dont_invalidate_source_invalidated), &result);

  gegl_node_link (src, sink);
  gegl_node_disconnect (sink, "input");

  g_object_unref (ptn);
  return result;
}

static gboolean
test_node_reconnect_many (void)
{
  gboolean result = TRUE;

  GeglNode *ptn, *child0, *child1, *child2;

  ptn    = gegl_node_new ();
  child0 = gegl_node_new_child (ptn,
                                "operation", "gegl:nop",
                                NULL);
  child1 = gegl_node_new_child (ptn,
                                "operation", "gegl:nop",
                                NULL);
  child2 = gegl_node_new_child (ptn,
                                "operation", "gegl:nop",
                                NULL);

  gegl_node_link (child0, child1);
  gegl_node_link (child0, child2);

  if (!(child0 == gegl_node_get_producer (child1, "input", NULL)))
    {
      g_warning ("Wrong producer node on child 1");
      result = FALSE;
    }
  
  if (!(child0 == gegl_node_get_producer (child2, "input", NULL)))
    {
      g_warning ("Wrong producer node on child 2");
      result = FALSE;
    }

  gegl_node_set (child0,
                 "operation", "gegl:color",
                 NULL);

  if (!(child0 == gegl_node_get_producer (child1, "input", NULL)))
    {
      g_warning ("Wrong producer node on child 1");
      result = FALSE;
    }

  if (!(child0 == gegl_node_get_producer (child2, "input", NULL)))
    {
      g_warning ("Wrong producer node on child 2");
      result = FALSE;
    }

  return result;
}

static gboolean
test_operation_get_source_node (void)
{
  gboolean result = FALSE;
  GeglNode *node, *ptn, *sink, *src;
  GeglOperation *sink_op;

  ptn  = gegl_node_new ();
  src  = gegl_node_new_child (ptn,
                              "operation", "gegl:color",
                              NULL);
  sink = gegl_node_new_child (ptn,
                              "operation", "gegl:nop",
                              NULL);
  gegl_node_link (src, sink);

  sink_op = gegl_node_get_gegl_operation (sink);
  if (GEGL_IS_OPERATION_META (sink_op))
    {
      g_warning ("Unexpected GeglOperationMeta");
      goto out;
    }

  node = gegl_operation_get_source_node (sink_op, "input");
  if (node != src)
    {
      g_warning ("Wrong source node");
      goto out;
    }

  result = TRUE;

 out:
  g_object_unref (ptn);
  return result;
}

static gboolean
test_operation_meta_get_source_node (void)
{
  gboolean result = FALSE;
  GeglNode *node, *ptn, *sink, *src;
  GeglOperation *sink_op;

  ptn  = gegl_node_new ();
  src  = gegl_node_new_child (ptn,
                              "operation", "gegl:color",
                              NULL);
  sink = gegl_node_new_child (ptn,
                              "operation", "gegl:gaussian-blur",
                              NULL);
  gegl_node_link (src, sink);

  sink_op = gegl_node_get_gegl_operation (sink);
  if (!GEGL_IS_OPERATION_META (sink_op))
    {
      g_warning ("Expected GeglOperationMeta");
      goto out;
    }

  node = gegl_operation_get_source_node (sink_op, "input");
  if (node != src)
    {
      g_warning ("Wrong source node");
      goto out;
    }

  result = TRUE;

 out:
  g_object_unref (ptn);
  return result;
}

#define RUN_TEST(test_name) \
{ \
  if (test_name()) \
    { \
      printf ("" #test_name " ... PASS\n"); \
      tests_passed++; \
    } \
  else \
    { \
      printf ("" #test_name " ... FAIL\n"); \
      tests_failed++; \
    } \
  tests_run++; \
}

int main(int argc, char **argv)
{
  gint tests_run    = 0;
  gint tests_passed = 0;
  gint tests_failed = 0;

  gegl_init (0, NULL);
  g_object_set(G_OBJECT(gegl_config()),
               "swap", "RAM",
               "use-opencl", FALSE,
               NULL);

  RUN_TEST (test_node_already_connected)
  RUN_TEST (test_node_dont_invalidate_source)
  RUN_TEST (test_node_reconnect_many)
  RUN_TEST (test_operation_get_source_node)
  RUN_TEST (test_operation_meta_get_source_node)

  gegl_exit ();

  if (tests_passed == tests_run)
    return 0;
  return -1;

  return 0;
}
