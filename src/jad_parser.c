#include "jad_parser.h"
#include "fileio.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <strings.h>

#define MAX_JAD_ENTRIES 64

int jad_parse(const char *buffer, size_t size, jad_info *info) {
    if (!buffer || !info) return -1;

    memset(info, 0, sizeof(jad_info));

    /* Allocate entry array */
    info->entries = (jad_entry*)calloc(MAX_JAD_ENTRIES, sizeof(jad_entry));
    if (!info->entries) return -1;
    info->count = 0;

    const char *p = buffer;
    const char *end = buffer + size;

    while (p < end && info->count < MAX_JAD_ENTRIES) {
        /* Skip whitespace and blank lines */
        while (p < end && (*p == '\r' || *p == '\n' || *p == ' ' || *p == '\t')) p++;
        if (p >= end) break;

        /* Skip comments */
        if (*p == '#') {
            while (p < end && *p != '\n') p++;
            continue;
        }

        /* Find colon separator */
        const char *colon = NULL;
        const char *line_end = p;
        while (line_end < end && *line_end != '\n' && *line_end != '\r') {
            if (*line_end == ':' && !colon) colon = line_end;
            line_end++;
        }

        if (colon && colon > p && colon < line_end) {
            size_t key_len = (size_t)(colon - p);
            size_t val_len = (size_t)(line_end - colon - 1);

            char *key = (char*)malloc(key_len + 1);
            char *val = (char*)malloc(val_len + 1);
            if (key && val) {
                strncpy(key, p, key_len);
                key[key_len] = 0;
                /* Trim trailing whitespace */
                while (key_len > 0 && (key[key_len - 1] == ' ' || key[key_len - 1] == '\t'))
                    key[--key_len] = 0;

                strncpy(val, colon + 1, val_len);
                val[val_len] = 0;
                /* Trim leading whitespace */
                char *v = val;
                while (*v == ' ' || *v == '\t') v++;
                if (v != val) memmove(val, v, strlen(v) + 1);

                info->entries[info->count].key = key;
                info->entries[info->count].value = val;
                info->count++;

                /* Populate structured fields */
                if (strcasecmp(key, "MIDlet-Name") == 0)
                    strncpy(info->midlet_name, val, sizeof(info->midlet_name) - 1);
                else if (strcasecmp(key, "MIDlet-Vendor") == 0)
                    strncpy(info->midlet_vendor, val, sizeof(info->midlet_vendor) - 1);
                else if (strcasecmp(key, "MIDlet-Version") == 0)
                    strncpy(info->midlet_version, val, sizeof(info->midlet_version) - 1);
                else if (strcasecmp(key, "MIDlet-Description") == 0)
                    strncpy(info->midlet_description, val, sizeof(info->midlet_description) - 1);
                else if (strcasecmp(key, "MIDlet-Jar-URL") == 0)
                    strncpy(info->jar_url, val, sizeof(info->jar_url) - 1);
                else if (strcasecmp(key, "MIDlet-Jar-Size") == 0)
                    info->jar_size = atoi(val);
                else if (strcasecmp(key, "MIDlet-1") == 0) {
                    /* Format: "Name, Icon, Class" */
                    char *icon_start = NULL;
                    char *class_start = NULL;
                    char *v_copy = strdup(val);
                    if (v_copy) {
                        /* First comma separates name from rest */
                        char *first_comma = strchr(v_copy, ',');
                        if (first_comma) {
                            *first_comma = 0;
                            icon_start = first_comma + 1;
                            while (*icon_start == ' ') icon_start++;
                            char *second_comma = strchr(icon_start, ',');
                            if (second_comma) {
                                *second_comma = 0;
                                class_start = second_comma + 1;
                                while (*class_start == ' ') class_start++;
                            }
                            if (class_start) {
                                strncpy(info->midlet_class, class_start, sizeof(info->midlet_class) - 1);
                            }
                            if (icon_start) {
                                strncpy(info->icon_path, icon_start, sizeof(info->icon_path) - 1);
                            }
                        }
                        free(v_copy);
                    }
                }
            } else {
                free(key);
                free(val);
            }
        }

        /* If line has no colon but has content, skip it */
        if (line_end < end && *line_end == '\r') line_end++;
        if (line_end < end && *line_end == '\n') line_end++;
        p = line_end;
    }

    return 0;
}

void jad_free(jad_info *info) {
    if (!info || !info->entries) return;
    for (int i = 0; i < info->count; i++) {
        free(info->entries[i].key);
        free(info->entries[i].value);
    }
    free(info->entries);
    info->entries = NULL;
    info->count = 0;
}

int jad_load(const char *path, jad_info *info) {
    if (!path || !info) return -1;

    size_t size;
    uint8_t *buf = fileio_read_file(path, &size);
    if (!buf) return -1;

    int ret = jad_parse((const char*)buf, size, info);
    free(buf);

    /* Set jar_path based on jar_url if available */
    if (info->jar_url[0]) {
        const char *jar_name = strrchr(info->jar_url, '/');
        if (jar_name) jar_name++;
        else jar_name = info->jar_url;

        snprintf(info->jar_path, sizeof(info->jar_path) - 1, "%s/%s",
                 fileio_get_base_path(), jar_name);
    }

    return ret;
}

int jad_load_for_jar(const char *jar_path, jad_info *info) {
    if (!jar_path || !info) return -1;

    /* Try .jad file alongside .jar */
    size_t path_len = strlen(jar_path);
    char *jad_path = (char*)malloc(path_len + 4);
    if (!jad_path) return -1;
    strcpy(jad_path, jar_path);
    char *dot = strrchr(jad_path, '.');
    if (dot && (strcasecmp(dot, ".jar") == 0)) {
        strcpy(dot, ".jad");
        if (fileio_exists(jad_path)) {
            int ret = jad_load(jad_path, info);
            free(jad_path);
            return ret;
        }
    }
    free(jad_path);

    /* No .jad file, create minimal info from jar_path */
    memset(info, 0, sizeof(jad_info));
    const char *name = fileio_basename(jar_path);
    if (name) {
        strncpy(info->midlet_name, name, sizeof(info->midlet_name) - 1);
        /* Strip .jar */
        char *dot2 = strrchr(info->midlet_name, '.');
        if (dot2 && strcasecmp(dot2, ".jar") == 0) *dot2 = 0;
    }
    strncpy(info->midlet_vendor, "Unknown", sizeof(info->midlet_vendor) - 1);
    strncpy(info->midlet_version, "1.0", sizeof(info->midlet_version) - 1);
    strncpy(info->jar_path, jar_path, sizeof(info->jar_path) - 1);
    info->entries = NULL;
    info->count = 0;

    return 0;
}
