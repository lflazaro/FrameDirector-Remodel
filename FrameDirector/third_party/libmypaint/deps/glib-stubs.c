#include "glib.h"

#include <stdio.h>

static void *dup_memory(const void *src, gsize size)
{
    if (!src || size == 0)
    {
        return NULL;
    }
    void *dst = g_malloc(size);
    if (dst)
    {
        memcpy(dst, src, size);
    }
    return dst;
}

GBytes *g_bytes_new(const void *data, gsize size)
{
    GBytes *bytes = g_malloc(sizeof(GBytes));
    if (!bytes)
    {
        return NULL;
    }
    bytes->data = dup_memory(data, size);
    bytes->size = size;
    bytes->ref_count = 1;
    bytes->destroy = g_free;
    return bytes;
}

GBytes *g_bytes_new_take(void *data, gsize size)
{
    GBytes *bytes = g_malloc(sizeof(GBytes));
    if (!bytes)
    {
        g_free(data);
        return NULL;
    }
    bytes->data = data;
    bytes->size = size;
    bytes->ref_count = 1;
    bytes->destroy = g_free;
    return bytes;
}

GBytes *g_bytes_ref(GBytes *bytes)
{
    if (bytes)
    {
        bytes->ref_count++;
    }
    return bytes;
}

void g_bytes_unref(GBytes *bytes)
{
    if (!bytes)
    {
        return;
    }
    if (bytes->ref_count > 0)
    {
        bytes->ref_count--;
    }
    if (bytes->ref_count == 0)
    {
        if (bytes->destroy && bytes->data)
        {
            bytes->destroy(bytes->data);
        }
        g_free(bytes);
    }
}

gsize g_bytes_get_size(const GBytes *bytes)
{
    return bytes ? bytes->size : 0;
}

const void *g_bytes_get_data(const GBytes *bytes, gsize *size)
{
    if (!bytes)
    {
        if (size)
        {
            *size = 0;
        }
        return NULL;
    }
    if (size)
    {
        *size = bytes->size;
    }
    return bytes->data;
}

static GError *g_error_alloc(guint32 domain, gint code, const gchar *message)
{
    GError *error = g_malloc(sizeof(GError));
    if (!error)
    {
        return NULL;
    }
    error->domain = domain;
    error->code = code;
    error->message = g_strdup(message ? message : "");
    return error;
}

GError *g_error_new(guint32 domain, gint code, const gchar *format, ...)
{
    va_list args;
    va_start(args, format);
    va_list copy;
    va_copy(copy, args);
    int len = vsnprintf(NULL, 0, format, copy);
    va_end(copy);
    if (len < 0)
    {
        va_end(args);
        return g_error_alloc(domain, code, "");
    }
    gchar *buffer = g_malloc((gsize)len + 1);
    if (!buffer)
    {
        va_end(args);
        return g_error_alloc(domain, code, "");
    }
    vsnprintf(buffer, (size_t)len + 1, format, args);
    va_end(args);
    GError *error = g_error_alloc(domain, code, buffer);
    g_free(buffer);
    return error;
}

GError *g_error_new_literal(guint32 domain, gint code, const gchar *message)
{
    return g_error_alloc(domain, code, message);
}

void g_error_free(GError *error)
{
    if (!error)
    {
        return;
    }
    g_free(error->message);
    g_free(error);
}

gboolean g_error_matches(const GError *error, guint32 domain, gint code)
{
    if (!error)
    {
        return FALSE;
    }
    return error->domain == domain && error->code == code;
}

void g_set_error(GError **err, guint32 domain, gint code, const gchar *format, ...)
{
    if (!err)
    {
        return;
    }

    va_list args;
    va_start(args, format);
    va_list copy;
    va_copy(copy, args);
    int len = vsnprintf(NULL, 0, format, copy);
    va_end(copy);

    gchar *buffer = NULL;
    if (len >= 0)
    {
        buffer = g_malloc((gsize)len + 1);
        if (buffer)
        {
            vsnprintf(buffer, (size_t)len + 1, format, args);
        }
    }
    va_end(args);

    if (!buffer)
    {
        buffer = g_strdup("");
    }

    if (!buffer)
    {
        return;
    }

    g_error_free(*err);
    *err = g_error_alloc(domain, code, buffer);
    g_free(buffer);
}

