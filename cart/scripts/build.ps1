# Copyright (C) 2026 Derek Quenneville
#
# Licensed under the GNU General Public License, version 2 or later; see
# COPYING.md.
#
# Build the template ROM from Windows. git runs here, where it sees the tree
# correctly; the compiler runs in WSL via scripts/build.sh.
#
# Usage: cart\scripts\build.ps1 [make args...]

$cart = Split-Path -Parent $PSScriptRoot
$id = git -C $cart rev-parse --short=8 HEAD 2>$null
if (-not $id) { $id = "dev" } else { $id = "g$id" }

$wslCart = wsl -e wslpath -a ($cart -replace '\\', '/')
wsl -e env "BUILD_ID=$id" bash "$wslCart/scripts/build.sh" @args
exit $LASTEXITCODE
