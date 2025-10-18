#include "glib-object.h"

GType g_boxed_type_register_static(const char *name,
                                   GBoxedCopyFunc boxed_copy,
                                   GBoxedFreeFunc boxed_free)
{
    static GType next_type = 1;
    (void)name;
    (void)boxed_copy;
    (void)boxed_free;
    return next_type++;
}

gpointer g_object_ref(gpointer object)
{
    GObject *obj = (GObject *)object;
    if (obj)
    {
        obj->ref_count++;
    }
    return object;
}

void g_object_unref(gpointer object)
{
    GObject *obj = (GObject *)object;
    if (!obj)
    {
        return;
    }
    if (obj->ref_count > 0)
    {
        obj->ref_count--;
    }
}
