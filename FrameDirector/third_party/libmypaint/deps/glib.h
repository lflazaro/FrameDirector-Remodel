#ifndef GLIB_H
#define GLIB_H

#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include <float.h>
#include <math.h>

#ifdef __cplusplus
#define G_BEGIN_DECLS extern "C" {
#define G_END_DECLS }
#else
#define G_BEGIN_DECLS
#define G_END_DECLS
#endif

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

typedef void *gpointer;
typedef const void *gconstpointer;
typedef char gchar;
typedef signed char gint8;
typedef unsigned char guint8;
typedef short gint16;
typedef unsigned short guint16;
typedef int gint;
typedef unsigned int guint;
typedef long glong;
typedef unsigned long gulong;
typedef long long gint64;
typedef unsigned long long guint64;
typedef float gfloat;
typedef double gdouble;
typedef gint gboolean;

typedef void (*GDestroyNotify)(gpointer data);

typedef struct _GArray GArray;
typedef struct _GPtrArray GPtrArray;

typedef int (*GCompareFunc)(gconstpointer a, gconstpointer b);

typedef void (*GFunc)(gpointer data, gpointer user_data);

#define G_GNUC_UNUSED
#define G_GNUC_WARN_UNUSED_RESULT
#define G_GNUC_PRINTF(format_idx, arg_idx)

#define G_GNUC_CONST
#define G_GNUC_PURE

#define G_MAXINT        INT_MAX
#define G_MININT        INT_MIN
#define G_MAXUINT       UINT_MAX
#define G_MAXULONG      ULONG_MAX
#define G_MINLONG       LONG_MIN
#define G_MAXLONG       LONG_MAX
#define G_MAXDOUBLE     DBL_MAX
#define G_PI            3.14159265358979323846

#define G_LIKELY(x)   (x)
#define G_UNLIKELY(x) (x)

#define G_N_ELEMENTS(arr) ((guint)(sizeof(arr) / sizeof((arr)[0])))

#define g_return_if_fail(expr) do { if (!(expr)) return; } while (0)
#define g_return_val_if_fail(expr, val) do { if (!(expr)) return (val); } while (0)

typedef struct _GValue
{
    unsigned long g_type;
    union {
        gint v_int;
        gdouble v_double;
        gpointer v_pointer;
    } data[2];
} GValue;

static inline void g_value_init(GValue *value, unsigned long g_type)
{
    if (value)
    {
        value->g_type = g_type;
        value->data[0].v_pointer = NULL;
        value->data[1].v_pointer = NULL;
    }
}

static inline void g_value_unset(GValue *value)
{
    (void)value;
}


#endif /* GLIB_H */
