#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-only
# Copy the rtl8xxxu sources from a kernel tree (e.g. a checkout of the
# rtw-next branch with the rtl8xxxu-dkms patches applied) into src/.
set -e
[ -n "$1" ] || { echo "usage: $0 KERNEL_TREE" >&2; exit 1; }
src="$1/drivers/net/wireless/realtek/rtl8xxxu"
dst="$(dirname "$0")/../src"
[ -f "$src/core.c" ] || { echo "no rtl8xxxu sources in $src" >&2; exit 1; }
rm -f "$dst"/*.c "$dst"/*.h
cp "$src"/*.c "$src"/*.h "$src"/Makefile "$dst"/
rm -f "$dst"/*.mod.c
if git -C "$1" rev-parse HEAD >/dev/null 2>&1; then
	echo "$(git -C "$1" describe --always --tags HEAD)" > "$dst"/BASE  # edit to name the public upstream base before committing
fi
echo "synced from $src"
