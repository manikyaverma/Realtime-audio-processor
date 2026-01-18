#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "ring_buffer.h"

#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

#define RING_BUFFER_SIZE 8192
#define PROCESS_CHUNK_SIZE 256

int main(int argc, char *argv[]) {
    printf("Real-Time Audio Processor - File Processing Mode\n");
    printf("=================================================\n\n");
    
    // Input/output file paths
    const char *input_file = (argc > 1) ? argv[1] : "test_audio/input.wav";
    const char *output_file = (argc > 2) ? argv[2] : "test_audio/output.wav";
    
    printf("Input:  %s\n", input_file);
    printf("Output: %s\n", output_file);
    
    // Load input WAV file
    unsigned int channels;
    unsigned int sample_rate;
    drwav_uint64 total_frames;
    
    float *input_data = drwav_open_file_and_read_pcm_frames_f32(
        input_file, &channels, &sample_rate, &total_frames, NULL
    );
    
    if (!input_data) {
        fprintf(stderr, "Error: Failed to open input file '%s'\n", input_file);
        return 1;
    }
    
    printf("Loaded: %llu frames, %u Hz, %u channel(s)\n", 
           (unsigned long long)total_frames, sample_rate, channels);
    
    if (channels != 1) {
        fprintf(stderr, "Error: Only mono (1 channel) audio supported for now\n");
        drwav_free(input_data, NULL);
        return 1;
    }
    
    // Create ring buffer
    ring_buffer_t *rb = ring_buffer_create(RING_BUFFER_SIZE);
    if (!rb) {
        fprintf(stderr, "Error: Failed to create ring buffer\n");
        drwav_free(input_data, NULL);
        return 1;
    }
    
    // Allocate output buffer
    float *output_data = malloc(total_frames * sizeof(float));
    if (!output_data) {
        fprintf(stderr, "Error: Failed to allocate output buffer\n");
        ring_buffer_free(rb);
        drwav_free(input_data, NULL);
        return 1;
    }
    
    printf("\nProcessing through ring buffer...\n");
    
    // Start timing
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    // Process audio through ring buffer
    size_t input_pos = 0;
    size_t output_pos = 0;
    size_t total_processed = 0;
    
    // Prefill ring buffer
    size_t prefill = PROCESS_CHUNK_SIZE * 2;
    if (prefill > total_frames) prefill = total_frames;
    ring_buffer_write(rb, input_data, prefill);
    input_pos = prefill;
    
    while (output_pos < total_frames) {
        size_t to_write = 0;
        size_t to_read = 0;

        // Write more input if available and buffer has space
        if (input_pos < total_frames) {
            size_t available_space = ring_buffer_write_available(rb);
            to_write = PROCESS_CHUNK_SIZE;
            if (to_write > available_space) to_write = available_space;
            if (to_write > total_frames - input_pos) to_write = total_frames - input_pos;
            if (to_write > 0) {
                ring_buffer_write(rb, &input_data[input_pos], to_write);
                input_pos += to_write;
            }
        }

        // Read and process output
        size_t available_data = ring_buffer_read_available(rb);
        to_read = PROCESS_CHUNK_SIZE;
        if (to_read > available_data) to_read = available_data;
        if (to_read > total_frames - output_pos) to_read = total_frames - output_pos;
        if (to_read > 0) {
            ring_buffer_read(rb, &output_data[output_pos], to_read);
            output_pos += to_read;
            total_processed += to_read;
            // Progress indicator
            if (total_processed % (sample_rate / 10) == 0) {
                float progress = (float)total_processed / total_frames * 100.0f;
                printf("\rProgress: %.1f%%", progress);
                fflush(stdout);
            }
        }

        // Safety: break if nothing is happening
        if (to_write == 0 && to_read == 0) {
            break;
        }
    }
    
    // End timing
    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    
    printf("\rProgress: 100.0%%\n");
    printf("\nProcessed %zu frames in %.3f seconds\n", total_processed, elapsed);
    printf("Processing speed: %.2fx realtime\n", 
           (total_frames / (double)sample_rate) / elapsed);
    
    // Write output WAV file
    printf("Writing output file...\n");
    drwav wav;
    drwav_data_format format;
    format.container = drwav_container_riff;
    format.format = DR_WAVE_FORMAT_IEEE_FLOAT;
    format.channels = channels;
    format.sampleRate = sample_rate;
    format.bitsPerSample = 32;
    
    if (!drwav_init_file_write(&wav, output_file, &format, NULL)) {
        fprintf(stderr, "Error: Failed to create output file '%s'\n", output_file);
        free(output_data);
        ring_buffer_free(rb);
        drwav_free(input_data, NULL);
        return 1;
    }
    
    drwav_uint64 frames_written = drwav_write_pcm_frames(&wav, total_frames, output_data);
    drwav_uninit(&wav);
    
    printf("Wrote %llu frames to '%s'\n", (unsigned long long)frames_written, output_file);
    
    // Cleanup
    free(output_data);
    ring_buffer_free(rb);
    drwav_free(input_data, NULL);
    
    printf("\n✓ Done! Play the output file to hear the result.\n");
    return 0;
}