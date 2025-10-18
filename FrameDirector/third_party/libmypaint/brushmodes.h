#ifndef BRUSHMODES_H
#define BRUSHMODES_H

#include <stdint.h>

void draw_dab_pixels_BlendMode_Normal (uint16_t * mask,
                                       uint16_t * rgba,
                                       uint16_t color_r,
                                       uint16_t color_g,
                                       uint16_t color_b,
                                       uint16_t opacity);

void draw_dab_pixels_BlendMode_Normal_Paint (uint16_t * mask,
                                       uint16_t * rgba,
                                       uint16_t color_r,
                                       uint16_t color_g,
                                       uint16_t color_b,
                                       uint16_t opacity);
void
draw_dab_pixels_BlendMode_Color (uint16_t *mask,
                                 uint16_t *rgba, // b=bottom, premult
                                 uint16_t color_r,  // }
                                 uint16_t color_g,  // }-- a=top, !premult
                                 uint16_t color_b,  // }
                                 uint16_t opacity);
void
draw_dab_pixels_BlendMode_Posterize (uint16_t *mask,
                                 uint16_t *rgba, // b=bottom, premult
                                 uint16_t posterize,
                                 uint16_t posterize_num);

void draw_dab_pixels_BlendMode_Normal_and_Eraser (uint16_t * mask,
                                                  uint16_t * rgba,
                                                  uint16_t color_r,
                                                  uint16_t color_g,
                                                  uint16_t color_b,
                                                  uint16_t color_a,
                                                  uint16_t opacity);

void draw_dab_pixels_BlendMode_Normal_and_Eraser_Paint (uint16_t * mask,
                                                  uint16_t * rgba,
                                                  uint16_t color_r,
                                                  uint16_t color_g,
                                                  uint16_t color_b,
                                                  uint16_t color_a,
                                                  uint16_t opacity);

void draw_dab_pixels_BlendMode_LockAlpha (uint16_t * mask,
                                          uint16_t * rgba,
                                          uint16_t color_r,
                                          uint16_t color_g,
                                          uint16_t color_b,
                                          uint16_t opacity);

void draw_dab_pixels_BlendMode_LockAlpha_Paint (uint16_t * mask,
                                          uint16_t * rgba,
                                          uint16_t color_r,
                                          uint16_t color_g,
                                          uint16_t color_b,
                                          uint16_t opacity);

void get_color_pixels_accumulate (uint16_t * mask,
                                  uint16_t * rgba,
                                  float * sum_weight,
                                  float * sum_r,
                                  float * sum_g,
                                  float * sum_b,
                                  float * sum_a,
                                  float paint,
                                  uint16_t sample_interval,
                                  float random_sample_rate
                                  );



#endif // BRUSHMODES_H
