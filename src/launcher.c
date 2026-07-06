#include "launcher.h"
#include "fileio.h"
#include "graphics.h"
#include "input.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <psp2/kernel/threadmgr.h>
#include <time.h>

/* ── Aytum Launcher colour palette ── */
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
#define COL_SEL_BG    0xC8A02718  /* gold at ~10% */
#define COL_FAV       0xF0C040

static launcher_app apps[MAX_APPS];
static int app_count = 0;

/* ── Main menu state ── */
static launcher_section current_section = SECTION_MAIN;
static int main_sel = 0;       /* index in main menu */
static int browse_sel = 0;
static int recent_sel = 0;
static int fav_sel = 0;
static int settings_sel = 0;
static int scroll_offset = 0;

/* ── Menu labels ── */
static const char *main_labels[MENU_COUNT] = {
    "Recent",
    "Browse",
    "Favorites",
    "Settings",
    "Refresh",
    "About",
};
static const char *main_icons[MENU_COUNT] = {
    "\xC2\xBB", /* » */
    "\xE2\x97\x8B", /* ○ */
    "\xE2\x98\x85", /* ★ */
    "\xE2\x9A\x99", /* ⚙ */
    "\xE2\x86\xBB", /* ↻ */
    "\xE2\x93\xA1", /* ⓡ */
};

/* ── Recent tracking ── */
#define RECENT_FILE "ux0:data/java/.recent"
#define FAV_FILE    "ux0:data/java/.favorites"

