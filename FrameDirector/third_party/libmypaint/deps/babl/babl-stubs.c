#include "babl.h"

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum
{
    BABL_KIND_GENERIC,
    BABL_KIND_TYPE,
    BABL_KIND_MODEL,
    BABL_KIND_SPACE,
    BABL_KIND_FORMAT,
    BABL_KIND_COMPONENT,
    BABL_KIND_TRC,
    BABL_KIND_FISH
} BablKind;

struct _Babl
{
    BablKind kind;
    char *name;
    const char *base_name;
    int components;
    int has_alpha;
    int bytes_per_pixel;
    int flags;
    const char *encoding;
    const Babl *model;
    const Babl *type;
    const Babl *space;
    double luminance[3];
    double gamma;
    const char *icc_data;
    int icc_length;
    const Babl *trc[3];
    const Babl *from;
    const Babl *to;
    BablFishProcess process;
};

static Babl **registry = NULL;
static size_t registry_count = 0;
static size_t registry_capacity = 0;

static unsigned long tick_counter = 0;

static char *dup_string(const char *text)
{
    if (!text)
        return NULL;
    size_t len = strlen(text) + 1;
    char *copy = malloc(len);
    if (!copy)
    {
        fprintf(stderr, "babl stub: allocation failed\n");
        abort();
    }
    memcpy(copy, text, len);
    return copy;
}

static void *xrealloc(void *ptr, size_t size)
{
    void *result = realloc(ptr, size);
    if (!result && size)
    {
        fprintf(stderr, "babl stub: allocation failed\n");
        abort();
    }
    return result;
}

static Babl *register_babl(BablKind kind, const char *name)
{
    for (size_t i = 0; i < registry_count; ++i)
    {
        if (registry[i]->kind == kind && registry[i]->name && name && strcmp(registry[i]->name, name) == 0)
            return registry[i];
    }

    if (registry_count == registry_capacity)
    {
        registry_capacity = registry_capacity ? registry_capacity * 2 : 64;
        registry = xrealloc(registry, registry_capacity * sizeof(Babl *));
    }

    Babl *babl = calloc(1, sizeof(Babl));
    if (!babl)
        return NULL;
    if (name)
    {
        babl->name = dup_string(name);
        babl->base_name = babl->name;
    }
    babl->kind = kind;
    registry[registry_count++] = babl;
    return babl;
}

static int string_equals(const char *a, const char *b)
{
    return (a == b) || (a && b && strcmp(a, b) == 0);
}

static int infer_flags(const char *name, int has_alpha)
{
    int flags = has_alpha ? BABL_MODEL_FLAG_ALPHA : 0;
    if (!name)
        return flags;

    if (strstr(name, "CMYK") || strstr(name, "cmyk") || strstr(name, "camayaka") || strstr(name, "cmk") || strstr(name, "cyk"))
        flags |= BABL_MODEL_FLAG_CMYK;
    if (strstr(name, "Y'CbCr") || strstr(name, "Y'"))
        flags |= BABL_MODEL_FLAG_GRAY;
    if (strcmp(name, "Y") == 0 || strstr(name, "YA") || strstr(name, "Y " ) || strstr(name, " Ya"))
        flags |= BABL_MODEL_FLAG_GRAY;
    if (strstr(name, "CIE"))
        flags |= BABL_MODEL_FLAG_CIE;
    if (strstr(name, "R~") || strstr(name, "~"))
        flags |= BABL_MODEL_FLAG_LINEAR;
    if (strstr(name, "Ra") || strstr(name, "aA") || strstr(name, "~a"))
        flags |= BABL_MODEL_FLAG_ASSOCIATED;
    return flags;
}

static int type_bytes(const char *type_name)
{
    if (!type_name)
        return 4;
    if (strcmp(type_name, "u8") == 0)
        return 1;
    if (strcmp(type_name, "u16") == 0)
        return 2;
    if (strcmp(type_name, "u15") == 0)
        return 2;
    if (strcmp(type_name, "u32") == 0)
        return 4;
    if (strcmp(type_name, "float") == 0)
        return 4;
    if (strcmp(type_name, "double") == 0)
        return 8;
    if (strcmp(type_name, "half") == 0)
        return 2;
    return 4;
}

