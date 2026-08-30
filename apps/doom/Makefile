# Optional pinned DOOM application. Never built by apps/build.sh unless requested.

APP_NAME := doom
TABOS_APP_HEAP_BYTES := 8388608
TABOS_APP_STACK_BYTES := 65536
TABOS_CUSTOM_BUILD := 1
APP_DIR := $(CURDIR)

include ../../sdk/make/application.mk

DOOMGENERIC_REPOSITORY := https://github.com/ozkl/doomgeneric.git
DOOMGENERIC_COMMIT := dcb7a8dbc7a16ce3dda29382ac9aae9d77d21284
DOOMGENERIC_CACHE_DIR := $(APP_DIR)/doomgeneric
DOOM_BUILD_SOURCE_DIR := $(BUILD_DIR)/source
DOOM_UPSTREAM_DIR := $(DOOM_BUILD_SOURCE_DIR)/doomgeneric
DOOM_OBJECT_DIR := $(BUILD_DIR)/objects
DOOMGENERIC_SOURCE_DIR ?=

DOOM_UPSTREAM_SOURCES := \
    dummy.c am_map.c doomdef.c doomstat.c dstrings.c d_event.c d_items.c d_iwad.c \
    d_loop.c d_main.c d_mode.c d_net.c f_finale.c f_wipe.c g_game.c hu_lib.c \
    hu_stuff.c info.c i_cdmus.c i_endoom.c i_joystick.c i_scale.c i_sound.c \
    i_system.c i_timer.c memio.c m_argv.c m_bbox.c m_cheat.c m_config.c \
    m_controls.c m_fixed.c m_menu.c m_misc.c m_random.c p_ceilng.c p_doors.c \
    p_enemy.c p_floor.c p_inter.c p_lights.c p_map.c p_maputl.c p_mobj.c \
    p_plats.c p_pspr.c p_saveg.c p_setup.c p_sight.c p_spec.c p_switch.c \
    p_telept.c p_tick.c p_user.c r_bsp.c r_data.c r_draw.c r_main.c r_plane.c \
    r_segs.c r_sky.c r_things.c sha1.c sounds.c statdump.c st_lib.c st_stuff.c \
    s_sound.c tables.c v_video.c wi_stuff.c w_checksum.c w_file.c w_main.c \
    w_wad.c z_zone.c w_file_stdc.c i_input.c i_video.c doomgeneric.c
DOOM_UPSTREAM_OBJECTS := $(addprefix $(DOOM_OBJECT_DIR)/upstream/,$(DOOM_UPSTREAM_SOURCES:.c=.o))
DOOM_UPSTREAM_SOURCE_PATHS := $(addprefix $(DOOM_UPSTREAM_DIR)/,$(DOOM_UPSTREAM_SOURCES))
DOOM_ADAPTER_SOURCES := $(APP_DIR)/src/doom_tabos.c $(APP_DIR)/src/doom_tabos_input.c \
    $(APP_DIR)/src/doom_tabos_storage.c $(APP_DIR)/src/doom_tabos_video.c
