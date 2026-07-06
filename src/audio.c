#include "audio.h"
#include "decoder.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/audioout.h>
#include <psp2/types.h>

#define PI 3.14159265358979323846
#define MAX_PLAYERS 4

static int audio_port = -1;
static int audio_volume = 80;

/* Tone state */
static int    tone_active = 0;
static double tone_phase = 0.0;
static double tone_freq = 440.0;
static int    tone_vol = 80;

/* Audio buffer for output */
static int16_t audio_buffer[AUDIO_BUFFER_SIZE * AUDIO_CHANNELS];

/* Note frequency table */
static double note_freq[128];
static int freq_table_built = 0;

/* Player state */
struct audio_player {
    int            used;
    player_state   state;
    audio_decoder *decoder;
    audio_format   fmt;
    char           path[512];
    int            loop_count;
    int            loop_remaining;
    int            position_ms;
    int            duration_ms;
    int            started;
};

static audio_player players[MAX_PLAYERS];

double audio_note_to_freq(int note) {
    if (!freq_table_built) {
        for (int i = 0; i < 128; i++)
            note_freq[i] = 440.0 * pow(2.0, (i - 69.0) / 12.0);
        freq_table_built = 1;
    }
    if (note < 0 || note > 127) return 440.0;
    return note_freq[note];
}

int audio_init(void) {
    audio_note_to_freq(0); /* Build table */
    memset(players, 0, sizeof(players));

    audio_port = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_BGM,
                                     AUDIO_BUFFER_SIZE,
                                     AUDIO_SAMPLE_RATE,
                                     SCE_AUDIO_OUT_MODE_STEREO);
    if (audio_port < 0) {
        printf("audio: sceAudioOutOpenPort failed (0x%08X)\n", audio_port);
        audio_port = -1;
        return -1;
    }

    int vol = (audio_volume * 32767) / 100;
    sceAudioOutSetVolume(audio_port,
                         SCE_AUDIO_VOLUME_FLAG_L_CH | SCE_AUDIO_VOLUME_FLAG_R_CH,
                         &vol);
    printf("audio: initialized (port=%d, %dHz)\n", audio_port, AUDIO_SAMPLE_RATE);
    return 0;
}

void audio_shutdown(void) {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (players[i].used)
            audio_player_close(&players[i]);
    }
    if (audio_port >= 0) {
        sceAudioOutReleasePort(audio_port);
        audio_port = -1;
    }
}

void audio_update(void) {
    if (audio_port < 0) return;

    /* Check if any player needs mixing */
    int has_active_player = 0;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (players[i].used && players[i].state == PLAYER_STATE_STARTED) {
            has_active_player = 1;
            break;
        }
    }

    if (!has_active_player && !tone_active) return;

    int16_t mix_buf[AUDIO_BUFFER_SIZE * AUDIO_CHANNELS];
    memset(mix_buf, 0, sizeof(mix_buf));

    /* Mix active players */
    for (int i = 0; i < MAX_PLAYERS; i++) {
        audio_player *p = &players[i];
        if (!p->used || p->state != PLAYER_STATE_STARTED) continue;

        int frames = decoder_read_pcm(p->decoder, mix_buf, AUDIO_BUFFER_SIZE);
        if (frames <= 0) {
            if (p->loop_remaining > 0) {
                p->loop_remaining--;
                decoder_seek(p->decoder, 0.0);
                frames = decoder_read_pcm(p->decoder, mix_buf, AUDIO_BUFFER_SIZE);
            } else {
                p->state = PLAYER_STATE_PREFETCHED;
                continue;
            }
        }
        p->position_ms += (frames * 1000) / p->fmt.sample_rate;
    }

    /* Mix tone if active */
    if (tone_active) {
        double amp = (tone_vol / 100.0) * 20000.0;
        double phase_inc = (2.0 * PI * tone_freq) / AUDIO_SAMPLE_RATE;
        for (int i = 0; i < AUDIO_BUFFER_SIZE; i++) {
            int16_t s = (int16_t)(sin(tone_phase) * amp);
            mix_buf[i * 2]     += s;
            mix_buf[i * 2 + 1] += s;
            tone_phase += phase_inc;
            if (tone_phase >= 2.0 * PI) tone_phase -= 2.0 * PI;
        }
    }

    sceAudioOutOutput(audio_port, mix_buf);
}