static const Babl *ensure_type(const char *name)
{
    if (!name)
        return NULL;
    Babl *type = register_babl(BABL_KIND_TYPE, name);
    if (type && type->bytes_per_pixel == 0)
    {
        type->bytes_per_pixel = type_bytes(name);
        type->components = 1;
        type->encoding = type->name;
    }
    return type;
}

static Babl *ensure_model(const char *name)
{
    if (!name)
        return NULL;
    Babl *model = register_babl(BABL_KIND_MODEL, name);
    if (model && model->flags == 0)
    {
        int has_alpha = (strstr(name, "A") != NULL || strstr(name, "a") != NULL);
        model->flags = infer_flags(name, has_alpha);
        model->components = (model->flags & BABL_MODEL_FLAG_GRAY) ? 1 : 4;
        if (has_alpha && model->components < 2)
            model->components = 2;
        model->has_alpha = (model->flags & BABL_MODEL_FLAG_ALPHA) != 0;
    }
    return model;
}

static Babl *ensure_space(const char *name)
{
    if (!name)
        return NULL;
    Babl *space = register_babl(BABL_KIND_SPACE, name);
    if (space && space->luminance[0] == 0 && space->luminance[1] == 0 && space->luminance[2] == 0)
    {
        space->luminance[0] = 0.2126;
        space->luminance[1] = 0.7152;
        space->luminance[2] = 0.0722;
        space->flags = infer_flags(name, 1);
    }
    return space;
}

typedef struct
{
    const char *name;
    const char *model_name;
    const char *type_name;
    int components;
    int has_alpha;
    int flags;
} FormatDefinition;