DOOM_ADAPTER_OBJECTS := $(patsubst $(APP_DIR)/src/%.c,$(DOOM_OBJECT_DIR)/adapter/%.o,$(DOOM_ADAPTER_SOURCES))
DOOM_PATCHES := $(wildcard $(APP_DIR)/patches/*.patch)

DOOM_UPSTREAM_CFLAGS := -D_POSIX_C_SOURCE=200809L
DOOM_UPSTREAM_CFLAGS += -DDOOMGENERIC_RESX=320 -DDOOMGENERIC_RESY=200
DOOM_UPSTREAM_CFLAGS += -DDOOMGENERIC_DISABLE_TEXTMODE
DOOM_ADAPTER_CFLAGS := -Wall -Wextra -Werror -I$(APP_DIR)/include

.PHONY: fetch verify-source prepare-source build clean install
.NOTPARALLEL:

all: install

fetch: $(DOOM_UPSTREAM_DIR)

$(DOOM_UPSTREAM_DIR): $(DOOM_PATCHES)
	@mkdir -p $(BUILD_DIR)
	@if [ -n "$(DOOMGENERIC_SOURCE_DIR)" ]; then \
		if [ ! -d "$(DOOMGENERIC_SOURCE_DIR)/doomgeneric" ]; then \
			printf '%s\n' 'apps/doom: DOOMGENERIC_SOURCE_DIR must contain doomgeneric/' >&2; \
			exit 1; \
		fi; \
		source_dir="$(DOOMGENERIC_SOURCE_DIR)"; \
	else \
		if [ ! -d "$(DOOMGENERIC_CACHE_DIR)/.git" ]; then \
			mkdir -p "$(dir $(DOOMGENERIC_CACHE_DIR))"; \
			git clone --no-checkout "$(DOOMGENERIC_REPOSITORY)" "$(DOOMGENERIC_CACHE_DIR)"; \
		fi; \
		git -C "$(DOOMGENERIC_CACHE_DIR)" fetch --depth 1 origin "$(DOOMGENERIC_COMMIT)"; \
		git -C "$(DOOMGENERIC_CACHE_DIR)" checkout --detach --force "$(DOOMGENERIC_COMMIT)"; \
		source_dir="$(DOOMGENERIC_CACHE_DIR)"; \
	fi; \
	actual_commit=$$(git -C "$$source_dir" rev-parse HEAD); \
	if [ "$$actual_commit" != "$(DOOMGENERIC_COMMIT)" ]; then \
		printf 'apps/doom: expected pinned commit %s, got %s\n' "$(DOOMGENERIC_COMMIT)" "$$actual_commit" >&2; \
		exit 1; \
	fi; \
	rm -rf "$(DOOM_BUILD_SOURCE_DIR)"; \
	cp -R "$$source_dir" "$(DOOM_BUILD_SOURCE_DIR)"; \
	for patch in $(DOOM_PATCHES); do \
		git -C "$(DOOM_BUILD_SOURCE_DIR)" apply --check "$$patch"; \
		git -C "$(DOOM_BUILD_SOURCE_DIR)" apply "$$patch"; \
	done; \
	printf 'apps/doom: prepared pinned source %s\n' "$(DOOMGENERIC_COMMIT)"

verify-source: fetch
	@test "$$(git -C "$(DOOM_BUILD_SOURCE_DIR)" rev-parse HEAD)" = "$(DOOMGENERIC_COMMIT)"

build: verify-source $(OUTPUT)

$(DOOM_UPSTREAM_SOURCE_PATHS): | $(DOOM_UPSTREAM_DIR)

$(DOOM_OBJECT_DIR)/upstream/%.o: $(DOOM_UPSTREAM_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(TABOS_CPPFLAGS) $(TABOS_CFLAGS) $(DOOM_UPSTREAM_CFLAGS) -w -I$(DOOM_UPSTREAM_DIR) -c $< -o $@

$(DOOM_ADAPTER_OBJECTS): $(DOOM_OBJECT_DIR)/adapter/%.o: $(APP_DIR)/src/%.c $(DOOM_UPSTREAM_DIR)/doomgeneric.h
	@mkdir -p $(dir $@)
	$(CC) $(TABOS_CPPFLAGS) $(TABOS_CFLAGS) $(DOOM_ADAPTER_CFLAGS) $(DOOM_UPSTREAM_CFLAGS) -I$(DOOM_UPSTREAM_DIR) -c $< -o $@

$(UNSTRIPPED): verify-source $(DOOM_UPSTREAM_OBJECTS) $(DOOM_ADAPTER_OBJECTS) $(TABOS_RUNTIME_SOURCES) $(SDK_ROOT)/linker/app-riscv32.ld $(TABOS_APPLICATION_MAKEFILE)
	@mkdir -p $(dir $@)
	$(CC) $(TABOS_CPPFLAGS) $(TABOS_CFLAGS) $(TABOS_LDFLAGS) -o $@ $(TABOS_RUNTIME_SOURCES) $(DOOM_UPSTREAM_OBJECTS) $(DOOM_ADAPTER_OBJECTS) -lm

clean:
	rm -rf $(BUILD_DIR)
