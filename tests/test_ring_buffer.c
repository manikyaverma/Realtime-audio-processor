#include "../src/ring_buffer.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>

// Test utilities
#define TEST(name) \
    static void test_##name(); \
    static void run_test_##name() { \
        printf("Running: %s...", #name); \
        test_##name(); \
        printf(" PASSED\n"); \
    } \
    static void test_##name()

#define RUN_TEST(name) run_test_##name()

// Test 1: Basic creation and destruction
TEST(create_destroy) {
    ring_buffer_t *rb = ring_buffer_create(1024);
    assert(rb != NULL);
    assert(rb->capacity == 1024);
    assert(rb->mask == 1023);
    ring_buffer_free(rb);
}

// Test 2: Reject non-power-of-2 sizes
TEST(reject_non_power_of_2) {
    ring_buffer_t *rb = ring_buffer_create(1000);
    assert(rb == NULL);
}

// Test 3: Write and read single sample
TEST(write_read_single) {
    ring_buffer_t *rb = ring_buffer_create(64);
    assert(rb != NULL);
    
    float write_data = 0.5f;
    float read_data = 0.0f;
    
    size_t written = ring_buffer_write(rb, &write_data, 1);
    assert(written == 1);
    assert(ring_buffer_read_available(rb) == 1);
    
    size_t read = ring_buffer_read(rb, &read_data, 1);
    assert(read == 1);
    assert(read_data == 0.5f);
    assert(ring_buffer_read_available(rb) == 0);
    
    ring_buffer_free(rb);
}

// Test 4: Write and read multiple samples
TEST(write_read_multiple) {
    ring_buffer_t *rb = ring_buffer_create(128);
    assert(rb != NULL);
    
    float write_data[32];
    float read_data[32];
    
    for (int i = 0; i < 32; i++) {
        write_data[i] = (float)i / 32.0f;
    }
    
    size_t written = ring_buffer_write(rb, write_data, 32);
    assert(written == 32);
    assert(ring_buffer_read_available(rb) == 32);
    
    size_t read = ring_buffer_read(rb, read_data, 32);
    assert(read == 32);
    
    for (int i = 0; i < 32; i++) {
        assert(read_data[i] == write_data[i]);
    }
    
    ring_buffer_free(rb);
}

// Test 5: Buffer full condition
TEST(buffer_full) {
    ring_buffer_t *rb = ring_buffer_create(64);
    assert(rb != NULL);
    
    float data[64];
    memset(data, 0, sizeof(data));
    
    // Fill buffer completely
    size_t written = ring_buffer_write(rb, data, 64);
    assert(written == 64);
    assert(ring_buffer_write_available(rb) == 0);
    
    // Try to write more - should fail
    written = ring_buffer_write(rb, data, 10);
    assert(written == 0);
    
    ring_buffer_free(rb);
}

// Test 6: Buffer empty condition
TEST(buffer_empty) {
    ring_buffer_t *rb = ring_buffer_create(64);
    assert(rb != NULL);
    
    float data[10];
    
    // Try to read from empty buffer
    size_t read = ring_buffer_read(rb, data, 10);
    assert(read == 0);
    
    ring_buffer_free(rb);
}

// Test 7: Wrap-around behavior
TEST(wrap_around) {
    ring_buffer_t *rb = ring_buffer_create(64);
    assert(rb != NULL);
    
    float write_data[40];
    float read_data[40];
    
    for (int i = 0; i < 40; i++) {
        write_data[i] = (float)i;
    }
    
    // Write 40, read 30, write 30 (this will wrap around)
    ring_buffer_write(rb, write_data, 40);
    ring_buffer_read(rb, read_data, 30);
    
    // Now write 30 more (should wrap)
    size_t written = ring_buffer_write(rb, write_data, 30);
    assert(written == 30);
    
    // Read remaining 10 from first write + 30 from second write
    assert(ring_buffer_read_available(rb) == 40);
    
    ring_buffer_free(rb);
}

// Test 8: Reset functionality
TEST(reset) {
    ring_buffer_t *rb = ring_buffer_create(64);
    assert(rb != NULL);
    
    float data[32];
    memset(data, 1, sizeof(data));
    
    ring_buffer_write(rb, data, 32);
    assert(ring_buffer_read_available(rb) == 32);
    
    ring_buffer_reset(rb);
    assert(ring_buffer_read_available(rb) == 0);
    assert(ring_buffer_write_available(rb) == 64);
    
    ring_buffer_free(rb);
}

// Test 9: Producer-consumer threading test
typedef struct {
    ring_buffer_t *rb;
    int samples_to_produce;
    int samples_consumed;
} thread_test_data_t;

static void* producer_thread(void *arg) {
    thread_test_data_t *data = (thread_test_data_t*)arg;
    
    for (int i = 0; i < data->samples_to_produce; i++) {
        float sample = (float)i;
        
        // Busy wait until we can write
        while (ring_buffer_write(data->rb, &sample, 1) == 0) {
            // Spin (in real code, you'd yield or sleep)
        }
    }
    
    return NULL;
}

static void* consumer_thread(void *arg) {
    thread_test_data_t *data = (thread_test_data_t*)arg;
    
    int consumed = 0;
    while (consumed < data->samples_to_produce) {
        float sample;
        if (ring_buffer_read(data->rb, &sample, 1) == 1) {
            assert(sample == (float)consumed);
            consumed++;
        }
    }
    
    data->samples_consumed = consumed;
    return NULL;
}

TEST(threading) {
    ring_buffer_t *rb = ring_buffer_create(256);
    assert(rb != NULL);
    
    thread_test_data_t data = {
        .rb = rb,
        .samples_to_produce = 10000,
        .samples_consumed = 0
    };
    
    pthread_t prod_thread, cons_thread;
    
    pthread_create(&cons_thread, NULL, consumer_thread, &data);
    pthread_create(&prod_thread, NULL, producer_thread, &data);
    
    pthread_join(prod_thread, NULL);
    pthread_join(cons_thread, NULL);
    
    assert(data.samples_consumed == 10000);
    
    ring_buffer_free(rb);
}

// Main test runner
int main(void) {
    printf("===== Ring Buffer Tests =====\n");
    
    RUN_TEST(create_destroy);
    RUN_TEST(reject_non_power_of_2);
    RUN_TEST(write_read_single);
    RUN_TEST(write_read_multiple);
    RUN_TEST(buffer_full);
    RUN_TEST(buffer_empty);
    RUN_TEST(wrap_around);
    RUN_TEST(reset);
    RUN_TEST(threading);
    
    printf("\n✓ All tests passed!\n");
    return 0;
}