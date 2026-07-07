#include "midp_api.h"
#include "graphics.h"
#include "input.h"
#include "fileio.h"
#include "network.h"
#include "audio.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/sysmem.h>
#include <vita2d.h>
#include "decoder.h"

#undef JVM_UNUSED
#define JVM_UNUSED

/* Repaint flag — native_canvas_repaint sets this; the render loop owns frame lifecycle */
static volatile int g_repaint_requested = 0;

int midp_repaint_requested(void) {
    return g_repaint_requested;
}

void midp_clear_repaint_requested(void) {
    g_repaint_requested = 0;
}

/* ============================================================
 * javax.microedition.midlet.MIDlet
 * ============================================================ */

static void native_midlet_init(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    printf("midp: MIDlet.init()\n");
}

static void native_midlet_start_app(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    printf("midp: MIDlet.startApp()\n");
}

static void native_midlet_pause_app(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    printf("midp: MIDlet.pauseApp()\n");
}

static void native_midlet_destroy_app(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    printf("midp: MIDlet.destroyApp()\n");
}

static void native_midlet_get_app_property(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_push(thread, (jvm_value){.ref = NULL});
}

static void native_midlet_notify_destroyed(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    printf("midp: MIDlet.notifyDestroyed()\n");
}

static void native_midlet_notify_paused(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    printf("midp: MIDlet.notifyPaused()\n");
}

/* ============================================================
 * javax.microedition.lcdui.Display
 * ============================================================ */

static void native_display_get_display(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    printf("midp: Display.getDisplay()\n");
    jvm_value val; memset(&val, 0, sizeof(val));
    jvm_stack_push(thread, val);
}

static void native_display_set_current(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    printf("midp: Display.setCurrent()\n");
}

static void native_display_is_color(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_push(thread, (jvm_value){.i = 1});
}

static void native_display_num_colors(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_push(thread, (jvm_value){.i = 65536});
}

static void native_display_get_width(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_push(thread, (jvm_value){.i = 960});
}

static void native_display_get_height(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_push(thread, (jvm_value){.i = 544});
}

/* ============================================================
 * javax.microedition.lcdui.Canvas
 * ============================================================ */

static void native_canvas_get_width(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_push(thread, (jvm_value){.i = 960});
}

static void native_canvas_get_height(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_push(thread, (jvm_value){.i = 544});
}

static void native_canvas_repaint(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    g_repaint_requested = 1;
}

static void native_canvas_service_repaints(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    printf("midp: Canvas.serviceRepaints()\n");
}

static void native_canvas_has_pointer_events(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_push(thread, (jvm_value){.i = 1});
}

static void native_canvas_has_repeat_events(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_push(thread, (jvm_value){.i = 1});
}

/* ============================================================
 * javax.microedition.lcdui.Graphics
 * ============================================================ */

static void native_graphics_set_color(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    s4 color = jvm_stack_pop(thread).i;
    lcdui_set_color(color);
}

static void native_graphics_get_color(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_push(thread, (jvm_value){.i = lcdui_get_color()});
}

static void native_graphics_draw_line(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    s4 y2 = jvm_stack_pop(thread).i;
    s4 x2 = jvm_stack_pop(thread).i;
    s4 y1 = jvm_stack_pop(thread).i;
    s4 x1 = jvm_stack_pop(thread).i;
    lcdui_draw_line(x1, y1, x2, y2);
}

static void native_graphics_draw_rect(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    s4 height = jvm_stack_pop(thread).i;
    s4 width  = jvm_stack_pop(thread).i;
    s4 y      = jvm_stack_pop(thread).i;
    s4 x      = jvm_stack_pop(thread).i;
    lcdui_draw_rect(x, y, width, height, 0);
}

static void native_graphics_fill_rect(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    s4 height = jvm_stack_pop(thread).i;
    s4 width  = jvm_stack_pop(thread).i;
    s4 y      = jvm_stack_pop(thread).i;
    s4 x      = jvm_stack_pop(thread).i;
    lcdui_fill_rect(x, y, width, height);
}

static void native_graphics_draw_string(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    s4 anchor = jvm_stack_pop(thread).i;
    s4 y      = jvm_stack_pop(thread).i;
    s4 x      = jvm_stack_pop(thread).i;
    char *str = (char*)jvm_stack_pop(thread).ref;
    lcdui_draw_string(str, x, y, anchor);
}

static void native_graphics_set_font(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_pop(thread);
}

static void native_graphics_get_font(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_push(thread, (jvm_value){.ref = NULL});
}

static void native_graphics_draw_image(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    s4 anchor = jvm_stack_pop(thread).i;
    s4 y      = jvm_stack_pop(thread).i;
    s4 x      = jvm_stack_pop(thread).i;
    jvm_stack_pop(thread);
    lcdui_draw_rect(x, y, 16, 16, 0);
}

static void native_graphics_translate(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_pop(thread); jvm_stack_pop(thread);
}

static void native_graphics_get_translate_x(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_push(thread, (jvm_value){.i = 0});
}

static void native_graphics_get_translate_y(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_push(thread, (jvm_value){.i = 0});
}

static void native_graphics_set_clip(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_pop(thread); jvm_stack_pop(thread);
    jvm_stack_pop(thread); jvm_stack_pop(thread);
}

static void native_graphics_get_clip_x(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_push(thread, (jvm_value){.i = 0});
}

static void native_graphics_get_clip_y(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_push(thread, (jvm_value){.i = 0});
}

static void native_graphics_get_clip_width(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_push(thread, (jvm_value){.i = 960});
}

static void native_graphics_get_clip_height(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_push(thread, (jvm_value){.i = 544});
}

/* ============================================================
 * javax.microedition.lcdui.Font
 * ============================================================ */

static void native_font_get_default_font(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_push(thread, (jvm_value){.ref = NULL});
}

static void native_font_get_height(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_push(thread, (jvm_value){.i = 16});
}

static void native_font_string_width(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    char *str = (char*)jvm_stack_pop(thread).ref;
    if (str) jvm_stack_push(thread, (jvm_value){.i = (int)strlen(str) * 8});
    else jvm_stack_push(thread, (jvm_value){.i = 0});
}

/* ============================================================
 * javax.microedition.lcdui.Image
 * ============================================================ */

static void native_image_create_image(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_pop(thread); jvm_stack_pop(thread);
    jvm_stack_push(thread, (jvm_value){.ref = NULL});
}

static void native_image_get_width(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_push(thread, (jvm_value){.i = 16});
}

static void native_image_get_height(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_push(thread, (jvm_value){.i = 16});
}

static void native_image_get_graphics(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_push(thread, (jvm_value){.ref = NULL});
}

/* ============================================================
 * javax.microedition.lcdui.List / Alert / TextBox / Form
 * ============================================================ */

static void native_list_append(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_pop(thread);
    jvm_stack_push(thread, (jvm_value){.i = 0});
}

static void native_list_delete(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_pop(thread);
}

static void native_list_get_selected_index(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_push(thread, (jvm_value){.i = 0});
}

static void native_list_set_select_command(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_pop(thread);
}

static void native_alert_set_string(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_pop(thread);
}

static void native_textbox_set_string(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_pop(thread);
}

static void native_textbox_get_string(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_push(thread, (jvm_value){.ref = NULL});
}

static void native_textbox_get_chars(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_push(thread, (jvm_value){.i = 0});
}

/* ============================================================
 * javax.microedition.lcdui.Command / Displayable
 * ============================================================ */

