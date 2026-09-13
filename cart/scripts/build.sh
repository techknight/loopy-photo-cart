#!/bin/bash
#
# Copyright (C) 2026 Derek Quenneville
#
# Licensed under the GNU General Public License, version 2 or later; see
# COPYING.md.
#
# Build the template ROM inside WSL.
#
# Locates the Wonderful Toolchain, then builds. When the source tree is on a
# Windows drive (/mnt/...), the build runs against a WSL-native copy: drvfs is
# slow enough that copying the tree is still faster than compiling on it.
#
# git is never run here (WSL's git sees every file as modified under the
# Windows autocrlf setting); scripts/build.ps1 passes BUILD_ID in instead.
#
# Usage: scripts/build.sh [make args...]
#
set -e

for d in /opt/wonderful "$HOME/wonderful"; do
    if [ -x "$d/toolchain/gcc-sh-elf/bin/sh-elf-gcc" ]; then
        WF="$d"
        break
    fi
done

if [ -z "$WF" ]; then
    echo "Wonderful Toolchain not found in /opt/wonderful or \$HOME/wonderful." >&2
    exit 1
fi

SRC="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_ID="${BUILD_ID:-dev}"
# The copy below holds cart/ alone, so read the version where web/ is.
VERSION="${VERSION:-$(sed -n 's/^ *"version": *"\([^"]*\)".*/\1/p' "$SRC/../web/package.json" 2>/dev/null)}"
VERSION="${VERSION:-dev}"

case "$SRC" in
    /mnt/*)
        WORK="$HOME/.cache/loopy-photo-cart-build"
        mkdir -p "$WORK"
        # Not --delete for obj/: it is what makes an incremental build
        # incremental. Sources are mirrored exactly.
        rsync -a --delete --exclude obj/ --exclude build/ "$SRC/" "$WORK/"
        make -C "$WORK" WONDERFUL_TOOLCHAIN="$WF" BUILD_ID="$BUILD_ID" VERSION="$VERSION" "$@"
        mkdir -p "$SRC/build"
        cp "$WORK"/build/*.bin "$WORK"/build/*.elf "$SRC/build/"
        ;;
    *)
        make -C "$SRC" WONDERFUL_TOOLCHAIN="$WF" BUILD_ID="$BUILD_ID" VERSION="$VERSION" "$@"
        ;;
esac
