#ifndef GLIB_OBJECT_H
#define GLIB_OBJECT_H

#include "glib.h"

G_BEGIN_DECLS

typedef gulong GType;
typedef gpointer (*GBoxedCopyFunc)(gconstpointer boxed);
typedef void (*GBoxedFreeFunc)(gpointer boxed);

GType g_boxed_type_register_static(const char *name,
                                   GBoxedCopyFunc boxed_copy,
                                   GBoxedFreeFunc boxed_free);

G_END_DECLS

#endif /* GLIB_OBJECT_H */
