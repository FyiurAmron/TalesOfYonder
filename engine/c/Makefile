ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
BUILD ?= $(abspath $(ROOT)/../../build/c)

CC ?= cc
CFLAGS ?= -std=c11 -O2 -g
WARNINGS := -Wall -Wextra -Wpedantic -Werror
CPPFLAGS += -I$(ROOT)/include
COMPILE := $(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) -MMD -MP

LIB_SOURCES := \
	src/cpu.c \
	src/memory.c \
	src/platform.c \
	src/protocol.c \
	src/service.c \
	src/transport_folder.c \
	src/transport_null.c \
	src/transport_stream.c \
	src/video.c

TEST_SOURCES := \
	tests/test_main.c \
	tests/test_memory.c \
	tests/test_cpu.c \
	tests/test_protocol.c \
	tests/test_service.c \
	tests/test_video.c

LIB_OBJECTS := $(LIB_SOURCES:src/%.c=$(BUILD)/obj/src/%.o)
TEST_OBJECTS := $(TEST_SOURCES:tests/%.c=$(BUILD)/obj/tests/%.o)
STUB_OBJECT := $(BUILD)/obj/stub.o

STUB := $(BUILD)/yt2stub
TESTS := $(BUILD)/yt2core-tests

.PHONY: all test clean

all: $(STUB) $(TESTS)

$(BUILD)/obj/src/%.o: $(ROOT)/src/%.c
	@mkdir -p $(dir $@)
	$(COMPILE) -c $< -o $@

$(BUILD)/obj/tests/%.o: $(ROOT)/tests/%.c
	@mkdir -p $(dir $@)
	$(COMPILE) -c $< -o $@

$(STUB_OBJECT): $(ROOT)/src/stub.c
	@mkdir -p $(dir $@)
	$(COMPILE) -c $< -o $@

$(STUB): $(LIB_OBJECTS) $(STUB_OBJECT)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $^ -o $@

$(TESTS): $(LIB_OBJECTS) $(TEST_OBJECTS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $^ -o $@

test: all
	$(TESTS)

clean:
	rm -rf $(BUILD)

-include $(LIB_OBJECTS:.o=.d) $(TEST_OBJECTS:.o=.d) $(STUB_OBJECT:.o=.d)
