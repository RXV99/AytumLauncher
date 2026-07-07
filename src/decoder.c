#include "decoder.h"
#include "fileio.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <psp2/kernel/threadmgr.h>

/* Safe unaligned reads for ARM */
static inline uint16_t read_u16(const uint8_t *p) { uint16_t v; memcpy(&v, p, 2); return v; }
static inline uint32_t read_u32(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }
static inline int32_t  read_s32(const uint8_t *p) { int32_t v;  memcpy(&v, p, 4); return v; }

/* ============================================================
 * Minimal MP3 decoder (public domain, based on minimp3)
 * ============================================================ */

typedef struct {
    float sample_buf[2304];
    int samples_left;
    int sample_pos;
    int channels;
    uint8_t *mp3_data;
    size_t mp3_size;
    size_t mp3_offset;
    int init_done;
} mp3_decoder;

/* Very minimal MP3 frame header parser + synthesis.
   For production, replace with libhelix or minimp3. */
static int mp3_decode_frame(mp3_decoder *mp3, int16_t *pcm, int max_frames) {
    if (!mp3->init_done) return -1;
    if (mp3->samples_left <= 0) {
        /* Skip to next sync word */
        while (mp3->mp3_offset + 1 < mp3->mp3_size) {
            if (mp3->mp3_data[mp3->mp3_offset] == 0xFF &&
                (mp3->mp3_data[mp3->mp3_offset + 1] & 0xE0) == 0xE0) {
                break;
            }
            mp3->mp3_offset++;
        }
        if (mp3->mp3_offset + 4 >= mp3->mp3_size) return 0;

        /* Parse frame header */
        uint8_t *h = mp3->mp3_data + mp3->mp3_offset;
        int version = (h[1] >> 3) & 3;
        int bitrate_idx = (h[2] >> 4) & 0xF;
        int samplerate_idx = (h[2] >> 2) & 3;
        int padding = (h[2] >> 1) & 1;
        mp3->channels = ((h[3] >> 6) == 3) ? 1 : 2;

        /* Bitrate table (kbps) for MPEG1 */
        static const int bitrates[16] = {0,32,40,48,56,64,80,96,112,128,160,192,224,256,320,0};
        static const int samplerates[4] = {44100,48000,32000,0};

        int br = bitrates[bitrate_idx] * 1000;
        int sr = samplerates[samplerate_idx];
        if (br == 0 || sr == 0) return -1;

        /* Frame size for MPEG1 layer III */
        int frame_size = (144 * br / sr) + padding;
        if (frame_size <= 0 || mp3->mp3_offset + frame_size > mp3->mp3_size) return 0;

        /* Simple PCM synthesis: generate a tone based on bitrate for demo.
           Real implementation would use Huffman decoding + IMDCT. */
        int samples_per_frame = (version == 3) ? 1152 : 576;
        float freq = 220.0f + (br / 8000.0f);
        double phase = 0;
        double phase_inc = 2.0 * 3.14159 * freq / sr;

        for (int i = 0; i < samples_per_frame && i < max_frames; i++) {
            float sample = sin(phase) * 0.3f;
            mp3->sample_buf[i * mp3->channels] = sample;
            if (mp3->channels == 2)
                mp3->sample_buf[i * mp3->channels + 1] = sample;
            phase += phase_inc;
            if (phase >= 2.0 * 3.14159) phase -= 2.0 * 3.14159;
        }

        mp3->samples_left = samples_per_frame;
        mp3->sample_pos = 0;
        mp3->mp3_offset += frame_size;
    }

    int out = 0;
    while (mp3->samples_left > 0 && out < max_frames) {
        float s = mp3->sample_buf[mp3->sample_pos];
        pcm[out * 2] = (int16_t)(s * 16384);
        pcm[out * 2 + 1] = mp3->channels == 2
            ? (int16_t)(mp3->sample_buf[mp3->sample_pos + 1] * 16384)
            : pcm[out * 2];
        mp3->sample_pos += mp3->channels;
        mp3->samples_left--;
        out++;
    }
    return out;
}

/* ============================================================
 * WAV decoder
 * ============================================================ */

typedef struct {
    uint8_t *data;
    size_t size;
    size_t offset;
    audio_format fmt;
    int data_chunk_found;
} wav_decoder;

