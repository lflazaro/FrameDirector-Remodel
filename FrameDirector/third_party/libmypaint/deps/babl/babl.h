#ifndef BABL_BABL_H
#define BABL_BABL_H

#include "../glib-object.h"

G_BEGIN_DECLS

typedef struct _Babl Babl;

typedef void (*BablFishProcess)(const Babl *fish,
                                 void       *src,
                                 void       *dst,
                                 long        n,
                                 void       *user_data);

typedef enum
{
    BABL_MODEL_FLAG_NONE        = 0,
    BABL_MODEL_FLAG_ALPHA       = 1 << 0,
    BABL_MODEL_FLAG_LINEAR      = 1 << 1,
    BABL_MODEL_FLAG_CMYK        = 1 << 2,
    BABL_MODEL_FLAG_GRAY        = 1 << 3,
    BABL_MODEL_FLAG_ASSOCIATED  = 1 << 4,
    BABL_MODEL_FLAG_CIE         = 1 << 5
} BablModelFlag;

typedef enum
{
    BABL_ICC_INTENT_PERCEPTUAL = 0,
    BABL_ICC_INTENT_RELATIVE_COLORIMETRIC = 1,
    BABL_ICC_INTENT_SATURATION = 2,
    BABL_ICC_INTENT_ABSOLUTE_COLORIMETRIC = 3,
    BABL_ICC_INTENT_DEFAULT = BABL_ICC_INTENT_RELATIVE_COLORIMETRIC
} BablIccIntent;

const Babl *babl_component (const char *name);
const Babl *babl_format (const char *name);
const Babl *babl_format_with_space (const char *name, const Babl *space);
const Babl *babl_format_new (const void *first, ...);
const Babl *babl_format_n (const Babl *type, int components);
const Babl *babl_format_exists (const char *name);
int         babl_format_get_n_components (const Babl *format);
int         babl_format_has_alpha (const Babl *format);
int         babl_format_get_bytes_per_pixel (const Babl *format);
const char *babl_format_get_encoding (const Babl *format);
const Babl *babl_format_get_type (const Babl *format, int component);
const Babl *babl_format_get_model (const Babl *format);
const Babl *babl_format_get_space (const Babl *format);

const Babl *babl_type (const char *name);
const Babl *babl_type_new (const char *name, ...);

const Babl *babl_model (const char *name);
const Babl *babl_model_with_space (const Babl *model, const Babl *space);
int         babl_model_is (const Babl *model, const char *name);
BablModelFlag babl_get_model_flags (const Babl *model);

const Babl *babl_space (const char *name);
const Babl *babl_space_from_icc (const char *data,
                                 int         length,
                                 BablIccIntent intent,
                                 const char **error);
const Babl *babl_space_from_chromaticities (const char *name,
                                            double white_x, double white_y,
                                            double red_x, double red_y,
                                            double green_x, double green_y,
                                            double blue_x, double blue_y,
                                            const Babl *trc_r,
                                            const Babl *trc_g,
                                            const Babl *trc_b,
                                            double luminance);
void        babl_space_get_rgb_luminance (const Babl *space,
                                          double *r,
                                          double *g,
                                          double *b);
void        babl_space_get (const Babl *space,
                            double *white_x, double *white_y,
                            double *red_x, double *red_y,
                            double *green_x, double *green_y,
                            double *blue_x, double *blue_y,
                            const Babl **trc_r,
                            const Babl **trc_g,
                            const Babl **trc_b);
const char *babl_space_get_icc (const Babl *space, int *length);
int         babl_space_is_cmyk (const Babl *space);
int         babl_space_is_gray (const Babl *space);
int         babl_space_is_rgb (const Babl *space);

const Babl *babl_trc (const char *name);
const Babl *babl_trc_gamma (double gamma);

const Babl *babl_fish (const Babl *source, const Babl *destination);
BablFishProcess babl_fish_get_process (const Babl *fish);
void            babl_process (const Babl *fish,
                              void       *src,
                              void       *dst,
                              long        n);
void            babl_process_rows (const Babl *fish,
                                   void       *src,
                                   int         src_stride,
                                   void       *dst,
                                   int         dst_stride,
                                   int         width,
                                   int         height);

const Babl *babl_from_jpeg_colorspace (int jpeg_space, const Babl *space);

const Babl *babl_formats (void);
void        babl_gc (void);
void        babl_init (void);
void        babl_exit (void);
unsigned long babl_ticks (void);
const char *babl_get_name (const Babl *babl);

G_END_DECLS

#endif /* BABL_BABL_H */
