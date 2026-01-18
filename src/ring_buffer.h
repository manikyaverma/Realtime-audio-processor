#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stddef.h>
#include <stdatomic.h>
#include <stdbool.h>

/**
 * Lock-free Single-Producer-Single-Consumer (SPSC) ring buffer.
 * 
 * Thread safety:
 *   - ONE thread writes (producer)
 *   - ONE thread reads (consumer)
 *   - No locks, uses atomic operations
 * 
 * Design:
 *   - Power-of-2 size for fast modulo (bitmask)
 *   - Atomic read/write indices
 *   - Memory ordering: relaxed for indices (data dependencies provide ordering)
 */

typedef struct {
    float *buffer;              // Audio samples (heap allocated)
    size_t capacity;            // Must be power of 2
    size_t mask;                // capacity - 1 (for fast modulo)
    
    atomic_size_t write_index;  // Producer updates this
    atomic_size_t read_index;   // Consumer updates this
} ring_buffer_t;

/**
 * Create a ring buffer.
 * capacity_samples must be a power of 2.
 * Returns NULL on failure.
 */
ring_buffer_t* ring_buffer_create(size_t capacity_samples);

/**
 * Destroy ring buffer and free memory.
 */
void ring_buffer_free(ring_buffer_t *rb);

/**
 * Write samples to the ring buffer (producer side).
 * Returns number of samples actually written (may be less if buffer is full).
 */
size_t ring_buffer_write(ring_buffer_t *rb, const float *data, size_t count);

/**
 * Read samples from the ring buffer (consumer side).
 * Returns number of samples actually read (may be less if buffer is empty).
 */
size_t ring_buffer_read(ring_buffer_t *rb, float *data, size_t count);

/**
 * Get number of samples available to read (consumer side).
 */
size_t ring_buffer_read_available(const ring_buffer_t *rb);

/**
 * Get number of samples available to write (producer side).
 */
size_t ring_buffer_write_available(const ring_buffer_t *rb);

/**
 * Reset the buffer (NOT thread-safe, call only when no I/O is happening).
 */
void ring_buffer_reset(ring_buffer_t *rb);

#endif // RING_BUFFER_H