static void native_command_init(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_pop(thread); jvm_stack_pop(thread); jvm_stack_pop(thread);
}

static void native_displayable_add_command(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_pop(thread);
}

static void native_displayable_set_command_listener(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_pop(thread);
}

static void native_displayable_is_shown(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_push(thread, (jvm_value){.i = 1});
}

static void native_displayable_get_title(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_push(thread, (jvm_value){.ref = NULL});
}

static void native_displayable_set_title(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_pop(thread);
}

/* ============================================================
 * java.lang.System
 * ============================================================ */

static void native_system_current_time_millis(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_push(thread, (jvm_value){.i = (s4)(sceKernelGetSystemTimeWide() / 1000)});
}

static void native_system_nano_time(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_push(thread, (jvm_value){.i = (s4)(sceKernelGetSystemTimeWide() * 1000)});
}

static void native_system_array_copy(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_pop(thread); jvm_stack_pop(thread);
    jvm_stack_pop(thread); jvm_stack_pop(thread);
    jvm_stack_pop(thread);
}

static void native_system_identity_hash_code(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    void *ref = jvm_stack_pop(thread).ref;
    jvm_stack_push(thread, (jvm_value){.i = (s4)(uintptr_t)ref});
}

/* ============================================================
 * java.lang.String
 * ============================================================ */

static void native_string_intern(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    /* Returns the same string */
}

static void native_string_get_chars(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_pop(thread); jvm_stack_pop(thread); jvm_stack_pop(thread); jvm_stack_pop(thread);
}

static void native_string_equals(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    void *other = jvm_stack_pop(thread).ref;
    void *self = jvm_stack_pop(thread).ref;
    jvm_stack_push(thread, (jvm_value){.i = (self == other) ? 1 : 0});
}

static void native_string_hash_code(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    char *str = (char*)jvm_stack_pop(thread).ref;
    if (str) {
        s4 hash = 0;
        for (char *p = str; *p; p++) hash = 31 * hash + *p;
        jvm_stack_push(thread, (jvm_value){.i = hash});
    } else {
        jvm_stack_push(thread, (jvm_value){.i = 0});
    }
}

static void native_string_length(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    char *str = (char*)jvm_stack_pop(thread).ref;
    jvm_stack_push(thread, (jvm_value){.i = str ? (int)strlen(str) : 0});
}

static void native_string_char_at(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    s4 idx = jvm_stack_pop(thread).i;
    char *str = (char*)jvm_stack_pop(thread).ref;
    if (str && idx >= 0 && idx < (s4)strlen(str))
        jvm_stack_push(thread, (jvm_value){.i = str[idx]});
    else
        jvm_stack_push(thread, (jvm_value){.i = 0});
}

static void native_string_substring(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_pop(thread);
    jvm_stack_push(thread, (jvm_value){.ref = NULL});
}

static void native_string_trim(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    /* Return same string */
}

static void native_string_to_lower_case(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    /* Return same string */
}

static void native_string_to_upper_case(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    /* Return same string */
}

static void native_string_concat(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_pop(thread);
    jvm_stack_push(thread, (jvm_value){.ref = NULL});
}

static void native_string_index_of(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_pop(thread);
    jvm_stack_push(thread, (jvm_value){.i = -1});
}

/* ============================================================
 * java.lang.StringBuffer
 * ============================================================ */

static void native_string_buffer_init(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    printf("midp: StringBuffer init\n");
}

static void native_string_buffer_append(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_pop(thread);
    jvm_stack_push(thread, (jvm_value){.ref = NULL});
}

static void native_string_buffer_to_string(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_push(thread, (jvm_value){.ref = NULL});
}

/* ============================================================
 * java.lang.Integer / Long / Float / Boolean
 * ============================================================ */

static void native_integer_int_value(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_push(thread, (jvm_value){.i = jvm_stack_pop(thread).i});
}

static void native_integer_value_of(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_pop(thread);
    jvm_stack_push(thread, (jvm_value){.ref = NULL});
}

static void native_integer_parse_int(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    char *str = (char*)jvm_stack_pop(thread).ref;
    if (str) jvm_stack_push(thread, (jvm_value){.i = (s4)atoi(str)});
    else jvm_stack_push(thread, (jvm_value){.i = 0});
}

static void native_integer_to_hex_string(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_pop(thread);
    jvm_stack_push(thread, (jvm_value){.ref = NULL});
}

static void native_long_value_of(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_pop(thread);
    jvm_stack_push(thread, (jvm_value){.ref = NULL});
}

static void native_float_float_value(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_value v = jvm_stack_pop(thread);
    v.f = (float)v.i;
    jvm_stack_push(thread, v);
}

static void native_boolean_value_of(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_pop(thread);
    jvm_stack_push(thread, (jvm_value){.ref = NULL});
}

/* ============================================================
 * java.lang.Math
 * ============================================================ */

static void native_math_min(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    s4 b = jvm_stack_pop(thread).i;
    s4 a = jvm_stack_pop(thread).i;
    jvm_stack_push(thread, (jvm_value){.i = a < b ? a : b});
}

static void native_math_max(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    s4 b = jvm_stack_pop(thread).i;
    s4 a = jvm_stack_pop(thread).i;
    jvm_stack_push(thread, (jvm_value){.i = a > b ? a : b});
}

static void native_math_abs(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    s4 v = jvm_stack_pop(thread).i;
    jvm_stack_push(thread, (jvm_value){.i = v < 0 ? -v : v});
}

static void native_math_sqrt(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_value v = jvm_stack_pop(thread);
    v.d = sqrt(v.d);
    jvm_stack_push(thread, v);
}

/* ============================================================
 * java.lang.Object
 * ============================================================ */

static void native_object_get_class(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_push(thread, (jvm_value){.ref = NULL});
}

static void native_object_to_string(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_push(thread, (jvm_value){.ref = NULL});
}

static void native_object_hash_code(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    void *ref = jvm_stack_pop(thread).ref;
    jvm_stack_push(thread, (jvm_value){.i = (s4)(uintptr_t)ref});
}

static void native_object_equals(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    void *obj = jvm_stack_pop(thread).ref;
    jvm_stack_push(thread, (jvm_value){.i = (obj == jvm_stack_pop(thread).ref) ? 1 : 0});
}

