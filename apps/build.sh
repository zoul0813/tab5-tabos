#!/bin/bash

set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
project_root=$(dirname "$script_dir")
msc_mount=${TABOS_MSC_MOUNT:-}
msc_requested=false
[ -z "$msc_mount" ] || msc_requested=true
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

# Rotate each original argument once. Non-script arguments remain as positional
# parameters, preserving their exact bytes and boundaries for every make call.
original_argument_count=$#
while [ "$original_argument_count" -gt 0 ]; do
    argument=$1
    shift
    case "$argument" in
        --msc)
            msc_requested=true
            ;;
        --msc-mount=*)
            msc_mount=${argument#--msc-mount=}
            msc_requested=true
            ;;
        --with-doom)
            with_doom=true
            ;;
        clean)
            clean_requested=true
            set -- "$@" "$argument"
            ;;
        *)
            set -- "$@" "$argument"
            ;;
    esac
    original_argument_count=$((original_argument_count - 1))
done

if [ "$clean_requested" = true ]; then
    rm -rf "$project_root/build/apps"
fi

if ! command -v riscv32-esp-elf-gcc >/dev/null 2>&1; then
    activation=$("$project_root/tools/tabos" activate-idf) || exit $?
    eval "$activation"
fi

for application_dir in "$script_dir"/*; do
    if [ ! -f "$application_dir/Makefile" ]; then
        continue
    fi
    if [ "$(basename "$application_dir")" = doom ] && [ "$with_doom" = false ]; then
        continue
    fi
    make -C "$application_dir" "$@"
done

if [ "$msc_requested" = true ]; then
    upload_command=(python3 "$project_root/tools/msc_upload.py" --mounted)
    if [ -n "$msc_mount" ]; then
        upload_command+=(--mount "$msc_mount")
    fi
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
    while IFS= read -r binary; do
        if [ ! -f "$binary" ]; then
            printf 'apps/build.sh: declared application output missing: %s\n' "$binary" >&2
            exit 1
        fi
        binary_name=${binary##*/}
        upload_command+=(--file "$binary" "bin/$binary_name")
    done <<< "$application_outputs"

    if [ -n "$application_assets" ]; then
        while IFS= read -r asset; do
            if [ ! -f "$asset" ]; then
                printf 'apps/build.sh: declared runtime asset missing: %s\n' "$asset" >&2
                exit 1
            fi
            data_dir=${asset%/*}
            app_build_dir=${data_dir%/data}
            app_name=${app_build_dir##*/}
            asset_name=${asset##*/}
            upload_command+=(--file "$asset" "data/$app_name/$asset_name")
        done <<< "$application_assets"
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
                upload_command+=(--file "$application_dir/$notice" "share/licenses/$application_name/$notice")
            fi
        done
    done

    "${upload_command[@]}"
fi
