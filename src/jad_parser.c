#include "jad_parser.h"
#include "fileio.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <strings.h>
#include <zlib.h>

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

/* Try to extract a file from a ZIP/JAR archive.
 * Returns malloc'd buffer with content, sets *size. Returns NULL if not found.
 * Handles stored (method 0) and deflated (method 8) entries. */
static uint8_t *zip_extract_file(const uint8_t *data, size_t data_size,
                                  const char *target_name, size_t *out_size) {
    *out_size = 0;
    const uint8_t *p = data;
    const uint8_t *end = data + data_size;

    while (p + 30 <= end) {
        uint32_t sig = (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
                       ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
        if (sig != 0x04034b50) break;

        uint16_t method   = (uint16_t)p[8]  | ((uint16_t)p[9]  << 8);
        uint32_t comp_sz  = (uint32_t)p[18] | ((uint32_t)p[19] << 8) |
                            ((uint32_t)p[20] << 16) | ((uint32_t)p[21] << 24);
        uint32_t uncomp_sz = (uint32_t)p[22] | ((uint32_t)p[23] << 8) |
                             ((uint32_t)p[24] << 16) | ((uint32_t)p[25] << 24);
        uint16_t name_len = (uint16_t)p[26] | ((uint16_t)p[27] << 8);
        uint16_t extra_len = (uint16_t)p[28] | ((uint16_t)p[29] << 8);
        const char *name_ptr = (const char*)p + 30;
        const uint8_t *data_ptr = p + 30 + name_len + extra_len;

        /* Case-insensitive name match */
        if (strcasecmp(name_ptr, target_name) == 0) {
            if (method == 0) {
                /* Stored */
                uint8_t *buf = (uint8_t*)malloc(uncomp_sz + 1);
                if (!buf) return NULL;
                memcpy(buf, data_ptr, uncomp_sz);
                buf[uncomp_sz] = 0;
                *out_size = uncomp_sz;
                return buf;
            } else if (method == 8) {
                /* Deflated — decompress with zlib */
                uint8_t *buf = (uint8_t*)malloc(uncomp_sz + 1);
                if (!buf) return NULL;
                z_stream strm;
                memset(&strm, 0, sizeof(strm));
                strm.next_in   = (uint8_t*)data_ptr;
                strm.avail_in  = comp_sz;
                strm.next_out  = buf;
                strm.avail_out = uncomp_sz;
                if (inflateInit2(&strm, -MAX_WBITS) != Z_OK) {
                    free(buf);
                    return NULL;
                }
                int ret = inflate(&strm, Z_FINISH);
                inflateEnd(&strm);
                if (ret != Z_STREAM_END) {
                    free(buf);
                    return NULL;
                }
                buf[uncomp_sz] = 0;
                *out_size = uncomp_sz;
                return buf;
            }
            return NULL; /* Unsupported compression */
        }

        uint32_t total = 30 + name_len + extra_len + comp_sz;
        p += total;
    }
    return NULL;
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

    /* No .jad file — try reading MANIFEST.MF from the JAR */
    printf("jad: No JAD found. Using JAR manifest.\n");

    size_t jar_size;
    uint8_t *jar_data = fileio_read_file(jar_path, &jar_size);
    if (jar_data) {
        size_t mf_size;
        uint8_t *mf = zip_extract_file(jar_data, jar_size, "META-INF/MANIFEST.MF", &mf_size);
        if (mf) {
            int ret = jad_parse((const char*)mf, mf_size, info);
            free(mf);
            free(jar_data);
            if (ret == 0) {
                snprintf(info->jar_path, sizeof(info->jar_path) - 1, "%s", jar_path);
                return 0;
            }
        }
        free(jar_data);
    }

    /* Final fallback: create minimal info from jar_path */
    memset(info, 0, sizeof(jad_info));
    printf("jad: No manifest found. Using filename.\n");
    const char *name = fileio_basename(jar_path);
    if (name) {
        strncpy(info->midlet_name, name, sizeof(info->midlet_name) - 1);
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
