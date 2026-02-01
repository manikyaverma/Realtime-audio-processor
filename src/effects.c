#include "effects.h"
#include <math.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include <string.h>

// ============================================================================
// GAIN EFFECT
// ============================================================================

void gain_init(gain_effect_t *effect, float gain_db) {
    // Convert dB to linear gain: gain = 10^(dB/20)
    effect->gain = powf(10.0f, gain_db / 20.0f);
}

void gain_process(gain_effect_t *effect, float *buffer, size_t frames) {
    for (size_t i = 0; i < frames; i++) {
        buffer[i] *= effect->gain;
    }
}

// ============================================================================
// BIQUAD FILTER
// ============================================================================

void biquad_lowpass_init(biquad_t *filter, float sample_rate, float cutoff_freq, float q) {
    float w0 = 2.0f * M_PI * cutoff_freq / sample_rate;
    float cos_w0 = cosf(w0);
    float sin_w0 = sinf(w0);
    float alpha = sin_w0 / (2.0f * q);
    
    float a0 = 1.0f + alpha;
    filter->b0 = ((1.0f - cos_w0) / 2.0f) / a0;
    filter->b1 = (1.0f - cos_w0) / a0;
    filter->b2 = ((1.0f - cos_w0) / 2.0f) / a0;
    filter->a1 = (-2.0f * cos_w0) / a0;
    filter->a2 = (1.0f - alpha) / a0;
    
    biquad_reset(filter);
}

void biquad_highpass_init(biquad_t *filter, float sample_rate, float cutoff_freq, float q) {
    float w0 = 2.0f * M_PI * cutoff_freq / sample_rate;
    float cos_w0 = cosf(w0);
    float sin_w0 = sinf(w0);
    float alpha = sin_w0 / (2.0f * q);
    
    float a0 = 1.0f + alpha;
    filter->b0 = ((1.0f + cos_w0) / 2.0f) / a0;
    filter->b1 = -(1.0f + cos_w0) / a0;
    filter->b2 = ((1.0f + cos_w0) / 2.0f) / a0;
    filter->a1 = (-2.0f * cos_w0) / a0;
    filter->a2 = (1.0f - alpha) / a0;
    
    biquad_reset(filter);
}

float biquad_process_sample(biquad_t *filter, float input) {
    float output = filter->b0 * input + 
                   filter->b1 * filter->x1 + 
                   filter->b2 * filter->x2 - 
                   filter->a1 * filter->y1 - 
                   filter->a2 * filter->y2;
    
    filter->x2 = filter->x1;
    filter->x1 = input;
    filter->y2 = filter->y1;
    filter->y1 = output;
    
    return output;
}

void biquad_process(biquad_t *filter, float *buffer, size_t frames) {
    for (size_t i = 0; i < frames; i++) {
        buffer[i] = biquad_process_sample(filter, buffer[i]);
    }
}

void biquad_reset(biquad_t *filter) {
    filter->x1 = filter->x2 = 0.0f;
    filter->y1 = filter->y2 = 0.0f;
}

// ============================================================================
// COMPRESSOR
// ============================================================================

void compressor_init(compressor_t *comp, float threshold_db, float ratio,
                     float attack_ms, float release_ms, float sample_rate) {
    comp->threshold = powf(10.0f, threshold_db / 20.0f);
    comp->ratio = ratio;
    comp->attack_coef = expf(-1.0f / (attack_ms * 0.001f * sample_rate));
    comp->release_coef = expf(-1.0f / (release_ms * 0.001f * sample_rate));
    comp->envelope = 0.0f;
}

void compressor_process(compressor_t *comp, float *buffer, size_t frames) {
    for (size_t i = 0; i < frames; i++) {
        float input = fabsf(buffer[i]);
        
        // Envelope follower
        if (input > comp->envelope) {
            comp->envelope = comp->attack_coef * comp->envelope + 
                            (1.0f - comp->attack_coef) * input;
        } else {
            comp->envelope = comp->release_coef * comp->envelope + 
                            (1.0f - comp->release_coef) * input;
        }
        
        // Compute gain reduction
        float gain = 1.0f;
        if (comp->envelope > comp->threshold) {
            float overshoot = comp->envelope / comp->threshold;
            gain = powf(overshoot, (1.0f / comp->ratio) - 1.0f);
        }
        
        buffer[i] *= gain;
    }
}

// ============================================================================
// EFFECT CHAIN
// ============================================================================

void effect_chain_init(effect_chain_t *chain, float sample_rate) {
    // Initialize with default settings
    gain_init(&chain->gain, 0.0f);  // 0 dB (unity gain)
    biquad_lowpass_init(&chain->filter, sample_rate, 2000.0f, 0.707f);  // 2kHz lowpass
    compressor_init(&chain->compressor, -20.0f, 4.0f, 10.0f, 100.0f, sample_rate);
    
    // All disabled by default
    chain->gain_enabled = false;
    chain->filter_enabled = false;
    chain->compressor_enabled = false;
}

void effect_chain_process(effect_chain_t *chain, float *buffer, size_t frames) {
    if (chain->gain_enabled) {
        gain_process(&chain->gain, buffer, frames);
    }
    
    if (chain->filter_enabled) {
        biquad_process(&chain->filter, buffer, frames);
    }
    
    if (chain->compressor_enabled) {
        compressor_process(&chain->compressor, buffer, frames);
    }
}