# Shared build rules for independently compiled TabOS applications.

TABOS_APPLICATION_MAKEFILE := $(lastword $(MAKEFILE_LIST))
TABOS_APPLICATION_MAKEFILES := $(MAKEFILE_LIST)

ifndef APP_NAME
$(error APP_NAME must be set before including application.mk)
endif

# Keep paths in Make's dependency graph relative. GNU Make treats whitespace as
# a separator in target and prerequisite names, so absolute paths derived from
# CURDIR break when the repository lives below a directory containing spaces.
APP_DIR ?= .
SDK_ROOT ?= ../../sdk
PROJECT_ROOT ?= ../..
BUILD_DIR ?= $(PROJECT_ROOT)/build/apps/$(APP_NAME)
OUTPUT ?= $(BUILD_DIR)/$(APP_NAME)
UNSTRIPPED ?= $(BUILD_DIR)/$(APP_NAME).elf
INSTALL_PATH ?= $(PROJECT_ROOT)/.local/rootfs/T/bin/$(APP_NAME)
INSTALL_DATA_PATH ?= $(PROJECT_ROOT)/.local/rootfs/T/data/$(APP_NAME)
TABOS_RUNTIME_ASSETS ?=
TABOS_BUILD_PREREQUISITES ?=

RISCV_PREFIX ?= riscv32-esp-elf-
CC := $(RISCV_PREFIX)gcc
STRIP := $(RISCV_PREFIX)strip
SIZE := $(RISCV_PREFIX)size
READELF := $(RISCV_PREFIX)readelf

TABOS_APP_HEAP_BYTES ?= 262144
TABOS_APP_STACK_BYTES ?= 16384
TABOS_APP_CAPABILITIES ?= 1
TABOS_APP_ABI_VERSION ?= 3
TABOS_POINTER_MAX_CONTACTS ?= 5

TABOS_CPPFLAGS := -I$(SDK_ROOT)/include -I$(APP_DIR)/include -DTABOS_APPLICATION=1
TABOS_CPPFLAGS += -DTABOS_APP_HEAP_BYTES=$(TABOS_APP_HEAP_BYTES)
TABOS_CPPFLAGS += -DTABOS_APP_STACK_BYTES=$(TABOS_APP_STACK_BYTES)
TABOS_CPPFLAGS += -DTABOS_APP_CAPABILITIES=$(TABOS_APP_CAPABILITIES)
TABOS_CPPFLAGS += -DTABOS_APP_ABI_VERSION=$(TABOS_APP_ABI_VERSION)
TABOS_CPPFLAGS += -DTABOS_POINTER_MAX_CONTACTS=$(TABOS_POINTER_MAX_CONTACTS)
TABOS_CFLAGS := -march=rv32i_zicsr_zifencei -mabi=ilp32 -Os
TABOS_CFLAGS += -std=c17 -fno-pic -mno-relax -msmall-data-limit=0
TABOS_CFLAGS += -ffunction-sections -fdata-sections
TABOS_CFLAGS += -fno-stack-protector -fno-asynchronous-unwind-tables
TABOS_LDFLAGS := -nostartfiles -Wl,-T,$(SDK_ROOT)/linker/app-riscv32.ld
TABOS_LDFLAGS += -Wl,--build-id=none -Wl,--gc-sections -Wl,-N -Wl,--emit-relocs
TABOS_RUNTIME_SOURCES := $(SDK_ROOT)/crt/crt0.c $(SDK_ROOT)/crt/metadata.S $(SDK_ROOT)/libc/syscalls.c \
                         $(SDK_ROOT)/lib/process.c \
                         $(SDK_ROOT)/lib/graphics.c \
                         $(SDK_ROOT)/lib/sprite.c \
                         $(SDK_ROOT)/lib/tilemap.c \
                         $(SDK_ROOT)/lib/input.c \
                         $(SDK_ROOT)/lib/network.c \
                         $(SDK_ROOT)/lib/wait.c \
                         $(SDK_ROOT)/lib/camera.c \
                         $(SDK_ROOT)/lib/tls.c \
                         $(SDK_ROOT)/lib/battery.c \
                         $(SDK_ROOT)/lib/audio.c \
                         $(SDK_ROOT)/lib/pointer.c \
                         $(SDK_ROOT)/lib/clock.c \
                         $(SDK_ROOT)/lib/reboot.c \
                         $(SDK_ROOT)/lib/runtime.c \
                         $(SDK_ROOT)/lib/device.c \
                         $(SDK_ROOT)/lib/posix_filesystem.c
TABOS_BUILD_CONFIG := $(BUILD_DIR)/.tabos-build-config
TABOS_DEPENDENCY_FILE := $(BUILD_DIR)/.tabos-dependencies.mk
TABOS_CONFIG_INVALIDATES ?= $(UNSTRIPPED) $(OUTPUT) $(TABOS_DEPENDENCY_FILE)

