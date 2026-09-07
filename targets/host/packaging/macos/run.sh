#!/bin/sh

set -eu

directory=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
xattr -dr com.apple.quarantine "$directory"
TABOS_HOST_ROOTFS=${TABOS_HOST_ROOTFS:-"$directory/rootfs"}
export TABOS_HOST_ROOTFS
exec "$directory/tabos_host" "$@"
