#ifndef EFFECTS_H
#define EFFECTS_H

#include <stddef.h>
#include <stdbool.h>

/**
 * Simple gain/volume control effect.
 */
typedef struct {
    float gain;  // Linear gain (1.0 = unity, 2.0 = double, 0.5 = half)
} gain_effect_t;

void gain_init(gain_effect_t *effect, float gain_db);
void gain_process(gain_effect_t *effect, float *buffer, size_t frames);

/**
 * Biquad filter (low-pass, high-pass, etc.)
 */
typedef struct {
    // Filter coefficients
    float b0, b1, b2;  // Numerator
    float a1, a2;      // Denominator (a0 is normalized to 1.0)
    
    // State variables
    float x1, x2;  // Input history
    float y1, y2;  // Output history
} biquad_t;

void biquad_lowpass_init(biquad_t *filter, float sample_rate, float cutoff_freq, float q);
void biquad_highpass_init(biquad_t *filter, float sample_rate, float cutoff_freq, float q);
float biquad_process_sample(biquad_t *filter, float input);
void biquad_process(biquad_t *filter, float *buffer, size_t frames);
void biquad_reset(biquad_t *filter);

/**
 * Simple compressor (reduces dynamic range)
 */
typedef struct {
    float threshold;     // Level above which compression starts (linear)
    float ratio;         // Compression ratio (4.0 = 4:1)
    float attack_coef;   // Attack time coefficient
    float release_coef;  // Release time coefficient
    float envelope;      // Current envelope follower state
} compressor_t;

void compressor_init(compressor_t *comp, float threshold_db, float ratio,
                     float attack_ms, float release_ms, float sample_rate);
void compressor_process(compressor_t *comp, float *buffer, size_t frames);

/**
 * Effect chain - combines multiple effects
 */
typedef struct {
    bool gain_enabled;
    bool filter_enabled;
    bool compressor_enabled;
    
    gain_effect_t gain;
    biquad_t filter;
    compressor_t compressor;
} effect_chain_t;

void effect_chain_init(effect_chain_t *chain, float sample_rate);
void effect_chain_process(effect_chain_t *chain, float *buffer, size_t frames);

#endif // EFFECTS_H