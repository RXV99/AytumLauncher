#include "graphics.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <psp2/types.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/gxm.h>
#include <psp2/display.h>
#include <psp2/sysmodule.h>
#include <vita2d.h>

/* Default vita2d texture font */
static vita2d_pvf *vita_font = NULL;

static int current_color = 0x000000;
static int translate_x = 0;
static int translate_y = 0;
static int clip_x = 0, clip_y = 0, clip_w = 960, clip_h = 544;

/* Vita uses RGBA8888 textures natively */
#define VITA_DISPLAY_W 960
#define VITA_DISPLAY_H 544

int lcdui_init(void) {
    /* Load font sysmodules (required on real hardware) */
    sceSysmoduleLoadModule(SCE_SYSMODULE_PVF);
    sceSysmoduleLoadModule(SCE_SYSMODULE_PGF);

    vita2d_init();
    vita2d_set_clear_color(RGBA8(0xFF, 0xFF, 0xFF, 0xFF));
    vita_font = vita2d_load_default_pvf();
    clip_w = VITA_DISPLAY_W;
    clip_h = VITA_DISPLAY_H;
    return 0;
}

void lcdui_shutdown(void) {
    if (vita_font) {
        vita2d_free_pvf(vita_font);
        vita_font = NULL;
    }
    vita2d_fini();
}

void lcdui_begin_frame(void) {
    vita2d_start_drawing();
    vita2d_clear_screen();
}

void lcdui_end_frame(void) {
    vita2d_end_drawing();
    vita2d_swap_buffers();
}

void lcdui_set_color(int rgb) {
    current_color = rgb;
}

int lcdui_get_color(void) {
    return current_color;
}

void lcdui_set_font(void *font) {
    (void)font;
}

void lcdui_set_stroke(int style) {
    (void)style;
}

void lcdui_set_translate(int x, int y) {
    translate_x = x;
    translate_y = y;
}

void lcdui_get_translate(int *x, int *y) {
    *x = translate_x;
    *y = translate_y;
}

void lcdui_set_clip(int x, int y, int w, int h) {
    clip_x = x;
    clip_y = y;
    clip_w = w;
    clip_h = h;
    vita2d_set_region_clip(x, y, x + w, y + h, 0);
}

void lcdui_get_clip(int *x, int *y, int *w, int *h) {
    *x = clip_x;
    *y = clip_y;
    *w = clip_w;
    *h = clip_h;
}

/* Drawing primitives */
void lcdui_draw_line(int x1, int y1, int x2, int y2) {
    x1 += translate_x; y1 += translate_y;
    x2 += translate_x; y2 += translate_y;
    unsigned int c = RGBA8(
        (current_color >> 16) & 0xFF,
        (current_color >> 8) & 0xFF,
        current_color & 0xFF,
        0xFF
    );
    vita2d_draw_line(x1, y1, x2, y2, c);
}

void lcdui_draw_rect(int x, int y, int w, int h, int arc) {
    x += translate_x; y += translate_y;
    unsigned int c = RGBA8(
        (current_color >> 16) & 0xFF,
        (current_color >> 8) & 0xFF,
        current_color & 0xFF,
        0xFF
    );
    vita2d_draw_rectangle(x, y, w, h, c);
}

void lcdui_fill_rect(int x, int y, int w, int h) {
    x += translate_x; y += translate_y;
    unsigned int c = RGBA8(
        (current_color >> 16) & 0xFF,
        (current_color >> 8) & 0xFF,
        current_color & 0xFF,
        0xFF
    );
    vita2d_draw_rectangle(x, y, w, h, c);
}

void lcdui_draw_round_rect(int x, int y, int w, int h, int aw, int ah) {
    x += translate_x; y += translate_y;
    unsigned int c = RGBA8(
        (current_color >> 16) & 0xFF,
        (current_color >> 8) & 0xFF,
        current_color & 0xFF,
        0xFF
    );
    vita2d_draw_rectangle(x, y, w, h, c);
}

void lcdui_fill_round_rect(int x, int y, int w, int h, int aw, int ah) {
    x += translate_x; y += translate_y;
    lcdui_fill_rect(x, y, w, h);
}

void lcdui_draw_arc(int x, int y, int r, int sa, int ea) {
    (void)x; (void)y; (void)r; (void)sa; (void)ea;
}

