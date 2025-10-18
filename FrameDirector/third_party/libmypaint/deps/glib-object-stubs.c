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
