#!/bin/sh

set -eu

directory=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
TABOS_HOST_ROOTFS=${TABOS_HOST_ROOTFS:-"$directory/rootfs"}
export TABOS_HOST_ROOTFS
LD_LIBRARY_PATH="$directory${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
  exec "$directory/tabos_host" "$@"
