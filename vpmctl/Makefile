CC := gcc

CFLAGS := -Wall -Wextra -Werror -O2

BUILD_DIR := build

TARGET := $(BUILD_DIR)/vpmctl

SRCS := main.c

OBJS := $(SRCS:%.c=$(BUILD_DIR)/%.o)

.PHONY: all clean run

all: $(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

$(BUILD_DIR)/%.o: %.c vpmctl.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET) info

clean:
	rm -rf $(BUILD_DIR)