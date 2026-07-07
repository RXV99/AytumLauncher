#include "launcher.h"
#include "fileio.h"
#include "graphics.h"
#include "input.h"
#include "audio.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <strings.h>
#include <psp2/kernel/threadmgr.h>

#define COL_BG        0x0D0D12
#define COL_SURFACE   0x15151E
#define COL_SURFACE2  0x1C1C28
#define COL_GOLD      0xC8A027
#define COL_GOLD_DIM  0x8A6F1A
#define COL_GOLD_BG   0x1E1A0E
#define COL_WHITE     0xE8E8E8
#define COL_TEXT      0xC0C0C8
#define COL_DIM       0x707078
#define COL_MUTED     0x505058
#define COL_SEP       0x22222E
#define COL_SEL_BG    0xC8A02718

#define SIDEBAR_W      164
#define SIDEBAR_ITEM_X 18
#define SIDEBAR_ITEM_H 44
#define SIDEBAR_ITEM_GAP 10
#define SIDEBAR_TOP_PAD 82

#define CONTENT_X   (SIDEBAR_W + 16)
#define CONTENT_Y   22
#define CONTENT_W   (960 - CONTENT_X - 16)

#define LIST_ROW_H  32
#define LIST_ROW_GAP 8

#define BROWSE_STATE_FILE "ux0:data/java/.browse_path"

/* ── Browse entry ── */
typedef struct {
    char name[256];
    char full_path[512];
    int  is_dir;
    int  is_jar;
    int  is_jad;
} browse_entry;

/* ── Static state ── */
static launcher_app apps[MAX_APPS];
static int app_count = 0;

static launcher_section current_section = SECTION_RECENT;
static int sidebar_sel = 0;
static int sidebar_focus = 1;

/* Browse */
static browse_entry *browse_items = NULL;
static int browse_item_count = 0;
static int browse_sel = 0;
static int browse_scroll = 0;
static char browse_path[512] = "ux0:";

/* Recent */
static int recent_sel = 0;
static char recent_sel_path[512];

/* Settings */
static int settings_sel = 0;
static int settings_item_count = 1;
static const char *res_options[] = {
    "960 x 544", "640 x 480", "480 x 320", "320 x 240"
};
static int res_count = 4;
static int res_current = 0;

/* ── Section labels ── */
static const char *section_labels[SECTION_COUNT] = {
    "Recents", "Browse", "Settings", "About"
};
static const char *section_icons[SECTION_COUNT] = {
    "\xC2\xBB",
    "\xE2\x97\x8B",
    "\xE2\x9A\x99",
    "\xE2\x93\xA1",
};

/* ── Internal helpers ── */
static void draw_gold_text(const char *text, int x, int y, int anchor) {
    lcdui_set_color(COL_GOLD);
    lcdui_draw_string(text, x, y, anchor);
}

static void draw_text(const char *text, int x, int y, int anchor, int color) {
    lcdui_set_color(color);
    lcdui_draw_string(text, x, y, anchor);
}

static void draw_separator(int x, int y, int w) {
    lcdui_set_color(COL_SEP);
    lcdui_draw_line(x, y, x + w, y);
}

/* Vertical center text Y inside a box */
static int center_text_y(int box_y, int box_h) {
    int fh = lcdui_font_height();
    return box_y + ((box_h - fh) / 2);
}

/* ── Browse directory lister ── */
static void browse_free_items(void) {
    if (browse_items) { free(browse_items); browse_items = NULL; }
    browse_item_count = 0;
}