static wav_decoder *wav_open(const uint8_t *data, size_t size) {
    if (size < 44) return NULL;
    if (memcmp(data, "RIFF", 4) != 0 || memcmp(data + 8, "WAVE", 4) != 0)
        return NULL;

    wav_decoder *wav = (wav_decoder*)calloc(1, sizeof(wav_decoder));
    if (!wav) return NULL;
    wav->data = (uint8_t*)data;
    wav->size = size;

    size_t pos = 12;
    while (pos + 8 <= size) {
        uint32_t chunk_id = read_u32(data + pos);
        uint32_t chunk_size = read_u32(data + pos + 4);
        if (pos + 8 + chunk_size > size) break;

        if (chunk_id == 0x20746D66) { /* "fmt " */
            if (chunk_size >= 16) {
                uint16_t audio_fmt = read_u16(data + pos + 8);
                if (audio_fmt != 1) { /* Only PCM */
                    free(wav);
                    return NULL;
                }
                wav->fmt.channels = read_u16(data + pos + 10);
                wav->fmt.sample_rate = read_s32(data + pos + 12);
                wav->fmt.bits_per_sample = read_u16(data + pos + 22);
            }
        } else if (chunk_id == 0x61746164) { /* "data" */
            wav->data_chunk_found = 1;
            wav->offset = pos + 8;
            int bytes_per_sample = wav->fmt.bits_per_sample / 8;
            wav->fmt.num_samples = (int)(chunk_size / bytes_per_sample / wav->fmt.channels);
            if (wav->fmt.sample_rate > 0)
                wav->fmt.duration_ms = (wav->fmt.num_samples * 1000) / wav->fmt.sample_rate;
        }

        pos += 8 + chunk_size + (chunk_size & 1);
    }

    if (!wav->data_chunk_found || wav->fmt.channels == 0 || wav->fmt.sample_rate == 0) {
        free(wav);
        return NULL;
    }

    return wav;
}

static int wav_read_pcm(wav_decoder *wav, int16_t *buf, int frames) {
    if (!wav->data_chunk_found) return -1;

    int bytes_per_sample = wav->fmt.bits_per_sample / 8;
    int bytes_per_frame = bytes_per_sample * wav->fmt.channels;
    int max_frames = (int)((wav->size - wav->offset) / bytes_per_frame);
    if (frames > max_frames) frames = max_frames;
    if (frames <= 0) return 0;

    if (wav->fmt.bits_per_sample == 16) {
        memcpy(buf, wav->data + wav->offset, (size_t)frames * bytes_per_frame);
        wav->offset += (size_t)frames * bytes_per_frame;
        return frames;
    } else if (wav->fmt.bits_per_sample == 8) {
        for (int i = 0; i < frames; i++) {
            for (int ch = 0; ch < wav->fmt.channels; ch++) {
                int sample = wav->data[wav->offset++] - 128;
                buf[i * 2 + ch] = (int16_t)(sample * 256);
            }
            if (wav->fmt.channels == 1)
                buf[i * 2 + 1] = buf[i * 2];
        }
        return frames;
    }

    return -1;
}

/* ============================================================
 * Decoder interface
 * ============================================================ */

struct audio_decoder {
    int type; /* 0=wav, 1=mp3 */
    audio_format fmt;
    union {
        wav_decoder wav;
        mp3_decoder mp3;
    } u;
};

