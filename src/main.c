#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/ctrl.h>
#include <psp2/touch.h>
#include <psp2/types.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "launcher.h"
#include "jad_parser.h"
#include "jvm_core.h"
#include "midp_api.h"
#include "graphics.h"
#include "input.h"
#include "fileio.h"
#include "network.h"
#include "audio.h"

static jvm_instance *g_jvm = NULL;

/* Aytum Launcher colors */
#define COL_BG       0x0D0D12
#define COL_GOLD     0xC8A027
#define COL_GOLD_DIM 0x8A6F1A
#define COL_WHITE    0xE8E8E8
#define COL_TEXT     0xC0C0C8
#define COL_DIM      0x707078
#define COL_MUTED    0x505058

/* Splashscreen colors */
#define COL_SPLASH_BG  0x0A0A0F
#define COL_SPLASH_GOLD 0xC8A027
#define COL_SPLASH_DIM 0x707078

static void draw_splashscreen(void) {
    lcdui_clear(COL_SPLASH_BG);
    lcdui_begin_frame();

    lcdui_set_color(COL_SPLASH_GOLD);
    lcdui_draw_string("AYTUM", 480, 180, ANCHOR_HCENTER | ANCHOR_TOP);
    lcdui_set_color(0x8A6F1A);
    lcdui_draw_string("JAVA LAUNCHER", 480, 220, ANCHOR_HCENTER | ANCHOR_TOP);

    lcdui_set_color(0x1E1A0E);
    lcdui_fill_rect(340, 248, 280, 1);

    lcdui_set_color(COL_SPLASH_DIM);
    lcdui_draw_string("Java ME Emulator for PS Vita", 480, 270, ANCHOR_HCENTER | ANCHOR_TOP);

    lcdui_set_color(0x505058);
    lcdui_draw_string("Developed using DeepSeek V4", 480, 300, ANCHOR_HCENTER | ANCHOR_TOP);

    lcdui_set_color(0x404048);
    lcdui_draw_string("v1.0", 480, 340, ANCHOR_HCENTER | ANCHOR_TOP);

    lcdui_end_frame();
    sceKernelDelayThread(2 * 1000 * 1000);
}

static int load_jar_into_jvm(jvm_instance *jvm, const char *jar_path) {
    size_t jar_size;
    uint8_t *jar_data = fileio_read_file(jar_path, &jar_size);
    if (!jar_data) {
        printf("main: failed to read JAR: %s\n", jar_path);
        return -1;
    }

    const uint8_t *p = jar_data;
    const uint8_t *end = jar_data + jar_size;
    int class_count = 0;

    while (p + 30 <= end) {
        uint32_t sig = (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
                       ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);

        if (sig == 0x04034b50) {
            uint16_t compression = (uint16_t)p[8] | ((uint16_t)p[9] << 8);
            uint16_t name_len    = (uint16_t)p[26] | ((uint16_t)p[27] << 8);
            uint16_t extra_len   = (uint16_t)p[28] | ((uint16_t)p[29] << 8);
            uint32_t comp_size   = (uint32_t)p[18] | ((uint32_t)p[19] << 8) |
                                   ((uint32_t)p[20] << 16) | ((uint32_t)p[21] << 24);
            uint32_t uncomp_size = (uint32_t)p[22] | ((uint32_t)p[23] << 8) |
                                   ((uint32_t)p[24] << 16) | ((uint32_t)p[25] << 24);
            const uint8_t *name_ptr = p + 30;
            const uint8_t *data_ptr = name_ptr + name_len + extra_len;

            /* Check .class extension */
            if (name_len >= 6 &&
                name_ptr[name_len - 1] == 's' &&
                name_ptr[name_len - 2] == 's' &&
                name_ptr[name_len - 3] == 'a' &&
                name_ptr[name_len - 4] == 'l' &&
                name_ptr[name_len - 5] == 'c' &&
                name_ptr[name_len - 6] == '.')
            {
                if (compression == 0 && data_ptr + uncomp_size <= end) {
                    (void)uncomp_size;
                    if (jvm_load_class(jvm, data_ptr, uncomp_size)) {
                        class_count++;
                        printf("main: loaded %.*s\n", (int)name_len, name_ptr);
                    }
                } else if (compression == 8) {
                    printf("main: skipping compressed %.*s\n", (int)name_len, name_ptr);
                }
            }

            uint32_t total = 30 + name_len + extra_len + comp_size;
            p += total;
        } else if (sig == 0x02014b50) {
            uint16_t name_len2 = (uint16_t)p[28] | ((uint16_t)p[29] << 8);
            uint16_t extra_len2 = (uint16_t)p[30] | ((uint16_t)p[31] << 8);
            uint16_t comment_len = (uint16_t)p[32] | ((uint16_t)p[33] << 8);
            p += 46 + name_len2 + extra_len2 + comment_len;
        } else if (sig == 0x06054b50) {
            break;
        } else {
            break;
        }
    }

    free(jar_data);
    printf("main: loaded %d classes from %s\n", class_count, jar_path);
    return class_count;
}

