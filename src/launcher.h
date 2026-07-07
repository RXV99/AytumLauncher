#ifndef LAUNCHER_H
#define LAUNCHER_H

#include "jad_parser.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_APPS 128
#define MAX_RECENT 10

typedef enum {
    SECTION_RECENT = 0,
    SECTION_BROWSE,
    SECTION_SETTINGS,
    SECTION_ABOUT,
    SECTION_COUNT,
} launcher_section;

typedef struct {
    char jar_path[512];
    char jad_path[512];
    char name[128];
    char vendor[64];
    char version[16];
    char class_name[256];
    int  has_jad;
} launcher_app;

typedef enum {
    LAUNCHER_RESULT_NONE,
    LAUNCHER_RESULT_LAUNCH,
    LAUNCHER_RESULT_REFRESH,
    LAUNCHER_RESULT_QUIT,
} launcher_result;

typedef struct {
    char path[512];
    char name[128];
} recent_entry;

int  launcher_init(void);
void launcher_shutdown(void);
int  launcher_scan(void);
int  launcher_get_count(void);
const launcher_app *launcher_get_app(int index);

void launcher_add_recent(const char *jar_path, const char *name);
int  launcher_get_recent(recent_entry *entries, int max);

void launcher_save_browse_path(const char *path);
int  launcher_load_browse_path(char *path, int max_len);

launcher_result launcher_run(const char **selected_path, jad_info *selected_info);

#ifdef __cplusplus
}
#endif

#endif
