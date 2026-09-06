APP_NAME := tdemo
.DEFAULT_GOAL := all
SOURCES := src/main.c
ASSET_BUILD := ../../build/apps/tdemo/generated
ASSET_HEADER := include/tdemo.h
TABOS_RUNTIME_ASSETS := $(ASSET_BUILD)/tdemo.tsp $(ASSET_BUILD)/world.tmap
TABOS_BUILD_PREREQUISITES := $(ASSET_HEADER)
TABOS_APP_HEAP_BYTES := 1048576
TABOS_HOST_PYTHON ?= python3

.PHONY: tdemo-assets
tdemo-assets: assets/manifest.json assets/world.tmj assets/demo.tsj assets/sprites-64.png
	"$(TABOS_HOST_PYTHON)" "$(PROJECT_ROOT)/tools/tabos" assets build assets/manifest.json --output "$(ASSET_BUILD)" --header-output "$(ASSET_HEADER)"

$(ASSET_HEADER) $(TABOS_RUNTIME_ASSETS): tdemo-assets
	@test -f $@

include ../../sdk/make/application.mk
