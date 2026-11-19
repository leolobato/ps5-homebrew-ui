// ps5-homebrew-ui - Component: KeyPicker.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/components/key_picker.hpp"

#include "ui/components/key_cap.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace hui::ui
{

using gfx::Color;
using gfx::Rect;

namespace
{

constexpr int kColumns = 15;
constexpr int kRows = 6;
constexpr int kKeyCount = static_cast<int>(std::size(kKeyPickerKeys));
constexpr int kDoneSpan = 2;
constexpr float kKeyShape = 0.8f;     // preferred_height(): a key's height over its width
constexpr float kChipPadding = 16.0f; // inside a chip of the summary, left and right
constexpr float kChipGap = 10.0f;     // between a chip and the "+" beside it

bool is_arrow(std::string_view name)
{
    return name == "up" || name == "down" || name == "left" || name == "right";
}

// An arrow from strokes, so every face has it.
void draw_arrow(gfx::DrawList &list, std::string_view name, float cx, float cy, float s, Color ink)
{
    const float pen = std::max(2.5f, s * 0.12f);
    float dx = 0.0f;
    float dy = 0.0f;
    if (name == "up")
        dy = -1.0f;
    else if (name == "down")
        dy = 1.0f;
    else if (name == "left")
        dx = -1.0f;
    else
        dx = 1.0f;
    const float h = s * 0.5f;
    // The shaft, then the two strokes of the head, turned to the direction.
    list.line(cx - dx * h, cy - dy * h, cx + dx * h, cy + dy * h, pen, ink);
    const float bx = cx + dx * h;
    const float by = cy + dy * h;
    const float back = s * 0.36f;
    list.line(bx, by, bx - dx * back - dy * back, by - dy * back - dx * back, pen, ink);
    list.line(bx, by, bx - dx * back + dy * back, by - dy * back + dx * back, pen, ink);
}

} // namespace

const KeyPickerKey *find_picker_key(std::string_view name)
{
    for (const KeyPickerKey &key : kKeyPickerKeys)
    {
        if (key.name == name)
            return &key;
    }
    return nullptr;
}

int picker_modifier_rank(std::string_view name)
{
    for (std::size_t i = 0; i < std::size(kKeyPickerModifiers); ++i)
    {
        if (kKeyPickerModifiers[i] == name)
            return static_cast<int>(i);
    }
    return -1;
}

// ---- the pick ---------------------------------------------------------------

KeyPicker::KeyPicker()
{
    layout_cells();
    lit_.assign(static_cast<std::size_t>(kKeyCount), 0.0f);
    retarget(true);
}

bool KeyPicker::set_keys(const std::vector<std::string_view> &names)
{
    keys_.clear();
    bool all = true;
    const int limit = std::max(style.max_keys, 1);
    for (std::string_view name : names)
    {
        if (find_picker_key(name) == nullptr || picked(name) ||
            static_cast<int>(keys_.size()) >= limit)
        {
            all = false;
            continue;
        }
        keys_.emplace_back(name);
    }
    order();
    // A pick given by the screen is shown as it is, without a fade.
    for (int i = 0; i < kKeyCount; ++i)
        lit_[static_cast<std::size_t>(i)] =
            picked(kKeyPickerKeys[static_cast<std::size_t>(i)].name) ? 1.0f : 0.0f;
    return all;
}

bool KeyPicker::set_keys(std::initializer_list<std::string_view> names)
{
    return set_keys(std::vector<std::string_view>(names));
}

bool KeyPicker::set_keys(const std::vector<std::string> &names)
{
    std::vector<std::string_view> views;
    views.reserve(names.size());
    for (const std::string &name : names)
        views.emplace_back(name);
    return set_keys(views);
}

bool KeyPicker::picked(std::string_view name) const
{
    return std::find(keys_.begin(), keys_.end(), name) != keys_.end();
}

std::string KeyPicker::summary() const
{
    std::string out;
    for (const std::string &name : keys_)
    {
        const KeyPickerKey *key = find_picker_key(name);
        if (!out.empty())
            out += " + ";
        out += key != nullptr ? key->label : std::string_view(name);
    }
    return out;
}

// Modifiers first, in the order a combination writes them; the other keys
// keep the order they were picked in, since that is the order they are
// pressed.
void KeyPicker::order()
{
    std::stable_sort(keys_.begin(), keys_.end(),
                     [](const std::string &a, const std::string &b)
                     {
                         const int ra = picker_modifier_rank(a);
                         const int rb = picker_modifier_rank(b);
                         const int ka = ra < 0 ? 1000 : ra;
                         const int kb = rb < 0 ? 1000 : rb;
                         return ka < kb;
                     });
}

void KeyPicker::changed()
{
    if (on_change)
        on_change(keys_);
}

// ---- geometry ---------------------------------------------------------------

void KeyPicker::layout_cells()
{
    cells_done_key_ = style.done_key;
    cells_.clear();
    int row = -1;
    int column = 0;
    for (const KeyPickerKey &key : kKeyPickerKeys)
    {
        if (key.row != row)
        {
            row = key.row;
            column = 0;
        }
        int span = key.span;
        // Without a Done key the space bar takes its place.
        if (key.name == "space" && !style.done_key)
            span += kDoneSpan;
        cells_.push_back({row, column, span});
        column += span;
    }
    if (style.done_key)
        cells_.push_back({kRows - 1, kColumns - kDoneSpan, kDoneSpan});
}

int KeyPicker::key_count() const
{
    return static_cast<int>(cells_.size());
}

bool KeyPicker::is_done(int index) const
{
    return index >= kKeyCount;
}

std::string_view KeyPicker::focused_name() const
{
    if (is_done(focus_))
        return {};
    return kKeyPickerKeys[static_cast<std::size_t>(focus_)].name;
}

Rect KeyPicker::summary_rect() const
{
    const Rect inner = style.panel ? bounds_.inset(style.panel_padding) : bounds_;
    return {inner.x, inner.y, inner.w, std::max(style.summary_height, 0.0f)};
}

Rect KeyPicker::grid() const
{
    const Rect inner = style.panel ? bounds_.inset(style.panel_padding) : bounds_;
    const float top = style.summary_height > 0.0f ? style.summary_height + style.summary_gap : 0.0f;
    return {inner.x, inner.y + top, inner.w, std::max(inner.h - top, 8.0f)};
}

Rect KeyPicker::key_rect(int index) const
{
    if (index < 0 || index >= key_count())
        return bounds_;
    const Rect board = grid();
    const float unit_w = std::max((board.w - static_cast<float>(kColumns - 1) * style.gap) /
                                      static_cast<float>(kColumns),
                                  4.0f);
    const float unit_h = std::max(
        (board.h - static_cast<float>(kRows - 1) * style.gap) / static_cast<float>(kRows), 4.0f);
    const Cell &cell = cells_[static_cast<std::size_t>(index)];
    const float pitch = unit_w + style.gap;
    return {board.x + static_cast<float>(cell.column) * pitch,
            board.y + static_cast<float>(cell.row) * (unit_h + style.gap),
            static_cast<float>(cell.span) * pitch - style.gap, unit_h};
}

float KeyPicker::preferred_height(float width) const
{
    const float pad = style.panel ? 2.0f * style.panel_padding : 0.0f;
    const float unit_w = std::max((width - pad - static_cast<float>(kColumns - 1) * style.gap) /
                                      static_cast<float>(kColumns),
                                  8.0f);
    const float top = style.summary_height > 0.0f ? style.summary_height + style.summary_gap : 0.0f;
    return static_cast<float>(kRows) * unit_w * kKeyShape +
           static_cast<float>(kRows - 1) * style.gap + top + pad;
}

void KeyPicker::set_bounds(const Rect &bounds)
{
    // A parent may place the picker every frame: only a real change snaps
    // the highlight, or it would never glide.
    const bool moved = bounds.x != bounds_.x || bounds.y != bounds_.y || bounds.w != bounds_.w ||
                       bounds.h != bounds_.h;
    bounds_ = bounds;
    if (moved)
        retarget(true);
}

void KeyPicker::enter()
{
    age_ = 0.0f;
}

float KeyPicker::key_radius(const Painter &paint, const Rect &r) const
{
    const float half = std::min(r.w, r.h) * 0.5f;
    switch (style.radius_source)
    {
    case RadiusSource::pill:
        return half;
    case RadiusSource::square:
        return 0.0f;
    case RadiusSource::custom:
        return std::min(style.radius, half);
    default:
        return paint.control_radius(r);
    }
}

// ---- focus ------------------------------------------------------------------

void KeyPicker::retarget(bool snap)
{
    if (cells_done_key_ != style.done_key)
    {
        layout_cells();
        focus_ = std::clamp(focus_, 0, key_count() - 1);
    }
    const Rect target = key_rect(focus_);
    highlight_.target(target);
    if (snap)
        highlight_.snap(target);
}

void KeyPicker::set_focus(int index, bool snap)
{
    focus_ = std::clamp(index, 0, key_count() - 1);
    const Cell &cell = cells_[static_cast<std::size_t>(focus_)];
    column_ = static_cast<float>(cell.column) + static_cast<float>(cell.span) * 0.5f;
    retarget(snap);
}

bool KeyPicker::focus_key(std::string_view name, bool snap)
{
    for (int i = 0; i < kKeyCount; ++i)
    {
        if (kKeyPickerKeys[static_cast<std::size_t>(i)].name == name)
        {
            set_focus(i, snap);
            return true;
        }
    }
    return false;
}

// The key of a row that covers a column, or the nearest one.
int KeyPicker::cell_under(int row, float column) const
{
    int nearest = -1;
    float distance = 1.0e9f;
    for (int i = 0; i < key_count(); ++i)
    {
        const Cell &cell = cells_[static_cast<std::size_t>(i)];
        if (cell.row != row)
            continue;
        const float start = static_cast<float>(cell.column);
        const float span = static_cast<float>(cell.span);
        if (column >= start && column < start + span)
            return i;
        const float away = std::fabs(start + span * 0.5f - column);
        if (away < distance)
        {
            distance = away;
            nearest = i;
        }
    }
    return nearest;
}

Event KeyPicker::move(const InputFrame &input, Feedback &feedback)
{
    const Cell here = cells_[static_cast<std::size_t>(focus_)];
    const Rect from = key_rect(focus_);
    if (input.nav == Direction::left || input.nav == Direction::right)
    {
        const bool right = input.nav == Direction::right;
        // The keys of a row are next to each other in the grid's order.
        int first = focus_;
        while (first > 0 && cells_[static_cast<std::size_t>(first - 1)].row == here.row)
            --first;
        int last = focus_;
        while (last + 1 < key_count() && cells_[static_cast<std::size_t>(last + 1)].row == here.row)
            ++last;
        int next = focus_ + (right ? 1 : -1);
        if (next < first || next > last)
        {
            if (style.exits.allows(input.nav))
            {
                exit_ = input.nav;
                return Event::none;
            }
            // A held direction stops at the edge instead of circling forever.
            if (!style.wrap || input.nav_repeat)
                return refuse(feedback, style, input, highlight_.refusal(), from.cx());
            next = right ? first : last;
        }
        focus_ = next;
        const Cell &cell = cells_[static_cast<std::size_t>(focus_)];
        column_ = static_cast<float>(cell.column) + static_cast<float>(cell.span) * 0.5f;
    }
    else
    {
        const int row = here.row + (input.nav == Direction::down ? 1 : -1);
        if (row < 0 || row >= kRows)
        {
            if (style.exits.allows(input.nav))
            {
                exit_ = input.nav;
                return Event::none;
            }
            return refuse(feedback, style, input, highlight_.refusal(), from.cx());
        }
        // column_ is where the player really is: a wide key covers several
        // columns, and going back returns to the one they left.
        const float centre = static_cast<float>(here.column) + static_cast<float>(here.span) * 0.5f;
        focus_ = cell_under(row, style.column_memory ? column_ : centre);
        if (!style.column_memory)
            column_ = centre;
    }
    retarget(false);
    // Rows sound like steps: lower rows, lower pitch.
    play_cue(feedback, style, style.sounds.move, key_rect(focus_).cx(),
             1.1f - 0.035f * static_cast<float>(cells_[static_cast<std::size_t>(focus_)].row));
    return Event::moved;
}

// ---- actions ----------------------------------------------------------------

Event KeyPicker::toggle(int index, Feedback &feedback)
{
    press_index_ = index;
    press_.trigger();
    if (is_done(index))
        return accept(feedback);
    const float x = key_rect(index).cx();
    const std::string_view name = kKeyPickerKeys[static_cast<std::size_t>(index)].name;
    const auto found = std::find(keys_.begin(), keys_.end(), name);
    if (found != keys_.end())
    {
        keys_.erase(found);
        play_cue(feedback, style, style.sounds.change, x, 0.9f);
        changed();
        return Event::changed;
    }
    if (style.max_keys <= 1)
    {
        keys_.assign(1, std::string(name));
    }
    else
    {
        if (static_cast<int>(keys_.size()) >= style.max_keys)
        {
            summary_shake_.trigger();
            return refuse(feedback, style, InputFrame{}, highlight_.refusal(), x);
        }
        keys_.emplace_back(name);
        order();
    }
    // Each key added sounds a little higher, so a combination climbs.
    play_cue(feedback, style, style.sounds.change, x,
             1.0f + 0.04f * static_cast<float>(keys_.size() - 1));
    changed();
    return Event::changed;
}

Event KeyPicker::tap(std::string_view name, Feedback &feedback)
{
    if (!focus_key(name, false))
        return refuse(feedback, style, InputFrame{}, highlight_.refusal(), bounds_.cx());
    return toggle(focus_, feedback);
}

Event KeyPicker::clear(Feedback &feedback)
{
    if (keys_.empty())
    {
        summary_shake_.trigger();
        return refuse(feedback, style, InputFrame{}, highlight_.refusal(), summary_rect().cx());
    }
    keys_.clear();
    play_cue(feedback, style, style.sounds.change, summary_rect().cx(), 0.85f);
    changed();
    return Event::changed;
}

Event KeyPicker::accept(Feedback &feedback)
{
    const float x = style.done_key ? key_rect(kKeyCount).cx() : bounds_.cx();
    if (keys_.empty() && !style.allow_empty)
    {
        summary_shake_.trigger();
        return refuse(feedback, style, InputFrame{}, highlight_.refusal(), x);
    }
    if (style.done_key)
    {
        press_index_ = kKeyCount;
        press_.trigger();
    }
    play_cue(feedback, style, style.sounds.activate, x);
    if (style.sounds.rumble > 0.0f)
        feedback.rumble(0.3f * style.sounds.rumble, 0.04f);
    if (on_accept)
        on_accept(keys_);
    return Event::activated;
}

Event KeyPicker::handle(const InputFrame &input, Feedback &feedback)
{
    exit_ = Direction::none;
    if (cells_done_key_ != style.done_key)
        retarget(true);

    if (input.is_pressed(Action::back))
    {
        play_cue(feedback, style, style.sounds.cancel, bounds_.cx());
        if (on_cancel)
            on_cancel();
        return Event::cancelled;
    }

    Event result = Event::none;
    if (input.nav != Direction::none)
        result = move(input, feedback);
    if (input.is_pressed(Action::confirm))
        result = toggle(focus_, feedback);
    if (style.clear != Action::count && input.is_pressed(style.clear))
        result = clear(feedback);
    if (style.accept != Action::count && input.is_pressed(style.accept))
        result = accept(feedback);
    return result;
}

void KeyPicker::update(float dt)
{
    age_ += dt;
    // The knobs may have changed since the last frame: follow them.
    retarget(false);
    highlight_.update(dt, style);
    press_.update(dt, 14.0f);
    summary_shake_.update(dt, 7.0f);
    active_amount_.target = active_ ? 1.0f : 0.0f;
    active_amount_.update(dt, 18.0f);
    const float follow = style.reduced_motion ? 1.0f : 1.0f - std::exp(-20.0f * dt);
    for (int i = 0; i < kKeyCount; ++i)
    {
        float &lit = lit_[static_cast<std::size_t>(i)];
        const float target = picked(kKeyPickerKeys[static_cast<std::size_t>(i)].name) ? 1.0f : 0.0f;
        lit += (target - lit) * follow;
    }
}

// ---- drawing ----------------------------------------------------------------

void KeyPicker::draw_summary(Canvas &canvas, Painter &paint) const
{
    const Theme &theme = style.theme;
    const Rect strip = summary_rect();
    if (strip.h <= 0.0f)
        return;
    const Color muted = style.panel ? theme.text_muted : paint.page_text_muted();
    const Color ink = style.panel ? theme.text : paint.page_text();
    const float baseline = strip.cy() + style.summary_size * 0.35f;
    float x = strip.x + (style.reduced_motion ? 0.0f : shake(summary_shake_.value, canvas.time));

    // The count sits at the right edge and the chips never run under it.
    float right = strip.x + strip.w;
    if (style.max_keys > 1)
    {
        char count[16];
        std::snprintf(count, sizeof(count), "%d / %d", static_cast<int>(keys_.size()),
                      style.max_keys);
        paint.label(count, right, baseline, style.summary_size, muted, gfx::Align::right);
        right -= paint.label_width(count, style.summary_size) + 2.0f * kChipGap;
    }

    if (keys_.empty())
    {
        paint.label(fit_label(paint, style.empty_text, style.summary_size, right - x), x, baseline,
                    style.summary_size, muted);
        return;
    }
    const float chip_h = std::min(strip.h, 44.0f);
    const float plus_w = paint.label_width("+", style.summary_size);
    for (std::size_t i = 0; i < keys_.size(); ++i)
    {
        const KeyPickerKey *key = find_picker_key(keys_[i]);
        const std::string_view label = key != nullptr ? key->label : std::string_view(keys_[i]);
        if (i > 0)
        {
            paint.label("+", x + plus_w * 0.5f, baseline, style.summary_size, ink,
                        gfx::Align::center);
            x += plus_w + kChipGap;
        }
        // Painter::chip sets its label at 19 px. A chip that fits keeps its
        // label whole; only one squeezed by the count is cut.
        const float natural = paint.label_width(label, 19.0f) + 2.0f * kChipPadding;
        const float w = std::min(natural, std::max(right - x, 0.0f));
        if (w <= 2.0f * kChipPadding)
            break;
        const std::string shown = w < natural
                                      ? fit_label(paint, label, 19.0f, w - 2.0f * kChipPadding)
                                      : std::string(label);
        paint.chip({x, strip.cy() - chip_h * 0.5f, w, chip_h}, shown, 1.0f, Look{});
        x += w + kChipGap;
    }
}

void KeyPicker::draw(Canvas &canvas) const
{
    const Theme &theme = style.theme;
    gfx::DrawList &list = canvas.list;
    Painter paint(list, canvas.fonts, theme, canvas.glass);
    if (style.panel)
        paint.panel(bounds_);
    // draw() is const: a layout knob changed since the last update() is
    // followed there, and until then the old cells are drawn.
    const int count = std::min(key_count(), kKeyCount + (style.done_key ? 1 : 0));

    draw_summary(canvas, paint);

    const bool still = style.reduced_motion;
    const auto entrance = [&](int row)
    {
        if (style.entrance_step <= 0.0f || still)
            return tween::cubic_out(age_ / 0.2f);
        return tween::stagger(age_, row, style.entrance_step, 0.34f);
    };
    const auto row_of = [&](int index) { return cells_[static_cast<std::size_t>(index)].row; };
    // A key arrives from a little below and dips around its centre when it
    // is pressed. Returns how far it is pressed.
    const auto push_key = [&](int index, const Rect &rect)
    {
        const float in = entrance(row_of(index));
        const float press = index == press_index_ ? press_.value : 0.0f;
        list.push_opacity(in);
        list.push_transform(still ? 1.0f : 1.0f - style.press_dip * press, rect.cx(), rect.cy(),
                            0.0f, still ? 0.0f : 14.0f * (1.0f - in));
        return press;
    };
    const auto pop_key = [&]()
    {
        list.pop_transform();
        list.pop_opacity();
    };
    const auto lit_of = [&](int index)
    { return is_done(index) ? 0.0f : tween::clamp01(lit_[static_cast<std::size_t>(index)]); };

    // 1. The keys. A picked key is lit in the accent colour.
    for (int i = 0; i < count; ++i)
    {
        const Rect rect = key_rect(i);
        const float radius = key_radius(paint, rect);
        const float press = push_key(i, rect);
        const float lit = lit_of(i);
        if (style.surfaces)
        {
            const KeyCapPaint look = key_cap_paint(theme, is_done(i));
            paint.surface(rect, radius, look.fill, look.edge, 1.0f - press, look.border);
        }
        if (lit > 0.01f)
            paint.fill(rect, radius, theme.accent.with_alpha(0.28f * lit));
        pop_key();
    }

    // 2. The highlight.
    HighlightStyle look = style.highlight;
    const Rect focus_rect = key_rect(focus_);
    if (look.radius < 0.0f)
        look.radius = key_radius(paint, focus_rect);
    if (press_index_ == focus_)
        look.grow -= 2.0f * press_.value;
    const float amount = (0.35f + 0.65f * active_amount_.value) * entrance(row_of(focus_));
    highlight_.draw(canvas, style, look, amount);
    if (style.done_key && look.kind == HighlightKind::fill && style.surfaces &&
        look.color.a <= 0.0f)
    {
        // The Done key already has the plate's colour: on it the focus would
        // vanish, so there the theme's ring joins the plate.
        const float cover = highlight_.coverage(key_rect(kKeyCount));
        paint.focus_ring(highlight_.rect(canvas.time), look.radius, cover * amount);
    }

    // The picked mark goes over the highlight, so a focused key still says
    // whether Cross will pick it or drop it.
    for (int i = 0; i < count; ++i)
    {
        const float lit = lit_of(i);
        if (lit <= 0.01f)
            continue;
        const Rect rect = key_rect(i);
        push_key(i, rect);
        paint.stroke(rect, key_radius(paint, rect), std::max(theme.border, 3.0f),
                     theme.accent.with_alpha(lit));
        pop_key();
    }

    // 3. The caps, coloured by the highlight above them.
    const Color flat = style.panel ? theme.text : paint.page_text();
    Color flat_done = theme.primary;
    if (theme.style == SurfaceStyle::bevel || theme.style == SurfaceStyle::pixel)
        flat_done = flat;
    else if (theme.style == SurfaceStyle::sketch)
        flat_done = theme.on_primary;
    for (int i = 0; i < count; ++i)
    {
        Rect rect = key_rect(i);
        const bool done = is_done(i);
        Color resting = style.surfaces ? key_cap_paint(theme, done).ink : (done ? flat_done : flat);
        // A picked key is written in the accent colour where it stays legible.
        if (!style.surfaces)
            resting = gfx::mix(resting, theme.accent, lit_of(i));
        const float cover = highlight_.coverage(rect) * active_amount_.value;
        const Color ink = Highlight::text_color(style, look, cover, resting);
        const float press = push_key(i, rect);
        if (style.surfaces)
        {
            float dx = 0.0f;
            float dy = 0.0f;
            key_cap_offset(theme, tween::clamp01(press), &dx, &dy);
            rect.x += dx;
            rect.y += dy;
        }
        const std::string_view name =
            done ? std::string_view() : kKeyPickerKeys[static_cast<std::size_t>(i)].name;
        if (is_arrow(name))
        {
            draw_arrow(list, name, rect.cx(), rect.cy(), std::min(style.icon_size, rect.h * 0.5f),
                       ink);
        }
        else
        {
            const std::string_view label = done ? std::string_view(style.done_label)
                                                : kKeyPickerKeys[static_cast<std::size_t>(i)].label;
            // Single characters at full size, words a size down; both shrink
            // to fit the key.
            const float base = label.size() <= 1 ? style.label_size : style.wide_label_size;
            const float wide = key_cap_width(canvas, paint, label, base);
            const float room = std::max(rect.w - 10.0f, 8.0f);
            const float size = wide > room ? base * room / wide : base;
            key_cap_text(canvas, paint, label, rect.cx(), rect.cy() + size * 0.35f, size, ink,
                         gfx::Align::center);
        }
        pop_key();
    }
}

} // namespace hui::ui
