#!/bin/sh

set -eu

tool_name=${0##*/}
if [ "$tool_name" = make ]; then
    [ "$#" -eq 8 ]
    [ "$1" = -C ]
    [ -f "$2/Makefile" ]
    [ "$3" = "$EXPECTED_SPACE" ]
    [ "$4" = "$EXPECTED_DOUBLE_QUOTE" ]
    [ "$5" = "$EXPECTED_SINGLE_QUOTE" ]
    [ "$6" = "$EXPECTED_DOLLAR" ]
    [ "$7" = "$EXPECTED_SUBSTITUTION" ]
    [ "$8" = "$EXPECTED_BACKTICK" ]
    : > "$ARGUMENT_RECORD"
    exit 0
fi
if [ "$tool_name" = riscv32-esp-elf-gcc ]; then
    exit 0
fi

project_root=$1
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
script_path=$script_dir/${0##*/}
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/tabos-application-arguments.XXXXXX")
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM

mkdir "$test_dir/bin"
cp "$script_path" "$test_dir/bin/make"
cp "$script_path" "$test_dir/bin/riscv32-esp-elf-gcc"
chmod +x "$test_dir/bin/make" "$test_dir/bin/riscv32-esp-elf-gcc"

ARGUMENT_RECORD=$test_dir/record
substitution_marker=$test_dir/substitution-ran
backtick_marker=$test_dir/backtick-ran
EXPECTED_SPACE='SPACE=value with spaces'
EXPECTED_DOUBLE_QUOTE='DOUBLE=value"quoted"'
EXPECTED_SINGLE_QUOTE="SINGLE=value'quoted'"
EXPECTED_DOLLAR='DOLLAR=$HOME'
EXPECTED_SUBSTITUTION='SUBSTITUTE=$(touch '"$substitution_marker"')'
EXPECTED_BACKTICK='BACKTICK=`touch '"$backtick_marker"'`'
export ARGUMENT_RECORD EXPECTED_SPACE EXPECTED_DOUBLE_QUOTE EXPECTED_SINGLE_QUOTE
export EXPECTED_DOLLAR EXPECTED_SUBSTITUTION EXPECTED_BACKTICK

PATH=$test_dir/bin:$PATH "$project_root/apps/build.sh" \
    "$EXPECTED_SPACE" \
    --with-doom \
    "$EXPECTED_DOUBLE_QUOTE" \
    "$EXPECTED_SINGLE_QUOTE" \
    "$EXPECTED_DOLLAR" \
    "$EXPECTED_SUBSTITUTION" \
    "$EXPECTED_BACKTICK"

[ -f "$ARGUMENT_RECORD" ]
[ ! -e "$substitution_marker" ]
[ ! -e "$backtick_marker" ]
