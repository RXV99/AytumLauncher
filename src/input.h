#ifndef INPUT_H
#define INPUT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Vita button -> Java ME key mapping */
#define KEY_UP       -1
#define KEY_DOWN     -2
#define KEY_LEFT     -3
#define KEY_RIGHT    -4
#define KEY_FIRE     -5
#define KEY_SOFT1    -6
#define KEY_SOFT2    -7
#define KEY_SOFT3    -8

#define KEY_NUM0     48
#define KEY_NUM1     49
#define KEY_NUM2     50
#define KEY_NUM3     51
#define KEY_NUM4     52
#define KEY_NUM5     53
#define KEY_NUM6     54
#define KEY_NUM7     55
#define KEY_NUM8     56
#define KEY_NUM9     57
#define KEY_STAR     42
#define KEY_POUND    35

/* Input event types */
typedef enum {
    INPUT_EVENT_KEY_PRESSED,
    INPUT_EVENT_KEY_RELEASED,
    INPUT_EVENT_KEY_REPEATED,
    INPUT_EVENT_POINTER_PRESSED,
    INPUT_EVENT_POINTER_RELEASED,
    INPUT_EVENT_POINTER_DRAGGED,
} input_event_type;

/* Input event */
typedef struct {
    input_event_type type;
    int              key_code;      /* For key events */
    int              game_action;   /* Mapped game action */
    int              x, y;          /* For pointer events */
} input_event;

/* Initialize input subsystem */
int  input_init(void);
void input_shutdown(void);

/* Poll for input events; returns number of events read */
int  input_poll(input_event *events, int max_events);

/* Check if a specific key is held */
int  input_is_key_held(int key_code);

/* Get last pointer position */
void input_get_pointer(int *x, int *y);

/* Process Vita hardware input into Java ME events (called each frame) */
void input_process(void);

/* Get current pending event count */
int  input_event_count(void);

#ifdef __cplusplus
}
#endif

#endif /* INPUT_H */
