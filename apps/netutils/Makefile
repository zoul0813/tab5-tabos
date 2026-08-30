# Network utilities are independently loaded command-line programs.

PROGRAMS := netctl ping nettest ntpdate httpsget fetch irc

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
APP_DIR := $(CURDIR)
PROJECT_ROOT := $(abspath $(CURDIR)/../..)
BUILD_DIR := $(PROJECT_ROOT)/build/apps/netutils/$(PROGRAM)
OUTPUT := $(BUILD_DIR)/$(PROGRAM)
UNSTRIPPED := $(BUILD_DIR)/$(PROGRAM).elf
INSTALL_PATH := $(PROJECT_ROOT)/.local/rootfs/T/bin/$(PROGRAM)
SOURCES := $(CURDIR)/src/$(PROGRAM)/main.c

include ../../sdk/make/application.mk

.PHONY: program
program: install

endif
