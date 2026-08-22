#!/bin/sh

set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
project_root=$(dirname "$script_dir")
msc_mount=${TABOS_MSC_MOUNT:-}
make_arguments=

for argument in "$@"; do
    case "$argument" in
        --msc)
            msc_mount=${msc_mount:-/Volumes/TAB5}
            ;;
        --msc-mount=*)
            msc_mount=${argument#--msc-mount=}
            ;;
        *)
            make_arguments="$make_arguments \"$argument\""
            ;;
    esac
done

if ! command -v riscv32-esp-elf-gcc >/dev/null 2>&1; then
    eval "$("$project_root/tools/tabos" activate-idf)"
fi

for application_dir in "$script_dir"/*; do
    if [ ! -f "$application_dir/Makefile" ]; then
        continue
    fi
    if [ -n "$make_arguments" ]; then
        # Keep make arguments word-separated while preserving the simple CLI
        # contract of this script.
        eval "make -C \"$application_dir\"$make_arguments"
    else
        make -C "$application_dir"
    fi
done

if [ -n "$msc_mount" ]; then
    if [ ! -d "$msc_mount" ]; then
        printf 'apps/build.sh: MSC mount not found: %s\n' "$msc_mount" >&2
        exit 1
    fi

    destination="$msc_mount/bin"
    mkdir -p "$destination"
    found_binary=false
    for binary in "$project_root"/build/apps/*/* "$project_root"/build/apps/*/*/*; do
        if [ ! -f "$binary" ]; then
            continue
        fi
        case "$binary" in
            *.elf) continue ;;
        esac
        found_binary=true
        relative_path=${binary#"$project_root/build/apps/"}
        case "$relative_path" in
            coreutils/*/*)
                relative_path=${relative_path#coreutils/}
                relative_path=${relative_path#*/}
                ;;
        esac
        relative_path=${relative_path#*/}
        target="$destination/$relative_path"
        mkdir -p "$(dirname "$target")"
        cp "$binary" "$target"
    done
    if [ "$found_binary" = false ]; then
        printf 'apps/build.sh: no built application binaries found\n' >&2
        exit 1
    fi

    sync
    if ! command -v diskutil >/dev/null 2>&1; then
        printf 'apps/build.sh: diskutil unavailable; leaving MSC mounted at %s\n' "$msc_mount" >&2
        exit 1
    fi
    # Full eject detaches the USB device. A plain `unmount` can leave the
    # MSC session attached, so TabOS would not receive TinyUSB DETACHED.
    # TabOS resets as soon as detach begins, so do not keep the build command
    # blocked while macOS waits for the disappearing USB device to respond.
    diskutil eject "$msc_mount" >/dev/null 2>&1 &
    printf 'apps/build.sh: eject requested; Tab5 should restart shortly\n'
fi
