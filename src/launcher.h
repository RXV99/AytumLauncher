#ifndef LAUNCHER_H
#define LAUNCHER_H

#include "jad_parser.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_APPS 128
#define MAX_RECENT 10

/* Menu sections */
typedef enum {
    SECTION_MAIN,    /* Main menu: Recent, Browse, Favorites, Settings, Refresh, About */
    SECTION_RECENT,  /* Recently opened apps list */
    SECTION_BROWSE,  /* File browser */
    SECTION_FAV,     /* Favorites list */
    SECTION_SETTINGS,/* Settings */
    SECTION_ABOUT,   /* About screen */
} launcher_section;

/* Main menu items */
typedef enum {
    MENU_RECENT = 0,
    MENU_BROWSE,
    MENU_FAVORITES,
    MENU_SETTINGS,
    MENU_REFRESH,
    MENU_ABOUT,
    MENU_COUNT,
} main_menu_item;

/* Launcher app entry */
typedef struct {
    char jar_path[512];
    char jad_path[512];
    char name[128];
    char vendor[64];
    char version[16];
    char class_name[256];
    int  has_jad;
    int  is_favorite;
} launcher_app;

/* Result from launcher */
typedef enum {
    LAUNCHER_RESULT_NONE,
    LAUNCHER_RESULT_LAUNCH,
    LAUNCHER_RESULT_REFRESH,
    LAUNCHER_RESULT_QUIT,
} launcher_result;

/* Recent entry */
typedef struct {
    char path[512];
    char name[128];
    int  last_opened; /* timestamp */
} recent_entry;

int  launcher_init(void);
void launcher_shutdown(void);

int  launcher_scan(void);
int  launcher_get_count(void);
const launcher_app *launcher_get_app(int index);

/* Track recently opened apps */
void launcher_add_recent(const char *jar_path, const char *name);
int  launcher_get_recent(recent_entry *entries, int max);

/* Favorites */
void launcher_toggle_favorite(const char *jar_path);
int  launcher_is_favorite(const char *jar_path);
int  launcher_get_favorites(launcher_app **apps, int *count);

/* Run the launcher UI */
launcher_result launcher_run(const char **selected_path, jad_info *selected_info);

#ifdef __cplusplus
}
#endif

#endif /* LAUNCHER_H */
