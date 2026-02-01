#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>
#include "ring_buffer.h"
#include "effects.h"

#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

#define RING_BUFFER_SIZE 8192
#define PROCESS_CHUNK_SIZE 256

void print_usage(const char *prog_name) {
    printf("Usage: %s [input.wav] [output.wav] [OPTIONS]\n", prog_name);
    printf("\nOptions:\n");
    printf("  --gain <dB>          Apply gain in decibels (default: 0.0)\n");
    printf("  --lowpass <Hz>       Apply low-pass filter at frequency (default: off)\n");
    printf("  --highpass <Hz>      Apply high-pass filter at frequency (default: off)\n");
    printf("  --compress           Enable compressor (default: off)\n");
    printf("  --no-effects         Bypass all effects (passthrough)\n");
    printf("\nExamples:\n");
    printf("  %s input.wav output/result.wav --gain 6.0\n", prog_name);
    printf("  %s test_audio/input.wav output/filtered.wav --lowpass 3000 --gain 3.0\n", prog_name);
    printf("  %s test_audio/input.wav output/compressed.wav --compress --lowpass 5000\n", prog_name);
    printf("\n");
}

int main(int argc, char *argv[]) {
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════╗\n");
    printf("║      REAL-TIME AUDIO PROCESSOR v1.0                  ║\n");
    printf("║      Lock-Free Ring Buffer + DSP Effects             ║\n");
    printf("╚═══════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    // Default file paths
    const char *input_file = "test_audio/input.wav";
    const char *output_file = "output/processed.wav";
    
    // Effect parameters
    float gain_db = 0.0f;
    float lowpass_freq = 0.0f;
    float highpass_freq = 0.0f;
    bool compress_enabled = false;
    bool effects_enabled = true;
    
    // Parse command-line arguments
    // First, collect non-flag arguments (not starting with '-') before any option
    int file_count = 0;
    int options_start = 1;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            options_start = i;
            break;
        }
        if (file_count == 0) {
            input_file = argv[i];
            file_count++;
        } else if (file_count == 1) {
            output_file = argv[i];
            file_count++;
        }
    }
    // Now parse options
    for (int i = options_start; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        }
        else if (strcmp(argv[i], "--gain") == 0 && i + 1 < argc) {
            gain_db = atof(argv[++i]);
        }
        else if (strcmp(argv[i], "--lowpass") == 0 && i + 1 < argc) {
            lowpass_freq = atof(argv[++i]);
        }
        else if (strcmp(argv[i], "--highpass") == 0 && i + 1 < argc) {
            highpass_freq = atof(argv[++i]);
        }
        else if (strcmp(argv[i], "--compress") == 0) {
            compress_enabled = true;
        }
        else if (strcmp(argv[i], "--no-effects") == 0) {
            effects_enabled = false;
        }
    }
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        }
        else if (strcmp(argv[i], "--gain") == 0 && i + 1 < argc) {
            gain_db = atof(argv[++i]);
        }
        else if (strcmp(argv[i], "--lowpass") == 0 && i + 1 < argc) {
            lowpass_freq = atof(argv[++i]);
        }
        else if (strcmp(argv[i], "--highpass") == 0 && i + 1 < argc) {
            highpass_freq = atof(argv[++i]);
        }
        else if (strcmp(argv[i], "--compress") == 0) {
            compress_enabled = true;
        }
        else if (strcmp(argv[i], "--no-effects") == 0) {
            effects_enabled = false;
        }
        else if (argv[i][0] != '-') {
            // Non-flag argument - must be input or output file
            static int file_count = 0;
            if (file_count == 0) {
                input_file = argv[i];
                file_count++;
            } else if (file_count == 1) {
                output_file = argv[i];
                file_count++;
            }
        }
    }
    
    printf("Configuration:\n");
    printf("  Input:  %s\n", input_file);
    printf("  Output: %s\n", output_file);
    printf("\n");
    
    // Load input WAV file
    unsigned int channels;
    unsigned int sample_rate;
    drwav_uint64 total_frames;
    
    float *input_data = drwav_open_file_and_read_pcm_frames_f32(
        input_file, &channels, &sample_rate, &total_frames, NULL
    );
    
    if (!input_data) {
        fprintf(stderr, "✗ Error: Failed to open input file '%s'\n", input_file);
        return 1;
    }
    
    printf("Audio Info:\n");
    printf("  Frames:      %llu\n", (unsigned long long)total_frames);
    printf("  Sample Rate: %u Hz\n", sample_rate);
    printf("  Channels:    %u\n", channels);
    printf("  Duration:    %.2f seconds\n", (float)total_frames / sample_rate);
    printf("\n");
    
    if (channels != 1) {
        fprintf(stderr, "✗ Error: Only mono (1 channel) audio supported\n");
        drwav_free(input_data, NULL);
        return 1;
    }
    
    // Create ring buffer
    ring_buffer_t *rb = ring_buffer_create(RING_BUFFER_SIZE);
    if (!rb) {
        fprintf(stderr, "✗ Error: Failed to create ring buffer\n");
        drwav_free(input_data, NULL);
        return 1;
    }
    
    // Allocate output buffer
    float *output_data = malloc(total_frames * sizeof(float));
    if (!output_data) {
        fprintf(stderr, "✗ Error: Failed to allocate output buffer\n");
        ring_buffer_free(rb);
        drwav_free(input_data, NULL);
        return 1;
    }
    
    // Create effect chain
    effect_chain_t effects;
    effect_chain_init(&effects, sample_rate);
    
    printf("Effects Chain:\n");
    if (!effects_enabled) {
        printf("  [BYPASS] All effects disabled\n");
    } else {
        if (gain_db != 0.0f) {
            effects.gain_enabled = true;
            gain_init(&effects.gain, gain_db);
            printf("  ✓ Gain:       %+.1f dB\n", gain_db);
        }
        
        if (lowpass_freq > 0) {
            effects.filter_enabled = true;
            biquad_lowpass_init(&effects.filter, sample_rate, lowpass_freq, 0.707f);
            printf("  ✓ Low-pass:   %.0f Hz\n", lowpass_freq);
        }
        else if (highpass_freq > 0) {
            effects.filter_enabled = true;
            biquad_highpass_init(&effects.filter, sample_rate, highpass_freq, 0.707f);
            printf("  ✓ High-pass:  %.0f Hz\n", highpass_freq);
        }
        
        if (compress_enabled) {
            effects.compressor_enabled = true;
            printf("  ✓ Compressor: 4:1 ratio, -20dB threshold\n");
        }
        
        if (!effects.gain_enabled && !effects.filter_enabled && !effects.compressor_enabled) {
            printf("  (No effects configured - passthrough mode)\n");
        }
    }
    printf("\n");
    
    printf("Processing...\n");
    
    // Start timing
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    // Process audio through ring buffer
    size_t input_pos = 0;
    size_t output_pos = 0;
    size_t total_processed = 0;
    
    float *process_buffer = malloc(PROCESS_CHUNK_SIZE * sizeof(float));
    if (!process_buffer) {
        fprintf(stderr, "✗ Error: Failed to allocate process buffer\n");
        free(output_data);
        ring_buffer_free(rb);
        drwav_free(input_data, NULL);
        return 1;
    }
    
    // Prefill ring buffer
    size_t prefill = PROCESS_CHUNK_SIZE * 2;
    if (prefill > total_frames) prefill = total_frames;
    ring_buffer_write(rb, input_data, prefill);
    input_pos = prefill;
    
    int last_percent = -1;
    
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
            ring_buffer_read(rb, process_buffer, to_read);
            
            // Apply effects if enabled
            if (effects_enabled) {
                effect_chain_process(&effects, process_buffer, to_read);
            }
            
            memcpy(&output_data[output_pos], process_buffer, to_read * sizeof(float));
            output_pos += to_read;
            total_processed += to_read;
            
            // Progress bar
            int percent = (int)((float)total_processed / total_frames * 100.0f);
            if (percent != last_percent) {
                int bar_width = 50;
                int filled = (percent * bar_width) / 100;
                printf("\r  [");
                for (int i = 0; i < bar_width; i++) {
                    if (i < filled) printf("█");
                    else printf("░");
                }
                printf("] %3d%%", percent);
                fflush(stdout);
                last_percent = percent;
            }
        }
        
        // Safety: break if nothing is happening
        if (to_write == 0 && to_read == 0) {
            break;
        }
    }
    
    printf("\n");
    
    free(process_buffer);
    
    // End timing
    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    
    printf("\nPerformance:\n");
    printf("  Processed:   %zu frames\n", total_processed);
    printf("  Time:        %.3f seconds\n", elapsed);
    printf("  Speed:       %.2fx realtime\n", (total_frames / (double)sample_rate) / elapsed);
    printf("  Latency:     %.2f ms (ring buffer size)\n", 
           (RING_BUFFER_SIZE / (float)sample_rate) * 1000.0f);
    printf("\n");
    
    // Write output WAV file
    printf("Writing output...\n");
    drwav wav;
    drwav_data_format format;
    format.container = drwav_container_riff;
    format.format = DR_WAVE_FORMAT_IEEE_FLOAT;
    format.channels = channels;
    format.sampleRate = sample_rate;
    format.bitsPerSample = 32;
    
    if (!drwav_init_file_write(&wav, output_file, &format, NULL)) {
        fprintf(stderr, "✗ Error: Failed to create output file '%s'\n", output_file);
        free(output_data);
        ring_buffer_free(rb);
        drwav_free(input_data, NULL);
        return 1;
    }
    
    drwav_uint64 frames_written = drwav_write_pcm_frames(&wav, total_frames, output_data);
    drwav_uninit(&wav);
    
    printf("  Wrote %llu frames to '%s'\n", (unsigned long long)frames_written, output_file);
    
    // Cleanup
    free(output_data);
    ring_buffer_free(rb);
    drwav_free(input_data, NULL);
    
    printf("\n✓ Done! Play '%s' to hear the result.\n\n", output_file);
    return 0;
}