static const FormatDefinition format_definitions[] =
{
    {"B'aG'aR'aA u8",       "B'aG'aR'aA",       "u8",    4, 1,  BABL_MODEL_FLAG_ALPHA},
    {"CIE LCH(ab) alpha float", "CIE LCH(ab)", "float", 4, 1,  BABL_MODEL_FLAG_ALPHA | BABL_MODEL_FLAG_CIE},
    {"CIE LCH(ab) float",   "CIE LCH(ab)",     "float", 3, 0,  BABL_MODEL_FLAG_CIE},
    {"CIE Lab alpha float", "CIE Lab",         "float", 4, 1,  BABL_MODEL_FLAG_ALPHA | BABL_MODEL_FLAG_CIE},
    {"CIE Lab float",       "CIE Lab",         "float", 3, 0,  BABL_MODEL_FLAG_CIE},
    {"CMYK float",          "CMYK",            "float", 4, 0,  BABL_MODEL_FLAG_CMYK},
    {"CMYKA float",         "CMYK",            "float", 5, 1,  BABL_MODEL_FLAG_CMYK | BABL_MODEL_FLAG_ALPHA},
    {"HSLA float",          "HSLA",            "float", 4, 1,  BABL_MODEL_FLAG_ALPHA},
    {"HSVA double",         "HSVA",            "double",4, 1,  BABL_MODEL_FLAG_ALPHA},
    {"R'G'B' float",        "R'G'B'",          "float", 3, 0,  0},
    {"R'G'B' u16",          "R'G'B'",          "u16",   3, 0,  0},
    {"R'G'B' u8",           "R'G'B'",          "u8",    3, 0,  0},
    {"R'G'B'A double",      "R'G'B'A",         "double",4, 1,  BABL_MODEL_FLAG_ALPHA},
    {"R'G'B'A float",       "R'G'B'A",         "float", 4, 1,  BABL_MODEL_FLAG_ALPHA},
    {"R'G'B'A u16",         "R'G'B'A",         "u16",   4, 1,  BABL_MODEL_FLAG_ALPHA},
    {"R'G'B'A u8",          "R'G'B'A",         "u8",    4, 1,  BABL_MODEL_FLAG_ALPHA},
    {"R'aG'aB'aA float",    "R'aG'aB'aA",      "float", 4, 1,  BABL_MODEL_FLAG_ALPHA},
    {"R'aG'aB'aA u8",       "R'aG'aB'aA",      "u8",    4, 1,  BABL_MODEL_FLAG_ALPHA},
    {"RGB double",          "RGB",             "double",3, 0,  0},
    {"RGB float",           "RGB",             "float", 3, 0,  0},
    {"RGB u8",              "RGB",             "u8",    3, 0,  0},
    {"RGBA float",          "RGBA",            "float", 4, 1,  BABL_MODEL_FLAG_ALPHA},
    {"RGBA u16",            "RGBA",            "u16",   4, 1,  BABL_MODEL_FLAG_ALPHA},
    {"RGBA u8",             "RGBA",            "u8",    4, 1,  BABL_MODEL_FLAG_ALPHA},
    {"RaGaBaA float",       "RaGaBaA",         "float", 4, 1,  BABL_MODEL_FLAG_ALPHA | BABL_MODEL_FLAG_ASSOCIATED},
    {"RaGaBaA u8",          "RaGaBaA",         "u8",    4, 1,  BABL_MODEL_FLAG_ALPHA | BABL_MODEL_FLAG_ASSOCIATED},
    {"R~G~B~ float",        "R~G~B~",          "float", 3, 0,  BABL_MODEL_FLAG_LINEAR},
    {"R~G~B~A float",       "R~G~B~A",         "float", 4, 1,  BABL_MODEL_FLAG_LINEAR | BABL_MODEL_FLAG_ALPHA},
    {"R~aG~aB~aA float",    "R~aG~aB~aA",      "float", 4, 1,  BABL_MODEL_FLAG_LINEAR | BABL_MODEL_FLAG_ALPHA | BABL_MODEL_FLAG_ASSOCIATED},
    {"Y double",            "Y",               "double",1, 0,  BABL_MODEL_FLAG_GRAY},
    {"Y float",             "Y",               "float", 1, 0,  BABL_MODEL_FLAG_GRAY},
    {"Y u16",               "Y",               "u16",   1, 0,  BABL_MODEL_FLAG_GRAY},
    {"Y u8",                "Y",               "u8",    1, 0,  BABL_MODEL_FLAG_GRAY},
    {"Y' float",            "Y'",              "float", 1, 0,  BABL_MODEL_FLAG_GRAY},
    {"Y' u16",              "Y'",              "u16",   1, 0,  BABL_MODEL_FLAG_GRAY},
    {"Y' u8",               "Y'",              "u8",    1, 0,  BABL_MODEL_FLAG_GRAY},
    {"Y'A float",           "Y'A",             "float", 2, 1,  BABL_MODEL_FLAG_GRAY | BABL_MODEL_FLAG_ALPHA},
    {"Y'CbCrA float",       "Y'CbCrA",         "float", 4, 1,  BABL_MODEL_FLAG_ALPHA},
    {"YA double",           "YA",              "double",2, 1,  BABL_MODEL_FLAG_GRAY | BABL_MODEL_FLAG_ALPHA},
    {"YA float",            "YA",              "float", 2, 1,  BABL_MODEL_FLAG_GRAY | BABL_MODEL_FLAG_ALPHA},
    {"YA u32",              "YA",              "u32",   2, 1,  BABL_MODEL_FLAG_GRAY | BABL_MODEL_FLAG_ALPHA},
    {"YaA float",           "YaA",             "float", 3, 1,  BABL_MODEL_FLAG_GRAY | BABL_MODEL_FLAG_ALPHA},
    {"cairo-ACMK32",        "cairo-ACMK32",    "u8",    4, 1,  BABL_MODEL_FLAG_ALPHA | BABL_MODEL_FLAG_CMYK},
    {"cairo-ACYK32",        "cairo-ACYK32",    "u8",    4, 1,  BABL_MODEL_FLAG_ALPHA | BABL_MODEL_FLAG_CMYK},
    {"cairo-ARGB32",        "cairo-ARGB32",    "u8",    4, 1,  BABL_MODEL_FLAG_ALPHA},
    {"camayakaA float",     "camayakaA",       "float", 5, 1,  BABL_MODEL_FLAG_ALPHA | BABL_MODEL_FLAG_CMYK},
    {"camayakaA u8",        "camayakaA",       "u8",    5, 1,  BABL_MODEL_FLAG_ALPHA | BABL_MODEL_FLAG_CMYK},
    {"cmkA u16",            "cmkA",            "u16",   4, 1,  BABL_MODEL_FLAG_ALPHA | BABL_MODEL_FLAG_CMYK},
    {"cmykA double",        "CMYK",            "double",5, 1,  BABL_MODEL_FLAG_ALPHA | BABL_MODEL_FLAG_CMYK},
    {"cykA u16",            "cykA",            "u16",   4, 1,  BABL_MODEL_FLAG_ALPHA | BABL_MODEL_FLAG_CMYK}
};