static void run_midlet(const char *jar_path, jad_info *info) {
    printf("main: launching %s (%s)\n", info->midlet_name, jar_path);
    printf("main: MIDlet class: %s\n", info->midlet_class);

    g_jvm = jvm_create();
    if (!g_jvm) {
        printf("main: JVM creation failed\n");
        return;
    }

    int loaded = load_jar_into_jvm(g_jvm, jar_path);
    if (loaded == 0) {
        printf("main: no classes loaded\n");
        jvm_destroy(g_jvm);
        g_jvm = NULL;
        return;
    }

    if (info->midlet_class[0])
        jvm_run(g_jvm, info->midlet_class);

    int running = 1;
    while (running) {
        input_process();
        audio_update();

        input_event events[16];
        int ev_count = input_poll(events, 16);

        for (int i = 0; i < ev_count; i++) {
            if (events[i].type == INPUT_EVENT_KEY_PRESSED) {
                if (events[i].key_code == KEY_SOFT3) {
                    audio_tone_stop();
                    running = 0;
                    break;
                }
            }
        }

        lcdui_clear(COL_BG);
        lcdui_begin_frame();

        if (midp_repaint_requested()) {
            midp_clear_repaint_requested();
            /* MIDlet requested a repaint via Canvas.repaint().
               The MIDlet's own Graphics calls have already drawn to the
               backbuffer during jvm execution. Just clear and swap. */
        }

        /* Play button indicator */
        lcdui_set_color(COL_GOLD);
        lcdui_draw_string("\xE2\x96\xB6", 36, 30, ANCHOR_LEFT | ANCHOR_TOP);

        /* MIDlet name */
        lcdui_set_color(COL_WHITE);
        lcdui_draw_string(info->midlet_name, 68, 32, ANCHOR_LEFT | ANCHOR_TOP);

        /* Status line */
        lcdui_set_color(COL_DIM);
        lcdui_draw_string("Now Playing  |  Java ME MIDlet", 68, 54, ANCHOR_LEFT | ANCHOR_TOP);

        lcdui_set_color(COL_MUTED);
        lcdui_draw_line(36, 78, 924, 78);

        /* Exit hint */
        lcdui_set_color(COL_MUTED);
        lcdui_draw_string("Triangle: Exit to Launcher", 480, 510, ANCHOR_HCENTER | ANCHOR_TOP);

        lcdui_end_frame();
        sceKernelDelayThread(16 * 1000);
    }

    jvm_destroy(g_jvm);
    g_jvm = NULL;
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    if (lcdui_init() < 0) { sceKernelExitProcess(0); return 0; }
    if (input_init() < 0)  { lcdui_shutdown(); sceKernelExitProcess(0); return 0; }
    if (fileio_init() < 0) { input_shutdown(); lcdui_shutdown(); sceKernelExitProcess(0); return 0; }

    if (network_init() < 0)
        printf("main: network init failed (continuing)\n");

    if (audio_init() < 0)
        printf("main: audio init failed (continuing without sound)\n");

    printf("main: Vita Java ME Emulator initialized\n");
    printf("main: scanning %s\n", fileio_get_base_path());

    if (launcher_init() < 0)
        printf("main: launcher init failed\n");

    printf("main: found %d app(s)\n", launcher_get_count());

    draw_splashscreen();

    int running = 1;
    while (running) {
        const char *selected_path = NULL;
        jad_info selected_info;

        switch (launcher_run(&selected_path, &selected_info)) {
            case LAUNCHER_RESULT_LAUNCH:
                if (selected_path) {
                    run_midlet(selected_path, &selected_info);
                    jad_free(&selected_info);
                }
                break;
            case LAUNCHER_RESULT_REFRESH:
                launcher_scan();
                break;
            case LAUNCHER_RESULT_QUIT:
                running = 0;
                break;
            default:
                break;
        }
    }

    launcher_shutdown();
    audio_shutdown();
    network_shutdown();
    fileio_shutdown();
    input_shutdown();
    lcdui_shutdown();

    printf("main: shutdown complete\n");
    sceKernelExitProcess(0);
    return 0;
}
