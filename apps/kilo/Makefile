APP_NAME := kilo
SOURCES := src/main.c src/editor.c src/input.c src/render.c src/storage.c
TABOS_APP_HEAP_BYTES := 2097152
TABOS_APP_STACK_BYTES := 32768

include ../../sdk/make/application.mk

$(UNSTRIPPED): Makefile $(wildcard include/kilo/*.h) $(wildcard ../../sdk/include/tabos/*.h) $(wildcard ../../sdk/include/tabos/internal/*.h)

install: install-license
.PHONY: install-license
install-license:
	@mkdir -p ../../.local/rootfs/T/share/licenses/kilo
	cp LICENSE UPSTREAM.md ../../.local/rootfs/T/share/licenses/kilo/