static int browse_list_dir(const char *path) {
    browse_free_items();

    int count;
    char **names = fileio_list_dir(path, &count);
    if (!names) return -1;

    browse_items = (browse_entry*)calloc(count + 1, sizeof(browse_entry));
    if (!browse_items) { fileio_free_list(names, count); return -1; }

    int n = 0;
    if (strcmp(path, "ux0:") != 0) {
        strcpy(browse_items[n].name, "..");
        snprintf(browse_items[n].full_path, sizeof(browse_items[n].full_path), "%s", path);
        char *slash = strrchr(browse_items[n].full_path, '/');
        if (slash) *slash = 0;
        else        strcpy(browse_items[n].full_path, "ux0:");
        browse_items[n].is_dir = 1;
        n++;
    }

    for (int i = 0; i < count; i++) {
        if (names[i][0] == '.' && strcmp(names[i], "..") != 0) continue;

        char full[512];
        snprintf(full, sizeof(full), "%s/%s", path, names[i]);

        int is_dir = fileio_is_directory(full);
        int is_jar = !is_dir && fileio_has_extension(names[i], ".jar");
        int is_jad = !is_dir && fileio_has_extension(names[i], ".jad");

        if (!is_dir && !is_jar && !is_jad) continue;

        strncpy(browse_items[n].name, names[i], sizeof(browse_items[n].name) - 1);
        strncpy(browse_items[n].full_path, full, sizeof(browse_items[n].full_path) - 1);
        browse_items[n].is_dir = is_dir;
        browse_items[n].is_jar = is_jar;
        browse_items[n].is_jad = is_jad;
        n++;
    }
    fileio_free_list(names, count);

    for (int i = 1; i < n - 1; i++) {
        for (int j = i + 1; j < n; j++) {
            int swap = 0;
            if (browse_items[i].is_dir && !browse_items[j].is_dir) swap = 0;
            else if (!browse_items[i].is_dir && browse_items[j].is_dir) swap = 1;
            else if (browse_items[i].is_dir && browse_items[j].is_dir)
                swap = strcasecmp(browse_items[i].name, browse_items[j].name) > 0;
            else
                swap = strcasecmp(browse_items[i].name, browse_items[j].name) > 0;
            if (swap) {
                browse_entry t = browse_items[i];
                browse_items[i] = browse_items[j];
                browse_items[j] = t;
            }
        }
    }

    browse_item_count = n;
    browse_sel = 0;
    browse_scroll = 0;
    return n;
}

static void browse_enter_dir(const char *path) {
    strncpy(browse_path, path, sizeof(browse_path) - 1);
    launcher_save_browse_path(browse_path);
    browse_list_dir(browse_path);
}

static void browse_go_up(void) {
    if (strcmp(browse_path, "ux0:") == 0) {
        sidebar_focus = 1;
        return;
    }
    char *slash = strrchr(browse_path, '/');
    if (slash) {
        *slash = 0;
        if (browse_path[0] == 0) strcpy(browse_path, "ux0:");
    } else {
        strcpy(browse_path, "ux0:");
    }
    launcher_save_browse_path(browse_path);
    browse_list_dir(browse_path);
}

/* ── Launcher init / scan ── */
int launcher_init(void) {
    app_count = 0;
    current_section = SECTION_RECENT;
    sidebar_sel = 0;
    sidebar_focus = 1;
    recent_sel = 0;
    settings_sel = 0;
    browse_sel = 0;
    browse_scroll = 0;
    browse_items = NULL;
    browse_item_count = 0;

    if (launcher_load_browse_path(browse_path, sizeof(browse_path)) != 0)
        strcpy(browse_path, "ux0:");

    return launcher_scan();
}

void launcher_shutdown(void) {
    app_count = 0;
    browse_free_items();
}

int launcher_scan(void) {
    app_count = 0;
    const char *app_dir = fileio_get_base_path();
    if (!app_dir) return -1;

    int file_count;
    char **files = fileio_list_dir(app_dir, &file_count);
    if (!files) return -1;

    for (int i = 0; i < file_count && app_count < MAX_APPS; i++) {
        if (!fileio_has_extension(files[i], ".jar")) continue;
        launcher_app *app = &apps[app_count];
        memset(app, 0, sizeof(launcher_app));

        snprintf(app->jar_path, sizeof(app->jar_path) - 1, "%s/%s", app_dir, files[i]);

        char *jad_name = (char*)malloc(strlen(files[i]) + 4);
        if (jad_name) {
            strcpy(jad_name, files[i]);
            char *dot = strrchr(jad_name, '.');
            if (dot && strcasecmp(dot, ".jar") == 0) strcpy(dot, ".jad");
            snprintf(app->jad_path, sizeof(app->jad_path) - 1, "%s/%s", app_dir, jad_name);
            app->has_jad = fileio_exists(app->jad_path);
            free(jad_name);
        }

        {
            jad_info info;
            if (jad_load_for_jar(app->jar_path, &info) == 0) {
                if (info.midlet_name[0])
                    strncpy(app->name, info.midlet_name, sizeof(app->name) - 1);
                if (info.midlet_vendor[0])
                    strncpy(app->vendor, info.midlet_vendor, sizeof(app->vendor) - 1);
                if (info.midlet_version[0])
                    strncpy(app->version, info.midlet_version, sizeof(app->version) - 1);
                if (info.midlet_class[0])
                    strncpy(app->class_name, info.midlet_class, sizeof(app->class_name) - 1);
                jad_free(&info);
            }
        }
        if (!app->name[0]) {
            strncpy(app->name, files[i], sizeof(app->name) - 1);
            char *dot = strrchr(app->name, '.');
            if (dot && strcasecmp(dot, ".jar") == 0) *dot = 0;
        }
        if (!app->vendor[0]) strncpy(app->vendor, "Unknown", sizeof(app->vendor) - 1);
        if (!app->version[0]) strncpy(app->version, "1.0", sizeof(app->version) - 1);
        app_count++;
    }

    fileio_free_list(files, file_count);

    for (int i = 0; i < app_count - 1; i++)
        for (int j = i + 1; j < app_count; j++)
            if (strcasecmp(apps[i].name, apps[j].name) > 0) {
                launcher_app t = apps[i]; apps[i] = apps[j]; apps[j] = t;
            }

    return app_count;
}

