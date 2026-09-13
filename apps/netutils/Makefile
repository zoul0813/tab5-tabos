# Network utilities are independently loaded command-line programs.

PROGRAMS := netctl ping nettest ntpdate httpsget fetch irc

ifeq ($(strip $(PROGRAM)),)

.PHONY: all build install clean tabos-list-outputs tabos-list-runtime-assets $(PROGRAMS)

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

tabos-list-outputs tabos-list-runtime-assets:
	@for program in $(PROGRAMS); do \
		$(MAKE) --no-print-directory -s PROGRAM=$$program -f Makefile $@; \
	done

else

APP_NAME := $(PROGRAM)
APP_DIR := .
PROJECT_ROOT := ../..
BUILD_DIR := $(PROJECT_ROOT)/build/apps/netutils/$(PROGRAM)
OUTPUT := $(BUILD_DIR)/$(PROGRAM)
UNSTRIPPED := $(BUILD_DIR)/$(PROGRAM).elf
INSTALL_PATH := $(PROJECT_ROOT)/.local/rootfs/T/bin/$(PROGRAM)
SOURCES := $(APP_DIR)/src/$(PROGRAM)/main.c
ifeq ($(PROGRAM),irc)
SOURCES += $(APP_DIR)/src/irc/protocol.c
endif

include ../../sdk/make/application.mk

.PHONY: program
program: install

endif