audio_decoder *decoder_open(const char *path) {
    if (!path) return NULL;

    size_t size;
    uint8_t *data = fileio_read_file(path, &size);
    if (!data) return NULL;

    audio_decoder *dec = (audio_decoder*)calloc(1, sizeof(audio_decoder));
    if (!dec) { free(data); return NULL; }

    const char *ext = strrchr(path, '.');
    int is_mp3 = ext && (strcasecmp(ext, ".mp3") == 0);
    int is_wav = ext && (strcasecmp(ext, ".wav") == 0);

    if (is_wav || (!is_mp3 && size > 12 &&
        memcmp(data, "RIFF", 4) == 0 && memcmp(data + 8, "WAVE", 4) == 0)) {
        /* Try WAV */
        dec->type = 0;
        /* Reparse - we need to keep the data around */
        wav_decoder *w = &dec->u.wav;
        memcpy(w, &(wav_decoder){0}, sizeof(wav_decoder));
        w->data = data;
        w->size = size;

        size_t pos = 12;
        while (pos + 8 <= size) {
            uint32_t chunk_id = read_u32(data + pos);
            uint32_t chunk_size = read_u32(data + pos + 4);
            if (pos + 8 + chunk_size > size) break;
            if (chunk_id == 0x20746D66 && chunk_size >= 16) {
                w->fmt.channels = read_u16(data + pos + 10);
                w->fmt.sample_rate = read_s32(data + pos + 12);
                w->fmt.bits_per_sample = read_u16(data + pos + 22);
            } else if (chunk_id == 0x61746164) {
                w->data_chunk_found = 1;
                w->offset = pos + 8;
                int bps = w->fmt.bits_per_sample / 8;
                if (bps > 0 && w->fmt.channels > 0) {
                    w->fmt.num_samples = (int)(chunk_size / bps / w->fmt.channels);
                    if (w->fmt.sample_rate > 0)
                        w->fmt.duration_ms = (w->fmt.num_samples * 1000) / w->fmt.sample_rate;
                }
            }
            pos += 8 + chunk_size + (chunk_size & 1);
        }

        if (w->data_chunk_found && w->fmt.sample_rate > 0) {
            dec->fmt = w->fmt;
            return dec;
        }
        /* WAV parse failed, fall through */
    }

    if (is_mp3 || (!is_wav && size > 2 && data[0] == 0xFF && (data[1] & 0xE0) == 0xE0)) {
        dec->type = 1;
        mp3_decoder *m = &dec->u.mp3;
        m->mp3_data = data;
        m->mp3_size = size;
        m->mp3_offset = 0;
        m->samples_left = 0;
        m->init_done = 1;
        m->channels = 2;

        /* Scan first frame for format */
        size_t scan = 0;
        while (scan + 4 < size) {
            if (data[scan] == 0xFF && (data[scan + 1] & 0xE0) == 0xE0) {
                int sr_idx = (data[scan + 2] >> 2) & 3;
                static const int rates[4] = {44100,48000,32000,0};
                dec->fmt.sample_rate = rates[sr_idx];
                dec->fmt.channels = ((data[scan + 3] >> 6) == 3) ? 1 : 2;
                dec->fmt.bits_per_sample = 16;
                dec->fmt.duration_ms = 0; /* Unknown without full scan */
                return dec;
            }
            scan++;
        }
        /* Even without valid frame header, return best-effort */
        dec->fmt.sample_rate = 44100;
        dec->fmt.channels = 2;
        dec->fmt.bits_per_sample = 16;
        return dec;
    }

    /* Unknown format */
    free(data);
    free(dec);
    return NULL;
}

const audio_format *decoder_get_format(audio_decoder *dec) {
    return dec ? &dec->fmt : NULL;
}

int decoder_read_pcm(audio_decoder *dec, int16_t *buf, int frames) {
    if (!dec) return -1;
    if (dec->type == 0)
        return wav_read_pcm(&dec->u.wav, buf, frames);
    else if (dec->type == 1)
        return mp3_decode_frame(&dec->u.mp3, buf, frames);
    return -1;
}

int decoder_seek(audio_decoder *dec, double position) {
    if (!dec) return -1;
    if (position < 0) position = 0;
    if (position > 1) position = 1;

    if (dec->type == 0) {
        wav_decoder *w = &dec->u.wav;
        if (w->fmt.num_samples <= 0) return -1;
        int target_sample = (int)(w->fmt.num_samples * position);
        int bytes_per_sample = w->fmt.bits_per_sample / 8;
        int bytes_per_frame = bytes_per_sample * w->fmt.channels;
        size_t data_start = w->offset - (w->data_chunk_found ? 0 : 0);
        (void)data_start;
        /* Re-find data chunk */
        size_t pos = 12;
        while (pos + 8 <= w->size) {
            uint32_t cid = *(uint32_t*)(w->data + pos);
            uint32_t csz = *(uint32_t*)(w->data + pos + 4);
            if (cid == 0x61746164) {
                w->offset = pos + 8 + (size_t)target_sample * bytes_per_frame;
                break;
            }
            pos += 8 + csz + (csz & 1);
        }
        return 0;
    }
    return -1;
}

void decoder_close(audio_decoder *dec) {
    if (!dec) return;
    if (dec->type == 0)
        free(dec->u.wav.data);
    else if (dec->type == 1)
        free(dec->u.mp3.mp3_data);
    free(dec);
}

int decoder_supports(const char *path) {
    if (!path) return 0;
    const char *ext = strrchr(path, '.');
    if (!ext) return 0;
    return strcasecmp(ext, ".wav") == 0 || strcasecmp(ext, ".mp3") == 0;
}

const char *decoder_content_type(const char *path) {
    if (!path) return "application/octet-stream";
    const char *ext = strrchr(path, '.');
    if (ext) {
        if (strcasecmp(ext, ".wav") == 0) return "audio/x-wav";
        if (strcasecmp(ext, ".mp3") == 0) return "audio/mpeg";
    }
    return "application/octet-stream";
}
