#ifndef AUDIO_IO_H
#define AUDIO_IO_H

#include <alsa/asoundlib.h>
#include <stddef.h>

/**
 * ALSA audio I/O wrapper for capture and playback.
 * Simplified interface for real-time audio processing.
 */

typedef struct {
    snd_pcm_t *handle;
    unsigned int sample_rate;
    unsigned int channels;
    size_t period_size;      // Frames per period (ALSA callback chunk)
} audio_device_t;

/**
 * Open audio capture device (microphone/line-in).
 * Returns NULL on failure.
 */
audio_device_t* audio_capture_open(const char *device_name,
                                    unsigned int sample_rate,
                                    unsigned int channels,
                                    size_t period_size);

/**
 * Open audio playback device (speakers/headphones).
 * Returns NULL on failure.
 */
audio_device_t* audio_playback_open(const char *device_name,
                                     unsigned int sample_rate,
                                     unsigned int channels,
                                     size_t period_size);

/**
 * Read audio samples from capture device.
 * Returns number of frames actually read, or negative on error.
 */
ssize_t audio_capture_read(audio_device_t *dev, float *buffer, size_t frames);

/**
 * Write audio samples to playback device.
 * Returns number of frames actually written, or negative on error.
 */
ssize_t audio_playback_write(audio_device_t *dev, const float *buffer, size_t frames);

/**
 * Close and free audio device.
 */
void audio_device_close(audio_device_t *dev);

/**
 * Recover from ALSA xrun (overrun/underrun).
 */
int audio_device_recover(audio_device_t *dev, int err);

#endif // AUDIO_IO_H