int launcher_get_count(void) { return app_count; }
const launcher_app *launcher_get_app(int index) {
    if (index < 0 || index >= app_count) return NULL;
    return &apps[index];
}

/* ── Recent tracking ── */
#define RECENT_FILE "ux0:data/java/.recent"

static recent_entry recent_cache[MAX_RECENT];
static int recent_cache_count = -1;

static void recent_cache_invalidate(void) {
    recent_cache_count = -1;
}

static int recent_cache_fill(void) {
    if (recent_cache_count >= 0) return recent_cache_count;

    size_t size;
    uint8_t *data = fileio_read_file(RECENT_FILE, &size);
    if (!data) { recent_cache_count = 0; return 0; }

    int count = 0;
    char *p = (char*)data;
    char *end = p + size;
    while (p < end && count < MAX_RECENT) {
        char *nl = strchr(p, '\n');
        if (!nl) break;
        *nl = 0;
        char *sep = strchr(p, '|');
        if (sep) {
            *sep = 0;
            strncpy(recent_cache[count].path, p, sizeof(recent_cache[count].path) - 1);
            strncpy(recent_cache[count].name, sep + 1, sizeof(recent_cache[count].name) - 1);
            count++;
        }
        p = nl + 1;
    }

    free(data);
    recent_cache_count = count;
    return count;
}

void launcher_add_recent(const char *jar_path, const char *name) {
    recent_cache_invalidate();
    int n = recent_cache_fill();

    int found = -1;
    for (int i = 0; i < n; i++) {
        if (strcmp(recent_cache[i].path, jar_path) == 0) { found = i; break; }
    }

    char tmp[4096] = {0};
    char line[1024];
    snprintf(line, sizeof(line), "%s|%s\n", jar_path, name ? name : "");
    strcat(tmp, line);

    for (int i = 0; i < n && i < MAX_RECENT; i++) {
        if (i == found) continue;
        snprintf(line, sizeof(line), "%s|%s\n", recent_cache[i].path, recent_cache[i].name);
        strcat(tmp, line);
    }

    fileio_write_file(RECENT_FILE, (uint8_t*)tmp, strlen(tmp));
    recent_cache_invalidate();
}

int launcher_get_recent(recent_entry *entries, int max) {
    int n = recent_cache_fill();
    int copy = n < max ? n : max;
    for (int i = 0; i < copy; i++)
        entries[i] = recent_cache[i];
    return copy;
}

/* ── Browse state persistence ── */
void launcher_save_browse_path(const char *path) {
    fileio_write_file(BROWSE_STATE_FILE, (uint8_t*)path, strlen(path));
}

int launcher_load_browse_path(char *path, int max_len) {
    size_t size;
    uint8_t *data = fileio_read_file(BROWSE_STATE_FILE, &size);
    if (!data || size == 0) {
        free(data);
        return -1;
    }
    if (data[size - 1] == '\n') data[size - 1] = 0;
    strncpy(path, (char*)data, max_len - 1);
    free(data);
    return 0;
}

/* ── Sidebar render ── */
static void render_sidebar(void) {
    lcdui_set_color(COL_SURFACE2);
    lcdui_fill_rect(0, 0, SIDEBAR_W, 544);

    draw_gold_text("AYTUM", SIDEBAR_W / 2, 16, ANCHOR_HCENTER | ANCHOR_TOP);
    draw_text("LAUNCHER", SIDEBAR_W / 2, 36, ANCHOR_HCENTER | ANCHOR_TOP, COL_GOLD_DIM);
    draw_separator(10, 54, SIDEBAR_W - 20);

    for (int i = 0; i < SECTION_COUNT; i++) {
        int y = SIDEBAR_TOP_PAD + i * (SIDEBAR_ITEM_H + SIDEBAR_ITEM_GAP);
        int is_sel = (i == sidebar_sel);
        int is_active = (i == current_section && !sidebar_focus);

        if (is_sel && sidebar_focus) {
            lcdui_set_color(COL_GOLD);
            lcdui_fill_rect(0, y, 4, SIDEBAR_ITEM_H);
            lcdui_set_color(COL_SEL_BG);
            lcdui_fill_rect(4, y, SIDEBAR_W - 4, SIDEBAR_ITEM_H);
        } else if (is_active) {
            lcdui_set_color(COL_GOLD_DIM);
            lcdui_fill_rect(0, y, 3, SIDEBAR_ITEM_H);
            lcdui_set_color(COL_GOLD_BG);
            lcdui_fill_rect(4, y, SIDEBAR_W - 4, SIDEBAR_ITEM_H);
        }

        int col = is_sel ? COL_GOLD : (is_active ? COL_GOLD_DIM : COL_MUTED);
        draw_text(section_icons[i], SIDEBAR_ITEM_X, center_text_y(y, SIDEBAR_ITEM_H),
                  ANCHOR_LEFT | ANCHOR_TOP, col);
        draw_text(section_labels[i], SIDEBAR_ITEM_X + 22, center_text_y(y, SIDEBAR_ITEM_H),
                  ANCHOR_LEFT | ANCHOR_TOP, col);
    }
}