void lcdui_fill_arc(int x, int y, int r, int sa, int ea) {
    (void)x; (void)y; (void)r; (void)sa; (void)ea;
}

void lcdui_draw_triangle(int x1, int y1, int x2, int y2, int x3, int y3) {
    x1 += translate_x; y1 += translate_y;
    x2 += translate_x; y2 += translate_y;
    x3 += translate_x; y3 += translate_y;
    unsigned int c = RGBA8(
        (current_color >> 16) & 0xFF,
        (current_color >> 8) & 0xFF,
        current_color & 0xFF,
        0xFF
    );
    vita2d_draw_line(x1, y1, x2, y2, c);
    vita2d_draw_line(x2, y2, x3, y3, c);
    vita2d_draw_line(x3, y3, x1, y1, c);
}

void lcdui_fill_triangle(int x1, int y1, int x2, int y2, int x3, int y3) {
    lcdui_draw_triangle(x1, y1, x2, y2, x3, y3);
}

/* Text rendering */
void lcdui_draw_string(const char *str, int x, int y, int anchor) {
    if (!str || !vita_font) return;

    int tx = x + translate_x;
    int ty = y + translate_y;
    int str_w = lcdui_string_width(str);
    int str_h = lcdui_font_height();

    /* Apply anchor */
    if (anchor & ANCHOR_HCENTER) tx -= str_w / 2;
    else if (anchor & ANCHOR_RIGHT) tx -= str_w;
    if (anchor & ANCHOR_VCENTER) ty -= str_h / 2;
    else if (anchor & ANCHOR_BOTTOM) ty -= str_h;
    else if (anchor & ANCHOR_BASELINE) ty -= str_h;

    unsigned int c = RGBA8(
        (current_color >> 16) & 0xFF,
        (current_color >> 8) & 0xFF,
        current_color & 0xFF,
        0xFF
    );

    vita2d_pvf_draw_text(vita_font, tx, ty, c, 16, str);
}

void lcdui_draw_char(char c, int x, int y, int anchor) {
    char str[2] = {c, 0};
    lcdui_draw_string(str, x, y, anchor);
}

int lcdui_string_width(const char *str) {
    if (!str || !vita_font) return 8 * (int)strlen(str);
    return vita2d_pvf_text_width(vita_font, 16, str);
}

int lcdui_char_width(char c) {
    char str[2] = {c, 0};
    return lcdui_string_width(str);
}

int lcdui_font_height(void) {
    return 18;
}

/* Image operations */
lcdui_image *lcdui_create_image(int w, int h) {
    lcdui_image *img = (lcdui_image*)calloc(1, sizeof(lcdui_image));
    if (!img) return NULL;
    img->width = w;
    img->height = h;
    img->pixels = (uint32_t*)calloc((size_t)w * h, 4);
    if (!img->pixels) { free(img); return NULL; }
    return img;
}

lcdui_image *lcdui_load_png(const uint8_t *data, size_t size) {
    (void)data;
    (void)size;
    return NULL;
}

void lcdui_destroy_image(lcdui_image *img) {
    if (img) {
        free(img->pixels);
        free(img);
    }
}

void lcdui_draw_image(lcdui_image *img, int x, int y, int anchor) {
    if (!img) return;
    int tx = x + translate_x;
    int ty = y + translate_y;
    if (anchor & ANCHOR_HCENTER) tx -= img->width / 2;
    else if (anchor & ANCHOR_RIGHT) tx -= img->width;
    if (anchor & ANCHOR_VCENTER) ty -= img->height / 2;
    else if (anchor & ANCHOR_BOTTOM) ty -= img->height;

    unsigned int c = RGBA8(
        (current_color >> 16) & 0xFF,
        (current_color >> 8) & 0xFF,
        current_color & 0xFF,
        0xFF
    );
    vita2d_draw_rectangle(tx, ty, img->width, img->height, c);
}

void lcdui_get_image_dimensions(lcdui_image *img, int *w, int *h) {
    if (img) { *w = img->width; *h = img->height; }
    else { *w = 0; *h = 0; }
}

void lcdui_clear(int color) {
    unsigned int c = RGBA8(
        (color >> 16) & 0xFF,
        (color >> 8) & 0xFF,
        color & 0xFF,
        0xFF
    );
    vita2d_set_clear_color(c);
}