static const FormatDefinition *lookup_format_definition(const char *name)
{
    for (size_t i = 0; i < sizeof(format_definitions) / sizeof(format_definitions[0]); ++i)
    {
        if (strcmp(format_definitions[i].name, name) == 0)
            return &format_definitions[i];
    }
    return NULL;
}

static Babl *find_format(const char *name, const Babl *space)
{
    for (size_t i = 0; i < registry_count; ++i)
    {
        Babl *candidate = registry[i];
        if (candidate->kind == BABL_KIND_FORMAT && candidate->base_name && strcmp(candidate->base_name, name) == 0)
        {
            if (candidate->space == space)
                return candidate;
        }
    }
    return NULL;
}

static int is_registered_babl(const void *ptr)
{
    for (size_t i = 0; i < registry_count; ++i)
    {
        if (registry[i] == ptr)
            return 1;
    }
    return 0;
}

static void configure_format(Babl *format, const char *name, const Babl *space)
{
    const FormatDefinition *def = lookup_format_definition(name);
    if (space)
    {
        if (!format->base_name || strcmp(format->base_name, name) != 0)
        {
            if (format->base_name && format->base_name != format->name)
                free((char *)format->base_name);
            format->base_name = dup_string(name);
        }
    }
    else
    {
        format->base_name = format->name;
    }
    if (def)
    {
        format->components = def->components;
        format->has_alpha = def->has_alpha;
        format->flags = def->flags ? def->flags : infer_flags(def->model_name, def->has_alpha);
        format->type = ensure_type(def->type_name);
        format->encoding = def->type_name;
        format->model = ensure_model(def->model_name);
    }
    else
    {
        format->components = 4;
        format->has_alpha = 1;
        format->flags = infer_flags(name, 1);
        format->type = ensure_type("float");
        format->encoding = "float";
        format->model = ensure_model("RGBA");
    }

    if (format->type)
        format->bytes_per_pixel = format->components * format->type->bytes_per_pixel;
    else
        format->bytes_per_pixel = format->components * 4;

    if (format->flags == 0 && format->model)
        format->flags = format->model->flags;

    format->space = space;
}

const Babl *babl_component(const char *name)
{
    return register_babl(BABL_KIND_COMPONENT, name);
}

const Babl *babl_format(const char *name)
{
    return babl_format_with_space(name, NULL);
}

const Babl *babl_format_with_space(const char *name, const Babl *space)
{
    if (!name)
        return NULL;
    Babl *existing = find_format(name, space);
    if (existing)
        return existing;

    char key[128];
    const char *key_name = name;
    if (space)
    {
        snprintf(key, sizeof(key), "%s@%p", name, (void *)space);
        key_name = key;
    }

    Babl *format = register_babl(BABL_KIND_FORMAT, key_name);
    if (!format)
        return NULL;
    configure_format(format, name, space);
    return format;
}