/* ── Content renders ── */
static void render_recent(void) {
    recent_entry recents[MAX_RECENT];
    int n = launcher_get_recent(recents, MAX_RECENT);

    draw_text("Recent", CONTENT_X, CONTENT_Y, ANCHOR_LEFT | ANCHOR_TOP, COL_WHITE);
    draw_separator(CONTENT_X, CONTENT_Y + 22, CONTENT_W);

    if (n == 0) {
        draw_text("No recently opened apps", 480, 240, ANCHOR_HCENTER | ANCHOR_TOP, COL_DIM);
    } else {
        for (int i = 0; i < n; i++) {
            int row_y = CONTENT_Y + 36 + i * (LIST_ROW_H + LIST_ROW_GAP);
            int row_h = LIST_ROW_H;
            if (i == recent_sel) {
                lcdui_set_color(COL_SEL_BG);
                lcdui_fill_rect(CONTENT_X, row_y, CONTENT_W, row_h);
                lcdui_set_color(COL_GOLD);
                lcdui_fill_rect(CONTENT_X, row_y, 4, row_h);
            }
            draw_text(recents[i].name[0] ? recents[i].name : recents[i].path,
                      CONTENT_X + 12, center_text_y(row_y, row_h),
                      ANCHOR_LEFT | ANCHOR_TOP,
                      i == recent_sel ? COL_GOLD : COL_TEXT);
            draw_text(recents[i].path,
                      CONTENT_X + 12, center_text_y(row_y, row_h) + 14,
                      ANCHOR_LEFT | ANCHOR_TOP, COL_DIM);
        }
    }
}

static void render_browse(void) {
    draw_text("Browse", CONTENT_X, CONTENT_Y, ANCHOR_LEFT | ANCHOR_TOP, COL_WHITE);
    draw_separator(CONTENT_X, CONTENT_Y + 22, CONTENT_W);

    draw_text(browse_path, CONTENT_X, CONTENT_Y + 30, ANCHOR_LEFT | ANCHOR_TOP, COL_DIM);

    int first_row = CONTENT_Y + 48;
    int vis_end = browse_scroll + 11;
    if (vis_end > browse_item_count) vis_end = browse_item_count;

    for (int i = browse_scroll; i < vis_end; i++) {
        int idx = i - browse_scroll;
        int row_y = first_row + idx * (LIST_ROW_H + LIST_ROW_GAP);
        int row_h = LIST_ROW_H;
        int is_sel = (i == browse_sel);

        if (is_sel) {
            lcdui_set_color(COL_SEL_BG);
            lcdui_fill_rect(CONTENT_X, row_y, CONTENT_W, row_h);
            lcdui_set_color(COL_GOLD);
            lcdui_fill_rect(CONTENT_X, row_y, 4, row_h);
        }

        const char *prefix = browse_items[i].is_dir ? "\xE2\x96\xB6 " : "  ";
        draw_text(prefix, CONTENT_X + 6, center_text_y(row_y, row_h),
                  ANCHOR_LEFT | ANCHOR_TOP, is_sel ? COL_GOLD : COL_TEXT);
        draw_text(browse_items[i].name, CONTENT_X + 26, center_text_y(row_y, row_h),
                  ANCHOR_LEFT | ANCHOR_TOP, is_sel ? COL_GOLD : COL_TEXT);

        if (browse_items[i].is_jar || browse_items[i].is_jad) {
            draw_text("JAR", CONTENT_X + CONTENT_W - 4, center_text_y(row_y, row_h),
                      ANCHOR_RIGHT | ANCHOR_TOP, COL_MUTED);
        } else if (browse_items[i].is_dir) {
            draw_text("DIR", CONTENT_X + CONTENT_W - 4, center_text_y(row_y, row_h),
                      ANCHOR_RIGHT | ANCHOR_TOP, COL_DIM);
        }
    }

    if (browse_item_count == 0) {
        draw_text("(empty)", CONTENT_X, CONTENT_Y + 60, ANCHOR_LEFT | ANCHOR_TOP, COL_MUTED);
    }

    if (browse_item_count > 11) {
        int scroll_h = 400;
        int bar_h = (11 * scroll_h) / browse_item_count;
        int bar_y = first_row + (browse_sel * (scroll_h - bar_h)) / browse_item_count;
        lcdui_set_color(COL_GOLD_DIM);
        lcdui_fill_rect(960 - 8, bar_y, 4, bar_h > 6 ? bar_h : 6);
    }
}

