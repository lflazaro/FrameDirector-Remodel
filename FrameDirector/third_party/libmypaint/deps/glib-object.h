#ifndef GLIB_OBJECT_H
#define GLIB_OBJECT_H

#include "glib.h"

G_BEGIN_DECLS

typedef gulong GType;
typedef gpointer (*GBoxedCopyFunc)(gconstpointer boxed);
typedef void (*GBoxedFreeFunc)(gpointer boxed);

typedef enum
{
    G_PARAM_READABLE      = 1 << 0,
    G_PARAM_WRITABLE      = 1 << 1,
    G_PARAM_CONSTRUCT     = 1 << 2,
    G_PARAM_CONSTRUCT_ONLY= 1 << 3,
    G_PARAM_LAX_VALIDATION= 1 << 4,
    G_PARAM_STATIC_NAME   = 1 << 5,
    G_PARAM_STATIC_NICK   = 1 << 6,
    G_PARAM_STATIC_BLURB  = 1 << 7,
    G_PARAM_EXPLICIT_NOTIFY = 1 << 8,
    G_PARAM_DEPRECATED    = 1 << 9
} GParamFlags;

#define G_PARAM_STATIC_STRINGS (G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB)
#define G_PARAM_READWRITE      (G_PARAM_READABLE | G_PARAM_WRITABLE)
#define G_PARAM_MASK           0x0FFF
#define G_PARAM_USER_SHIFT     10

typedef struct _GParamSpec
{
    const gchar *name;
    const gchar *nick;
    const gchar *blurb;
    GParamFlags  flags;
    GType        value_type;
} GParamSpec;

typedef struct _GObject
{
    guint ref_count;
    gpointer qdata;
} GObject;

typedef struct _GObjectClass
{
    GType g_type;
} GObjectClass;

typedef void (*GClassInitFunc)(gpointer klass, gpointer class_data);

GType g_boxed_type_register_static(const char *name,
                                   GBoxedCopyFunc boxed_copy,
                                   GBoxedFreeFunc boxed_free);

gpointer g_object_ref(gpointer object);
void     g_object_unref(gpointer object);

G_END_DECLS

#endif /* GLIB_OBJECT_H */
