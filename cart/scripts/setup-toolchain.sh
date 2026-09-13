#!/bin/bash
#
# Copyright (C) 2026 Derek Quenneville
#
# Licensed under the GNU General Public License, version 2 or later; see
# COPYING.md.
#
# Install the SuperH (SH-1) cross-compiler the cartridge build needs: the
# Wonderful Toolchain's toolchain-gcc-sh-elf. By default everything goes into
# ~/wonderful, which needs no root; set WONDERFUL_TOOLCHAIN to install
# elsewhere. Linux only: on Windows, run it inside WSL2. CI runs it too
# (.github/workflows/cart.yml).
#
# Usage:
#   cart/scripts/setup-toolchain.sh
#   WONDERFUL_TOOLCHAIN=/opt/wonderful cart/scripts/setup-toolchain.sh
#
set -e

WF="${WONDERFUL_TOOLCHAIN:-$HOME/wonderful}"
BOOTSTRAP_URL="https://wonderful.asie.pl/bootstrap/wf-bootstrap-x86_64.tar.gz"

echo "Installing the Wonderful Toolchain into: $WF"

# wf-pacman resolves its paths relative to its own binary, so an in-place
# unpack works.
if [ ! -x "$WF/bin/wf-pacman" ]; then
    echo "==> Fetching bootstrap"
    mkdir -p "$WF"
    tmp="$(mktemp -d)"
    curl -fsSL -o "$tmp/wf-bootstrap.tar.gz" "$BOOTSTRAP_URL"
    tar xzf "$tmp/wf-bootstrap.tar.gz" -C "$WF"
    rm -rf "$tmp"
else
    echo "==> wf-pacman already present, skipping bootstrap"
fi

export PATH="$WF/bin:$PATH"

echo "==> Syncing package databases"
wf-pacman -Syu --noconfirm

echo "==> Installing toolchain-gcc-sh-elf"
wf-pacman -S --noconfirm --needed toolchain-gcc-sh-elf

CC="$WF/toolchain/gcc-sh-elf/bin/sh-elf-gcc"
if [ ! -x "$CC" ]; then
    echo "ERROR: expected compiler not found at $CC" >&2
    exit 1
fi

"$CC" --version | head -1
echo "Build with: make -C cart WONDERFUL_TOOLCHAIN=$WF"