static void render_settings(void) {
    draw_text("Settings", CONTENT_X, CONTENT_Y, ANCHOR_LEFT | ANCHOR_TOP, COL_WHITE);
    draw_separator(CONTENT_X, CONTENT_Y + 22, CONTENT_W);

    const char *items[] = {
        "Internal Resolution",
    };
    int item_count = 1;

    for (int i = 0; i < item_count; i++) {
        int row_y = CONTENT_Y + 36 + i * (LIST_ROW_H + LIST_ROW_GAP);
        int row_h = LIST_ROW_H;
        if (i == settings_sel) {
            lcdui_set_color(COL_SEL_BG);
            lcdui_fill_rect(CONTENT_X, row_y, CONTENT_W, row_h);
            lcdui_set_color(COL_GOLD);
            lcdui_fill_rect(CONTENT_X, row_y, 4, row_h);
        }
        draw_text(items[i], CONTENT_X + 12, center_text_y(row_y, row_h),
                  ANCHOR_LEFT | ANCHOR_TOP, i == settings_sel ? COL_GOLD : COL_TEXT);
        draw_text(res_options[res_current], CONTENT_X + CONTENT_W - 12,
                  center_text_y(row_y, row_h),
                  ANCHOR_RIGHT | ANCHOR_TOP, COL_DIM);
    }

    draw_text("Cross: Cycle resolution", CONTENT_X + 12,
              CONTENT_Y + 36 + item_count * (LIST_ROW_H + LIST_ROW_GAP) + 8,
              ANCHOR_LEFT | ANCHOR_TOP, COL_MUTED);
}

static void render_about(void) {
    draw_text("About", CONTENT_X, CONTENT_Y, ANCHOR_LEFT | ANCHOR_TOP, COL_WHITE);
    draw_separator(CONTENT_X, CONTENT_Y + 22, CONTENT_W);

    draw_gold_text("Aytum Java Launcher", 480, 90, ANCHOR_HCENTER | ANCHOR_TOP);
    draw_text("Java ME Emulator for PS Vita", 480, 122, ANCHOR_HCENTER | ANCHOR_TOP, COL_TEXT);
    draw_separator(340, 150, 280);

    int ly = 176;
    draw_text("Developer", CONTENT_X + 40, ly, ANCHOR_LEFT | ANCHOR_TOP, COL_DIM);
    draw_text("RXV99", 360, ly, ANCHOR_LEFT | ANCHOR_TOP, COL_TEXT);
    ly += 30;

    draw_text("Built with", CONTENT_X + 40, ly, ANCHOR_LEFT | ANCHOR_TOP, COL_DIM);
    draw_text("DeepSeek V4", 360, ly, ANCHOR_LEFT | ANCHOR_TOP, COL_TEXT);
    ly += 30;

    draw_text("Version", CONTENT_X + 40, ly, ANCHOR_LEFT | ANCHOR_TOP, COL_DIM);
    draw_text("1.0.0", 360, ly, ANCHOR_LEFT | ANCHOR_TOP, COL_TEXT);
    ly += 30;

    draw_text("Runtime", CONTENT_X + 40, ly, ANCHOR_LEFT | ANCHOR_TOP, COL_DIM);
    draw_text("Custom JVM + MIDP 2.0", 360, ly, ANCHOR_LEFT | ANCHOR_TOP, COL_TEXT);
    ly += 30;

    draw_text("Graphics", CONTENT_X + 40, ly, ANCHOR_LEFT | ANCHOR_TOP, COL_DIM);
    draw_text("libvita2d | 960x544", 360, ly, ANCHOR_LEFT | ANCHOR_TOP, COL_TEXT);
    ly += 30;

    draw_text("Audio", CONTENT_X + 40, ly, ANCHOR_LEFT | ANCHOR_TOP, COL_DIM);
    draw_text("SceAudio | WAV/MP3", 360, ly, ANCHOR_LEFT | ANCHOR_TOP, COL_TEXT);
    ly += 40;

    draw_text("built with vitasdk", 480, ly, ANCHOR_HCENTER | ANCHOR_TOP, COL_MUTED);
}

