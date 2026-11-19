// ps5-homebrew-ui - Components: the key caps shared by Keyboard and KeyPicker.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// A key of an on-screen keyboard is a button of the theme, and its cap shows
// the case it types. Keyboard and KeyPicker draw the same keys, so the recipe
// lives here once.

#pragma once

#include "ui/components/component.hpp"

#include <string_view>

namespace hui::ui
{

struct KeyCapPaint
{
    gfx::Color fill;
    gfx::Color edge;
    gfx::Color ink;
    float border = -1.0f;
};

// The recipe of Painter::button: its primary kind for a key that ends the
// entry (Done) and its secondary kind for every other.
KeyCapPaint key_cap_paint(const Theme &theme, bool primary);

// How far Painter::surface moves a surface's content when it is pressed. The
// caps are drawn in a later pass than the surfaces (the highlight lies
// between them), so the offset is worked out again for them.
void key_cap_offset(const Theme &theme, float press, float *dx, float *dy);

// Painter::label sets a theme's labels in capitals where the theme asks for
// them. A key cap must show the case it types, so caps are set in the label
// face with the same sizing rules and no case change.
float key_cap_text(Canvas &canvas, Painter &paint, std::string_view value, float x, float baseline,
                   float size, gfx::Color color, gfx::Align align);
float key_cap_width(const Canvas &canvas, const Painter &paint, std::string_view value, float size);

} // namespace hui::ui
