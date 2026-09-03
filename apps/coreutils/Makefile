# Small independently loaded command-line utilities.

PROGRAMS := ls mkdir rmdir cp rm mv cat hexdump date reboot shutdown wc battery devices audiotest touchtest

ifeq ($(strip $(PROGRAM)),)

.PHONY: all build install clean $(PROGRAMS)

all: $(PROGRAMS)
build: $(PROGRAMS:%=build-%)
install: $(PROGRAMS)

$(PROGRAMS):
	$(MAKE) --no-print-directory PROGRAM=$@ -f Makefile program

$(PROGRAMS:%=build-%): build-%:
	$(MAKE) --no-print-directory PROGRAM=$* -f Makefile build

clean:
	@for program in $(PROGRAMS); do \
		$(MAKE) --no-print-directory PROGRAM=$$program -f Makefile clean; \
	done

else

APP_NAME := $(PROGRAM)
APP_DIR := .
PROJECT_ROOT := ../..
BUILD_DIR := $(PROJECT_ROOT)/build/apps/coreutils/$(PROGRAM)
OUTPUT := $(BUILD_DIR)/$(PROGRAM)
UNSTRIPPED := $(BUILD_DIR)/$(PROGRAM).elf
INSTALL_PATH := $(PROJECT_ROOT)/.local/rootfs/T/bin/$(PROGRAM)
SOURCES := $(APP_DIR)/src/$(PROGRAM)/main.c

include ../../sdk/make/application.mk

.PHONY: program
program: install

endif
