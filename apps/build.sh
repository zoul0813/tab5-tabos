#!/bin/sh

set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
project_root=$(dirname "$script_dir")
msc_mount=${TABOS_MSC_MOUNT:-}
make_arguments=
with_doom=false
clean_requested=false

if [ -z "${TABOS_POINTER_MAX_CONTACTS:-}" ] && [ -f "$project_root/.local/tabos.config" ]; then
    configured_pointer_contacts=$(awk -F= '$1 == "TABOS_POINTER_MAX_CONTACTS" { print $2; exit }' \
        "$project_root/.local/tabos.config")
    if [ -n "$configured_pointer_contacts" ]; then
        TABOS_POINTER_MAX_CONTACTS=$configured_pointer_contacts
        export TABOS_POINTER_MAX_CONTACTS
    fi
fi

for argument in "$@"; do
    case "$argument" in
        --msc)
            msc_mount=${msc_mount:-/Volumes/TAB5}
            ;;
        --msc-mount=*)
            msc_mount=${argument#--msc-mount=}
            ;;
        --with-doom)
            with_doom=true
            ;;
        clean)
            clean_requested=true
            make_arguments="$make_arguments \"$argument\""
            ;;
        *)
            make_arguments="$make_arguments \"$argument\""
            ;;
    esac
done

if [ "$clean_requested" = true ]; then
    rm -rf "$project_root/build/apps"
fi

if ! command -v riscv32-esp-elf-gcc >/dev/null 2>&1; then
    eval "$("$project_root/tools/tabos" activate-idf)"
fi

for application_dir in "$script_dir"/*; do
    if [ ! -f "$application_dir/Makefile" ]; then
        continue
    fi
    if [ "$(basename "$application_dir")" = doom ] && [ "$with_doom" = false ]; then
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
    application_outputs=
    application_assets=
    for application_dir in "$script_dir"/*; do
        if [ ! -f "$application_dir/Makefile" ]; then
            continue
        fi
        if [ "$(basename "$application_dir")" = doom ] && [ "$with_doom" = false ]; then
            continue
        fi
        outputs=$(make --no-print-directory -s -C "$application_dir" tabos-list-outputs)
        assets=$(make --no-print-directory -s -C "$application_dir" tabos-list-runtime-assets)
        if [ -n "$outputs" ]; then
            application_outputs="${application_outputs}${application_outputs:+
}${outputs}"
        fi
        if [ -n "$assets" ]; then
            application_assets="${application_assets}${application_assets:+
}${assets}"
        fi
    done
    if [ -z "$application_outputs" ]; then
        printf 'apps/build.sh: no declared application outputs\n' >&2
        exit 1
    fi
    printf '%s\n' "$application_outputs" | while IFS= read -r binary; do
        if [ ! -f "$binary" ]; then
            printf 'apps/build.sh: declared application output missing: %s\n' "$binary" >&2
            exit 1
        fi
        binary_name=${binary##*/}
        target="$destination/$binary_name"
        cp "$binary" "$target"
    done

    if [ -n "$application_assets" ]; then
        printf '%s\n' "$application_assets" | while IFS= read -r asset; do
            if [ ! -f "$asset" ]; then
                printf 'apps/build.sh: declared runtime asset missing: %s\n' "$asset" >&2
                exit 1
            fi
            data_dir=${asset%/*}
            app_build_dir=${data_dir%/data}
            app_name=${app_build_dir##*/}
            asset_name=${asset##*/}
            mkdir -p "$msc_mount/data/$app_name"
            cp "$asset" "$msc_mount/data/$app_name/$asset_name"
        done
    fi

    # Copy notices from selected source projects, including `build --msc` which
    # does not run application install recipes or populate the local rootfs.
    for application_dir in "$script_dir"/*; do
        if [ ! -f "$application_dir/Makefile" ]; then
            continue
        fi
        application_name=${application_dir##*/}
        if [ "$application_name" = doom ] && [ "$with_doom" = false ]; then
            continue
        fi
        for notice in LICENSE COPYING UPSTREAM.md; do
            if [ -f "$application_dir/$notice" ]; then
                mkdir -p "$msc_mount/share/licenses/$application_name"
                cp "$application_dir/$notice" "$msc_mount/share/licenses/$application_name/"
            fi
        done
    done

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
