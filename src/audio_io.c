#include <alsa/asoundlib.h>
#include "audio_io.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <alloca.h>
/**
 * Common PCM setup for both capture and playback.
 */
static int audio_setup_pcm(snd_pcm_t *handle,
                           unsigned int sample_rate,
                           unsigned int channels,
                           size_t period_size,
                           snd_pcm_stream_t stream_type) {
    snd_pcm_hw_params_t *hw_params;
    int err;
    
    snd_pcm_hw_params_alloca(&hw_params);
    
    // Initialize hardware parameters
    err = snd_pcm_hw_params_any(handle, hw_params);
    if (err < 0) {
        fprintf(stderr, "Cannot initialize hardware parameters: %s\n", snd_strerror(err));
        return err;
    }
    
    // Set access type (interleaved)
    err = snd_pcm_hw_params_set_access(handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
    if (err < 0) {
        fprintf(stderr, "Cannot set access type: %s\n", snd_strerror(err));
        return err;
    }
    
    // Set sample format (32-bit float)
    err = snd_pcm_hw_params_set_format(handle, hw_params, SND_PCM_FORMAT_FLOAT_LE);
    if (err < 0) {
        fprintf(stderr, "Cannot set sample format: %s\n", snd_strerror(err));
        return err;
    }
    
    // Set sample rate
    unsigned int actual_rate = sample_rate;
    err = snd_pcm_hw_params_set_rate_near(handle, hw_params, &actual_rate, 0);
    if (err < 0) {
        fprintf(stderr, "Cannot set sample rate: %s\n", snd_strerror(err));
        return err;
    }
    if (actual_rate != sample_rate) {
        fprintf(stderr, "Warning: Requested rate %u Hz, got %u Hz\n", sample_rate, actual_rate);
    }
    
    // Set number of channels
    err = snd_pcm_hw_params_set_channels(handle, hw_params, channels);
    if (err < 0) {
        fprintf(stderr, "Cannot set channel count: %s\n", snd_strerror(err));
        return err;
    }
    
    // Set period size
    snd_pcm_uframes_t period_frames = period_size;
    err = snd_pcm_hw_params_set_period_size_near(handle, hw_params, &period_frames, 0);
    if (err < 0) {
        fprintf(stderr, "Cannot set period size: %s\n", snd_strerror(err));
        return err;
    }
    
    // Set buffer size (4 periods)
    snd_pcm_uframes_t buffer_frames = period_frames * 4;
    err = snd_pcm_hw_params_set_buffer_size_near(handle, hw_params, &buffer_frames);
    if (err < 0) {
        fprintf(stderr, "Cannot set buffer size: %s\n", snd_strerror(err));
        return err;
    }
    
    // Apply hardware parameters
    err = snd_pcm_hw_params(handle, hw_params);
    if (err < 0) {
        fprintf(stderr, "Cannot apply hardware parameters: %s\n", snd_strerror(err));
        return err;
    }
    
    // Prepare device
    err = snd_pcm_prepare(handle);
    if (err < 0) {
        fprintf(stderr, "Cannot prepare audio device: %s\n", snd_strerror(err));
        return err;
    }
    
    return 0;
}

audio_device_t* audio_capture_open(const char *device_name,
                                    unsigned int sample_rate,
                                    unsigned int channels,
                                    size_t period_size) {
    audio_device_t *dev = malloc(sizeof(audio_device_t));
    if (!dev) {
        return NULL;
    }
    
    int err = snd_pcm_open(&dev->handle, device_name, SND_PCM_STREAM_CAPTURE, 0);
    if (err < 0) {
        fprintf(stderr, "Cannot open capture device %s: %s\n", device_name, snd_strerror(err));
        free(dev);
        return NULL;
    }
    
    err = audio_setup_pcm(dev->handle, sample_rate, channels, period_size, SND_PCM_STREAM_CAPTURE);
    if (err < 0) {
        snd_pcm_close(dev->handle);
        free(dev);
        return NULL;
    }
    
    dev->sample_rate = sample_rate;
    dev->channels = channels;
    dev->period_size = period_size;
    
    return dev;
}

audio_device_t* audio_playback_open(const char *device_name,
                                     unsigned int sample_rate,
                                     unsigned int channels,
                                     size_t period_size) {
    audio_device_t *dev = malloc(sizeof(audio_device_t));
    if (!dev) {
        return NULL;
    }
    
    int err = snd_pcm_open(&dev->handle, device_name, SND_PCM_STREAM_PLAYBACK, 0);
    if (err < 0) {
        fprintf(stderr, "Cannot open playback device %s: %s\n", device_name, snd_strerror(err));
        free(dev);
        return NULL;
    }
    
    err = audio_setup_pcm(dev->handle, sample_rate, channels, period_size, SND_PCM_STREAM_PLAYBACK);
    if (err < 0) {
        snd_pcm_close(dev->handle);
        free(dev);
        return NULL;
    }
    
    dev->sample_rate = sample_rate;
    dev->channels = channels;
    dev->period_size = period_size;
    
    return dev;
}

ssize_t audio_capture_read(audio_device_t *dev, float *buffer, size_t frames) {
    snd_pcm_sframes_t result = snd_pcm_readi(dev->handle, buffer, frames);
    
    if (result < 0) {
        result = audio_device_recover(dev, result);
    }
    
    return result;
}

ssize_t audio_playback_write(audio_device_t *dev, const float *buffer, size_t frames) {
    snd_pcm_sframes_t result = snd_pcm_writei(dev->handle, buffer, frames);
    
    if (result < 0) {
        result = audio_device_recover(dev, result);
    }
    
    return result;
}

void audio_device_close(audio_device_t *dev) {
    if (dev) {
        snd_pcm_drain(dev->handle);
        snd_pcm_close(dev->handle);
        free(dev);
    }
}

int audio_device_recover(audio_device_t *dev, int err) {
    if (err == -EPIPE) {
        // Buffer overrun/underrun
        fprintf(stderr, "XRUN detected, recovering...\n");
        err = snd_pcm_prepare(dev->handle);
        if (err < 0) {
            fprintf(stderr, "Cannot recover from xrun: %s\n", snd_strerror(err));
        }
        return err;
    } else if (err == -ESTRPIPE) {
        // Device suspended
        while ((err = snd_pcm_resume(dev->handle)) == -EAGAIN) {
            sleep(1);
        }
        if (err < 0) {
            err = snd_pcm_prepare(dev->handle);
            if (err < 0) {
                fprintf(stderr, "Cannot recover from suspend: %s\n", snd_strerror(err));
            }
        }
        return err;
    }
    
    return err;
}