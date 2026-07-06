#ifndef DECODER_H
#define DECODER_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Audio format info */
typedef struct {
    int sample_rate;
    int channels;       /* 1 mono, 2 stereo */
    int bits_per_sample;
    int num_samples;    /* total samples per channel */
    int duration_ms;    /* total duration */
} audio_format;

/* Decoder state (opaque) */
typedef struct audio_decoder audio_decoder;

/* Open a decoder for a given file path or URL.
   Detects format from extension/content.
   Returns NULL on failure. */
audio_decoder *decoder_open(const char *path);

/* Get format info */
const audio_format *decoder_get_format(audio_decoder *dec);

/* Decode next chunk of PCM samples (interleaved int16).
   Returns number of frames decoded (0 = EOF, < 0 = error).
   Each frame = channels * sizeof(int16_t) bytes. */
int decoder_read_pcm(audio_decoder *dec, int16_t *buf, int frames);

/* Seek to position (0.0 = start, 1.0 = end).
   Returns 0 on success. */
int decoder_seek(audio_decoder *dec, double position);

/* Close decoder and free resources */
void decoder_close(audio_decoder *dec);

/* Check if a file path has a supported audio extension */
int decoder_supports(const char *path);

/* Get content type string for a path */
const char *decoder_content_type(const char *path);

#ifdef __cplusplus
}
#endif

#endif /* DECODER_H */
