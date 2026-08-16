# Shared build rules for independently compiled TabOS applications.

TABOS_APPLICATION_MAKEFILE := $(lastword $(MAKEFILE_LIST))

ifndef APP_NAME
$(error APP_NAME must be set before including application.mk)
endif

APP_DIR ?= $(CURDIR)
SDK_ROOT ?= $(abspath $(APP_DIR)/../../sdk)
PROJECT_ROOT ?= $(abspath $(SDK_ROOT)/..)
BUILD_DIR ?= $(PROJECT_ROOT)/build/apps/$(APP_NAME)
OUTPUT ?= $(BUILD_DIR)/$(APP_NAME).bin
UNSTRIPPED ?= $(BUILD_DIR)/$(APP_NAME).elf
INSTALL_PATH ?= $(PROJECT_ROOT)/.local/rootfs/T/bin/$(APP_NAME).bin

RISCV_PREFIX ?= riscv32-esp-elf-
CC := $(RISCV_PREFIX)gcc
STRIP := $(RISCV_PREFIX)strip
SIZE := $(RISCV_PREFIX)size

TABOS_CPPFLAGS := -I$(SDK_ROOT)/include -I$(APP_DIR)/include
TABOS_CFLAGS := -march=rv32i_zicsr_zifencei -mabi=ilp32 -Os
TABOS_CFLAGS += -std=c17 -fno-pic -mno-relax -msmall-data-limit=0
TABOS_CFLAGS += -ffunction-sections -fdata-sections
TABOS_CFLAGS += -fno-stack-protector -fno-asynchronous-unwind-tables
TABOS_LDFLAGS := -nostartfiles -Wl,-T,$(SDK_ROOT)/linker/app-riscv32.ld
TABOS_LDFLAGS += -Wl,--build-id=none -Wl,--gc-sections -Wl,-N -Wl,--emit-relocs
TABOS_RUNTIME_SOURCES := $(SDK_ROOT)/crt/crt0.c $(SDK_ROOT)/libc/syscalls.c \
                         $(SDK_ROOT)/lib/process.c

.PHONY: all build clean install size

all: install

build: $(OUTPUT)

$(UNSTRIPPED): $(SOURCES) $(TABOS_RUNTIME_SOURCES) $(SDK_ROOT)/linker/app-riscv32.ld $(TABOS_APPLICATION_MAKEFILE)
	@mkdir -p $(dir $@)
	$(CC) $(TABOS_CPPFLAGS) $(TABOS_CFLAGS) $(TABOS_LDFLAGS) -o $@ $(TABOS_RUNTIME_SOURCES) $(SOURCES)

$(OUTPUT): $(UNSTRIPPED)
	$(STRIP) --strip-debug $< -o $@
	$(SIZE) $@

install: $(OUTPUT)
	@mkdir -p $(dir $(INSTALL_PATH))
	cp $(OUTPUT) $(INSTALL_PATH)

size: $(OUTPUT)
	$(SIZE) $(OUTPUT)

clean:
	rm -rf $(BUILD_DIR)
