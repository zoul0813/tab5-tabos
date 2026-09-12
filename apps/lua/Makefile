APP_NAME := lua
include sources.mk
SOURCES := src/main.c $(LUA_LOCAL_SOURCES) $(LUA_VENDOR_SOURCES)
TABOS_APP_HEAP_BYTES := 4194304
TABOS_APP_STACK_BYTES := 65536
TABOS_RUNTIME_ASSETS := examples/hello.lua examples/files.lua examples/system.lua
TABOS_LDLIBS := -lm
include ../../sdk/make/application.mk
TABOS_CPPFLAGS += -Ivendor/lua/src -include include/lua_tabos/config.h
install: install-license
.PHONY: install-license
install-license:
	@mkdir -p ../../.local/rootfs/T/share/licenses/lua
	cp LICENSE UPSTREAM.md ../../.local/rootfs/T/share/licenses/lua/
