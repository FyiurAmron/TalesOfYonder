ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
BUILD ?= $(abspath $(ROOT)/../../build/c)
GENERATED ?= $(BUILD)/generated

CC ?= cc
CFLAGS ?= -std=c11 -O2 -g
WARNINGS := -Wall -Wextra -Wpedantic -Werror
CPPFLAGS += -I$(ROOT)/include -I$(GENERATED)
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

GAME_SOURCES := \
	src/game/game_link.c \
	src/game/game_machine.c \
	src/game/game_services.c

GAME_MAIN_SOURCE := src/game/game_main.c

GAME_GENERATED := $(sort $(wildcard $(GENERATED)/game_*.c))

TEST_SOURCES := \
	tests/test_main.c \
	tests/test_memory.c \
	tests/test_cpu.c \
	tests/test_protocol.c \
	tests/test_service.c \
	tests/test_video.c

GAME_TEST_SOURCES := tests/test_game_main.c

LIB_OBJECTS := $(LIB_SOURCES:src/%.c=$(BUILD)/obj/src/%.o)
GAME_OBJECTS := $(GAME_SOURCES:src/%.c=$(BUILD)/obj/src/%.o) \
	$(GAME_GENERATED:$(GENERATED)/%.c=$(BUILD)/obj/generated/%.o)
GAME_MAIN_OBJECT := $(GAME_MAIN_SOURCE:src/%.c=$(BUILD)/obj/src/%.o)
TEST_OBJECTS := $(TEST_SOURCES:tests/%.c=$(BUILD)/obj/tests/%.o)
GAME_TEST_OBJECTS := $(GAME_TEST_SOURCES:tests/%.c=$(BUILD)/obj/tests/%.o)
STUB_OBJECT := $(BUILD)/obj/stub.o

STUB := $(BUILD)/yt2stub
GAME := $(BUILD)/yt2game
TESTS := $(BUILD)/yt2core-tests
GAME_TESTS := $(BUILD)/yt2game-tests

.PHONY: all test clean

all: $(STUB) $(GAME) $(TESTS) $(GAME_TESTS)

$(BUILD)/obj/src/%.o: $(ROOT)/src/%.c
	@mkdir -p $(dir $@)
	$(COMPILE) -c $< -o $@

$(BUILD)/obj/generated/%.o: $(GENERATED)/%.c
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

$(GAME): $(LIB_OBJECTS) $(GAME_OBJECTS) $(GAME_MAIN_OBJECT)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $^ -o $@

$(TESTS): $(LIB_OBJECTS) $(TEST_OBJECTS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $^ -o $@

$(GAME_TESTS): $(LIB_OBJECTS) $(GAME_OBJECTS) $(GAME_TEST_OBJECTS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $^ -o $@

test: all
	$(TESTS)
	$(GAME_TESTS)

clean:
	rm -rf $(BUILD)

-include $(LIB_OBJECTS:.o=.d) $(GAME_OBJECTS:.o=.d) $(GAME_MAIN_OBJECT:.o=.d) \
	$(TEST_OBJECTS:.o=.d) $(GAME_TEST_OBJECTS:.o=.d) $(STUB_OBJECT:.o=.d)
