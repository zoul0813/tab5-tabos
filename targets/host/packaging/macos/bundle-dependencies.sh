#!/usr/bin/env bash

set -euo pipefail

if [ "$#" -ne 2 ]; then
    echo "usage: $0 <source-executable> <package-directory>" >&2
    exit 2
fi

source_executable=$1
package_directory=$2
packaged_executable="$package_directory/$(basename "$source_executable")"

test -f "$source_executable"
test -f "$packaged_executable"

sources=("$source_executable")
targets=("$packaged_executable")

for ((index = 0; index < ${#sources[@]}; index++)); do
    source_binary=${sources[$index]}
    target_binary=${targets[$index]}
    source_id=$(otool -D "$source_binary" | sed -n '2p')

    while IFS= read -r dependency; do
        [ -n "$dependency" ] || continue
        [ "$dependency" != "$source_id" ] || continue

        case "$dependency" in
            /usr/lib/*|/System/Library/*)
                continue
                ;;
            @executable_path/*)
                continue
                ;;
            @loader_path/*|@rpath/*)
                echo "unsupported unresolved dependency in $source_binary: $dependency" >&2
                exit 1
                ;;
            /*)
                ;;
            *)
                echo "unsupported dependency in $source_binary: $dependency" >&2
                exit 1
                ;;
        esac

        dependency_name=$(basename "$dependency")
        packaged_dependency="$package_directory/$dependency_name"
        if [ -e "$packaged_dependency" ]; then
            if ! cmp -s "$dependency" "$packaged_dependency"; then
                echo "dependency basename collision: $dependency_name" >&2
                exit 1
            fi
        else
            cp "$dependency" "$packaged_dependency"
            chmod u+w "$packaged_dependency"
            sources+=("$dependency")
            targets+=("$packaged_dependency")
        fi

        install_name_tool -change "$dependency" "@executable_path/$dependency_name" "$target_binary"
    done < <(otool -L "$source_binary" | tail -n +2 | sed -E 's/^[[:space:]]+//; s/ \(compatibility version.*$//')

    if [ "$target_binary" != "$packaged_executable" ]; then
        install_name_tool -id "@executable_path/$(basename "$target_binary")" "$target_binary"
    fi
done

for target_binary in "${targets[@]}"; do
    target_id=$(otool -D "$target_binary" | sed -n '2p')
    while IFS= read -r dependency; do
        [ -n "$dependency" ] || continue
        [ "$dependency" != "$target_id" ] || continue

        case "$dependency" in
            /usr/lib/*|/System/Library/*)
                ;;
            @executable_path/*)
                dependency_name=${dependency#@executable_path/}
                if [ ! -f "$package_directory/$dependency_name" ]; then
                    echo "missing packaged dependency for $target_binary: $dependency" >&2
                    exit 1
                fi
                ;;
            *)
                echo "nonportable packaged dependency in $target_binary: $dependency" >&2
                exit 1
                ;;
        esac
    done < <(otool -L "$target_binary" | tail -n +2 | sed -E 's/^[[:space:]]+//; s/ \(compatibility version.*$//')
done

for ((index = ${#targets[@]} - 1; index >= 0; index--)); do
    codesign --force --sign - "${targets[$index]}"
done
