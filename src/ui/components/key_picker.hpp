// ps5-homebrew-ui - Component: KeyPicker, a PC keyboard to pick a key or a combination from.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "ui/components/card.hpp"
#include "ui/components/component.hpp"
#include "ui/components/progress.hpp"

#include <cstdint>
#include <functional>
#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

namespace hui::ui
{

// One key of the PC keyboard the picker draws. `name` is what the picker
// returns: a lower-case word with no punctuation in it (`a`, `f1`, `ctrl`,
// `semicolon`, `pageup`, `left`), so it can be written into a text format
// that uses `+`, `,`, `;` and `=` as separators. `label` is what the cap and
// the summary show.
struct KeyPickerKey
{
    std::string_view name;
    std::string_view label;
    std::uint8_t row = 0;  // 0 (Esc, F1...) to 5 (Ctrl, Space, the arrows)
    std::uint8_t span = 1; // columns of the 15-column grid it covers
};

// Every key of the grid, row by row, left to right. The main block of a PC
// keyboard plus its editing keys and arrows; no number pad, no mouse.
inline constexpr KeyPickerKey kKeyPickerKeys[] = {
    {"escape", "Esc", 0, 1},    {"f1", "F1", 0, 1},
    {"f2", "F2", 0, 1},         {"f3", "F3", 0, 1},
    {"f4", "F4", 0, 1},         {"f5", "F5", 0, 1},
    {"f6", "F6", 0, 1},         {"f7", "F7", 0, 1},
    {"f8", "F8", 0, 1},         {"f9", "F9", 0, 1},
    {"f10", "F10", 0, 1},       {"f11", "F11", 0, 1},
    {"f12", "F12", 0, 1},       {"insert", "Ins", 0, 1},
    {"delete", "Del", 0, 1},

    {"backquote", "`", 1, 1},   {"1", "1", 1, 1},
    {"2", "2", 1, 1},           {"3", "3", 1, 1},
    {"4", "4", 1, 1},           {"5", "5", 1, 1},
    {"6", "6", 1, 1},           {"7", "7", 1, 1},
    {"8", "8", 1, 1},           {"9", "9", 1, 1},
    {"0", "0", 1, 1},           {"minus", "-", 1, 1},
    {"equals", "=", 1, 1},      {"backspace", "Backspace", 1, 2},

    {"tab", "Tab", 2, 1},       {"q", "Q", 2, 1},
    {"w", "W", 2, 1},           {"e", "E", 2, 1},
    {"r", "R", 2, 1},           {"t", "T", 2, 1},
    {"y", "Y", 2, 1},           {"u", "U", 2, 1},
    {"i", "I", 2, 1},           {"o", "O", 2, 1},
    {"p", "P", 2, 1},           {"lbracket", "[", 2, 1},
    {"rbracket", "]", 2, 1},    {"backslash", "\\", 2, 1},
    {"home", "Home", 2, 1},

    {"capslock", "Caps", 3, 1}, {"a", "A", 3, 1},
    {"s", "S", 3, 1},           {"d", "D", 3, 1},
    {"f", "F", 3, 1},           {"g", "G", 3, 1},
    {"h", "H", 3, 1},           {"j", "J", 3, 1},
    {"k", "K", 3, 1},           {"l", "L", 3, 1},
    {"semicolon", ";", 3, 1},   {"quote", "'", 3, 1},
    {"enter", "Enter", 3, 2},   {"end", "End", 3, 1},

    {"shift", "Shift", 4, 2},   {"z", "Z", 4, 1},
    {"x", "X", 4, 1},           {"c", "C", 4, 1},
    {"v", "V", 4, 1},           {"b", "B", 4, 1},
    {"n", "N", 4, 1},           {"m", "M", 4, 1},
    {"comma", ",", 4, 1},       {"period", ".", 4, 1},
    {"slash", "/", 4, 1},       {"pageup", "PgUp", 4, 1},
    {"up", "Up", 4, 1},         {"pagedown", "PgDn", 4, 1},

    {"ctrl", "Ctrl", 5, 2},     {"lwin", "Win", 5, 1},
    {"alt", "Alt", 5, 1},       {"space", "Space", 5, 6},
    {"left", "Left", 5, 1},     {"down", "Down", 5, 1},
    {"right", "Right", 5, 1},
};

// The modifier names, in the order a combination writes them. The grid has
// ctrl, shift, alt and lwin; the side-specific names are known so a
// selection given to set_keys() is put in the same order.
inline constexpr std::string_view kKeyPickerModifiers[] = {
    "ctrl", "lctrl", "rctrl", "shift", "lshift", "rshift", "alt", "lalt", "ralt", "lwin", "rwin",
};

// The key with this name in kKeyPickerKeys, or nullptr.
const KeyPickerKey *find_picker_key(std::string_view name);
// Its place in kKeyPickerModifiers, or -1 for a key that is not a modifier.
int picker_modifier_rank(std::string_view name);

struct KeyPickerStyle : ComponentStyle
{
    KeyPickerStyle()
    {
        // A filled plate is what players know from console keyboards.
        highlight.kind = HighlightKind::fill;
    }

