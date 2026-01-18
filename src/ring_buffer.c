#include "ring_buffer.h"
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

/**
 * Check if a number is a power of 2.
 */
static bool is_power_of_2(size_t n) {
    return n > 0 && (n & (n - 1)) == 0;
}

ring_buffer_t* ring_buffer_create(size_t capacity_samples) {
    if (!is_power_of_2(capacity_samples)) {
        return NULL;
    }
    
    ring_buffer_t *rb = malloc(sizeof(ring_buffer_t));
    if (!rb) {
        return NULL;
    }
    
    rb->buffer = malloc(capacity_samples * sizeof(float));
    if (!rb->buffer) {
        free(rb);
        return NULL;
    }
    
    rb->capacity = capacity_samples;
    rb->mask = capacity_samples - 1;
    
    atomic_init(&rb->write_index, 0);
    atomic_init(&rb->read_index, 0);
    
    // Zero out buffer (good practice for audio)
    memset(rb->buffer, 0, capacity_samples * sizeof(float));
    
    return rb;
}

void ring_buffer_free(ring_buffer_t *rb) {
    if (rb) {
        free(rb->buffer);
        free(rb);
    }
}

size_t ring_buffer_read_available(const ring_buffer_t *rb) {
    size_t w = atomic_load_explicit(&rb->write_index, memory_order_relaxed);
    size_t r = atomic_load_explicit(&rb->read_index, memory_order_relaxed);
    return w - r;
}

size_t ring_buffer_write_available(const ring_buffer_t *rb) {
    size_t w = atomic_load_explicit(&rb->write_index, memory_order_relaxed);
    size_t r = atomic_load_explicit(&rb->read_index, memory_order_relaxed);
    return rb->capacity - (w - r);
}

size_t ring_buffer_write(ring_buffer_t *rb, const float *data, size_t count) {
    size_t available = ring_buffer_write_available(rb);
    size_t to_write = (count < available) ? count : available;
    
    if (to_write == 0) {
        return 0;
    }
    
    size_t w = atomic_load_explicit(&rb->write_index, memory_order_relaxed);
    
    // Write in up to two chunks (handle wrap-around)
    size_t write_pos = w & rb->mask;
    size_t chunk1 = rb->capacity - write_pos;
    
    if (chunk1 > to_write) {
        chunk1 = to_write;
    }
    
    memcpy(&rb->buffer[write_pos], data, chunk1 * sizeof(float));
    
    if (to_write > chunk1) {
        size_t chunk2 = to_write - chunk1;
        memcpy(&rb->buffer[0], &data[chunk1], chunk2 * sizeof(float));
    }
    
    // Update write index (release semantics ensures data is visible)
    atomic_store_explicit(&rb->write_index, w + to_write, memory_order_release);
    
    return to_write;
}

size_t ring_buffer_read(ring_buffer_t *rb, float *data, size_t count) {
    size_t available = ring_buffer_read_available(rb);
    size_t to_read = (count < available) ? count : available;
    
    if (to_read == 0) {
        return 0;
    }
    
    size_t r = atomic_load_explicit(&rb->read_index, memory_order_acquire);
    
    // Read in up to two chunks (handle wrap-around)
    size_t read_pos = r & rb->mask;
    size_t chunk1 = rb->capacity - read_pos;
    
    if (chunk1 > to_read) {
        chunk1 = to_read;
    }
    
    memcpy(data, &rb->buffer[read_pos], chunk1 * sizeof(float));
    
    if (to_read > chunk1) {
        size_t chunk2 = to_read - chunk1;
        memcpy(&data[chunk1], &rb->buffer[0], chunk2 * sizeof(float));
    }
    
    // Update read index
    atomic_store_explicit(&rb->read_index, r + to_read, memory_order_relaxed);
    
    return to_read;
}

void ring_buffer_reset(ring_buffer_t *rb) {
    atomic_store(&rb->write_index, 0);
    atomic_store(&rb->read_index, 0);
    memset(rb->buffer, 0, rb->capacity * sizeof(float));
}