/* ── Footer ── */
static void render_footer(void) {
    lcdui_set_color(COL_SURFACE);
    lcdui_fill_rect(0, 510, 960, 34);
    lcdui_set_color(COL_MUTED);

    if (sidebar_focus) {
        char buf[80];
        snprintf(buf, sizeof(buf), "Cross: Open %s   Circle: Refresh   Triangle: Quit",
                 section_labels[sidebar_sel]);
        draw_text(buf, 12, 516, ANCHOR_LEFT | ANCHOR_TOP, COL_MUTED);
    } else {
        switch (current_section) {
            case SECTION_RECENT:
                draw_text("Cross: Launch   Square: Remove   Triangle: Back",
                          12, 516, ANCHOR_LEFT | ANCHOR_TOP, COL_MUTED);
                break;
            case SECTION_BROWSE:
                draw_text("Cross: Open   Triangle: Up / Back",
                          12, 516, ANCHOR_LEFT | ANCHOR_TOP, COL_MUTED);
                break;
            case SECTION_SETTINGS:
                draw_text("Triangle: Back",
                          12, 516, ANCHOR_LEFT | ANCHOR_TOP, COL_MUTED);
                break;
            case SECTION_ABOUT:
                draw_text("Triangle: Back",
                          12, 516, ANCHOR_LEFT | ANCHOR_TOP, COL_MUTED);
                break;
            default:
                break;
        }
    }
    draw_text("v1.0", 948, 516, ANCHOR_RIGHT | ANCHOR_TOP, COL_DIM);
}

/* ── Input handlers ── */

static launcher_result handle_pointer(int px, int py, const char **sel_path, jad_info *sel_info) {
    /* Sidebar hit */
    if (px < SIDEBAR_W) {
        for (int i = 0; i < SECTION_COUNT; i++) {
            int y0 = SIDEBAR_TOP_PAD + i * (SIDEBAR_ITEM_H + SIDEBAR_ITEM_GAP);
            if (py >= y0 && py < y0 + SIDEBAR_ITEM_H) {
                if (i == current_section && !sidebar_focus) {
                    sidebar_focus = 1;
                } else {
                    sidebar_sel = i;
                    current_section = (launcher_section)i;
                    sidebar_focus = 0;
                    if (current_section == SECTION_BROWSE)
                        browse_list_dir(browse_path);
                    if (current_section == SECTION_RECENT)
                        recent_sel = 0;
                    if (current_section == SECTION_SETTINGS)
                        settings_sel = 0;
                }
                return LAUNCHER_RESULT_NONE;
            }
        }
        return LAUNCHER_RESULT_NONE;
    }

    if (sidebar_focus) return LAUNCHER_RESULT_NONE;

    /* Content hit based on section */
    switch (current_section) {
        case SECTION_RECENT: {
            recent_entry recents[MAX_RECENT];
            int n = launcher_get_recent(recents, MAX_RECENT);
            for (int i = 0; i < n; i++) {
                int y0 = 58 + i * 40;
                if (py >= y0 && py < y0 + 36) {
                    recent_sel = i;
                    strncpy(recent_sel_path, recents[recent_sel].path, sizeof(recent_sel_path) - 1);
                    recent_sel_path[sizeof(recent_sel_path) - 1] = 0;
                    *sel_path = recent_sel_path;
                    memset(sel_info, 0, sizeof(jad_info));
                    strncpy(sel_info->midlet_name, recents[recent_sel].name,
                            sizeof(sel_info->midlet_name) - 1);
                    char jad_path[512];
                    snprintf(jad_path, sizeof(jad_path), "%s", *sel_path);
                    char *dot = strrchr(jad_path, '.');
                    if (dot && strcasecmp(dot, ".jar") == 0) {
                        strcpy(dot, ".jad");
                        jad_load(jad_path, sel_info);
                    }
                    return LAUNCHER_RESULT_LAUNCH;
                }
            }
            break;
        }
        case SECTION_BROWSE: {
            if (!browse_items) break;
            for (int i = browse_scroll; i < browse_item_count && i < browse_scroll + 11; i++) {
                int idx = i - browse_scroll;
                int y0 = 68 + idx * 38;
                if (py >= y0 && py < y0 + 34) {
                    browse_sel = i;
                    if (browse_items[browse_sel].is_dir) {
                        browse_enter_dir(browse_items[browse_sel].full_path);
                    } else if (browse_items[browse_sel].is_jar) {
                        *sel_path = browse_items[browse_sel].full_path;
                        memset(sel_info, 0, sizeof(jad_info));
                        char jad_path[512];
                        snprintf(jad_path, sizeof(jad_path), "%s", *sel_path);
                        char *dot = strrchr(jad_path, '.');
                        if (dot && strcasecmp(dot, ".jar") == 0) {
                            strcpy(dot, ".jad");
                            if (fileio_exists(jad_path))
                                jad_load(jad_path, sel_info);
                        }
                        if (!sel_info->midlet_name[0]) {
                            strncpy(sel_info->midlet_name, browse_items[browse_sel].name,
                                    sizeof(sel_info->midlet_name) - 1);
                            char *d = strrchr(sel_info->midlet_name, '.');
                            if (d && strcasecmp(d, ".jar") == 0) *d = 0;
                        }
                        launcher_add_recent(*sel_path, sel_info->midlet_name);
                        return LAUNCHER_RESULT_LAUNCH;
                    }
                    return LAUNCHER_RESULT_NONE;
                }
            }
            break;
        }
        case SECTION_SETTINGS: {
            for (int i = 0; i < settings_item_count; i++) {
                int y0 = 68 + i * 44;
                if (py >= y0 && py < y0 + 38) {
                    settings_sel = i;
                    res_current = (res_current + 1) % res_count;
                    return LAUNCHER_RESULT_NONE;
                }
            }
            break;
        }
        default:
            break;
    }
    return LAUNCHER_RESULT_NONE;
}

