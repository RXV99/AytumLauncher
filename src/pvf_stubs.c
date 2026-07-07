#include <psp2/types.h>

void *scePvfNewLib(void) { return NULL; }
int scePvfDoneLib(void *lib) { return 0; }
int scePvfSetEM(void *lib, unsigned int em) { return 0; }
int scePvfSetResolution(void *lib, unsigned int dpiX, unsigned int dpiY) { return 0; }
int scePvfFindOptimumFont(void *lib, void *req, void *font) { return -1; }
int scePvfOpen(void *lib, void *font, void *handle) { return -1; }
int scePvfClose(void *lib, void *handle) { return 0; }
int scePvfSetCharSize(void *lib, void *handle, unsigned int w, unsigned int h) { return 0; }
int scePvfGetFontInfo(void *lib, void *handle, void *info) { return 0; }
int scePvfGetCharInfo(void *lib, void *handle, unsigned int c, void *ci) { return 0; }
int scePvfGetCharImageRect(void *lib, void *handle, unsigned int c, void *rect) { return 0; }
int scePvfGetCharGlyphImage(void *lib, void *handle, unsigned int c, void *glyph) { return 0; }
int scePvfGetKerningInfo(void *lib, void *handle, unsigned int c1, unsigned int c2, void *kern) { return 0; }
int scePvfOpenUserFile(void *lib, const char *path, void *handle) { return -1; }