static GSList *g_slist_new_link(gpointer data)
{
    GSList *node = g_malloc(sizeof(GSList));
    if (!node)
    {
        return NULL;
    }
    node->data = data;
    node->next = NULL;
    return node;
}

GSList *g_slist_prepend(GSList *list, gpointer data)
{
    GSList *node = g_slist_new_link(data);
    if (!node)
    {
        return list;
    }
    node->next = list;
    return node;
}

GSList *g_slist_append(GSList *list, gpointer data)
{
    GSList *node = g_slist_new_link(data);
    if (!node)
    {
        return list;
    }
    if (!list)
    {
        return node;
    }
    GSList *iter = list;
    while (iter->next)
    {
        iter = iter->next;
    }
    iter->next = node;
    return list;
}

GSList *g_slist_remove(GSList *list, gconstpointer data)
{
    GSList *prev = NULL;
    GSList *iter = list;
    while (iter)
    {
        if (iter->data == data)
        {
            if (prev)
            {
                prev->next = iter->next;
            }
            else
            {
                list = iter->next;
            }
            g_free(iter);
            break;
        }
        prev = iter;
        iter = iter->next;
    }
    return list;
}

GSList *g_slist_remove_link(GSList *list, GSList *link)
{
    if (!list || !link)
    {
        return list;
    }
    GSList *prev = NULL;
    GSList *iter = list;
    while (iter)
    {
        if (iter == link)
        {
            if (prev)
            {
                prev->next = link->next;
            }
            else
            {
                list = link->next;
            }
            link->next = NULL;
            break;
        }
        prev = iter;
        iter = iter->next;
    }
    return list;
}

GSList *g_slist_delete_link(GSList *list, GSList *link)
{
    list = g_slist_remove_link(list, link);
    if (link)
    {
        g_free(link);
    }
    return list;
}

GSList *g_slist_next(GSList *list)
{
    return list ? list->next : NULL;
}

GSList *g_slist_nth(GSList *list, guint n)
{
    GSList *iter = list;
    while (iter && n--)
    {
        iter = iter->next;
    }
    return iter;
}

gpointer g_slist_nth_data(GSList *list, guint n)
{
    GSList *node = g_slist_nth(list, n);
    return node ? node->data : NULL;
}

guint g_slist_length(GSList *list)
{
    guint len = 0;
    for (GSList *iter = list; iter; iter = iter->next)
    {
        ++len;
    }
    return len;
}

GSList *g_slist_find(GSList *list, gconstpointer data)
{
    for (GSList *iter = list; iter; iter = iter->next)
    {
        if (iter->data == data)
        {
            return iter;
        }
    }
    return NULL;
}

GSList *g_slist_find_custom(GSList *list, gconstpointer data, GCompareFunc func)
{
    if (!func)
    {
        return NULL;
    }
    for (GSList *iter = list; iter; iter = iter->next)
    {
        if (func(iter->data, data) == 0)
        {
            return iter;
        }
    }
    return NULL;
}

GSList *g_slist_insert(GSList *list, gpointer data, gint position)
{
    if (position <= 0 || !list)
    {
        return g_slist_prepend(list, data);
    }
    GSList *prev = list;
    while (prev->next && --position)
    {
        prev = prev->next;
    }
    GSList *node = g_slist_new_link(data);
    if (!node)
    {
        return list;
    }
    node->next = prev->next;
    prev->next = node;
    return list;
}

static GSList *g_slist_insert_before_sorted(GSList *list, gpointer data, GCompareFunc func)
{
    GSList *node = g_slist_new_link(data);
    if (!node)
    {
        return list;
    }
    if (!list || func(data, list->data) < 0)
    {
        node->next = list;
        return node;
    }
    GSList *prev = list;
    GSList *iter = list->next;
    while (iter && func(data, iter->data) >= 0)
    {
        prev = iter;
        iter = iter->next;
    }
    prev->next = node;
    node->next = iter;
    return list;
}