/* Tone API */
void audio_play_tone(int note, int duration_ms, int volume) {
    tone_active = 1;
    tone_freq = audio_note_to_freq(note);
    tone_vol = (volume > 100) ? 100 : (volume < 0 ? 0 : volume);
    tone_phase = 0.0;

    /* Schedule stop after duration */
    int iterations = (duration_ms * AUDIO_SAMPLE_RATE) / (1000 * AUDIO_BUFFER_SIZE);
    for (int i = 0; i < iterations; i++) {
        audio_update();
    }
    tone_active = 0;
}

void audio_tone_start(int note, int volume) {
    tone_active = 1;
    tone_freq = audio_note_to_freq(note);
    tone_vol = (volume > 100) ? 100 : (volume < 0 ? 0 : volume);
    tone_phase = 0.0;
}

void audio_tone_stop(void) {
    tone_active = 0;
}

int audio_is_playing(void) {
    if (tone_active) return 1;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (players[i].used && players[i].state == PLAYER_STATE_STARTED)
            return 1;
    }
    return 0;
}

void audio_set_volume(int volume) {
    audio_volume = (volume > 100) ? 100 : (volume < 0 ? 0 : volume);
    if (audio_port >= 0) {
        int vol = (audio_volume * 32767) / 100;
        sceAudioOutSetVolume(audio_port,
                             SCE_AUDIO_VOLUME_FLAG_L_CH | SCE_AUDIO_VOLUME_FLAG_R_CH,
                             &vol);
    }
}

int audio_get_volume(void) { return audio_volume; }

/* Streaming player API */
audio_player *audio_player_create(const char *path) {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (!players[i].used) {
            audio_player *p = &players[i];
            memset(p, 0, sizeof(audio_player));
            p->used = 1;
            p->state = PLAYER_STATE_UNREALIZED;
            strncpy(p->path, path ? path : "", sizeof(p->path) - 1);
            return p;
        }
    }
    return NULL;
}

player_state audio_player_get_state(audio_player *p) {
    return p ? p->state : PLAYER_STATE_CLOSED;
}

int audio_player_realize(audio_player *p) {
    if (!p || p->state != PLAYER_STATE_UNREALIZED) return -1;
    p->decoder = decoder_open(p->path);
    if (!p->decoder) {
        printf("audio: failed to open: %s\n", p->path);
        return -1;
    }
    const audio_format *fmt = decoder_get_format(p->decoder);
    if (fmt) {
        p->fmt = *fmt;
        p->duration_ms = fmt->duration_ms;
    }
    p->state = PLAYER_STATE_REALIZED;
    return 0;
}

int audio_player_prefetch(audio_player *p) {
    if (!p || p->state != PLAYER_STATE_REALIZED) return -1;
    p->state = PLAYER_STATE_PREFETCHED;
    p->position_ms = 0;
    return 0;
}

int audio_player_start(audio_player *p) {
    if (!p || p->state != PLAYER_STATE_PREFETCHED) return -1;
    p->state = PLAYER_STATE_STARTED;
    p->started = 1;
    p->loop_remaining = p->loop_count;
    return 0;
}

int audio_player_stop(audio_player *p) {
    if (!p) return -1;
    if (p->state == PLAYER_STATE_STARTED) {
        decoder_seek(p->decoder, 0.0);
        p->state = PLAYER_STATE_PREFETCHED;
        p->position_ms = 0;
    }
    return 0;
}

int audio_player_close(audio_player *p) {
    if (!p || !p->used) return -1;
    if (p->decoder) {
        decoder_close(p->decoder);
        p->decoder = NULL;
    }
    p->state = PLAYER_STATE_CLOSED;
    p->used = 0;
    return 0;
}

int audio_player_get_duration(audio_player *p) {
    return p ? p->duration_ms : 0;
}

int audio_player_get_position(audio_player *p) {
    return p ? p->position_ms : 0;
}

int audio_player_set_loop_count(audio_player *p, int count) {
    if (!p) return -1;
    p->loop_count = count;
    p->loop_remaining = count;
    return 0;
}

const char *audio_player_get_content_type(audio_player *p) {
    if (!p) return "application/octet-stream";
    return decoder_content_type(p->path);
}
