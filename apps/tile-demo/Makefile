APP_NAME := tile-demo
.DEFAULT_GOAL := all
SOURCES := $(CURDIR)/src/main.c
ASSET_BUILD := $(CURDIR)/../../build/apps/tile-demo/generated
TABOS_RUNTIME_ASSETS := $(ASSET_BUILD)/tile-demo.tsp $(ASSET_BUILD)/world.tmap
TABOS_APP_HEAP_BYTES := 1048576
TABOS_HOST_PYTHON ?= python3

.PHONY: tile-demo-assets
tile-demo-assets: $(CURDIR)/assets/manifest.json $(CURDIR)/assets/world.tmj $(CURDIR)/assets/demo.tsj $(CURDIR)/assets/sprites-64.png
	"$(TABOS_HOST_PYTHON)" "$(PROJECT_ROOT)/tools/tabos" assets build "$(CURDIR)/assets/manifest.json" --output "$(ASSET_BUILD)"

$(TABOS_RUNTIME_ASSETS): tile-demo-assets
	@test -f $@

include ../../sdk/make/application.mk
