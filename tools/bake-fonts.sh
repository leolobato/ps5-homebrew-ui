#!/usr/bin/env bash
# ps5-homebrew-ui - Rebuild the baked SDF fonts in assets/fonts.
# Copyright (C) 2026 BlackBearReloaded
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Each line of the table is: source TTF, output name, pixel size, SDF range.
# The baker takes two more arguments, the atlas size and the glyph list. The
# faces carry tools/font-baker/store-names.glyphs (accented Latin, Greek,
# Cyrillic and the punctuation and signs of names from stores: ' (R) TM),
# which needs a 2048 atlas; an app that shows only its own text can bake
# `1024` with no list, a quarter of the texture memory.
# To add a typeface, drop its TTF and licence into third_party/fonts, add a
# line here, run `make fonts`, and load the new .huifont where the others are
# loaded (src/main.cpp and host/snapshot_main.cpp).

set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cxx=$(command -v "${HOST_CXX:-clang++}")
mkdir -p "$root/build/host" "$root/assets/fonts"
"$cxx" -std=c++20 -O2 -w "$root/tools/font-baker/bake_font.cpp" -o "$root/build/host/bake_font"
while read -r source output size range; do
    [[ -n $source ]] || continue
    "$root/build/host/bake_font" "$root/third_party/fonts/$source" \
        "$root/assets/fonts/$output.huifont" "$size" "$range" 2048 \
        "@$root/tools/font-baker/store-names.glyphs" 2>/dev/null
done <<'FONTS'
Inter-Regular.ttf inter-regular 56 8
Inter-SemiBold.ttf inter-semibold 56 8
Montserrat-Medium.ttf montserrat-medium 56 8
DejaVuSansMono.ttf dejavu-sans-mono 52 8
PressStart2P-Regular.ttf press-start-2p 32 4
PatrickHand-Regular.ttf patrick-hand 56 8
FONTS
cp "$root"/third_party/fonts/*-LICENSE.txt "$root/assets/fonts/"
