#!/bin/sh

set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

for application in hello_elf shell tester; do
    make -C "$script_dir/$application" "$@"
done