static launcher_result handle_sidebar_input(int key, const char **sel_path, jad_info *sel_info) {
    (void)sel_path;
    (void)sel_info;
    switch (key) {
        case KEY_UP:
            sidebar_sel = (sidebar_sel - 1 + SECTION_COUNT) % SECTION_COUNT;
            break;
        case KEY_DOWN:
            sidebar_sel = (sidebar_sel + 1) % SECTION_COUNT;
            break;
        case KEY_FIRE:
            current_section = (launcher_section)sidebar_sel;
            sidebar_focus = 0;
            if (current_section == SECTION_BROWSE)
                browse_list_dir(browse_path);
            if (current_section == SECTION_RECENT)
                recent_sel = 0;
            if (current_section == SECTION_SETTINGS)
                settings_sel = 0;
            break;
        case KEY_RIGHT:
            current_section = (launcher_section)sidebar_sel;
            sidebar_focus = 0;
            if (current_section == SECTION_BROWSE)
                browse_list_dir(browse_path);
            if (current_section == SECTION_RECENT)
                recent_sel = 0;
            if (current_section == SECTION_SETTINGS)
                settings_sel = 0;
            break;
        case KEY_SOFT2:
            return LAUNCHER_RESULT_REFRESH;
        case KEY_SOFT3:
            return LAUNCHER_RESULT_QUIT;
    }
    return LAUNCHER_RESULT_NONE;
}

static launcher_result handle_recent_input(int key, const char **sel_path, jad_info *sel_info) {
    recent_entry recents[MAX_RECENT];
    int n = launcher_get_recent(recents, MAX_RECENT);

    switch (key) {
        case KEY_UP:
            if (recent_sel > 0) recent_sel--;
            break;
        case KEY_DOWN:
            if (recent_sel < n - 1) recent_sel++;
            break;
        case KEY_FIRE:
            if (recent_sel >= 0 && recent_sel < n) {
                strncpy(recent_sel_path, recents[recent_sel].path, sizeof(recent_sel_path) - 1);
                recent_sel_path[sizeof(recent_sel_path) - 1] = 0;
                *sel_path = recent_sel_path;
                jad_load_for_jar(*sel_path, sel_info);
                return LAUNCHER_RESULT_LAUNCH;
            }
            break;
        case KEY_SOFT1:
            if (n > 0 && recent_sel >= 0 && recent_sel < n) {
                char tmp[4096] = {0};
                for (int i = 0; i < n; i++) {
                    if (i == recent_sel) continue;
                    char line[1024];
                    snprintf(line, sizeof(line), "%s|%s\n", recents[i].path, recents[i].name);
                    strcat(tmp, line);
                }
                fileio_write_file(RECENT_FILE, (uint8_t*)tmp, strlen(tmp));
                if (recent_sel >= n - 1) recent_sel--;
                if (recent_sel < 0) recent_sel = 0;
            }
            break;
        case KEY_LEFT:
        case KEY_SOFT3:
            sidebar_focus = 1;
            break;
    }
    return LAUNCHER_RESULT_NONE;
}