    // ---- geometry ----
    float gap = 6.0f; // between keys
    RadiusSource radius_source = RadiusSource::theme;
    float radius = 8.0f;          // RadiusSource::custom
    float panel_padding = 16.0f;  // between the panel and its content (panel = true)
    float summary_height = 56.0f; // the row of picked keys above the grid; 0 hides it
    float summary_gap = 14.0f;    // between that row and the grid
    float icon_size = 20.0f;      // the arrows on the arrow keys
    // ---- type ----
    float label_size = 24.0f;      // one-column keys
    float wide_label_size = 19.0f; // words ("Backspace", "PgUp")
    float summary_size = 20.0f;    // the chips of the picked keys
    // ---- look ----
    HighlightStyle highlight; // fill by default
    bool surfaces = true;     // every key is a themed surface; false: flat caps
    bool panel = true;        // a themed panel behind everything
    bool done_key = true;     // a Done key at the end of the last row
    std::string done_label = "Done";
    std::string empty_text = "No key"; // the summary while nothing is picked
    // ---- behaviour ----
    int max_keys = 4;              // 1: confirm replaces the pick instead of adding to it
    bool allow_empty = false;      // false: accepting with nothing picked is refused
    Action clear = Action::west;   // clears the pick; Action::count for none
    Action accept = Action::north; // the same as the Done key; Action::count for none
    bool wrap = true;              // left / right leave through one edge and enter by the other
    bool column_memory = true;     // up and down return to the column the player left
    EdgeExits exits;               // edges that hand the focus back; an exit beats the wrap
    // ---- feel ----
    float press_dip = 0.06f;     // a pressed key shrinks by this share
    float entrance_step = 0.03f; // seconds between rows arriving; 0 for none
};

// A PC keyboard for a controller, to pick one key or a combination of up to
// max_keys keys, such as Ctrl + S. Cross picks or drops the focused key, the
// picked keys stay lit and are listed above the grid. Square clears, Triangle
// or the Done key accepts, Circle cancels. Modifiers always come first, in
// the order of kKeyPickerModifiers; the other keys keep the order they were
// picked in. The picker returns key names (kKeyPickerKeys), never characters.
//
//   ui::KeyPicker picker;
//   picker.style.theme = theme;
//   picker.set_keys({"ctrl", "s"}); // what is bound now
//   picker.set_bounds({300, 300, 1320, picker.preferred_height(1320)});
//   picker.on_accept = [&](const std::vector<std::string> &keys) { bind(keys); };
//   ...
//   const ui::Event event = picker.handle(input, feedback);
//   if (event == ui::Event::activated) close();       // keys() holds the pick
//   if (event == ui::Event::cancelled) close();       // the pick was not taken
//   picker.update(dt);
//   picker.draw(canvas);
class KeyPicker
{
  public:
    KeyPickerStyle style;
    // The pick changed (a key picked or dropped, or a clear).
    std::function<void(const std::vector<std::string> &keys)> on_change;
    // Triangle or the Done key, with an accepted pick.
    std::function<void(const std::vector<std::string> &keys)> on_accept;
    // Circle.
    std::function<void()> on_cancel;