.PHONY: all build clean install stage-assets install-assets size metadata tabos-list-outputs tabos-list-runtime-assets
.PHONY: tabos-force-build-config

all: install

build: $(OUTPUT) stage-assets

tabos-force-build-config:

$(TABOS_BUILD_CONFIG): tabos-force-build-config
	@mkdir -p $(dir $@)
	@printf '%s\n' \
		'APP_NAME=$(APP_NAME)' \
		'CC=$(CC)' \
		'STRIP=$(STRIP)' \
		'TABOS_CPPFLAGS=$(TABOS_CPPFLAGS)' \
		'TABOS_CFLAGS=$(TABOS_CFLAGS)' \
		'TABOS_LDFLAGS=$(TABOS_LDFLAGS)' \
		'SOURCES=$(SOURCES)' \
		'TABOS_RUNTIME_SOURCES=$(TABOS_RUNTIME_SOURCES)' \
		'TABOS_BUILD_PREREQUISITES=$(TABOS_BUILD_PREREQUISITES)' > "$@.tmp"
	@if [ -f "$@" ] && cmp -s "$@.tmp" "$@"; then \
		rm -f "$@.tmp"; \
	else \
		mv "$@.tmp" "$@"; \
		rm -f $(TABOS_CONFIG_INVALIDATES); \
	fi

ifndef TABOS_CUSTOM_BUILD
$(TABOS_DEPENDENCY_FILE): $(TABOS_BUILD_CONFIG) $(SOURCES) $(TABOS_RUNTIME_SOURCES) $(TABOS_BUILD_PREREQUISITES) $(TABOS_APPLICATION_MAKEFILES)
	@mkdir -p $(dir $@)
	@$(CC) $(TABOS_CPPFLAGS) $(TABOS_CFLAGS) -MM -MP -MT "$(UNSTRIPPED)" -MT "$@" \
		$(TABOS_RUNTIME_SOURCES) $(SOURCES) > "$@.tmp"
	@mv "$@.tmp" "$@"

$(UNSTRIPPED): $(TABOS_BUILD_CONFIG) $(TABOS_DEPENDENCY_FILE) $(TABOS_APPLICATION_MAKEFILES)

$(UNSTRIPPED): $(SOURCES) $(TABOS_RUNTIME_SOURCES) $(TABOS_BUILD_PREREQUISITES) $(SDK_ROOT)/linker/app-riscv32.ld $(TABOS_APPLICATION_MAKEFILE)
	@mkdir -p $(dir $@)
	$(CC) $(TABOS_CPPFLAGS) $(TABOS_CFLAGS) $(TABOS_LDFLAGS) -o "$@" $(TABOS_RUNTIME_SOURCES) $(SOURCES)

clean:
	rm -rf "$(BUILD_DIR)"
endif

$(OUTPUT): $(TABOS_BUILD_CONFIG)

$(OUTPUT): $(UNSTRIPPED)
	$(STRIP) --strip-unneeded "$<" -o "$@"
	$(SIZE) "$@"

install: $(OUTPUT) install-assets
	@mkdir -p $(dir $(INSTALL_PATH))
	cp "$(OUTPUT)" "$(INSTALL_PATH)"

stage-assets: $(TABOS_RUNTIME_ASSETS)
	@mkdir -p $(BUILD_DIR)/data
	@for asset in "$(BUILD_DIR)/data/"*; do if [ -f "$$asset" ]; then rm -f "$$asset"; fi; done
ifneq ($(strip $(TABOS_RUNTIME_ASSETS)),)
	@for asset in $(TABOS_RUNTIME_ASSETS); do cp "$$asset" "$(BUILD_DIR)/data/$${asset##*/}"; done
endif

install-assets: stage-assets
ifneq ($(strip $(TABOS_RUNTIME_ASSETS)),)
	@mkdir -p $(INSTALL_DATA_PATH)
	@for asset in $(BUILD_DIR)/data/*; do cp "$$asset" "$(INSTALL_DATA_PATH)/$${asset##*/}"; done
endif

size: $(OUTPUT)
	$(SIZE) "$(OUTPUT)"

metadata: $(OUTPUT)
	$(READELF) -n "$(OUTPUT)"

tabos-list-outputs:
	@printf '%s\n' "$(abspath $(OUTPUT))"

tabos-list-runtime-assets:
	@for asset in $(TABOS_RUNTIME_ASSETS); do \
		printf '%s/%s\n' "$(abspath $(BUILD_DIR)/data)" "$${asset##*/}"; \
	done

ifeq ($(filter clean tabos-list-outputs tabos-list-runtime-assets,$(MAKECMDGOALS)),)
-include $(TABOS_DEPENDENCY_FILE)
endif
