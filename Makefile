
CC := gcc -std=c99 -pedantic
CFLAGS := -Wall -Werror
LDFLAGS :=

BUILD_DIR := build

SOURCE := $(wildcard *.c)
BINARIES := $(patsubst %.c,$(BUILD_DIR)/%,$(SOURCE))

.PHONY: all clean $(BUILD_DIR)/utils
all: $(BINARIES)
clean:
	rm -rf $(BUILD_DIR)

# This is a bit of a hack to get make to ignore utils.c
$(BUILD_DIR)/utils:

$(BUILD_DIR)/%: $(BUILD_DIR)/%.o $(BUILD_DIR)/utils.o
	@mkdir -p $(BUILD_DIR)
	$(CC) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/%.o: %.c utils.h
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<
