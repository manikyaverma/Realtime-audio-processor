#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include "audio_io.h"
#include "ring_buffer.h"
#include <math.h>

#define SAMPLE_RATE 48000
#define CHANNELS 1
#define PERIOD_SIZE 256
#define RING_BUFFER_SIZE 8192

static volatile int running = 1;

void signal_handler(int sig) {
    (void)sig;
    running = 0;
}

int main(void) {
    printf("Real-Time Audio Processor - Passthrough Test\n");
    printf("Sample rate: %d Hz, Channels: %d, Period: %d frames\n",
           SAMPLE_RATE, CHANNELS, PERIOD_SIZE);
    printf("Press Ctrl+C to stop...\n\n");
    
    signal(SIGINT, signal_handler);
    
    // Open audio devices
    audio_device_t *capture = audio_capture_open("default", SAMPLE_RATE, CHANNELS, PERIOD_SIZE);
    if (!capture) {
        fprintf(stderr, "Failed to open capture device\n");
        return 1;
    }
    
    audio_device_t *playback = audio_playback_open("default", SAMPLE_RATE, CHANNELS, PERIOD_SIZE);
    if (!playback) {
        fprintf(stderr, "Failed to open playback device\n");
        audio_device_close(capture);
        return 1;
    }
    
    // Create ring buffer
    ring_buffer_t *rb = ring_buffer_create(RING_BUFFER_SIZE);
    if (!rb) {
        fprintf(stderr, "Failed to create ring buffer\n");
        audio_device_close(capture);
        audio_device_close(playback);
        return 1;
    }
    
    printf("Audio devices opened successfully\n");
    printf("Running passthrough (you should hear yourself with slight delay)...\n\n");
    
    float *capture_buffer = malloc(PERIOD_SIZE * CHANNELS * sizeof(float));
    float *playback_buffer = malloc(PERIOD_SIZE * CHANNELS * sizeof(float));
    
    if (!capture_buffer || !playback_buffer) {
        fprintf(stderr, "Failed to allocate buffers\n");
        free(capture_buffer);
        free(playback_buffer);
        ring_buffer_free(rb);
        audio_device_close(capture);
        audio_device_close(playback);
        return 1;
    }
    
    // Prefill ring buffer with silence to avoid initial underrun
    float silence[PERIOD_SIZE] = {0};
    for (int i = 0; i < 4; i++) {
        ring_buffer_write(rb, silence, PERIOD_SIZE);
    }
    
// Test tone generator (440 Hz sine wave)
    float phase = 0.0f;
    float phase_increment = 2.0f * 3.14159f * 440.0f / SAMPLE_RATE; // 440 Hz (A note)
    
    printf("Generating 440 Hz test tone...\n");
    
    // Main loop
    while (running) {
        // Generate sine wave into capture buffer
        for (size_t i = 0; i < PERIOD_SIZE; i++) {
            capture_buffer[i] = 0.3f * sinf(phase);  // 0.3 = volume (not too loud)
            phase += phase_increment;
            if (phase >= 2.0f * 3.14159f) {
                phase -= 2.0f * 3.14159f;
            }
        }
        
        // Write to ring buffer
        size_t written = ring_buffer_write(rb, capture_buffer, PERIOD_SIZE);
        if (written < PERIOD_SIZE) {
            fprintf(stderr, "Ring buffer overflow! Dropped %zd frames\n", PERIOD_SIZE - written);
        }
        
        // Read from ring buffer
        size_t frames_available = ring_buffer_read(rb, playback_buffer, PERIOD_SIZE);
        
        // Playback audio
        if (frames_available > 0) {
            ssize_t frames_written = audio_playback_write(playback, playback_buffer, frames_available);
            if (frames_written < 0) {
                fprintf(stderr, "Playback error\n");
                break;
            }
        }
    }
    
    printf("\nStopping...\n");
    
    // Cleanup
    free(capture_buffer);
    free(playback_buffer);
    ring_buffer_free(rb);
    audio_device_close(capture);
    audio_device_close(playback);
    
    printf("Cleanup complete\n");
    return 0;
}