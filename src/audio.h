#ifndef AUDIO_H
#define AUDIO_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AUDIO_SAMPLE_RATE 44100
#define AUDIO_CHANNELS    2
#define AUDIO_BUFFER_SIZE 1024

/* Player states */
typedef enum {
    PLAYER_STATE_UNREALIZED = 0,
    PLAYER_STATE_REALIZED   = 1,
    PLAYER_STATE_PREFETCHED = 2,
    PLAYER_STATE_STARTED    = 3,
    PLAYER_STATE_CLOSED     = 4,
} player_state;

/* Tone constants */
#define TONE_C4  60
#define TONE_D4  62
#define TONE_E4  64
#define TONE_F4  65
#define TONE_G4  67
#define TONE_A4  69
#define TONE_B4  71
#define TONE_C5  72

#define TONE_VOLUME_MAX 100
#define TONE_VOLUME_MIN 0

/* Initialize audio subsystem */
int  audio_init(void);
void audio_shutdown(void);

/* Tone generation */
void audio_play_tone(int note, int duration_ms, int volume);
void audio_tone_start(int note, int volume);
void audio_tone_stop(void);
int  audio_is_playing(void);

/* Volume control */
void audio_set_volume(int volume);
int  audio_get_volume(void);

/* Streaming player (used by MMAPI Player)
   The player takes ownership of the decoder handle */
typedef struct audio_player audio_player;

audio_player *audio_player_create(const char *path);
player_state  audio_player_get_state(audio_player *p);
int           audio_player_realize(audio_player *p);
int           audio_player_prefetch(audio_player *p);
int           audio_player_start(audio_player *p);
int           audio_player_stop(audio_player *p);
int           audio_player_close(audio_player *p);
int           audio_player_get_duration(audio_player *p);
int           audio_player_get_position(audio_player *p);
int           audio_player_set_loop_count(audio_player *p, int count);
const char   *audio_player_get_content_type(audio_player *p);

/* Audio mixing thread (called each frame) */
void audio_update(void);

double audio_note_to_freq(int note);

#ifdef __cplusplus
}
#endif

#endif /* AUDIO_H */
