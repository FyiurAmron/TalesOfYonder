# B2 real C port: ordinary C11, no generated code, no guest CPU/memory.
#
#   make -C engine/port            # build into build/port
#   make -C engine/port test       # build and run the unit tests
#
# BUILD and CFLAGS may be overridden from the environment.

ROOT := $(abspath ../..)
BUILD ?= $(ROOT)/build/port
CC ?= cc
CFLAGS ?= -std=c11 -O2 -g -Wall -Wextra -Wpedantic -Werror
CPPFLAGS += -Iinclude

LIB_SOURCES := src/boot.c src/ingamemenu.c src/catalog.c src/classes.c \
               src/clock.c src/combat.c \
               src/cluebook.c src/create.c src/cursor.c src/file.c src/intro.c \
               src/items.c \
               src/lists.c src/mapview.c src/titlemenu.c src/monster_detail.c \
               src/monsters.c src/palette.c src/pcx.c \
               src/party.c src/pictures.c src/present.c src/protocol.c \
               src/room.c \
               src/savefile.c src/spell_detail.c src/spells.c src/surface.c \
               src/item_detail.c \
               src/text.c \
               src/transport_folder.c src/walkthrough.c src/world.c
LIB_OBJECTS := $(patsubst src/%.c,$(BUILD)/obj/src/%.o,$(LIB_SOURCES))
TEST_SOURCES := tests/test_boot.c tests/test_ingamemenu.c tests/test_catalog.c \
                tests/test_classes.c tests/test_combat.c \
                tests/test_cluebook.c tests/test_create.c tests/test_cursor.c \
                tests/test_intro.c tests/test_items.c \
                tests/test_main.c tests/test_mapview.c \
                tests/test_monster_detail.c tests/test_monsters.c \
                tests/test_palette.c tests/test_party.c tests/test_pictures.c \
                tests/test_present.c tests/test_protocol.c tests/test_room.c \
                tests/test_savefile.c tests/test_spells.c tests/test_surface.c \
                tests/test_text.c tests/test_transport.c tests/test_walkthrough.c \
                tests/test_world.c
TEST_OBJECTS := $(patsubst tests/%.c,$(BUILD)/obj/tests/%.o,$(TEST_SOURCES))

.PHONY: all test clean

all: $(BUILD)/yt2p-boot $(BUILD)/yt2p-ingamemenu $(BUILD)/yt2p-catalog \
     $(BUILD)/yt2p-classes $(BUILD)/yt2p-combat \
     $(BUILD)/yt2p-cluebook $(BUILD)/yt2p-info $(BUILD)/yt2p-intro \
     $(BUILD)/yt2p-items \
     $(BUILD)/yt2p-mapview $(BUILD)/yt2p-monsters $(BUILD)/yt2p-pictures \
     $(BUILD)/yt2p-play $(BUILD)/yt2p-render $(BUILD)/yt2p-room \
     $(BUILD)/yt2p-spells \
     $(BUILD)/yt2p-walkthrough $(BUILD)/yt2p-world $(BUILD)/yt2p-tests

test: all
	$(BUILD)/yt2p-tests

$(BUILD)/yt2p-items: $(LIB_OBJECTS) $(BUILD)/obj/src/tool_items.o
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD)/yt2p-info: $(LIB_OBJECTS) $(BUILD)/obj/src/tool_info.o
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD)/yt2p-intro: $(LIB_OBJECTS) $(BUILD)/obj/src/tool_intro.o
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD)/yt2p-world: $(LIB_OBJECTS) $(BUILD)/obj/src/tool_world.o
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD)/yt2p-pictures: $(LIB_OBJECTS) $(BUILD)/obj/src/tool_pictures.o
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD)/yt2p-boot: $(LIB_OBJECTS) $(BUILD)/obj/src/tool_boot.o
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD)/yt2p-ingamemenu: $(LIB_OBJECTS) $(BUILD)/obj/src/tool_ingamemenu.o
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD)/yt2p-combat: $(LIB_OBJECTS) $(BUILD)/obj/src/tool_combat.o
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD)/yt2p-catalog: $(LIB_OBJECTS) $(BUILD)/obj/src/tool_catalog.o
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD)/yt2p-classes: $(LIB_OBJECTS) $(BUILD)/obj/src/tool_classes.o
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD)/yt2p-cluebook: $(LIB_OBJECTS) $(BUILD)/obj/src/tool_cluebook.o
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD)/yt2p-mapview: $(LIB_OBJECTS) $(BUILD)/obj/src/tool_mapview.o
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD)/yt2p-monsters: $(LIB_OBJECTS) $(BUILD)/obj/src/tool_monsters.o
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD)/yt2p-spells: $(LIB_OBJECTS) $(BUILD)/obj/src/tool_spells.o
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD)/yt2p-render: $(LIB_OBJECTS) $(BUILD)/obj/src/tool_render.o
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD)/yt2p-room: $(LIB_OBJECTS) $(BUILD)/obj/src/tool_room.o
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD)/yt2p-play: $(LIB_OBJECTS) $(BUILD)/obj/src/tool_play.o
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD)/yt2p-walkthrough: $(LIB_OBJECTS) $(BUILD)/obj/src/tool_walkthrough.o
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD)/yt2p-tests: $(LIB_OBJECTS) $(TEST_OBJECTS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD)/obj/src/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/obj/tests/%.o: tests/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

clean:
	rm -rf $(BUILD)/obj $(BUILD)/yt2p-boot $(BUILD)/yt2p-catalog \
	       $(BUILD)/yt2p-classes $(BUILD)/yt2p-cluebook $(BUILD)/yt2p-info \
	       $(BUILD)/yt2p-intro \
	       $(BUILD)/yt2p-items $(BUILD)/yt2p-mapview \
	       $(BUILD)/yt2p-monsters $(BUILD)/yt2p-pictures $(BUILD)/yt2p-play \
	       $(BUILD)/yt2p-render $(BUILD)/yt2p-spells $(BUILD)/yt2p-walkthrough \
	       $(BUILD)/yt2p-world $(BUILD)/yt2p-tests

-include $(wildcard $(BUILD)/obj/src/*.d $(BUILD)/obj/tests/*.d)
