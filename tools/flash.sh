#!/bin/sh

set -eu

configuration="${1:-debug}"
case "$configuration" in
    debug|release)
        ;;
    *)
        echo "usage: $0 [debug|release]" >&2
        exit 2
        ;;
esac

root_directory=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build_directory="$root_directory/build/tab5-$configuration"
flash_arguments="$build_directory/flash_args"

if ! command -v esptool >/dev/null 2>&1; then
    echo "flash.sh: esptool not found; install it with 'pipx install esptool'" >&2
    exit 2
fi

if [ ! -f "$flash_arguments" ]; then
    echo "flash.sh: $flash_arguments not found; build tab5 $configuration first" >&2
    exit 2
fi

port="${ESPPORT:-}"
if [ -z "$port" ]; then
    set --
    for candidate in /dev/cu.usbmodem* /dev/ttyACM*; do
        if [ -e "$candidate" ]; then
            set -- "$@" "$candidate"
        fi
    done

    if [ "$#" -eq 0 ]; then
        echo "flash.sh: no Tab5 serial port found; set ESPPORT explicitly" >&2
        exit 2
    fi
    if [ "$#" -ne 1 ]; then
        echo "flash.sh: multiple serial ports found; set ESPPORT explicitly:" >&2
        printf '  %s\n' "$@" >&2
        exit 2
    fi
    port="$1"
fi

if [ ! -e "$port" ]; then
    echo "flash.sh: serial port does not exist: $port" >&2
    exit 2
fi

cd "$build_directory"
exec esptool \
    --chip esp32p4 \
    --port "$port" \
    --baud 460800 \
    --before default-reset \
    --after hard-reset \
    write-flash @flash_args