    KeyPicker();

    // ---- the pick ----
    // Silent. Unknown names, repeats and names past max_keys are dropped; the
    // rest are put in order (modifiers first). False if any name was dropped.
    bool set_keys(std::initializer_list<std::string_view> names);
    bool set_keys(const std::vector<std::string_view> &names);
    bool set_keys(const std::vector<std::string> &names);
    // The picked names, modifiers first.
    const std::vector<std::string> &keys() const
    {
        return keys_;
    }
    bool picked(std::string_view name) const;
    // The labels joined by " + " ("Ctrl + S"); empty while nothing is picked.
    std::string summary() const;

    // ---- what the buttons do, for a screen that maps its own ----
    // Moves the focus to the key and toggles it, as a press of Cross there.
    Event tap(std::string_view name, Feedback &feedback);
    Event clear(Feedback &feedback);
    Event accept(Feedback &feedback);

    // ---- the five rules ----
    void set_bounds(const gfx::Rect &bounds);
    const gfx::Rect &bounds() const
    {
        return bounds_;
    }
    // The height that gives keys of a pleasant shape at this width.
    float preferred_height(float width) const;
    // An inactive picker keeps a faint highlight.
    void set_active(bool active)
    {
        active_ = active;
    }
    // Replays the entrance animation.
    void enter();

    // changed: a key picked or dropped, or the pick cleared. activated: the
    // pick was accepted. cancelled: back. refused: a fifth key, a clear with
    // nothing picked, an empty accept, an edge.
    Event handle(const InputFrame &input, Feedback &feedback);
    void update(float dt);
    void draw(Canvas &canvas) const;

    // ---- focus ----
    // An index into the grid: kKeyPickerKeys in order, then the Done key.
    int focus() const
    {
        return focus_;
    }
    // The focused key's name; empty on the Done key.
    std::string_view focused_name() const;
    void set_focus(int index, bool snap = true);
    bool focus_key(std::string_view name, bool snap = true);
    int key_count() const; // the grid's keys, the Done key included
    gfx::Rect key_rect(int index) const;
    // The edge the focus left through when handle() returned none on a
    // direction (style.exits).
    Direction exit() const
    {
        return exit_;
    }

  private:
    struct Cell
    {
        int row = 0;
        int column = 0; // in grid columns
        int span = 1;
    };

    void layout_cells();
    gfx::Rect grid() const;
    gfx::Rect summary_rect() const;
    bool is_done(int index) const;
    int cell_under(int row, float column) const;
    void retarget(bool snap);
    Event move(const InputFrame &input, Feedback &feedback);
    Event toggle(int index, Feedback &feedback);
    void order();
    void changed();
    float key_radius(const Painter &paint, const gfx::Rect &r) const;
    void draw_summary(Canvas &canvas, Painter &paint) const;

    std::vector<Cell> cells_; // one per key of kKeyPickerKeys, then the Done key
    bool cells_done_key_ = true;
    std::vector<std::string> keys_;
    gfx::Rect bounds_{0.0f, 0.0f, 1320.0f, 520.0f};
    int focus_ = 0;
    float column_ = 0.5f; // where the player really is, in columns
    bool active_ = true;
    Direction exit_ = Direction::none;
    float age_ = 10.0f;
    Highlight highlight_;
    Pulse press_;
    int press_index_ = -1;
    Pulse summary_shake_;
    tween::Spring active_amount_{1.0f, 0.0f, 1.0f};
    std::vector<float> lit_; // per key: how picked it looks, 0..1
};

} // namespace hui::ui
