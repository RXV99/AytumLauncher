#ifndef MIDP_API_H
#define MIDP_API_H

#include "jvm_core.h"

#define JVM_UNUSED(x) (void)(x)

/* Audio tone constants matching Java ME */
#define JME_TONE_C4  60
#define JME_TONE_D4  62
#define JME_TONE_E4  64
#define JME_TONE_F4  65
#define JME_TONE_G4  67
#define JME_TONE_A4  69
#define JME_TONE_B4  71
#define JME_TONE_C5  72

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize all MIDP native methods */
void midp_init_natives(jvm_instance *jvm);

/* Java types used by native methods */
typedef struct midp_displayable midp_displayable;
typedef struct midp_display midp_display;
typedef struct midp_canvas midp_canvas;
typedef struct midp_image midp_image;
typedef struct midp_graphics midp_graphics;
typedef struct midp_font midp_font;

/* Display states */
enum {
    DISPLAY_STATE_VISIBLE = 1,
    DISPLAY_STATE_SHOWN = 2,
};

/* LCDUI Color constants */
#define LCDUI_COLOR_BLACK       0x000000
#define LCDUI_COLOR_WHITE       0xFFFFFF
#define LCDUI_COLOR_RED         0xFF0000
#define LCDUI_COLOR_GREEN       0x00FF00
#define LCDUI_COLOR_BLUE        0x0000FF

/* Key codes matching Java ME */
#define JME_KEY_NUM0     48
#define JME_KEY_NUM1     49
#define JME_KEY_NUM2     50
#define JME_KEY_NUM3     51
#define JME_KEY_NUM4     52
#define JME_KEY_NUM5     53
#define JME_KEY_NUM6     54
#define JME_KEY_NUM7     55
#define JME_KEY_NUM8     56
#define JME_KEY_NUM9     57
#define JME_KEY_STAR     42
#define JME_KEY_POUND    35
#define JME_KEY_UP       -1
#define JME_KEY_DOWN     -2
#define JME_KEY_LEFT     -3
#define JME_KEY_RIGHT    -4
#define JME_KEY_FIRE     -5
#define JME_KEY_SOFT1    -6
#define JME_KEY_SOFT2    -7
#define JME_KEY_SOFT3    -8

/* Game action mapping */
#define JME_GAME_UP      1
#define JME_GAME_DOWN    6
#define JME_GAME_LEFT    2
#define JME_GAME_RIGHT   5
#define JME_GAME_FIRE    8
#define JME_GAME_A       9
#define JME_GAME_B       10
#define JME_GAME_C       11
#define JME_GAME_D       12

#ifdef __cplusplus
}
#endif

#endif /* MIDP_API_H */
