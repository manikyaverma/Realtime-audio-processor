CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2 -pthread
LDFLAGS = -pthread -lm -lasound

SRC_DIR = src
TEST_DIR = tests

# Source files
SRCS = $(SRC_DIR)/ring_buffer.c $(SRC_DIR)/audio_io.c $(SRC_DIR)/effects.c

# Main executable
TARGET = audio_processor

# Test executable
TEST_TARGET = test_ring_buffer

.PHONY: all clean test run

all: $(TARGET)

$(TARGET): $(SRCS) $(SRC_DIR)/main.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

test: $(TEST_TARGET)
	./$(TEST_TARGET)

$(TEST_TARGET): $(SRC_DIR)/ring_buffer.c $(TEST_DIR)/test_ring_buffer.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) $(TEST_TARGET)