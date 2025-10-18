#ifndef GLIB_H
#define GLIB_H

#include <stddef.h>
#include <stdint.h>

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
typedef unsigned char guchar;
typedef int gint;
typedef unsigned int guint;
typedef unsigned long gulong;
typedef gint gboolean;

typedef void (*GDestroyNotify)(gpointer data);

typedef struct _GArray GArray;
typedef struct _GPtrArray GPtrArray;

typedef int (*GCompareFunc)(gconstpointer a, gconstpointer b);

typedef void (*GFunc)(gpointer data, gpointer user_data);

#define G_GNUC_UNUSED
#define G_GNUC_WARN_UNUSED_RESULT
#define G_GNUC_PRINTF(format_idx, arg_idx)

#endif /* GLIB_H */
