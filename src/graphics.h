#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* LCDUI color format: 0x00RRGGBB */
#define RGBA8888(r, g, b, a) (((a) << 24) | ((r) << 16) | ((g) << 8) | (b))
#define RGB888(r, g, b)      (((r) << 16) | ((g) << 8) | (b))

/* Anchor constants matching Java ME */
#define ANCHOR_LEFT       (1 << 0)
#define ANCHOR_RIGHT      (1 << 1)
#define ANCHOR_HCENTER    (1 << 2)
#define ANCHOR_TOP        (1 << 3)
#define ANCHOR_BOTTOM     (1 << 4)
#define ANCHOR_VCENTER    (1 << 5)
#define ANCHOR_BASELINE   (1 << 6)

/* Initialize vita2d graphics subsystem */
int  lcdui_init(void);
void lcdui_shutdown(void);

/* Begin/end drawing frame */
void lcdui_begin_frame(void);
void lcdui_end_frame(void);

/* Graphics state */
void lcdui_set_color(int rgb);
int  lcdui_get_color(void);
void lcdui_set_font(void *font);
void lcdui_set_stroke(int style);
void lcdui_set_translate(int x, int y);
void lcdui_get_translate(int *x, int *y);
void lcdui_set_clip(int x, int y, int w, int h);
void lcdui_get_clip(int *x, int *y, int *w, int *h);

/* Drawing primitives */
void lcdui_draw_line(int x1, int y1, int x2, int y2);
void lcdui_draw_rect(int x, int y, int w, int h, int arc);
void lcdui_fill_rect(int x, int y, int w, int h);
void lcdui_draw_round_rect(int x, int y, int w, int h, int aw, int ah);
void lcdui_fill_round_rect(int x, int y, int w, int h, int aw, int ah);
void lcdui_draw_arc(int x, int y, int r, int sa, int ea);
void lcdui_fill_arc(int x, int y, int r, int sa, int ea);
void lcdui_draw_triangle(int x1, int y1, int x2, int y2, int x3, int y3);
void lcdui_fill_triangle(int x1, int y1, int x2, int y2, int x3, int y3);

/* Text */
void lcdui_draw_string(const char *str, int x, int y, int anchor);
void lcdui_draw_char(char c, int x, int y, int anchor);
int  lcdui_string_width(const char *str);
int  lcdui_char_width(char c);
int  lcdui_font_height(void);

/* Image */
typedef struct lcdui_image {
    int width;
    int height;
    uint32_t *pixels;
} lcdui_image;

lcdui_image *lcdui_create_image(int w, int h);
lcdui_image *lcdui_load_png(const uint8_t *data, size_t size);
void lcdui_destroy_image(lcdui_image *img);
void lcdui_draw_image(lcdui_image *img, int x, int y, int anchor);
void lcdui_get_image_dimensions(lcdui_image *img, int *w, int *h);

/* Screen clear */
void lcdui_clear(int color);

#ifdef __cplusplus
}
#endif

#endif /* GRAPHICS_H */
