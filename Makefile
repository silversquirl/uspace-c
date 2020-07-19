CC := clang -std=c99 -pedantic
CFLAGS := -Wall -Werror -D_XOPEN_SOURCE=700
LDFLAGS :=

ifdef DEBUG
CFLAGS += -g
endif

BUILD_DIR := build

SOURCE := $(filter-out utils.c,$(wildcard *.c))
BINARIES := $(patsubst %.c,$(BUILD_DIR)/%,$(SOURCE))

.PHONY: all clean $(BUILD_DIR)/utils
all: $(BINARIES)
clean:
	rm -rf $(BUILD_DIR)

$(BUILD_DIR)/%: $(BUILD_DIR)/%.o $(BUILD_DIR)/utils.o
	@mkdir -p $(BUILD_DIR)
	$(CC) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/%.o: %.c utils.h
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<