/* ── Internal helpers ── */
static int strcasecmp(const char *a, const char *b) {
    while (*a && *b) {
        int ca = toupper((unsigned char)*a);
        int cb = toupper((unsigned char)*b);
        if (ca != cb) return ca - cb;
        a++; b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

/* ── Launcher init / scan ── */
int launcher_init(void) {
    app_count = 0;
    current_section = SECTION_MAIN;
    main_sel = 0;
    browse_sel = 0;
    scroll_offset = 0;
    return launcher_scan();
}

void launcher_shutdown(void) {
    app_count = 0;
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

        /* Look for .jad */
        char *jad_name = (char*)malloc(strlen(files[i]) + 4);
        if (jad_name) {
            strcpy(jad_name, files[i]);
            char *dot = strrchr(jad_name, '.');
            if (dot && strcasecmp(dot, ".jar") == 0) strcpy(dot, ".jad");
            snprintf(app->jad_path, sizeof(app->jad_path) - 1, "%s/%s", app_dir, jad_name);
            app->has_jad = fileio_exists(app->jad_path);
            free(jad_name);
        }

        if (app->has_jad) {
            jad_info info;
            if (jad_load(app->jad_path, &info) == 0) {
                strncpy(app->name, info.midlet_name, sizeof(app->name) - 1);
                strncpy(app->vendor, info.midlet_vendor, sizeof(app->vendor) - 1);
                strncpy(app->version, info.midlet_version, sizeof(app->version) - 1);
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
        app->is_favorite = launcher_is_favorite(app->jar_path);
        app_count++;
    }

    fileio_free_list(files, file_count);

    /* Sort by name */
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
void launcher_add_recent(const char *jar_path, const char *name) {
    /* Read existing recents */
    recent_entry recents[MAX_RECENT];
    int n = launcher_get_recent(recents, MAX_RECENT);

    /* Check if already in list; if so, move to top */
    int found = -1;
    for (int i = 0; i < n; i++) {
        if (strcmp(recents[i].path, jar_path) == 0) { found = i; break; }
    }

    char tmp[1024 * 4] = {0};
    /* Write new one at top */
    char line[1024];
    snprintf(line, sizeof(line), "%s|%s\n", jar_path, name ? name : "");
    strcat(tmp, line);

    /* Write rest (skip found entry) */
    for (int i = 0; i < n && i < MAX_RECENT; i++) {
        if (i == found) continue;
        snprintf(line, sizeof(line), "%s|%s\n", recents[i].path, recents[i].name);
        strcat(tmp, line);
    }

    fileio_write_file(RECENT_FILE, (uint8_t*)tmp, strlen(tmp));
}

int launcher_get_recent(recent_entry *entries, int max) {
    size_t size;
    uint8_t *data = fileio_read_file(RECENT_FILE, &size);
    if (!data) return 0;

    int count = 0;
    char *p = (char*)data;
    char *end = p + size;
    while (p < end && count < max) {
        char *nl = strchr(p, '\n');
        if (!nl) break;
        *nl = 0;
        char *sep = strchr(p, '|');
        if (sep) {
            *sep = 0;
            strncpy(entries[count].path, p, sizeof(entries[count].path) - 1);
            strncpy(entries[count].name, sep + 1, sizeof(entries[count].name) - 1);
            count++;
        }
        p = nl + 1;
    }

    free(data);
    return count;
}

/* ── Favorites ── */
void launcher_toggle_favorite(const char *jar_path) {
    if (!jar_path) return;

    /* Read existing */
    size_t size;
    uint8_t *data = fileio_read_file(FAV_FILE, &size);
    char *old = (char*)data;

    /* Check if already present */
    if (old) {
        char *p = old;
        char *end = old + size;
        while (p < end) {
            char *nl = strchr(p, '\n');
            if (nl) *nl = 0;
            if (strcmp(p, jar_path) == 0) {
                /* Remove it */
                size_t remaining = end - (nl ? nl + 1 : end);
                char *new_data = (char*)malloc(remaining + 1);
                if (new_data) {
                    memcpy(new_data, nl ? nl + 1 : end, remaining);
                    new_data[remaining] = 0;
                    fileio_write_file(FAV_FILE, (uint8_t*)new_data, remaining);
                    free(new_data);
                }
                free(data);
                /* Update the app entry if found */
                for (int i = 0; i < app_count; i++)
                    if (strcmp(apps[i].jar_path, jar_path) == 0)
                        apps[i].is_favorite = 0;
                return;
            }
            p = nl ? nl + 1 : end;
        }
    }

    /* Not found, add it */
    char *new_data;
    if (old) {
        new_data = (char*)malloc(size + strlen(jar_path) + 2);
        if (new_data) {
            memcpy(new_data, old, size);
            new_data[size] = 0;
            strcat(new_data, jar_path);
            strcat(new_data, "\n");
            fileio_write_file(FAV_FILE, (uint8_t*)new_data, strlen(new_data));
            free(new_data);
        }
    } else {
        char buf[1024];
        snprintf(buf, sizeof(buf), "%s\n", jar_path);
        fileio_write_file(FAV_FILE, (uint8_t*)buf, strlen(buf));
    }

    free(data);
    for (int i = 0; i < app_count; i++)
        if (strcmp(apps[i].jar_path, jar_path) == 0)
            apps[i].is_favorite = 1;
}

int launcher_is_favorite(const char *jar_path) {
    if (!jar_path) return 0;
    size_t size;
    uint8_t *data = fileio_read_file(FAV_FILE, &size);
    if (!data) return 0;
    int found = 0;
    char *p = (char*)data;
    char *end = p + size;
    while (p < end) {
        char *nl = strchr(p, '\n');
        if (nl) *nl = 0;
        if (strcmp(p, jar_path) == 0) { found = 1; break; }
        p = nl ? nl + 1 : end;
    }
    free(data);
    return found;
}

int launcher_get_favorites(launcher_app **favs, int *count) {
    *favs = NULL;
    *count = 0;

    size_t size;
    uint8_t *data = fileio_read_file(FAV_FILE, &size);
    if (!data) return 0;

    /* Count favorites */
    int total = 0;
    char *p = (char*)data;
    char *end = p + size;
    while (p < end) {
        char *nl = strchr(p, '\n');
        if (nl) *nl = 0;
        /* Find matching app */
        for (int i = 0; i < app_count; i++) {
            if (strcmp(apps[i].jar_path, p) == 0) {
                total++;
                break;
            }
        }
        p = nl ? nl + 1 : end;
    }

    if (total == 0) { free(data); return 0; }

    *favs = (launcher_app*)calloc(total, sizeof(launcher_app));
    if (!*favs) { free(data); return 0; }

    int idx = 0;
    p = (char*)data;
    end = (char*)data + size;
    while (p < end && idx < total) {
        char *nl = strchr(p, '\n');
        if (nl) *nl = 0;
        for (int i = 0; i < app_count && idx < total; i++) {
            if (strcmp(apps[i].jar_path, p) == 0) {
                (*favs)[idx++] = apps[i];
                break;
            }
        }
        p = nl ? nl + 1 : end;
    }

    free(data);
    *count = total;
    return total;
}

/* ── Rendering helpers ── */
static void draw_gold_text(const char *text, int x, int y, int anchor, int size) {
    lcdui_set_color(COL_GOLD);
    lcdui_draw_string(text, x, y, anchor);
}

static void draw_text(const char *text, int x, int y, int anchor, int color, int size) {
    lcdui_set_color(color);
    lcdui_draw_string(text, x, y, anchor);
}

static void draw_separator(int y) {
    lcdui_set_color(COL_SEP);
    lcdui_draw_line(32, y, 928, y);
}

static void draw_menu_item(const char *label, const char *icon, int x, int y,
                           int selected, int badge, const char *sub) {
    if (selected) {
        /* Gold accent bar */
        lcdui_set_color(COL_GOLD);
        lcdui_fill_rect(x - 4, y, 4, 36);
        /* Subtle background */
        lcdui_set_color(COL_SEL_BG);
        lcdui_fill_rect(x, y, 928 - x, 36);
    }

    /* Icon */
    draw_gold_text(icon, x + 8, y + 8, ANCHOR_LEFT | ANCHOR_TOP, 18);

    /* Label */
    draw_text(label, x + 40, y + 6, ANCHOR_LEFT | ANCHOR_TOP,
              selected ? COL_GOLD : COL_TEXT, 16);

    /* Badge */
    if (badge > 0) {
        char b[8];
        snprintf(b, sizeof(b), "%d", badge);
        lcdui_set_color(COL_GOLD);
        lcdui_fill_rect(910, y + 8, 24, 20);
        lcdui_set_color(COL_BG);
        lcdui_draw_string(b, 922, y + 10, ANCHOR_HCENTER | ANCHOR_TOP);
    }

    /* Subtitle */
    if (sub) {
        draw_text(sub, x + 40, y + 22, ANCHOR_LEFT | ANCHOR_TOP, COL_DIM, 12);
    }
}

static void draw_logo(void) {
    /* AYTUM LAUNCHER — centered, gold, prominent */
    draw_gold_text("AYTUM", 480, 24, ANCHOR_HCENTER | ANCHOR_TOP, 32);
    draw_text("LAUNCHER", 480, 58, ANCHOR_HCENTER | ANCHOR_TOP, COL_GOLD_DIM, 14);

    /* Gold underline */
    lcdui_set_color(COL_GOLD_DIM);
    lcdui_draw_line(360, 80, 600, 80);
}

static void draw_footer(const char *text) {
    lcdui_set_color(COL_SURFACE);
    lcdui_fill_rect(0, 510, 960, 34);
    lcdui_set_color(COL_MUTED);
    lcdui_draw_string(text ? text : "Cross: Select   Triangle: Back   Circle: Refresh",
                      16, 516, ANCHOR_LEFT | ANCHOR_TOP);
    lcdui_set_color(COL_DIM);
    lcdui_draw_string("v1.0", 940, 516, ANCHOR_RIGHT | ANCHOR_TOP);
}

/* ── Screen renders ── */
static void render_main(void) {
    int recent_count = launcher_get_recent(NULL, 0);
    int fav_count = 0;
    launcher_app *favs = NULL;
    launcher_get_favorites(&favs, &fav_count);
    free(favs);

    draw_logo();

    int start_y = 104;
    for (int i = 0; i < MENU_COUNT; i++) {
        int y = start_y + i * 52;
        int badge = 0;
        const char *sub = NULL;
        char sub_buf[64];

        if (i == MENU_RECENT && recent_count > 0) {
            badge = recent_count;
            snprintf(sub_buf, sizeof(sub_buf), "%d recently opened", recent_count);
            sub = sub_buf;
        }
        if (i == MENU_FAVORITES && fav_count > 0) {
            badge = fav_count;
            snprintf(sub_buf, sizeof(sub_buf), "%d favorites", fav_count);
            sub = sub_buf;
        }
        if (i == MENU_BROWSE) {
            snprintf(sub_buf, sizeof(sub_buf), "%d apps available", app_count);
            sub = sub_buf;
        }
        if (i == MENU_REFRESH) {
            sub = "Rescan ux0:data/java/";
        }
        if (i == MENU_ABOUT) {
            sub = "v1.0  |  Java ME Emulator";
        }
        if (i == MENU_SETTINGS) {
            sub = "Volume, display, performance";
        }

        draw_menu_item(main_labels[i], main_icons[i], 40, y,
                       i == main_sel, badge, sub);
        draw_separator(y + 44);
    }

    char info[64];
    snprintf(info, sizeof(info), "ux0:data/java/  -  %d app(s)", app_count);
    draw_text(info, 480, 492, ANCHOR_HCENTER | ANCHOR_TOP, COL_MUTED, 12);

    draw_footer("Cross: Select   Triangle: Quit   Circle: Refresh");
}

static void render_recent(void) {
    draw_text("RECENT", 480, 20, ANCHOR_HCENTER | ANCHOR_TOP, COL_GOLD, 22);
    draw_separator(44);

    recent_entry recents[MAX_RECENT];
    int n = launcher_get_recent(recents, MAX_RECENT);

    if (n == 0) {
        draw_text("No recently opened apps", 480, 200, ANCHOR_HCENTER | ANCHOR_TOP,
                  COL_DIM, 16);
    } else {
        for (int i = 0; i < n && i < 12; i++) {
            int y = 60 + i * 38;
            if (i == recent_sel) {
                lcdui_set_color(COL_SEL_BG);
                lcdui_fill_rect(32, y, 896, 34);
                lcdui_set_color(COL_GOLD);
                lcdui_fill_rect(28, y, 4, 34);
            }
            draw_text(recents[i].name[0] ? recents[i].name : recents[i].path,
                      48, y + 6, ANCHOR_LEFT | ANCHOR_TOP,
                      i == recent_sel ? COL_GOLD : COL_TEXT, 16);
            draw_text(recents[i].path, 48, y + 22, ANCHOR_LEFT | ANCHOR_TOP,
                      COL_DIM, 12);
        }
    }

    draw_footer(n > 0 ? "Cross: Launch   Square: Remove   Triangle: Back"
                       : "Triangle: Back");
}

static void render_browse(void) {
    draw_text("BROWSE", 480, 20, ANCHOR_HCENTER | ANCHOR_TOP, COL_GOLD, 22);
    draw_separator(44);

    /* Sub-header showing path */
    draw_text(fileio_get_base_path(), 480, 48, ANCHOR_HCENTER | ANCHOR_TOP, COL_DIM, 12);

    int vis_start = scroll_offset;
    int vis_end = vis_start + 12;
    if (vis_end > app_count) vis_end = app_count;

    for (int i = vis_start; i < vis_end; i++) {
        int idx = i - vis_start;
        int y = 68 + idx * 38;
        int is_sel = (i == browse_sel);

        if (is_sel) {
            lcdui_set_color(COL_SEL_BG);
            lcdui_fill_rect(32, y, 896, 34);
            lcdui_set_color(COL_GOLD);
            lcdui_fill_rect(28, y, 4, 34);
        }

        const launcher_app *app = &apps[i];
        draw_text(app->name, 48, y + 4, ANCHOR_LEFT | ANCHOR_TOP,
                  is_sel ? COL_GOLD : COL_TEXT, 16);
        char meta[128];
        snprintf(meta, sizeof(meta), "%s  v%s", app->vendor, app->version);
        draw_text(meta, 48, y + 22, ANCHOR_LEFT | ANCHOR_TOP, COL_DIM, 12);

        if (app->is_favorite) {
            draw_text("\xE2\x98\x85", 910, y + 4, ANCHOR_LEFT | ANCHOR_TOP, COL_FAV, 16);
        }
    }

    /* Scrollbar */
    if (app_count > 12) {
        int bar_h = (12 * 510) / app_count;
        int bar_y = 68 + (browse_sel * (510 - bar_h)) / app_count;
        lcdui_set_color(COL_GOLD_DIM);
        lcdui_fill_rect(948, bar_y, 4, bar_h > 6 ? bar_h : 6);
    }

    draw_footer("Cross: Launch   Square: Favorite   Triangle: Back");
}

static void render_favorites(void) {
    draw_text("FAVORITES", 480, 20, ANCHOR_HCENTER | ANCHOR_TOP, COL_GOLD, 22);
    draw_separator(44);

    launcher_app *favs = NULL;
    int fav_count = 0;
    launcher_get_favorites(&favs, &fav_count);

    if (fav_count == 0) {
        draw_text("No favorites yet", 480, 200, ANCHOR_HCENTER | ANCHOR_TOP,
                  COL_DIM, 16);
        draw_text("Press Square while browsing to add", 480, 230,
                  ANCHOR_HCENTER | ANCHOR_TOP, COL_MUTED, 12);
    } else {
        for (int i = 0; i < fav_count && i < 12; i++) {
            int y = 60 + i * 38;
            if (i == fav_sel) {
                lcdui_set_color(COL_SEL_BG);
                lcdui_fill_rect(32, y, 896, 34);
                lcdui_set_color(COL_GOLD);
                lcdui_fill_rect(28, y, 4, 34);
            }
            draw_text(favs[i].name, 48, y + 6, ANCHOR_LEFT | ANCHOR_TOP,
                      i == fav_sel ? COL_GOLD : COL_TEXT, 16);
            draw_text(favs[i].jar_path, 48, y + 22, ANCHOR_LEFT | ANCHOR_TOP,
                      COL_DIM, 12);
            draw_text("\xE2\x98\x85", 910, y + 4, ANCHOR_LEFT | ANCHOR_TOP, COL_FAV, 16);
        }
    }

    free(favs);
    draw_footer(fav_count > 0 ? "Cross: Launch   Square: Remove   Triangle: Back"
                              : "Triangle: Back");
}

static void render_settings(void) {
    draw_text("SETTINGS", 480, 20, ANCHOR_HCENTER | ANCHOR_TOP, COL_GOLD, 22);
    draw_separator(44);

    /* Settings items */
    const char *items[] = {
        "Audio Volume",
        "Screen Brightness",
    };
    const char *values[] = {
        "80%",
        "100%",
    };
    int item_count = 2;

    for (int i = 0; i < item_count; i++) {
        int y = 68 + i * 44;
        if (i == settings_sel) {
            lcdui_set_color(COL_SEL_BG);
            lcdui_fill_rect(32, y, 896, 38);
            lcdui_set_color(COL_GOLD);
            lcdui_fill_rect(28, y, 4, 38);
        }
        draw_text(items[i], 48, y + 8, ANCHOR_LEFT | ANCHOR_TOP,
                  i == settings_sel ? COL_GOLD : COL_TEXT, 16);
        draw_text(values[i], 900, y + 8, ANCHOR_RIGHT | ANCHOR_TOP, COL_DIM, 16);
    }

    draw_separator(68 + item_count * 44 + 4);
    draw_text("More settings coming soon", 480, 68 + item_count * 44 + 16,
              ANCHOR_HCENTER | ANCHOR_TOP, COL_MUTED, 12);

    draw_footer("Triangle: Back");
}

static void render_about(void) {
    draw_text("ABOUT", 480, 20, ANCHOR_HCENTER | ANCHOR_TOP, COL_GOLD, 22);
    draw_separator(44);

    draw_gold_text("AYTUM LAUNCHER", 480, 80, ANCHOR_HCENTER | ANCHOR_TOP, 28);
    draw_text("Java ME Emulator for PS Vita", 480, 116, ANCHOR_HCENTER | ANCHOR_TOP,
              COL_TEXT, 16);

    draw_separator(148);

    int ly = 168;
    draw_text("Version", 100, ly, ANCHOR_LEFT | ANCHOR_TOP, COL_DIM, 14);
    draw_text("1.0.0", 400, ly, ANCHOR_LEFT | ANCHOR_TOP, COL_TEXT, 14);
    ly += 28;

    draw_text("Runtime", 100, ly, ANCHOR_LEFT | ANCHOR_TOP, COL_DIM, 14);
    draw_text("Custom JVM + MIDP 2.0", 400, ly, ANCHOR_LEFT | ANCHOR_TOP, COL_TEXT, 14);
    ly += 28;

    draw_text("Graphics", 100, ly, ANCHOR_LEFT | ANCHOR_TOP, COL_DIM, 14);
    draw_text("libvita2d  |  960x544", 400, ly, ANCHOR_LEFT | ANCHOR_TOP, COL_TEXT, 14);
    ly += 28;

    draw_text("Audio", 100, ly, ANCHOR_LEFT | ANCHOR_TOP, COL_DIM, 14);
    draw_text("SceAudio  |  WAV/MP3 decoder", 400, ly, ANCHOR_LEFT | ANCHOR_TOP, COL_TEXT, 14);
    ly += 28;

    draw_text("Storage", 100, ly, ANCHOR_LEFT | ANCHOR_TOP, COL_DIM, 14);
    draw_text("ux0:data/java/", 400, ly, ANCHOR_LEFT | ANCHOR_TOP, COL_TEXT, 14);
    ly += 28;

    draw_text("Category", 100, ly, ANCHOR_LEFT | ANCHOR_TOP, COL_DIM, 14);
    draw_text("Game | Homebrew", 400, ly, ANCHOR_LEFT | ANCHOR_TOP, COL_TEXT, 14);
    ly += 40;

    draw_text("built with vitasdk", 480, ly + 10, ANCHOR_HCENTER | ANCHOR_TOP, COL_MUTED, 12);

    draw_footer("Triangle: Back");
}

/* ── Input handling per section ── */
static launcher_result handle_main_input(int key) {
    switch (key) {
        case KEY_UP:
            main_sel = (main_sel - 1 + MENU_COUNT) % MENU_COUNT;
            break;
        case KEY_DOWN:
            main_sel = (main_sel + 1) % MENU_COUNT;
            break;
        case KEY_FIRE:
            switch (main_sel) {
                case MENU_RECENT:    current_section = SECTION_RECENT; recent_sel = 0; break;
                case MENU_BROWSE:    current_section = SECTION_BROWSE; browse_sel = 0; scroll_offset = 0; break;
                case MENU_FAVORITES: current_section = SECTION_FAV; fav_sel = 0; break;
                case MENU_SETTINGS:  current_section = SECTION_SETTINGS; settings_sel = 0; break;
                case MENU_REFRESH:   return LAUNCHER_RESULT_REFRESH;
                case MENU_ABOUT:     current_section = SECTION_ABOUT; break;
            }
            break;
        case KEY_SOFT2:
            return LAUNCHER_RESULT_REFRESH;
        case KEY_SOFT3:
            return LAUNCHER_RESULT_QUIT;
    }
    return LAUNCHER_RESULT_NONE;
}

static launcher_result handle_browse_input(int key, const char **sel_path, jad_info *sel_info) {
    switch (key) {
        case KEY_UP:
            if (browse_sel > 0) browse_sel--;
            if (browse_sel < scroll_offset) scroll_offset--;
            break;
        case KEY_DOWN:
            if (browse_sel < app_count - 1) browse_sel++;
            if (browse_sel >= scroll_offset + 12) scroll_offset++;
            break;
        case KEY_FIRE:
            if (browse_sel >= 0 && browse_sel < app_count) {
                *sel_path = apps[browse_sel].jar_path;
                if (apps[browse_sel].has_jad)
                    jad_load(apps[browse_sel].jad_path, sel_info);
                else
                    memset(sel_info, 0, sizeof(jad_info));
                launcher_add_recent(*sel_path, apps[browse_sel].name);
                return LAUNCHER_RESULT_LAUNCH;
            }
            break;
        case KEY_SOFT1: /* Square = toggle favorite */
            if (browse_sel >= 0 && browse_sel < app_count) {
                launcher_toggle_favorite(apps[browse_sel].jar_path);
            }
            break;
        case KEY_SOFT2: /* Circle = refresh */
            return LAUNCHER_RESULT_REFRESH;
        case KEY_SOFT3: /* Triangle = back */
            current_section = SECTION_MAIN;
            break;
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
                *sel_path = recents[recent_sel].path;
                memset(sel_info, 0, sizeof(jad_info));
                strncpy(sel_info->midlet_name, recents[recent_sel].name,
                        sizeof(sel_info->midlet_name) - 1);
                /* Try to load .jad for the recent app */
                char jad_path[512];
                snprintf(jad_path, sizeof(jad_path), "%s", *sel_path);
                char *dot = strrchr(jad_path, '.');
                if (dot && strcasecmp(dot, ".jar") == 0) {
                    strcpy(dot, ".jad");
                    jad_load(jad_path, sel_info);
                }
                return LAUNCHER_RESULT_LAUNCH;
            }
            break;
        case KEY_SOFT1: /* Square = remove from recent */
            if (recent_sel >= 0 && recent_sel < n) {
                /* Rewrite without this entry */
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
        case KEY_SOFT3:
            current_section = SECTION_MAIN;
            break;
    }
    return LAUNCHER_RESULT_NONE;
}

static launcher_result handle_fav_input(int key, const char **sel_path, jad_info *sel_info) {
    launcher_app *favs = NULL;
    int fav_count = 0;
    launcher_get_favorites(&favs, &fav_count);

    switch (key) {
        case KEY_UP:
            if (fav_sel > 0) fav_sel--;
            break;
        case KEY_DOWN:
            if (fav_sel < fav_count - 1) fav_sel++;
            break;
        case KEY_FIRE:
            if (fav_sel >= 0 && fav_sel < fav_count) {
                *sel_path = favs[fav_sel].jar_path;
                if (favs[fav_sel].has_jad)
                    jad_load(favs[fav_sel].jad_path, sel_info);
                else
                    memset(sel_info, 0, sizeof(jad_info));
                free(favs);
                return LAUNCHER_RESULT_LAUNCH;
            }
            break;
        case KEY_SOFT1:
            if (fav_sel >= 0 && fav_sel < fav_count) {
                launcher_toggle_favorite(favs[fav_sel].jar_path);
                if (fav_sel >= fav_count - 1) fav_sel--;
                if (fav_sel < 0) fav_sel = 0;
            }
            break;
        case KEY_SOFT3:
            current_section = SECTION_MAIN;
            break;
    }
    free(favs);
    return LAUNCHER_RESULT_NONE;
}

static launcher_result handle_settings_input(int key) {
    switch (key) {
        case KEY_UP:
            if (settings_sel > 0) settings_sel--;
            break;
        case KEY_DOWN:
            if (settings_sel < 1) settings_sel++;
            break;
        case KEY_FIRE:
            if (settings_sel == 0) {
                /* Toggle volume (simple) */
                int vol = audio_get_volume();
                vol = (vol >= 100) ? 0 : (vol + 10);
                audio_set_volume(vol);
            }
            break;
        case KEY_SOFT3:
            current_section = SECTION_MAIN;
            break;
    }
    return LAUNCHER_RESULT_NONE;
}

static launcher_result handle_about_input(int key) {
    if (key == KEY_SOFT3)
        current_section = SECTION_MAIN;
    return LAUNCHER_RESULT_NONE;
}

/* ── Main launcher entry point ── */
launcher_result launcher_run(const char **selected_path, jad_info *selected_info) {
    *selected_path = NULL;

    if (app_count == 0) {
        lcdui_begin_frame();
        lcdui_clear(COL_BG);
        draw_text("No Java ME apps found in ux0:data/java/", 480, 260,
                  ANCHOR_HCENTER | ANCHOR_TOP, COL_DIM, 16);
        draw_text("Place .jar files and relaunch", 480, 290,
                  ANCHOR_HCENTER | ANCHOR_TOP, COL_MUTED, 12);
        lcdui_end_frame();
        sceKernelDelayThread(2 * 1000 * 1000);
        return LAUNCHER_RESULT_REFRESH;
    }

    while (1) {
        input_process();

        input_event events[16];
        int ev_count = input_poll(events, 16);

        /* Check backlog: if app launched and we return, stay on browse */
        for (int i = 0; i < ev_count; i++) {
            if (events[i].type == INPUT_EVENT_KEY_PRESSED) {
                launcher_result r = LAUNCHER_RESULT_NONE;

                switch (current_section) {
                    case SECTION_MAIN:
                        r = handle_main_input(events[i].key_code);
                        break;
                    case SECTION_BROWSE:
                        r = handle_browse_input(events[i].key_code, selected_path, selected_info);
                        break;
                    case SECTION_RECENT:
                        r = handle_recent_input(events[i].key_code, selected_path, selected_info);
                        break;
                    case SECTION_FAV:
                        r = handle_fav_input(events[i].key_code, selected_path, selected_info);
                        break;
                    case SECTION_SETTINGS:
                        r = handle_settings_input(events[i].key_code);
                        break;
                    case SECTION_ABOUT:
                        r = handle_about_input(events[i].key_code);
                        break;
                }

                if (r != LAUNCHER_RESULT_NONE)
                    return r;
            }
        }

        /* Render */
        lcdui_begin_frame();
        lcdui_clear(COL_BG);

        switch (current_section) {
            case SECTION_MAIN:     render_main(); break;
            case SECTION_BROWSE:   render_browse(); break;
            case SECTION_RECENT:   render_recent(); break;
            case SECTION_FAV:      render_favorites(); break;
            case SECTION_SETTINGS: render_settings(); break;
            case SECTION_ABOUT:    render_about(); break;
        }

        lcdui_end_frame();
        sceKernelDelayThread(16 * 1000);
    }
}