const Babl *babl_format_new(const void *first, ...)
{
    const char *explicit_name = NULL;
    const Babl *model = NULL;
    const Babl *type = NULL;

    if (first)
    {
        if (is_registered_babl(first))
        {
            const Babl *babl_first = (const Babl *)first;
            if (babl_first->kind == BABL_KIND_MODEL)
                model = babl_first;
            else if (babl_first->kind == BABL_KIND_TYPE)
                type = babl_first;
        }
        else
        {
            explicit_name = (const char *)first;
        }
    }

    va_list args;
    va_start(args, first);
    const void *arg = NULL;
    while ((arg = va_arg(args, const void *)))
    {
        if (is_registered_babl(arg))
        {
            const Babl *babl_arg = (const Babl *)arg;
            if (babl_arg->kind == BABL_KIND_MODEL)
                model = babl_arg;
            else if (babl_arg->kind == BABL_KIND_TYPE)
                type = babl_arg;
        }
        else if (!explicit_name)
        {
            explicit_name = (const char *)arg;
        }
    }
    va_end(args);

    const char *base_name = explicit_name ? explicit_name : (model ? model->name : "custom-format");

    Babl *format = register_babl(BABL_KIND_FORMAT, base_name);
    if (!format)
        return NULL;

    configure_format(format, base_name, NULL);

    if (model)
    {
        format->model = model;
        format->flags = model->flags;
    }
    if (type)
    {
        format->type = type;
        format->encoding = type->name;
    }

    format->bytes_per_pixel = format->components * (format->type ? format->type->bytes_per_pixel : 4);
    format->has_alpha = (format->flags & BABL_MODEL_FLAG_ALPHA) != 0 || format->has_alpha;
    if (format->flags == 0)
        format->flags = infer_flags(base_name, format->has_alpha);

    return format;
}

const Babl *babl_format_n(const Babl *type, int components)
{
    if (components <= 0)
        components = 1;
    const char *type_name = type ? type->name : "float";
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "generated-%s-%d", type_name, components);
    Babl *format = register_babl(BABL_KIND_FORMAT, buffer);
    if (!format)
        return NULL;

    format->type = type ? type : ensure_type(type_name);
    format->components = components;
    format->has_alpha = components > 1 ? 1 : 0;
    format->encoding = format->type ? format->type->name : type_name;
    format->bytes_per_pixel = components * (format->type ? format->type->bytes_per_pixel : 4);
    format->model = ensure_model("generated");
    format->flags = infer_flags(format->model->name, format->has_alpha);
    return format;
}

const Babl *babl_format_exists(const char *name)
{
    return babl_format(name);
}

int babl_format_get_n_components(const Babl *format)
{
    return format ? format->components : 0;
}

int babl_format_has_alpha(const Babl *format)
{
    return format ? format->has_alpha : 0;
}

int babl_format_get_bytes_per_pixel(const Babl *format)
{
    return format ? format->bytes_per_pixel : 0;
}

const char *babl_format_get_encoding(const Babl *format)
{
    return format ? format->encoding : NULL;
}

const Babl *babl_format_get_type(const Babl *format, int component)
{
    (void)component;
    return format ? format->type : NULL;
}

const Babl *babl_format_get_model(const Babl *format)
{
    return format ? format->model : NULL;
}

const Babl *babl_format_get_space(const Babl *format)
{
    return format ? format->space : NULL;
}

const Babl *babl_type(const char *name)
{
    return ensure_type(name);
}

const Babl *babl_type_new(const char *name, ...)
{
    Babl *type = register_babl(BABL_KIND_TYPE, name ? name : "generated-type");
    if (!type)
        return NULL;

    va_list args;
    va_start(args, name);
    const char *key = NULL;
    while ((key = va_arg(args, const char *)))
    {
        if (strcmp(key, "bits") == 0)
        {
            int bits = va_arg(args, int);
            type->bytes_per_pixel = (bits + 7) / 8;
        }
        else
        {
            (void)va_arg(args, const void *);
        }
    }
    va_end(args);

    if (type->bytes_per_pixel == 0)
        type->bytes_per_pixel = 4;
    type->components = 1;
    type->encoding = type->name;
    return type;
}

