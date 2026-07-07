#ifndef FILEIO_H
#define FILEIO_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize file I/O subsystem (ensures ux0:data/java/ exists) */
int  fileio_init(void);
void fileio_shutdown(void);

/* Directory listing: returns array of filenames, count in *count */
char **fileio_list_dir(const char *path, int *count);
void   fileio_free_list(char **list, int count);

/* File operations */
int    fileio_exists(const char *path);
size_t fileio_get_size(const char *path);
int    fileio_is_directory(const char *path);

/* Read entire file into malloc'd buffer */
uint8_t *fileio_read_file(const char *path, size_t *size);

/* Write buffer to file (creates/overwrites) */
int fileio_write_file(const char *path, const uint8_t *data, size_t size);

/* Delete file */
int fileio_delete(const char *path);

/* Create directory */
int fileio_mkdir(const char *path);

/* Get app data base path */
const char *fileio_get_base_path(void);

/* Check if path has a given extension */
int fileio_has_extension(const char *path, const char *ext);

/* Join path components (returns malloc'd string) */
char *fileio_join_path(const char *a, const char *b);

/* Get filename from path (no alloc, points into path) */
const char *fileio_basename(const char *path);

#ifdef __cplusplus
}
#endif

#endif /* FILEIO_H */