GSList *g_slist_insert_sorted(GSList *list, gpointer data, GCompareFunc func)
{
    if (!func)
    {
        return g_slist_append(list, data);
    }
    return g_slist_insert_before_sorted(list, data, func);
}

static GSList *merge_sorted(GSList *a, GSList *b, GCompareFunc func)
{
    GSList head = {0};
    GSList *tail = &head;
    while (a && b)
    {
        if (func(a->data, b->data) <= 0)
        {
            tail->next = a;
            a = a->next;
        }
        else
        {
            tail->next = b;
            b = b->next;
        }
        tail = tail->next;
    }
    tail->next = a ? a : b;
    return head.next;
}

static GSList *split_list(GSList *list)
{
    GSList *fast = list->next;
    GSList *slow = list;
    while (fast && fast->next)
    {
        fast = fast->next->next;
        slow = slow->next;
    }
    GSList *second = slow->next;
    slow->next = NULL;
    return second;
}

GSList *g_slist_sort(GSList *list, GCompareFunc func)
{
    if (!list || !list->next || !func)
    {
        return list;
    }
    GSList *second = split_list(list);
    list = g_slist_sort(list, func);
    second = g_slist_sort(second, func);
    return merge_sorted(list, second, func);
}

GSList *g_slist_copy(GSList *list)
{
    GSList *copy = NULL;
    GSList **tail = &copy;
    for (GSList *iter = list; iter; iter = iter->next)
    {
        GSList *node = g_slist_new_link(iter->data);
        if (!node)
        {
            continue;
        }
        *tail = node;
        tail = &node->next;
    }
    return copy;
}

void g_slist_foreach(GSList *list, GFunc func, gpointer user_data)
{
    if (!func)
    {
        return;
    }
    for (GSList *iter = list; iter; iter = iter->next)
    {
        func(iter->data, user_data);
    }
}

void g_slist_free(GSList *list)
{
    while (list)
    {
        GSList *next = list->next;
        g_free(list);
        list = next;
    }
}

void g_slist_free_full(GSList *list, GDestroyNotify free_func)
{
    while (list)
    {
        GSList *next = list->next;
        if (free_func)
        {
            free_func(list->data);
        }
        g_free(list);
        list = next;
    }
}

typedef struct _QuarkEntry
{
    gchar *string;
    GQuark id;
} QuarkEntry;

static QuarkEntry *quark_table = NULL;
static gsize quark_table_size = 0;
static GQuark next_quark = 1;

static QuarkEntry *find_quark(const gchar *string)
{
    if (!string)
    {
        return NULL;
    }
    for (gsize i = 0; i < quark_table_size; ++i)
    {
        if (g_str_equal(quark_table[i].string, string))
        {
            return &quark_table[i];
        }
    }
    return NULL;
}

static GQuark register_quark(const gchar *string)
{
    QuarkEntry *existing = find_quark(string);
    if (existing)
    {
        return existing->id;
    }
    QuarkEntry *new_table = g_realloc(quark_table, sizeof(QuarkEntry) * (quark_table_size + 1));
    if (!new_table)
    {
        return 0;
    }
    quark_table = new_table;
    quark_table[quark_table_size].string = g_strdup(string);
    quark_table[quark_table_size].id = next_quark++;
    quark_table_size++;
    return quark_table[quark_table_size - 1].id;
}

GQuark g_quark_from_string(const gchar *string)
{
    if (!string)
    {
        return 0;
    }
    return register_quark(string);
}

GQuark g_quark_from_static_string(const gchar *string)
{
    if (!string)
    {
        return 0;
    }
    QuarkEntry *existing = find_quark(string);
    if (existing)
    {
        return existing->id;
    }
    QuarkEntry *new_table = g_realloc(quark_table, sizeof(QuarkEntry) * (quark_table_size + 1));
    if (!new_table)
    {
        return 0;
    }
    quark_table = new_table;
    quark_table[quark_table_size].string = (gchar *)string;
    quark_table[quark_table_size].id = next_quark++;
    quark_table_size++;
    return quark_table[quark_table_size - 1].id;
}

const gchar *g_module_error(void)
{
    return "module loading not supported";
}

void g_warning(const gchar *format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    fputc('\n', stderr);
    va_end(args);
}

*** End ***
