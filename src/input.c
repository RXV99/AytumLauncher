#include "input.h"
#include <string.h>
#include <psp2/ctrl.h>
#include <psp2/touch.h>
#include <psp2/kernel/threadmgr.h>

#define MAX_INPUT_EVENTS 64
#define MAX_POINTERS 6

static input_event event_queue[MAX_INPUT_EVENTS];
static int event_head = 0;
static int event_tail = 0;

static int key_state[256];

static SceCtrlData pad;
static SceTouchData touch_ports[2];

/* Map Vita button to Java ME key */
static int vita_button_to_jme(unsigned int btn) {
    switch (btn) {
        case SCE_CTRL_UP:     return KEY_UP;
        case SCE_CTRL_DOWN:   return KEY_DOWN;
        case SCE_CTRL_LEFT:   return KEY_LEFT;
        case SCE_CTRL_RIGHT:  return KEY_RIGHT;
        case SCE_CTRL_CROSS:  return KEY_FIRE;
        case SCE_CTRL_CIRCLE: return KEY_SOFT1;
        case SCE_CTRL_TRIANGLE: return KEY_SOFT2;
        case SCE_CTRL_SQUARE:  return KEY_SOFT3;
        case SCE_CTRL_START:   return KEY_NUM5;
        case SCE_CTRL_SELECT:  return KEY_NUM0;
        case SCE_CTRL_LTRIGGER: return KEY_NUM1;
        case SCE_CTRL_RTRIGGER: return KEY_NUM3;
        default: return 0;
    }
}

static void queue_event(input_event_type type, int key) {
    if (((event_tail + 1) % MAX_INPUT_EVENTS) == event_head) return;
    input_event *ev = &event_queue[event_tail];
    ev->type = type;
    ev->key_code = key;
    ev->game_action = 0;
    ev->x = ev->y = 0;
    event_tail = (event_tail + 1) % MAX_INPUT_EVENTS;
}

static void queue_pointer(input_event_type type, int x, int y) {
    if (((event_tail + 1) % MAX_INPUT_EVENTS) == event_head) return;
    input_event *ev = &event_queue[event_tail];
    ev->type = type;
    ev->key_code = 0;
    ev->game_action = 0;
    ev->x = x;
    ev->y = y;
    event_tail = (event_tail + 1) % MAX_INPUT_EVENTS;
}

int input_init(void) {
    memset(key_state, 0, sizeof(key_state));
    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);
    sceTouchSetSamplingState(SCE_TOUCH_PORT_BACK, SCE_TOUCH_SAMPLING_STATE_START);
    sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, SCE_TOUCH_SAMPLING_STATE_START);
    event_head = event_tail = 0;
    return 0;
}

void input_shutdown(void) {
}

void input_process(void) {
    /* Read controller state */
    memset(&pad, 0, sizeof(pad));
    sceCtrlPeekBufferPositive(0, &pad, 1);

    /* Detect key transitions */
    static unsigned int prev_buttons = 0;
    unsigned int curr_buttons = pad.buttons;
    unsigned int pressed = curr_buttons & ~prev_buttons;
    unsigned int released = prev_buttons & ~curr_buttons;

    /* Queue pressed events */
    for (int i = 0; i < 32; i++) {
        unsigned int bit = 1u << i;
        if (pressed & bit) {
            int jme_key = vita_button_to_jme(bit);
            if (jme_key) {
                queue_event(INPUT_EVENT_KEY_PRESSED, jme_key);
                key_state[jme_key & 0xFF] = 1;
            }
        }
        if (released & bit) {
            int jme_key = vita_button_to_jme(bit);
            if (jme_key) {
                queue_event(INPUT_EVENT_KEY_RELEASED, jme_key);
                key_state[jme_key & 0xFF] = 0;
            }
        }
    }
    prev_buttons = curr_buttons;

    /* Read touch state */
    for (int port = 0; port < 2; port++) {
        memset(&touch_ports[port], 0, sizeof(SceTouchData));
        sceTouchPeek(port, &touch_ports[port], 1);
        if (touch_ports[port].reportNum > 0) {
            for (int i = 0; i < (int)touch_ports[port].reportNum && i < MAX_POINTERS; i++) {
                int tx = (int)(touch_ports[port].report[i].x * 960.0f / 1920.0f);
                int ty = (int)(touch_ports[port].report[i].y * 544.0f / 1080.0f);
                queue_pointer(INPUT_EVENT_POINTER_PRESSED, tx, ty);
            }
        }
    }
}

int input_poll(input_event *events, int max_events) {
    int count = 0;
    while (event_head != event_tail && count < max_events) {
        events[count++] = event_queue[event_head];
        event_head = (event_head + 1) % MAX_INPUT_EVENTS;
    }
    return count;
}

int input_is_key_held(int key_code) {
    int idx = key_code & 0xFF;
    if (idx < 0 || idx >= 256) return 0;
    return key_state[idx];
}

void input_get_pointer(int *x, int *y) {
    if (touch_ports[0].reportNum > 0) {
        *x = (int)(touch_ports[0].report[0].x * 960.0f / 1920.0f);
        *y = (int)(touch_ports[0].report[0].y * 544.0f / 1080.0f);
    } else {
        *x = 480;
        *y = 272;
    }
}

int input_event_count(void) {
    if (event_tail >= event_head)
        return event_tail - event_head;
    return MAX_INPUT_EVENTS - event_head + event_tail;
}
