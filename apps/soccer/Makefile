APP_NAME := soccer
TABOS_APP_HEAP_BYTES := 524288
TABOS_APP_STACK_BYTES := 16384
SOURCES := src/main.c src/game.c src/render.c src/camera.c src/sound.c src/visuals.c

include ../../sdk/make/application.mk

INSTALL_LICENSE_PATH ?= $(PROJECT_ROOT)/.local/rootfs/T/share/licenses/soccer

install: install-license
.PHONY: install-license
install-license:
	@mkdir -p "$(INSTALL_LICENSE_PATH)"
	cp LICENSE "$(INSTALL_LICENSE_PATH)/LICENSE"

PYTHON ?= python3
PACKAGE_PATH ?= $(PROJECT_ROOT)/build/packages/soccer-tabos.zip

.PHONY: package
package: build
	$(PYTHON) tools/generate_sound.py --check
	$(PYTHON) tools/package.py --binary "$(OUTPUT)" --output "$(PACKAGE_PATH)"