const Babl *babl_model(const char *name)
{
    return ensure_model(name ? name : "model");
}

const Babl *babl_model_with_space(const Babl *model, const Babl *space)
{
    (void)space;
    return model;
}

int babl_model_is(const Babl *model, const char *name)
{
    return model && name && string_equals(model->name, name);
}

BablModelFlag babl_get_model_flags(const Babl *model)
{
    return model ? (BablModelFlag)model->flags : BABL_MODEL_FLAG_NONE;
}

const Babl *babl_space(const char *name)
{
    return ensure_space(name ? name : "sRGB");
}

const Babl *babl_space_from_icc(const char *data,
                                int length,
                                BablIccIntent intent,
                                const char **error)
{
    (void)intent;
    if (error)
        *error = NULL;
    Babl *space = ensure_space("icc-space");
    if (space)
    {
        space->icc_data = data;
        space->icc_length = length;
    }
    return space;
}

const Babl *babl_space_from_chromaticities(const char *name,
                                           double white_x, double white_y,
                                           double red_x, double red_y,
                                           double green_x, double green_y,
                                           double blue_x, double blue_y,
                                           const Babl *trc_r,
                                           const Babl *trc_g,
                                           const Babl *trc_b,
                                           double luminance)
{
    Babl *space = ensure_space(name ? name : "custom-space");
    if (space)
    {
        space->luminance[0] = luminance > 0 ? luminance : 0.2126;
        space->luminance[1] = luminance > 0 ? luminance : 0.7152;
        space->luminance[2] = luminance > 0 ? luminance : 0.0722;
        space->trc[0] = trc_r;
        space->trc[1] = trc_g;
        space->trc[2] = trc_b;
        (void)white_x; (void)white_y; (void)red_x; (void)red_y;
        (void)green_x; (void)green_y; (void)blue_x; (void)blue_y;
    }
    return space;
}

void babl_space_get_rgb_luminance(const Babl *space,
                                  double *r,
                                  double *g,
                                  double *b)
{
    if (!space)
    {
        if (r) *r = 0.2126;
        if (g) *g = 0.7152;
        if (b) *b = 0.0722;
        return;
    }
    if (r) *r = space->luminance[0] ? space->luminance[0] : 0.2126;
    if (g) *g = space->luminance[1] ? space->luminance[1] : 0.7152;
    if (b) *b = space->luminance[2] ? space->luminance[2] : 0.0722;
}

void babl_space_get(const Babl *space,
                    double *white_x, double *white_y,
                    double *red_x, double *red_y,
                    double *green_x, double *green_y,
                    double *blue_x, double *blue_y,
                    const Babl **trc_r,
                    const Babl **trc_g,
                    const Babl **trc_b)
{
    (void)space;
    if (white_x) *white_x = 0.3127;
    if (white_y) *white_y = 0.3290;
    if (red_x) *red_x = 0.64;
    if (red_y) *red_y = 0.33;
    if (green_x) *green_x = 0.30;
    if (green_y) *green_y = 0.60;
    if (blue_x) *blue_x = 0.15;
    if (blue_y) *blue_y = 0.06;
    if (trc_r) *trc_r = babl_trc("sRGB");
    if (trc_g) *trc_g = babl_trc("sRGB");
    if (trc_b) *trc_b = babl_trc("sRGB");
}

const char *babl_space_get_icc(const Babl *space, int *length)
{
    if (length)
        *length = space ? space->icc_length : 0;
    return space ? space->icc_data : NULL;
}

int babl_space_is_cmyk(const Babl *space)
{
    return space ? ((space->flags & BABL_MODEL_FLAG_CMYK) != 0) : 0;
}

int babl_space_is_gray(const Babl *space)
{
    return space ? ((space->flags & BABL_MODEL_FLAG_GRAY) != 0) : 0;
}

int babl_space_is_rgb(const Babl *space)
{
    if (!space)
        return 1;
    if (babl_space_is_cmyk(space) || babl_space_is_gray(space))
        return 0;
    return 1;
}