static void native_object_notify(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {}
static void native_object_notify_all(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {}
static void native_object_wait(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {} /* Simplified */

/* ============================================================
 * java.lang.Class
 * ============================================================ */

static void native_class_for_name(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_pop(thread);
    jvm_stack_push(thread, (jvm_value){.ref = NULL});
}

static void native_class_get_name(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_push(thread, (jvm_value){.ref = NULL});
}

static void native_class_get_superclass(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_push(thread, (jvm_value){.ref = NULL});
}

static void native_class_is_array(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_push(thread, (jvm_value){.i = 0});
}

static void native_class_is_interface(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_push(thread, (jvm_value){.i = 0});
}

static void native_class_is_primitive(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_push(thread, (jvm_value){.i = 0});
}

static void native_class_new_instance(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_push(thread, (jvm_value){.ref = NULL});
}

/* ============================================================
 * java.lang.Runtime
 * ============================================================ */

static void native_runtime_get_runtime(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_push(thread, (jvm_value){.ref = NULL});
}

static void native_runtime_free_memory(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_push(thread, (jvm_value){.i = 4 * 1024 * 1024});
}

static void native_runtime_total_memory(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_push(thread, (jvm_value){.i = 8 * 1024 * 1024});
}

static void native_runtime_gc(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {}

/* ============================================================
 * java.lang.Throwable
 * ============================================================ */

static void native_throwable_get_message(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_push(thread, (jvm_value){.ref = NULL});
}

static void native_throwable_print_stack_trace(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    printf("midp: stack trace (simulated)\n");
}

/* ============================================================
 * java.util.Vector
 * ============================================================ */

static void native_vector_init(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    printf("midp: Vector init\n");
}

static void native_vector_add_element(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_pop(thread);
}

static void native_vector_element_at(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_pop(thread);
    jvm_stack_push(thread, (jvm_value){.ref = NULL});
}

static void native_vector_size(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_push(thread, (jvm_value){.i = 0});
}

static void native_vector_remove_element(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_pop(thread);
}

static void native_vector_remove_element_at(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_pop(thread);
}

static void native_vector_is_empty(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_push(thread, (jvm_value){.i = 1});
}

static void native_vector_insert_element_at(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_pop(thread); jvm_stack_pop(thread);
}

/* ============================================================
 * java.util.Hashtable / Enumeration
 * ============================================================ */

static void native_hashtable_init(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {}
static void native_hashtable_put(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) { jvm_stack_pop(thread); jvm_stack_pop(thread); }
static void native_hashtable_get(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) { jvm_stack_pop(thread); jvm_stack_push(thread, (jvm_value){.ref = NULL}); }
static void native_hashtable_remove(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) { jvm_stack_pop(thread); }
static void native_hashtable_size(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) { jvm_stack_push(thread, (jvm_value){.i = 0}); }
static void native_hashtable_keys(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) { jvm_stack_push(thread, (jvm_value){.ref = NULL}); }
static void native_hashtable_elements(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) { jvm_stack_push(thread, (jvm_value){.ref = NULL}); }
static void native_enum_has_more_elements(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) { jvm_stack_push(thread, (jvm_value){.i = 0}); }
static void native_enum_next_element(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) { jvm_stack_push(thread, (jvm_value){.ref = NULL}); }

/* ============================================================
 * java.util.Stack
 * ============================================================ */

static void native_stack_push(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_pop(thread);
    jvm_stack_push(thread, (jvm_value){.ref = NULL});
}
static void native_stack_pop(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_push(thread, (jvm_value){.ref = NULL});
}
static void native_stack_peek(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_push(thread, (jvm_value){.ref = NULL});
}
static void native_stack_empty(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_push(thread, (jvm_value){.i = 1});
}
static void native_stack_search(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_pop(thread);
    jvm_stack_push(thread, (jvm_value){.i = -1});
}

/* ============================================================
 * java.util.Date
 * ============================================================ */

static void native_date_init(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {}
static void native_date_get_time(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) { jvm_stack_push(thread, (jvm_value){.i = 0}); }
static void native_date_set_time(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) { jvm_stack_pop(thread); }

/* ============================================================
 * java.util.Random
 * ============================================================ */

static void native_random_init(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_pop(thread);
}
static void native_random_next_int(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_push(thread, (jvm_value){.i = rand()});
}
static void native_random_next_int_bounded(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    s4 n = jvm_stack_pop(thread).i;
    if (n > 0) jvm_stack_push(thread, (jvm_value){.i = rand() % n});
    else jvm_stack_push(thread, (jvm_value){.i = 0});
}

/* ============================================================
 * java.io.ByteArrayOutputStream
 * ============================================================ */

static void native_baos_init(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {}
static void native_baos_write(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) { jvm_stack_pop(thread); }
static void native_baos_to_byte_array(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) { jvm_stack_push(thread, (jvm_value){.ref = NULL}); }
static void native_baos_size(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) { jvm_stack_push(thread, (jvm_value){.i = 0}); }
static void native_baos_reset(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {}

/* ============================================================
 * java.io.DataInputStream / DataOutputStream
 * ============================================================ */

static void native_data_in_read_int(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) { jvm_stack_push(thread, (jvm_value){.i = 0}); }
static void native_data_in_read_byte(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) { jvm_stack_push(thread, (jvm_value){.i = 0}); }
static void native_data_in_read_short(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) { jvm_stack_push(thread, (jvm_value){.i = 0}); }
static void native_data_in_read_long(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) { jvm_stack_push(thread, (jvm_value){.i = 0}); }
static void native_data_in_read_utf(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) { jvm_stack_push(thread, (jvm_value){.ref = NULL}); }
static void native_data_in_skip_bytes(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) { jvm_stack_pop(thread); }
static void native_data_in_available(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) { jvm_stack_push(thread, (jvm_value){.i = 0}); }
static void native_data_in_close(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {}
static void native_data_out_write_int(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) { jvm_stack_pop(thread); }
static void native_data_out_write_byte(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) { jvm_stack_pop(thread); }
static void native_data_out_write_short(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) { jvm_stack_pop(thread); }
static void native_data_out_write_long(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) { jvm_stack_pop(thread); }
static void native_data_out_write_utf(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) { jvm_stack_pop(thread); }
static void native_data_out_flush(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {}
static void native_data_out_close(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {}

/* ============================================================
 * java.io.InputStream / OutputStream
 * ============================================================ */

static void native_input_stream_read(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) { jvm_stack_push(thread, (jvm_value){.i = -1}); }
static void native_input_stream_skip(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) { jvm_stack_pop(thread); }
static void native_input_stream_available(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) { jvm_stack_push(thread, (jvm_value){.i = 0}); }
static void native_input_stream_close(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {}
static void native_output_stream_write(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) { jvm_stack_pop(thread); }
static void native_output_stream_flush(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {}
static void native_output_stream_close(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {}

/* ============================================================
 * javax.microedition.io.Connector
 * ============================================================ */

static void native_connector_open(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    char *url = (char*)jvm_stack_pop(thread).ref;
    printf("midp: Connector.open(%s)\n", url ? url : "null");
    jvm_stack_push(thread, (jvm_value){.ref = NULL});
}

static void native_connector_open_with_mode(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_pop(thread);
    char *url = (char*)jvm_stack_pop(thread).ref;
    printf("midp: Connector.open(%s, mode)\n", url ? url : "null");
    jvm_stack_push(thread, (jvm_value){.ref = NULL});
}

static void native_http_connection_open(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    char *url = (char*)jvm_stack_pop(thread).ref;
    printf("midp: HttpConnection open %s\n", url ? url : "null");
    jvm_stack_push(thread, (jvm_value){.i = 200});
}

static void native_http_connection_get_response_code(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_push(thread, (jvm_value){.i = 200});
}

static void native_http_connection_get_response_message(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_push(thread, (jvm_value){.ref = "OK"});
}

static void native_http_connection_get_header_field(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_pop(thread);
    jvm_stack_push(thread, (jvm_value){.ref = NULL});
}

static void native_http_connection_get_length(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_push(thread, (jvm_value){.i = 0});
}

static void native_http_connection_get_type(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_push(thread, (jvm_value){.ref = "application/octet-stream"});
}

static void native_http_connection_get_encoding(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_push(thread, (jvm_value){.ref = NULL});
}

static void native_http_connection_open_input_stream(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_push(thread, (jvm_value){.ref = NULL});
}

static void native_http_connection_open_data_input_stream(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_push(thread, (jvm_value){.ref = NULL});
}

static void native_http_connection_close(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    printf("midp: HttpConnection close\n");
}

static void native_socket_connection_open(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_push(thread, (jvm_value){.ref = NULL});
}

/* ============================================================
 * javax.microedition.io.file.FileConnection
 * ============================================================ */

static void native_file_connection_open(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_pop(thread);
    jvm_stack_push(thread, (jvm_value){.ref = NULL});
}

static void native_file_connection_exists(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_push(thread, (jvm_value){.i = 0});
}

static void native_file_connection_is_directory(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_push(thread, (jvm_value){.i = 0});
}

static void native_file_connection_list(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_push(thread, (jvm_value){.ref = NULL});
}

static void native_file_connection_file_size(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_push(thread, (jvm_value){.i = 0});
}

static void native_file_connection_can_read(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_push(thread, (jvm_value){.i = 1});
}

static void native_file_connection_can_write(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_push(thread, (jvm_value){.i = 1});
}

static void native_file_connection_create(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {}
static void native_file_connection_delete(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {}
static void native_file_connection_mkdir(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {}
static void native_file_connection_rename(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) { jvm_stack_pop(thread); }
static void native_file_connection_set_readable(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) { jvm_stack_pop(thread); }
static void native_file_connection_set_writable(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) { jvm_stack_pop(thread); }

/* ============================================================
 * java.lang.Thread
 * ============================================================ */

static void native_thread_current_thread(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_push(thread, (jvm_value){.ref = NULL});
}

static void native_thread_sleep(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    s4 ms = jvm_stack_pop(thread).i;
    sceKernelDelayThread(ms * 1000);
}

static void native_thread_yield(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    sceKernelDelayThread(1);
}

static void native_thread_start(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    printf("midp: Thread.start() - stub\n");
}

static void native_thread_init(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    printf("midp: Thread init\n");
}

static void native_thread_set_priority(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_pop(thread);
}

static void native_thread_get_name(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_push(thread, (jvm_value){.ref = ""});
}

static void native_thread_interrupt(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {}
static void native_thread_is_alive(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) { jvm_stack_push(thread, (jvm_value){.i = 1}); }

/* ============================================================
 * javax.microedition.media.Manager
 * ============================================================ */

/* Player handle table */
static audio_player *player_table[16];
static int player_table_count = 0;

static int player_alloc_handle(audio_player *p) {
    for (int i = 0; i < 16; i++) {
        if (!player_table[i]) {
            player_table[i] = p;
            return i + 1;
        }
    }
    return 0;
}

static audio_player *player_from_handle(int h) {
    if (h <= 0 || h > 16) return NULL;
    return player_table[h - 1];
}

static void player_free_handle(int h) {
    if (h > 0 && h <= 16) player_table[h - 1] = NULL;
}

/* Resolve a locator string to a local path.
   "file:///ux0:data/..." -> "ux0:data/..."
   "http://..." -> returns NULL (unsupported)
   bare paths returned as-is. */
static char *resolve_locator(const char *locator) {
    if (!locator) return NULL;
    if (strncmp(locator, "file://", 7) == 0) {
        return strdup(locator + 7);
    }
    if (strncmp(locator, "http://", 7) == 0 ||
        strncmp(locator, "https://", 8) == 0) {
        return NULL;
    }
    return strdup(locator);
}

/* ============================================================
 * javax.microedition.media.Manager
 * ============================================================ */

static void native_manager_play_tone(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    s4 volume = jvm_stack_pop(thread).i;
    s4 duration = jvm_stack_pop(thread).i;
    s4 note = jvm_stack_pop(thread).i;
    printf("midp: Manager.playTone(%d, %dms, vol=%d)\n", note, duration, volume);
    audio_play_tone(note, duration, volume);
}

static void native_manager_create_player(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    char *locator = (char*)jvm_stack_pop(thread).ref;
    printf("midp: Manager.createPlayer(%s)\n", locator ? locator : "null");

    if (!locator) {
        jvm_stack_push(thread, (jvm_value){.ref = NULL});
        return;
    }

    char *path = resolve_locator(locator);
    if (!path || !decoder_supports(path)) {
        printf("midp: unsupported locator\n");
        free(path);
        jvm_stack_push(thread, (jvm_value){.ref = NULL});
        return;
    }

    audio_player *player = audio_player_create(path);
    free(path);
    if (!player) {
        jvm_stack_push(thread, (jvm_value){.ref = NULL});
        return;
    }

    /* Auto-realize for file-based players */
    if (audio_player_realize(player) < 0) {
        audio_player_close(player);
        jvm_stack_push(thread, (jvm_value){.ref = NULL});
        return;
    }
    audio_player_prefetch(player);

    int handle = player_alloc_handle(player);
    if (!handle) {
        audio_player_close(player);
        jvm_stack_push(thread, (jvm_value){.ref = NULL});
        return;
    }

    jvm_stack_push(thread, (jvm_value){.ref = (void*)(uintptr_t)handle});
}

/* ============================================================
 * javax.microedition.media.Player
 * ============================================================ */

static audio_player *get_this_player(jvm_thread *thread) {
    void *ref = jvm_stack_pop(thread).ref;
    int h = (int)(uintptr_t)ref;
    return player_from_handle(h);
}

static void native_player_realize(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    audio_player *p = get_this_player(thread);
    if (p) audio_player_realize(p);
}

static void native_player_prefetch(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    audio_player *p = get_this_player(thread);
    if (p) audio_player_prefetch(p);
}

static void native_player_start(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    audio_player *p = get_this_player(thread);
    if (p) audio_player_start(p);
}

static void native_player_stop(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    audio_player *p = get_this_player(thread);
    if (p) audio_player_stop(p);
}

static void native_player_close(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    audio_player *p = get_this_player(thread);
    if (p) {
        int h = 0;
        for (int i = 0; i < 16; i++) {
            if (player_table[i] == p) { h = i + 1; break; }
        }
        audio_player_close(p);
        player_free_handle(h);
        audio_tone_stop();
    }
}

static void native_player_get_state(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    audio_player *p = get_this_player(thread);
    jvm_stack_push(thread, (jvm_value){.i = p ? (int)audio_player_get_state(p) : 4});
}

static void native_player_get_content_type(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    audio_player *p = get_this_player(thread);
    jvm_stack_push(thread, (jvm_value){.ref = (void*)audio_player_get_content_type(p)});
}

static void native_player_set_loop_count(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    s4 count = jvm_stack_pop(thread).i;
    audio_player *p = get_this_player(thread);
    if (p) audio_player_set_loop_count(p, count);
}

static void native_player_get_duration(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    audio_player *p = get_this_player(thread);
    jvm_stack_push(thread, (jvm_value){.i = p ? audio_player_get_duration(p) : 0});
}

static void native_player_get_position(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    audio_player *p = get_this_player(thread);
    jvm_stack_push(thread, (jvm_value){.i = p ? audio_player_get_position(p) : 0});
}

/* ============================================================
 * javax.microedition.media.control.ToneControl
 * ============================================================ */

static void native_tone_control_set_sequence(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    printf("midp: ToneControl.setSequence()\n");
}

/* ============================================================
 * javax.microedition.media.control.VolumeControl
 * ============================================================ */

static void native_volume_control_set_volume(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    s4 vol = jvm_stack_pop(thread).i;
    audio_set_volume(vol);
    jvm_stack_push(thread, (jvm_value){.i = audio_get_volume()});
}

static void native_volume_control_get_volume(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_push(thread, (jvm_value){.i = audio_get_volume()});
}

static void native_volume_control_is_muted(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_push(thread, (jvm_value){.i = 0});
}

static void native_volume_control_set_muted(JVM_UNUSED jvm_instance *jvm, jvm_thread *thread) {
    jvm_stack_pop(thread);
}

/* ============================================================
 * MIDP native registration table
 * ============================================================ */

void midp_init_natives(jvm_instance *jvm) {
    /* javax.microedition.midlet.MIDlet */
    jvm_register_native(jvm, "javax/microedition/midlet/MIDlet", "<init>", "()V", native_midlet_init);
    jvm_register_native(jvm, "javax/microedition/midlet/MIDlet", "startApp", "()V", native_midlet_start_app);
    jvm_register_native(jvm, "javax/microedition/midlet/MIDlet", "pauseApp", "()V", native_midlet_pause_app);
    jvm_register_native(jvm, "javax/microedition/midlet/MIDlet", "destroyApp", "(Z)V", native_midlet_destroy_app);
    jvm_register_native(jvm, "javax/microedition/midlet/MIDlet", "getAppProperty", "(Ljava/lang/String;)Ljava/lang/String;", native_midlet_get_app_property);
    jvm_register_native(jvm, "javax/microedition/midlet/MIDlet", "notifyDestroyed", "()V", native_midlet_notify_destroyed);
    jvm_register_native(jvm, "javax/microedition/midlet/MIDlet", "notifyPaused", "()V", native_midlet_notify_paused);

    /* javax.microedition.lcdui.Display */
    jvm_register_native(jvm, "javax/microedition/lcdui/Display", "getDisplay", "(Ljavax/microedition/midlet/MIDlet;)Ljavax/microedition/lcdui/Display;", native_display_get_display);
    jvm_register_native(jvm, "javax/microedition/lcdui/Display", "setCurrent", "(Ljavax/microedition/lcdui/Displayable;)V", native_display_set_current);
    jvm_register_native(jvm, "javax/microedition/lcdui/Display", "isColor", "()Z", native_display_is_color);
    jvm_register_native(jvm, "javax/microedition/lcdui/Display", "numColors", "()I", native_display_num_colors);
    jvm_register_native(jvm, "javax/microedition/lcdui/Display", "getWidth", "(Ljavax/microedition/lcdui/Display;)I", native_display_get_width);
    jvm_register_native(jvm, "javax/microedition/lcdui/Display", "getHeight", "(Ljavax/microedition/lcdui/Display;)I", native_display_get_height);

    /* javax.microedition.lcdui.Canvas */
    jvm_register_native(jvm, "javax/microedition/lcdui/Canvas", "getWidth", "(Ljavax/microedition/lcdui/Canvas;)I", native_canvas_get_width);
    jvm_register_native(jvm, "javax/microedition/lcdui/Canvas", "getHeight", "(Ljavax/microedition/lcdui/Canvas;)I", native_canvas_get_height);
    jvm_register_native(jvm, "javax/microedition/lcdui/Canvas", "repaint", "(IIII)V", native_canvas_repaint);
    jvm_register_native(jvm, "javax/microedition/lcdui/Canvas", "serviceRepaints", "()V", native_canvas_service_repaints);
    jvm_register_native(jvm, "javax/microedition/lcdui/Canvas", "hasPointerEvents", "()Z", native_canvas_has_pointer_events);
    jvm_register_native(jvm, "javax/microedition/lcdui/Canvas", "hasRepeatEvents", "()Z", native_canvas_has_repeat_events);

    /* javax.microedition.lcdui.Graphics */
    jvm_register_native(jvm, "javax/microedition/lcdui/Graphics", "setColor", "(I)V", native_graphics_set_color);
    jvm_register_native(jvm, "javax/microedition/lcdui/Graphics", "getColor", "()I", native_graphics_get_color);
    jvm_register_native(jvm, "javax/microedition/lcdui/Graphics", "drawLine", "(IIII)V", native_graphics_draw_line);
    jvm_register_native(jvm, "javax/microedition/lcdui/Graphics", "drawRect", "(IIII)V", native_graphics_draw_rect);
    jvm_register_native(jvm, "javax/microedition/lcdui/Graphics", "fillRect", "(IIII)V", native_graphics_fill_rect);
    jvm_register_native(jvm, "javax/microedition/lcdui/Graphics", "drawString", "(Ljava/lang/String;III)V", native_graphics_draw_string);
    jvm_register_native(jvm, "javax/microedition/lcdui/Graphics", "setFont", "(Ljavax/microedition/lcdui/Font;)V", native_graphics_set_font);
    jvm_register_native(jvm, "javax/microedition/lcdui/Graphics", "getFont", "()Ljavax/microedition/lcdui/Font;", native_graphics_get_font);
    jvm_register_native(jvm, "javax/microedition/lcdui/Graphics", "drawImage", "(Ljavax/microedition/lcdui/Image;III)V", native_graphics_draw_image);
    jvm_register_native(jvm, "javax/microedition/lcdui/Graphics", "translate", "(II)V", native_graphics_translate);
    jvm_register_native(jvm, "javax/microedition/lcdui/Graphics", "getTranslateX", "()I", native_graphics_get_translate_x);
    jvm_register_native(jvm, "javax/microedition/lcdui/Graphics", "getTranslateY", "()I", native_graphics_get_translate_y);
    jvm_register_native(jvm, "javax/microedition/lcdui/Graphics", "setClip", "(IIII)V", native_graphics_set_clip);
    jvm_register_native(jvm, "javax/microedition/lcdui/Graphics", "getClipX", "()I", native_graphics_get_clip_x);
    jvm_register_native(jvm, "javax/microedition/lcdui/Graphics", "getClipY", "()I", native_graphics_get_clip_y);
    jvm_register_native(jvm, "javax/microedition/lcdui/Graphics", "getClipWidth", "()I", native_graphics_get_clip_width);
    jvm_register_native(jvm, "javax/microedition/lcdui/Graphics", "getClipHeight", "()I", native_graphics_get_clip_height);

    /* javax.microedition.lcdui.Font */
    jvm_register_native(jvm, "javax/microedition/lcdui/Font", "getDefaultFont", "()Ljavax/microedition/lcdui/Font;", native_font_get_default_font);
    jvm_register_native(jvm, "javax/microedition/lcdui/Font", "getHeight", "()I", native_font_get_height);
    jvm_register_native(jvm, "javax/microedition/lcdui/Font", "stringWidth", "(Ljava/lang/String;)I", native_font_string_width);

    /* javax.microedition.lcdui.Image */
    jvm_register_native(jvm, "javax/microedition/lcdui/Image", "createImage", "(II)Ljavax/microedition/lcdui/Image;", native_image_create_image);
    jvm_register_native(jvm, "javax/microedition/lcdui/Image", "getWidth", "(Ljavax/microedition/lcdui/Image;)I", native_image_get_width);
    jvm_register_native(jvm, "javax/microedition/lcdui/Image", "getHeight", "(Ljavax/microedition/lcdui/Image;)I", native_image_get_height);
    jvm_register_native(jvm, "javax/microedition/lcdui/Image", "getGraphics", "(Ljavax/microedition/lcdui/Image;)Ljavax/microedition/lcdui/Graphics;", native_image_get_graphics);

    /* javax.microedition.lcdui.List */
    jvm_register_native(jvm, "javax/microedition/lcdui/List", "append", "(Ljava/lang/String;Ljavax/microedition/lcdui/Image;)I", native_list_append);
    jvm_register_native(jvm, "javax/microedition/lcdui/List", "delete", "(I)V", native_list_delete);
    jvm_register_native(jvm, "javax/microedition/lcdui/List", "getSelectedIndex", "()I", native_list_get_selected_index);
    jvm_register_native(jvm, "javax/microedition/lcdui/List", "setSelectCommand", "(Ljavax/microedition/lcdui/Command;)V", native_list_set_select_command);

    /* javax.microedition.lcdui.Alert */
    jvm_register_native(jvm, "javax/microedition/lcdui/Alert", "setString", "(Ljava/lang/String;)V", native_alert_set_string);

    /* javax.microedition.lcdui.TextBox */
    jvm_register_native(jvm, "javax/microedition/lcdui/TextBox", "setString", "(Ljava/lang/String;)V", native_textbox_set_string);
    jvm_register_native(jvm, "javax/microedition/lcdui/TextBox", "getString", "()Ljava/lang/String;", native_textbox_get_string);
    jvm_register_native(jvm, "javax/microedition/lcdui/TextBox", "getChars", "()[C", native_textbox_get_chars);

    /* javax.microedition.lcdui.Command */
    jvm_register_native(jvm, "javax/microedition/lcdui/Command", "<init>", "(Ljava/lang/String;II)V", native_command_init);

    /* javax.microedition.lcdui.Displayable */
    jvm_register_native(jvm, "javax/microedition/lcdui/Displayable", "addCommand", "(Ljavax/microedition/lcdui/Command;)V", native_displayable_add_command);
    jvm_register_native(jvm, "javax/microedition/lcdui/Displayable", "setCommandListener", "(Ljavax/microedition/lcdui/CommandListener;)V", native_displayable_set_command_listener);
    jvm_register_native(jvm, "javax/microedition/lcdui/Displayable", "isShown", "()Z", native_displayable_is_shown);
    jvm_register_native(jvm, "javax/microedition/lcdui/Displayable", "getTitle", "()Ljava/lang/String;", native_displayable_get_title);
    jvm_register_native(jvm, "javax/microedition/lcdui/Displayable", "setTitle", "(Ljava/lang/String;)V", native_displayable_set_title);

    /* java.lang.System */
    jvm_register_native(jvm, "java/lang/System", "currentTimeMillis", "()J", native_system_current_time_millis);
    jvm_register_native(jvm, "java/lang/System", "nanoTime", "()J", native_system_nano_time);
    jvm_register_native(jvm, "java/lang/System", "arraycopy", "(Ljava/lang/Object;ILjava/lang/Object;II)V", native_system_array_copy);
    jvm_register_native(jvm, "java/lang/System", "identityHashCode", "(Ljava/lang/Object;)I", native_system_identity_hash_code);

    /* java.lang.String */
    jvm_register_native(jvm, "java/lang/String", "intern", "()Ljava/lang/String;", native_string_intern);
    jvm_register_native(jvm, "java/lang/String", "getChars", "(II[CI)V", native_string_get_chars);
    jvm_register_native(jvm, "java/lang/String", "equals", "(Ljava/lang/Object;)Z", native_string_equals);
    jvm_register_native(jvm, "java/lang/String", "hashCode", "()I", native_string_hash_code);
    jvm_register_native(jvm, "java/lang/String", "length", "()I", native_string_length);
    jvm_register_native(jvm, "java/lang/String", "charAt", "(I)C", native_string_char_at);
    jvm_register_native(jvm, "java/lang/String", "substring", "(II)Ljava/lang/String;", native_string_substring);
    jvm_register_native(jvm, "java/lang/String", "trim", "()Ljava/lang/String;", native_string_trim);
    jvm_register_native(jvm, "java/lang/String", "toLowerCase", "()Ljava/lang/String;", native_string_to_lower_case);
    jvm_register_native(jvm, "java/lang/String", "toUpperCase", "()Ljava/lang/String;", native_string_to_upper_case);
    jvm_register_native(jvm, "java/lang/String", "concat", "(Ljava/lang/String;)Ljava/lang/String;", native_string_concat);
    jvm_register_native(jvm, "java/lang/String", "indexOf", "(II)I", native_string_index_of);

    /* java.lang.StringBuffer */
    jvm_register_native(jvm, "java/lang/StringBuffer", "<init>", "()V", native_string_buffer_init);
    jvm_register_native(jvm, "java/lang/StringBuffer", "append", "(Ljava/lang/String;)Ljava/lang/StringBuffer;", native_string_buffer_append);
    jvm_register_native(jvm, "java/lang/StringBuffer", "toString", "()Ljava/lang/String;", native_string_buffer_to_string);

    /* java.lang.Integer */
    jvm_register_native(jvm, "java/lang/Integer", "intValue", "()I", native_integer_int_value);
    jvm_register_native(jvm, "java/lang/Integer", "valueOf", "(I)Ljava/lang/Integer;", native_integer_value_of);
    jvm_register_native(jvm, "java/lang/Integer", "parseInt", "(Ljava/lang/String;)I", native_integer_parse_int);
    jvm_register_native(jvm, "java/lang/Integer", "toHexString", "(I)Ljava/lang/String;", native_integer_to_hex_string);
    jvm_register_native(jvm, "java/lang/Long", "valueOf", "(J)Ljava/lang/Long;", native_long_value_of);
    jvm_register_native(jvm, "java/lang/Float", "floatValue", "()F", native_float_float_value);
    jvm_register_native(jvm, "java/lang/Boolean", "valueOf", "(Z)Ljava/lang/Boolean;", native_boolean_value_of);

    /* java.lang.Math */
    jvm_register_native(jvm, "java/lang/Math", "min", "(II)I", native_math_min);
    jvm_register_native(jvm, "java/lang/Math", "max", "(II)I", native_math_max);
    jvm_register_native(jvm, "java/lang/Math", "abs", "(I)I", native_math_abs);
    jvm_register_native(jvm, "java/lang/Math", "sqrt", "(D)D", native_math_sqrt);

    /* java.lang.Object */
    jvm_register_native(jvm, "java/lang/Object", "getClass", "()Ljava/lang/Class;", native_object_get_class);
    jvm_register_native(jvm, "java/lang/Object", "toString", "()Ljava/lang/String;", native_object_to_string);
    jvm_register_native(jvm, "java/lang/Object", "hashCode", "()I", native_object_hash_code);
    jvm_register_native(jvm, "java/lang/Object", "equals", "(Ljava/lang/Object;)Z", native_object_equals);
    jvm_register_native(jvm, "java/lang/Object", "notify", "()V", native_object_notify);
    jvm_register_native(jvm, "java/lang/Object", "notifyAll", "()V", native_object_notify_all);
    jvm_register_native(jvm, "java/lang/Object", "wait", "(J)V", native_object_wait);

    /* java.lang.Class */
    jvm_register_native(jvm, "java/lang/Class", "forName", "(Ljava/lang/String;)Ljava/lang/Class;", native_class_for_name);
    jvm_register_native(jvm, "java/lang/Class", "getName", "()Ljava/lang/String;", native_class_get_name);
    jvm_register_native(jvm, "java/lang/Class", "getSuperclass", "()Ljava/lang/Class;", native_class_get_superclass);
    jvm_register_native(jvm, "java/lang/Class", "isArray", "()Z", native_class_is_array);
    jvm_register_native(jvm, "java/lang/Class", "isInterface", "()Z", native_class_is_interface);
    jvm_register_native(jvm, "java/lang/Class", "isPrimitive", "()Z", native_class_is_primitive);
    jvm_register_native(jvm, "java/lang/Class", "newInstance", "()Ljava/lang/Object;", native_class_new_instance);

    /* java.lang.Runtime */
    jvm_register_native(jvm, "java/lang/Runtime", "getRuntime", "()Ljava/lang/Runtime;", native_runtime_get_runtime);
    jvm_register_native(jvm, "java/lang/Runtime", "freeMemory", "()J", native_runtime_free_memory);
    jvm_register_native(jvm, "java/lang/Runtime", "totalMemory", "()J", native_runtime_total_memory);
    jvm_register_native(jvm, "java/lang/Runtime", "gc", "()V", native_runtime_gc);

    /* java.lang.Throwable */
    jvm_register_native(jvm, "java/lang/Throwable", "getMessage", "()Ljava/lang/String;", native_throwable_get_message);
    jvm_register_native(jvm, "java/lang/Throwable", "printStackTrace", "()V", native_throwable_print_stack_trace);

    /* java.util.Vector */
    jvm_register_native(jvm, "java/util/Vector", "<init>", "(II)V", native_vector_init);
    jvm_register_native(jvm, "java/util/Vector", "addElement", "(Ljava/lang/Object;)V", native_vector_add_element);
    jvm_register_native(jvm, "java/util/Vector", "elementAt", "(I)Ljava/lang/Object;", native_vector_element_at);
    jvm_register_native(jvm, "java/util/Vector", "size", "()I", native_vector_size);
    jvm_register_native(jvm, "java/util/Vector", "removeElement", "(Ljava/lang/Object;)Z", native_vector_remove_element);
    jvm_register_native(jvm, "java/util/Vector", "removeElementAt", "(I)V", native_vector_remove_element_at);
    jvm_register_native(jvm, "java/util/Vector", "isEmpty", "()Z", native_vector_is_empty);
    jvm_register_native(jvm, "java/util/Vector", "insertElementAt", "(Ljava/lang/Object;I)V", native_vector_insert_element_at);

    /* java.util.Hashtable */
    jvm_register_native(jvm, "java/util/Hashtable", "<init>", "()V", native_hashtable_init);
    jvm_register_native(jvm, "java/util/Hashtable", "put", "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;", native_hashtable_put);
    jvm_register_native(jvm, "java/util/Hashtable", "get", "(Ljava/lang/Object;)Ljava/lang/Object;", native_hashtable_get);
    jvm_register_native(jvm, "java/util/Hashtable", "remove", "(Ljava/lang/Object;)Ljava/lang/Object;", native_hashtable_remove);
    jvm_register_native(jvm, "java/util/Hashtable", "size", "()I", native_hashtable_size);
    jvm_register_native(jvm, "java/util/Hashtable", "keys", "()Ljava/util/Enumeration;", native_hashtable_keys);
    jvm_register_native(jvm, "java/util/Hashtable", "elements", "()Ljava/util/Enumeration;", native_hashtable_elements);
    jvm_register_native(jvm, "java/util/Enumeration", "hasMoreElements", "()Z", native_enum_has_more_elements);
    jvm_register_native(jvm, "java/util/Enumeration", "nextElement", "()Ljava/lang/Object;", native_enum_next_element);

    /* java.util.Stack */
    jvm_register_native(jvm, "java/util/Stack", "push", "(Ljava/lang/Object;)Ljava/lang/Object;", native_stack_push);
    jvm_register_native(jvm, "java/util/Stack", "pop", "()Ljava/lang/Object;", native_stack_pop);
    jvm_register_native(jvm, "java/util/Stack", "peek", "()Ljava/lang/Object;", native_stack_peek);
    jvm_register_native(jvm, "java/util/Stack", "empty", "()Z", native_stack_empty);
    jvm_register_native(jvm, "java/util/Stack", "search", "(Ljava/lang/Object;)I", native_stack_search);

    /* java.util.Date */
    jvm_register_native(jvm, "java/util/Date", "<init>", "()V", native_date_init);
    jvm_register_native(jvm, "java/util/Date", "getTime", "()J", native_date_get_time);
    jvm_register_native(jvm, "java/util/Date", "setTime", "(J)V", native_date_set_time);

    /* java.util.Random */
    jvm_register_native(jvm, "java/util/Random", "<init>", "(J)V", native_random_init);
    jvm_register_native(jvm, "java/util/Random", "nextInt", "()I", native_random_next_int);
    jvm_register_native(jvm, "java/util/Random", "nextInt", "(I)I", native_random_next_int_bounded);

    /* java.io.ByteArrayOutputStream */
    jvm_register_native(jvm, "java/io/ByteArrayOutputStream", "<init>", "()V", native_baos_init);
    jvm_register_native(jvm, "java/io/ByteArrayOutputStream", "write", "(I)V", native_baos_write);
    jvm_register_native(jvm, "java/io/ByteArrayOutputStream", "toByteArray", "()[B", native_baos_to_byte_array);
    jvm_register_native(jvm, "java/io/ByteArrayOutputStream", "size", "()I", native_baos_size);
    jvm_register_native(jvm, "java/io/ByteArrayOutputStream", "reset", "()V", native_baos_reset);

    /* java.io.DataInputStream */
    jvm_register_native(jvm, "java/io/DataInputStream", "readInt", "()I", native_data_in_read_int);
    jvm_register_native(jvm, "java/io/DataInputStream", "readByte", "()B", native_data_in_read_byte);
    jvm_register_native(jvm, "java/io/DataInputStream", "readShort", "()S", native_data_in_read_short);
    jvm_register_native(jvm, "java/io/DataInputStream", "readLong", "()J", native_data_in_read_long);
    jvm_register_native(jvm, "java/io/DataInputStream", "readUTF", "()Ljava/lang/String;", native_data_in_read_utf);
    jvm_register_native(jvm, "java/io/DataInputStream", "skipBytes", "(I)I", native_data_in_skip_bytes);
    jvm_register_native(jvm, "java/io/DataInputStream", "available", "()I", native_data_in_available);
    jvm_register_native(jvm, "java/io/DataInputStream", "close", "()V", native_data_in_close);

    /* java.io.DataOutputStream */
    jvm_register_native(jvm, "java/io/DataOutputStream", "writeInt", "(I)V", native_data_out_write_int);
    jvm_register_native(jvm, "java/io/DataOutputStream", "writeByte", "(I)V", native_data_out_write_byte);
    jvm_register_native(jvm, "java/io/DataOutputStream", "writeShort", "(I)V", native_data_out_write_short);
    jvm_register_native(jvm, "java/io/DataOutputStream", "writeLong", "(J)V", native_data_out_write_long);
    jvm_register_native(jvm, "java/io/DataOutputStream", "writeUTF", "(Ljava/lang/String;)V", native_data_out_write_utf);
    jvm_register_native(jvm, "java/io/DataOutputStream", "flush", "()V", native_data_out_flush);
    jvm_register_native(jvm, "java/io/DataOutputStream", "close", "()V", native_data_out_close);

    /* java.io.InputStream */
    jvm_register_native(jvm, "java/io/InputStream", "read", "()I", native_input_stream_read);
    jvm_register_native(jvm, "java/io/InputStream", "skip", "(J)J", native_input_stream_skip);
    jvm_register_native(jvm, "java/io/InputStream", "available", "()I", native_input_stream_available);
    jvm_register_native(jvm, "java/io/InputStream", "close", "()V", native_input_stream_close);

    /* java.io.OutputStream */
    jvm_register_native(jvm, "java/io/OutputStream", "write", "(I)V", native_output_stream_write);
    jvm_register_native(jvm, "java/io/OutputStream", "flush", "()V", native_output_stream_flush);
    jvm_register_native(jvm, "java/io/OutputStream", "close", "()V", native_output_stream_close);

    /* javax.microedition.io.Connector */
    jvm_register_native(jvm, "javax/microedition/io/Connector", "open", "(Ljava/lang/String;)Ljavax/microedition/io/Connection;", native_connector_open);
    jvm_register_native(jvm, "javax/microedition/io/Connector", "open", "(Ljava/lang/String;I)Ljavax/microedition/io/Connection;", native_connector_open_with_mode);

    /* javax.microedition.io.HttpConnection */
    jvm_register_native(jvm, "javax/microedition/io/HttpConnection", "open", "(Ljava/lang/String;)Ljavax/microedition/io/HttpConnection;", native_http_connection_open);
    jvm_register_native(jvm, "javax/microedition/io/HttpConnection", "getResponseCode", "()I", native_http_connection_get_response_code);
    jvm_register_native(jvm, "javax/microedition/io/HttpConnection", "getResponseMessage", "()Ljava/lang/String;", native_http_connection_get_response_message);
    jvm_register_native(jvm, "javax/microedition/io/HttpConnection", "getHeaderField", "(Ljava/lang/String;)Ljava/lang/String;", native_http_connection_get_header_field);
    jvm_register_native(jvm, "javax/microedition/io/HttpConnection", "getLength", "()J", native_http_connection_get_length);
    jvm_register_native(jvm, "javax/microedition/io/HttpConnection", "getType", "()Ljava/lang/String;", native_http_connection_get_type);
    jvm_register_native(jvm, "javax/microedition/io/HttpConnection", "getEncoding", "()Ljava/lang/String;", native_http_connection_get_encoding);
    jvm_register_native(jvm, "javax/microedition/io/HttpConnection", "openInputStream", "()Ljava/io/InputStream;", native_http_connection_open_input_stream);
    jvm_register_native(jvm, "javax/microedition/io/HttpConnection", "openDataInputStream", "()Ljava/io/DataInputStream;", native_http_connection_open_data_input_stream);
    jvm_register_native(jvm, "javax/microedition/io/HttpConnection", "close", "()V", native_http_connection_close);

    /* javax.microedition.io.SocketConnection (simplified as HttpConnection fallback) */
    jvm_register_native(jvm, "javax/microedition/io/SocketConnection", "open", "(Ljava/lang/String;)Ljavax/microedition/io/SocketConnection;", native_socket_connection_open);

    /* javax.microedition.io.file.FileConnection */
    jvm_register_native(jvm, "javax/microedition/io/file/FileConnection", "open", "(Ljava/lang/String;)Ljavax/microedition/io/file/FileConnection;", native_file_connection_open);
    jvm_register_native(jvm, "javax/microedition/io/file/FileConnection", "exists", "()Z", native_file_connection_exists);
    jvm_register_native(jvm, "javax/microedition/io/file/FileConnection", "isDirectory", "()Z", native_file_connection_is_directory);
    jvm_register_native(jvm, "javax/microedition/io/file/FileConnection", "list", "(Z)[Ljava/lang/String;", native_file_connection_list);
    jvm_register_native(jvm, "javax/microedition/io/file/FileConnection", "fileSize", "()J", native_file_connection_file_size);
    jvm_register_native(jvm, "javax/microedition/io/file/FileConnection", "canRead", "()Z", native_file_connection_can_read);
    jvm_register_native(jvm, "javax/microedition/io/file/FileConnection", "canWrite", "()Z", native_file_connection_can_write);
    jvm_register_native(jvm, "javax/microedition/io/file/FileConnection", "create", "()V", native_file_connection_create);
    jvm_register_native(jvm, "javax/microedition/io/file/FileConnection", "delete", "()V", native_file_connection_delete);
    jvm_register_native(jvm, "javax/microedition/io/file/FileConnection", "mkdir", "()V", native_file_connection_mkdir);
    jvm_register_native(jvm, "javax/microedition/io/file/FileConnection", "rename", "(Ljava/lang/String;)V", native_file_connection_rename);
    jvm_register_native(jvm, "javax/microedition/io/file/FileConnection", "setReadable", "(Z)V", native_file_connection_set_readable);
    jvm_register_native(jvm, "javax/microedition/io/file/FileConnection", "setWritable", "(Z)V", native_file_connection_set_writable);

    /* java.lang.Thread */
    jvm_register_native(jvm, "java/lang/Thread", "currentThread", "()Ljava/lang/Thread;", native_thread_current_thread);
    jvm_register_native(jvm, "java/lang/Thread", "sleep", "(J)V", native_thread_sleep);
    jvm_register_native(jvm, "java/lang/Thread", "yield", "()V", native_thread_yield);
    jvm_register_native(jvm, "java/lang/Thread", "start", "()V", native_thread_start);
    jvm_register_native(jvm, "java/lang/Thread", "<init>", "()V", native_thread_init);
    jvm_register_native(jvm, "java/lang/Thread", "setPriority", "(I)V", native_thread_set_priority);
    jvm_register_native(jvm, "java/lang/Thread", "getName", "()Ljava/lang/String;", native_thread_get_name);
    jvm_register_native(jvm, "java/lang/Thread", "interrupt", "()V", native_thread_interrupt);
    jvm_register_native(jvm, "java/lang/Thread", "isAlive", "()Z", native_thread_is_alive);

    /* javax.microedition.media.Manager */
    jvm_register_native(jvm, "javax/microedition/media/Manager", "playTone", "(III)V", native_manager_play_tone);
    jvm_register_native(jvm, "javax/microedition/media/Manager", "createPlayer", "(Ljava/lang/String;)Ljavax/microedition/media/Player;", native_manager_create_player);

    /* javax.microedition.media.Player */
    jvm_register_native(jvm, "javax/microedition/media/Player", "realize", "()V", native_player_realize);
    jvm_register_native(jvm, "javax/microedition/media/Player", "prefetch", "()V", native_player_prefetch);
    jvm_register_native(jvm, "javax/microedition/media/Player", "start", "()V", native_player_start);
    jvm_register_native(jvm, "javax/microedition/media/Player", "stop", "()V", native_player_stop);
    jvm_register_native(jvm, "javax/microedition/media/Player", "close", "()V", native_player_close);
    jvm_register_native(jvm, "javax/microedition/media/Player", "getState", "()I", native_player_get_state);
    jvm_register_native(jvm, "javax/microedition/media/Player", "getContentType", "()Ljava/lang/String;", native_player_get_content_type);
    jvm_register_native(jvm, "javax/microedition/media/Player", "setLoopCount", "(I)V", native_player_set_loop_count);
    jvm_register_native(jvm, "javax/microedition/media/Player", "getDuration", "()J", native_player_get_duration);
    jvm_register_native(jvm, "javax/microedition/media/Player", "getMediaTime", "()J", native_player_get_position);

    /* javax.microedition.media.control.ToneControl */
    jvm_register_native(jvm, "javax/microedition/media/control/ToneControl", "setSequence", "([B)V", native_tone_control_set_sequence);

    /* javax.microedition.media.control.VolumeControl */
    jvm_register_native(jvm, "javax/microedition/media/control/VolumeControl", "setLevel", "(I)I", native_volume_control_set_volume);
    jvm_register_native(jvm, "javax/microedition/media/control/VolumeControl", "getLevel", "()I", native_volume_control_get_volume);
    jvm_register_native(jvm, "javax/microedition/media/control/VolumeControl", "isMuted", "()Z", native_volume_control_is_muted);
    jvm_register_native(jvm, "javax/microedition/media/control/VolumeControl", "setMuted", "(Z)V", native_volume_control_set_muted);
}
