#!/bin/sh

set -eu

directory=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
xattr -dr com.apple.quarantine "$directory"
exec "$directory/tabos_host" "$@"