const Babl *babl_trc(const char *name)
{
    Babl *trc = register_babl(BABL_KIND_TRC, name ? name : "trc");
    if (trc && trc->gamma == 0.0)
    {
        if (name && strcmp(name, "linear") == 0)
            trc->gamma = 1.0;
        else if (name && strcmp(name, "2.2") == 0)
            trc->gamma = 2.2;
        else if (name && strcmp(name, "sRGB") == 0)
            trc->gamma = 2.2;
        else
            trc->gamma = 2.2;
    }
    return trc;
}

const Babl *babl_trc_gamma(double gamma)
{
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "gamma-%g", gamma);
    Babl *trc = register_babl(BABL_KIND_TRC, buffer);
    if (trc)
        trc->gamma = gamma;
    return trc;
}

static void default_process(const Babl *fish,
                            void *src,
                            void *dst,
                            long n,
                            void *user_data)
{
    (void)user_data;
    const Babl *from = fish ? fish->from : NULL;
    const Babl *to = fish ? fish->to : NULL;
    int from_bpp = from ? from->bytes_per_pixel : 0;
    int to_bpp = to ? to->bytes_per_pixel : from_bpp;
    if (from_bpp <= 0)
        from_bpp = to_bpp;
    if (to_bpp <= 0)
        to_bpp = from_bpp;
    int copy_bpp = from_bpp < to_bpp ? from_bpp : to_bpp;
    if (copy_bpp <= 0)
        copy_bpp = 4;
    unsigned char *src_bytes = src;
    unsigned char *dst_bytes = dst;
    for (long i = 0; i < n; ++i)
    {
        memcpy(dst_bytes + i * to_bpp, src_bytes + i * from_bpp, (size_t)copy_bpp);
        if (to_bpp > copy_bpp)
        {
            memset(dst_bytes + i * to_bpp + copy_bpp, 0, (size_t)(to_bpp - copy_bpp));
        }
    }
}

const Babl *babl_fish(const Babl *source, const Babl *destination)
{
    Babl *fish = register_babl(BABL_KIND_FISH, "fish");
    if (fish)
    {
        fish->from = source;
        fish->to = destination;
        fish->process = (BablFishProcess)default_process;
    }
    return fish;
}

BablFishProcess babl_fish_get_process(const Babl *fish)
{
    return fish ? fish->process : NULL;
}

void babl_process(const Babl *fish,
                  void *src,
                  void *dst,
                  long n)
{
    BablFishProcess process = fish ? fish->process : NULL;
    if (!process)
        process = (BablFishProcess)default_process;
    process(fish, src, dst, n, NULL);
}

void babl_process_rows(const Babl *fish,
                       void *src,
                       int src_stride,
                       void *dst,
                       int dst_stride,
                       int width,
                       int height)
{
    unsigned char *src_bytes = src;
    unsigned char *dst_bytes = dst;
    for (int y = 0; y < height; ++y)
    {
        babl_process(fish,
                     src_bytes + (size_t)y * (size_t)src_stride,
                     dst_bytes + (size_t)y * (size_t)dst_stride,
                     width);
    }
}

const Babl *babl_from_jpeg_colorspace(int jpeg_space, const Babl *space)
{
    switch (jpeg_space)
    {
        case 1: /* grayscale */
            return babl_format_with_space("Y' u8", space);
        case 3: /* YCbCr */
            return babl_format_with_space("Y'CbCrA float", space);
        case 4: /* CMYK */
            return babl_format_with_space("CMYK float", space);
        default:
            return babl_format_with_space("R'G'B' u8", space);
    }
}

const Babl *babl_formats(void)
{
    return NULL;
}

void babl_gc(void)
{
}

void babl_init(void)
{
}

void babl_exit(void)
{
}

unsigned long babl_ticks(void)
{
    return ++tick_counter;
}

const char *babl_get_name(const Babl *babl)
{
    if (!babl)
        return "";
    if (babl->base_name)
        return babl->base_name;
    return babl->name ? babl->name : "";
}