static launcher_result handle_browse_input(int key, const char **sel_path, jad_info *sel_info) {
    switch (key) {
        case KEY_UP:
            if (browse_sel > 0) browse_sel--;
            if (browse_sel < browse_scroll) browse_scroll--;
            break;
        case KEY_DOWN:
            if (browse_sel < browse_item_count - 1) browse_sel++;
            if (browse_sel >= browse_scroll + 11) browse_scroll++;
            break;
        case KEY_FIRE:
            if (browse_sel >= 0 && browse_sel < browse_item_count) {
                if (browse_items[browse_sel].is_dir) {
                    browse_enter_dir(browse_items[browse_sel].full_path);
                } else if (browse_items[browse_sel].is_jar) {
                    *sel_path = browse_items[browse_sel].full_path;
                    jad_load_for_jar(*sel_path, sel_info);
                    if (!sel_info->midlet_name[0]) {
                        strncpy(sel_info->midlet_name,
                                browse_items[browse_sel].name,
                                sizeof(sel_info->midlet_name) - 1);
                        char *d = strrchr(sel_info->midlet_name, '.');
                        if (d && strcasecmp(d, ".jar") == 0) *d = 0;
                    }
                    launcher_add_recent(*sel_path, sel_info->midlet_name);
                    return LAUNCHER_RESULT_LAUNCH;
                }
            }
            break;
        case KEY_LEFT:
        case KEY_SOFT3:
            if (strcmp(browse_path, "ux0:") != 0)
                browse_go_up();
            else
                sidebar_focus = 1;
            break;
    }
    return LAUNCHER_RESULT_NONE;
}

static launcher_result handle_settings_input(int key) {
    switch (key) {
        case KEY_UP:
            if (settings_sel > 0) settings_sel--;
            break;
        case KEY_DOWN:
            if (settings_sel < settings_item_count - 1) settings_sel++;
            break;
        case KEY_FIRE:
            res_current = (res_current + 1) % res_count;
            break;
        case KEY_LEFT:
        case KEY_SOFT3:
            sidebar_focus = 1;
            break;
    }
    return LAUNCHER_RESULT_NONE;
}

static launcher_result handle_about_input(int key) {
    if (key == KEY_LEFT || key == KEY_SOFT3)
        sidebar_focus = 1;
    return LAUNCHER_RESULT_NONE;
}

/* ── Main launcher entry point ── */
launcher_result launcher_run(const char **selected_path, jad_info *selected_info) {
    *selected_path = NULL;

    if (app_count == 0) {
        while (1) {
            input_process();
            input_event evs[16];
            int ec = input_poll(evs, 16);
            for (int i = 0; i < ec; i++) {
                if (evs[i].type == INPUT_EVENT_KEY_PRESSED && evs[i].key_code == KEY_SOFT3)
                    return LAUNCHER_RESULT_QUIT;
            }
            lcdui_clear(COL_BG);
            lcdui_begin_frame();
            draw_text("No Java ME apps found in ux0:data/java/", 480, 260,
                      ANCHOR_HCENTER | ANCHOR_TOP, COL_DIM);
            draw_text("Place .jar files and relaunch", 480, 290,
                      ANCHOR_HCENTER | ANCHOR_TOP, COL_MUTED);
            draw_text("Triangle: Quit", 480, 340,
                      ANCHOR_HCENTER | ANCHOR_TOP, COL_DIM);
            lcdui_end_frame();
            sceKernelDelayThread(16 * 1000);
        }
    }

    while (1) {
        input_process();

        input_event events[16];
        int ev_count = input_poll(events, 16);

        for (int i = 0; i < ev_count; i++) {
            if (events[i].type == INPUT_EVENT_KEY_PRESSED) {
                launcher_result r = LAUNCHER_RESULT_NONE;

                if (sidebar_focus) {
                    r = handle_sidebar_input(events[i].key_code, selected_path, selected_info);
                } else {
                    switch (current_section) {
                        case SECTION_RECENT:
                            r = handle_recent_input(events[i].key_code, selected_path, selected_info);
                            break;
                        case SECTION_BROWSE:
                            r = handle_browse_input(events[i].key_code, selected_path, selected_info);
                            break;
                        case SECTION_SETTINGS:
                            r = handle_settings_input(events[i].key_code);
                            break;
                        case SECTION_ABOUT:
                            r = handle_about_input(events[i].key_code);
                            break;
                        default:
                            break;
                    }
                }

                if (r != LAUNCHER_RESULT_NONE)
                    return r;
            }
        }

        lcdui_clear(COL_BG);
        lcdui_begin_frame();
        lcdui_set_translate(0, 0);

        render_sidebar();

        switch (current_section) {
            case SECTION_RECENT:   render_recent(); break;
            case SECTION_BROWSE:   render_browse(); break;
            case SECTION_SETTINGS: render_settings(); break;
            case SECTION_ABOUT:    render_about(); break;
            default: break;
        }

        render_footer();
        lcdui_end_frame();
        sceKernelDelayThread(16 * 1000);
    }
}
