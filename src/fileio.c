#include "fileio.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <psp2/io/dirent.h>
#include <psp2/io/stat.h>
#include <psp2/io/fcntl.h>

#ifndef strcasecmp
static int strcasecmp(const char *a, const char *b) {
    while (*a && *b) {
        int ca = toupper((unsigned char)*a);
        int cb = toupper((unsigned char)*b);
        if (ca != cb) return ca - cb;
        a++; b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}
#endif

#define BASE_PATH "ux0:data/java"

static const char *base_path = BASE_PATH;

int fileio_init(void) {
    /* Ensure base directory exists */
    SceIoStat stat;
    int ret = sceIoGetstat(base_path, &stat);
    if (ret < 0) {
        ret = sceIoMkdir(base_path, 0777);
        if (ret < 0) {
            printf("fileio: failed to create %s (0x%08X)\n", base_path, ret);
            return -1;
        }
    }
    return 0;
}

void fileio_shutdown(void) {
}

char **fileio_list_dir(const char *path, int *count) {
    *count = 0;
    int max_entries = 256;
    char **list = (char**)calloc(max_entries, sizeof(char*));
    if (!list) return NULL;

    SceUID dfd = sceIoDopen(path);
    if (dfd < 0) {
        free(list);
        return NULL;
    }

    int idx = 0;
    SceIoDirent entry;
    while (idx < max_entries) {
        memset(&entry, 0, sizeof(entry));
        int ret = sceIoDread(dfd, &entry);
        if (ret <= 0) break;
        if (strcmp(entry.d_name, ".") == 0 || strcmp(entry.d_name, "..") == 0)
            continue;
        list[idx] = strdup(entry.d_name);
        if (list[idx]) idx++;
    }
    sceIoDclose(dfd);
    *count = idx;
    return list;
}

void fileio_free_list(char **list, int count) {
    if (!list) return;
    for (int i = 0; i < count; i++) free(list[i]);
    free(list);
}

int fileio_exists(const char *path) {
    SceIoStat stat;
    return sceIoGetstat(path, &stat) >= 0;
}

size_t fileio_get_size(const char *path) {
    SceIoStat stat;
    if (sceIoGetstat(path, &stat) < 0) return 0;
    return (size_t)stat.st_size;
}

int fileio_is_directory(const char *path) {
    SceIoStat stat;
    if (sceIoGetstat(path, &stat) < 0) return 0;
    return SCE_S_ISDIR(stat.st_mode);
}

uint8_t *fileio_read_file(const char *path, size_t *size) {
    SceUID fd = sceIoOpen(path, SCE_O_RDONLY, 0);
    if (fd < 0) {
        *size = 0;
        return NULL;
    }
    size_t file_size = fileio_get_size(path);
    if (file_size == 0) {
        sceIoClose(fd);
        *size = 0;
        return NULL;
    }
    uint8_t *buf = (uint8_t*)malloc(file_size + 1);
    if (!buf) {
        sceIoClose(fd);
        *size = 0;
        return NULL;
    }
    size_t total_read = 0;
    while (total_read < file_size) {
        int n = sceIoRead(fd, buf + total_read, (SceSize)(file_size - total_read));
        if (n <= 0) break;
        total_read += n;
    }
    buf[total_read] = 0;
    sceIoClose(fd);
    *size = total_read;
    return buf;
}

int fileio_write_file(const char *path, const uint8_t *data, size_t size) {
    SceUID fd = sceIoOpen(path, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
    if (fd < 0) return -1;
    size_t written = 0;
    while (written < size) {
        int n = sceIoWrite(fd, data + written, (SceSize)(size - written));
        if (n <= 0) break;
        written += n;
    }
    sceIoClose(fd);
    return (written == size) ? 0 : -1;
}

int fileio_delete(const char *path) {
    return sceIoRemove(path);
}

int fileio_mkdir(const char *path) {
    return sceIoMkdir(path, 0777);
}

const char *fileio_get_base_path(void) {
    return base_path;
}

int fileio_has_extension(const char *path, const char *ext) {
    if (!path || !ext) return 0;
    const char *dot = strrchr(path, '.');
    if (!dot) return 0;
    return strcasecmp(dot, ext) == 0;
}

char *fileio_join_path(const char *a, const char *b) {
    size_t alen = strlen(a);
    size_t blen = strlen(b);
    char *result = (char*)malloc(alen + blen + 2);
    if (!result) return NULL;
    strcpy(result, a);
    if (result[alen - 1] != '/') {
        result[alen] = '/';
        result[alen + 1] = 0;
    }
    strcat(result, b);
    return result;
}

const char *fileio_basename(const char *path) {
    if (!path) return NULL;
    const char *p = strrchr(path, '/');
    return p ? p + 1 : path;
}
