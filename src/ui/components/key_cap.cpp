// ps5-homebrew-ui - Components: the key caps shared by Keyboard and KeyPicker.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/components/key_cap.hpp"

#include <algorithm>

namespace hui::ui
{

using gfx::Color;

KeyCapPaint key_cap_paint(const Theme &theme, bool primary)
{
    const bool framed = theme.style == SurfaceStyle::hard || theme.style == SurfaceStyle::pixel ||
                        theme.style == SurfaceStyle::bevel;
    const float line = theme.button_border < 0.0f ? theme.border : theme.button_border;
    if (primary)
    {
        // On lit consoles the edge carries the colour, not the fill.
        if (theme.style == SurfaceStyle::glow)
            return {theme.surface, theme.primary, theme.primary, -1.0f};
        const bool sketch = theme.style == SurfaceStyle::sketch;
        return {theme.primary, framed ? theme.outline : (sketch ? theme.on_primary : theme.primary),
                theme.on_primary, framed || sketch ? theme.border : line};
    }
    const bool hollow = theme.secondary.a <= 0.01f;
    return {theme.secondary, hollow ? theme.on_secondary : theme.outline, theme.on_secondary,
            hollow ? std::max(theme.border, 1.5f) : (framed ? theme.border : line)};
}

void key_cap_offset(const Theme &theme, float press, float *dx, float *dy)
{
    *dx = 0.0f;
    *dy = 0.0f;
    switch (theme.style)
    {
    case SurfaceStyle::hard:
        *dx = *dy = theme.shadow_offset * press;
        break;
    case SurfaceStyle::bevel:
        *dx = *dy = press > 0.5f ? 2.0f : 0.0f;
        break;
    case SurfaceStyle::sketch:
        *dy = 2.0f * press;
        break;
    default:
        break;
    }
}

float key_cap_text(Canvas &canvas, Painter &paint, std::string_view value, float x, float baseline,
                   float size, Color color, gfx::Align align)
{
    const Theme &theme = paint.theme();
    if (theme.label == FontRole::pixel)
    {
        if (canvas.fonts.pixel.font == nullptr)
            return paint.label(value, x, baseline, size, color, align);
        return text(canvas.list, canvas.fonts.pixel, value, x, baseline - size * 0.04f,
                    Painter::pixel_em(size * 0.62f), color, align);
    }
    const float scale = theme.label == FontRole::hand ? 1.18f : 1.0f;
    return text(canvas.list, paint.font(theme.label), value, x, baseline, size * scale, color,
                align);
}

float key_cap_width(const Canvas &canvas, const Painter &paint, std::string_view value, float size)
{
    const Theme &theme = paint.theme();
    if (theme.label == FontRole::pixel)
    {
        if (canvas.fonts.pixel.font == nullptr)
            return paint.label_width(value, size);
        return canvas.fonts.pixel.measure(value, Painter::pixel_em(size * 0.62f));
    }
    const float scale = theme.label == FontRole::hand ? 1.18f : 1.0f;
    return paint.font(theme.label).measure(value, size * scale);
}

} // namespace hui::ui
