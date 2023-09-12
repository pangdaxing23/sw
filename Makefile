CC := gcc
CFLAGS := -Wall -Wextra -Werror
TARGET := sw
BIN := ./bin
SRC := ./src

DEBUG_FLAGS := -g -O0
RELEASE_FLAGS := -O3

BUILD ?= release
ifeq ($(BUILD),debug)
    CFLAGS += $(DEBUG_FLAGS)
else
    CFLAGS += $(RELEASE_FLAGS)
endif

BIN := ./bin/$(BUILD)

ifeq ($(PREFIX),)
	PREFIX := /usr/local
endif

$(BIN)/$(TARGET): $(SRC)/$(TARGET).c
	mkdir -p $(BIN)
	$(CC) $(CFLAGS) $^ -o $@

install: $(BIN)/$(TARGET)
	install -d $(PREFIX)/bin
	install $(BIN)/$(TARGET) $(PREFIX)/bin

uninstall: $(PREFIX)/bin/$(TARGET)
	rm -f $(PREFIX)/bin/$(TARGET)

clean:
	rm -rf ./bin

.PHONY: install uninstall clean

