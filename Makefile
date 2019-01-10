.PHONY: all clean

CC := gcc -std=c99 -pedantic
CFLAGS := -Wall -Werror -g
LDFLAGS :=

BUILD_DIR := build

SOURCE := $(wildcard *.c)
BINARIES := $(patsubst %.c,$(BUILD_DIR)/%,$(SOURCE))

all: $(BINARIES)
clean:
	rm -rf $(BUILD_DIR)

$(BUILD_DIR)/%: %.c utils.h
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $<
