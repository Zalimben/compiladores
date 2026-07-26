CC ?= cc
CPPFLAGS := -Iinclude
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic

TARGET := minic
SOURCES := \
	src/main.c \
	src/options.c \
	src/driver.c \
	src/process.c \
	src/diagnostics.c
OBJECTS := $(SOURCES:src/%.c=build/%.o)
HEADERS := $(wildcard include/*.h)

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o $@

build/%.o: src/%.c $(HEADERS)
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

test: $(TARGET)
	sh tests/driver/test_driver.sh ./$(TARGET)

clean:
	rm -rf build $(TARGET)
