#ifndef GLIB_H
#define GLIB_H

#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

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
typedef int32_t gint32;
typedef uint32_t guint32;
typedef int64_t gint64;
typedef uint64_t guint64;
typedef float gfloat;
typedef double gdouble;
typedef size_t gsize;
typedef ptrdiff_t gssize;
typedef gint gboolean;

typedef void (*GDestroyNotify)(gpointer data);
typedef void (*GFunc)(gpointer data, gpointer user_data);
typedef void (*GCallback)(void);

typedef struct _GArray GArray;
typedef struct _GPtrArray GPtrArray;

typedef int (*GCompareFunc)(gconstpointer a, gconstpointer b);

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

#define G_STMT_START do
#define G_STMT_END while (0)

#define G_LITTLE_ENDIAN 1234
#define G_BIG_ENDIAN    4321
#define G_BYTE_ORDER    G_LITTLE_ENDIAN

#define GINT_TO_POINTER(i)   ((gpointer)(intptr_t)(i))
#define GUINT_TO_POINTER(u)  ((gpointer)(uintptr_t)(u))
#define GPOINTER_TO_INT(p)   ((gint)(intptr_t)(p))
#define GPOINTER_TO_UINT(p)  ((guint)(uintptr_t)(p))

#define G_N_ELEMENTS(arr) ((guint)(sizeof(arr) / sizeof((arr)[0])))

#define g_new(type, count)   ((type *)g_malloc(sizeof(type) * (gsize)(count)))
#define g_new0(type, count)  ((type *)g_malloc0(sizeof(type) * (gsize)(count)))
#define g_renew(type, mem, count) ((type *)g_realloc((mem), sizeof(type) * (gsize)(count)))
#define g_slice_free(type, mem) g_free(mem)

#define g_return_if_fail(expr) do { if (!(expr)) return; } while (0)
#define g_return_val_if_fail(expr, val) do { if (!(expr)) return (val); } while (0)
#define g_return_if_reached() do { assert(!"g_return_if_reached"); return; } while (0)
#define g_return_val_if_reached(val) do { assert(!"g_return_val_if_reached"); return (val); } while (0)

typedef struct _GError
{
    guint32 domain;
    gint code;
    gchar *message;
} GError;

typedef struct _GSList
{
    gpointer data;
    struct _GSList *next;
} GSList;

typedef struct _GBytes
{
    gpointer data;
    gsize size;
    guint ref_count;
    GDestroyNotify destroy;
} GBytes;

typedef struct _GOptionGroup
{
    const gchar *name;
    gpointer user_data;
} GOptionGroup;

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

static inline gpointer g_malloc(gsize n)
{
    return malloc(n);
}

static inline gpointer g_malloc0(gsize n)
{
    gpointer mem = calloc(1, n);
    return mem;
}

static inline gpointer g_malloc0_n(gsize count, gsize size)
{
    if (count == 0 || size == 0)
    {
        return calloc(1, count * size);
    }
    if (SIZE_MAX / count < size)
    {
        return NULL;
    }
    return calloc(count, size);
}

static inline gpointer g_realloc(gpointer ptr, gsize size)
{
    return realloc(ptr, size);
}

static inline void g_free(gpointer ptr)
{
    free(ptr);
}

static inline gchar *g_strdup(const gchar *str)
{
    if (!str)
    {
        return NULL;
    }
    gsize len = strlen(str) + 1;
    gchar *dup = g_malloc(len);
    if (dup)
    {
        memcpy(dup, str, len);
    }
    return dup;
}

static inline gchar *g_strdup_printf(const gchar *format, ...)
{
    va_list args;
    va_start(args, format);
    va_list args_copy;
    va_copy(args_copy, args);
    int len = vsnprintf(NULL, 0, format, args_copy);
    va_end(args_copy);
    if (len < 0)
    {
        va_end(args);
        return NULL;
    }
    gchar *buffer = g_malloc((gsize)len + 1);
    if (buffer)
    {
        vsnprintf(buffer, (size_t)len + 1, format, args);
    }
    va_end(args);
    return buffer;
}

static inline gboolean g_str_equal(gconstpointer v1, gconstpointer v2)
{
    const gchar *s1 = (const gchar *)v1;
    const gchar *s2 = (const gchar *)v2;
    if (s1 == s2)
    {
        return TRUE;
    }
    if (!s1 || !s2)
    {
        return FALSE;
    }
    return strcmp(s1, s2) == 0;
}

static inline void g_assert_not_reached(void)
{
    assert(!"g_assert_not_reached()");
}

static inline void g_assert(gboolean expr)
{
    assert(expr);
}

GBytes *g_bytes_new(const void *data, gsize size);
GBytes *g_bytes_new_take(void *data, gsize size);
GBytes *g_bytes_ref(GBytes *bytes);
void    g_bytes_unref(GBytes *bytes);
gsize   g_bytes_get_size(const GBytes *bytes);
const void *g_bytes_get_data(const GBytes *bytes, gsize *size);

GError *g_error_new(guint32 domain, gint code, const gchar *format, ...);
GError *g_error_new_literal(guint32 domain, gint code, const gchar *message);
void    g_error_free(GError *error);
gboolean g_error_matches(const GError *error, guint32 domain, gint code);
void    g_set_error(GError **err, guint32 domain, gint code, const gchar *format, ...);

GSList *g_slist_prepend(GSList *list, gpointer data);
GSList *g_slist_append(GSList *list, gpointer data);
GSList *g_slist_remove(GSList *list, gconstpointer data);
GSList *g_slist_remove_link(GSList *list, GSList *link);
GSList *g_slist_delete_link(GSList *list, GSList *link);
GSList *g_slist_next(GSList *list);
GSList *g_slist_nth(GSList *list, guint n);
gpointer g_slist_nth_data(GSList *list, guint n);
guint   g_slist_length(GSList *list);
GSList *g_slist_find(GSList *list, gconstpointer data);
GSList *g_slist_find_custom(GSList *list, gconstpointer data, GCompareFunc func);
GSList *g_slist_insert(GSList *list, gpointer data, gint position);
GSList *g_slist_insert_sorted(GSList *list, gpointer data, GCompareFunc func);
GSList *g_slist_sort(GSList *list, GCompareFunc func);
GSList *g_slist_copy(GSList *list);
void    g_slist_foreach(GSList *list, GFunc func, gpointer user_data);
void    g_slist_free(GSList *list);
void    g_slist_free_full(GSList *list, GDestroyNotify free_func);

typedef guint32 GQuark;
GQuark g_quark_from_string(const gchar *string);
GQuark g_quark_from_static_string(const gchar *string);

const gchar *g_module_error(void);

void g_warning(const gchar *format, ...);



#endif /* GLIB_H */
