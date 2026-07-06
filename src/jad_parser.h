#ifndef JAD_PARSER_H
#define JAD_PARSER_H

#ifdef __cplusplus
extern "C" {
#endif

/* JAD parsed entry */
typedef struct {
    char *key;
    char *value;
} jad_entry;

/* JAD file data */
typedef struct {
    jad_entry *entries;
    int        count;
    char       midlet_name[128];
    char       midlet_vendor[64];
    char       midlet_version[16];
    char       midlet_description[256];
    char       jar_url[512];
    char       jar_path[512];     /* Local path to JAR */
    int        jar_size;
    char       midlet_class[256]; /* Main MIDlet class */
    char       icon_path[256];
} jad_info;

/* Parse a JAD file (text buffer) into structured data */
int jad_parse(const char *buffer, size_t size, jad_info *info);

/* Free a jad_info structure */
void jad_free(jad_info *info);

/* Try to load a JAD file from path; returns 0 on success */
int jad_load(const char *path, jad_info *info);

/* Generate JAD info from JAR path alone (scans for JAD alongside JAR) */
int jad_load_for_jar(const char *jar_path, jad_info *info);

#ifdef __cplusplus
}
#endif

#endif /* JAD_PARSER_H */
