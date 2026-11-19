// ps5-homebrew-ui - Tests: Keyboard, PinEntry, SearchField, KeyBinder, KeyPicker and InputPrompt
// behaviour. Copyright (C) 2026 BlackBearReloaded SPDX-License-Identifier: GPL-3.0-or-later

#include "component_fixture.hpp"
#include "concepts/components/page.hpp"
#include "ui/components/input_prompt.hpp"
#include "ui/components/key_binder.hpp"
#include "ui/components/key_picker.hpp"
#include "ui/components/keyboard.hpp"
#include "ui/components/pin_entry.hpp"
#include "ui/components/search_field.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace
{

using hui::Action;
using hui::action_bit;
using hui::Direction;
using hui::audio::Cue;
using hui::ui::Event;
using hui::ui::HighlightKind;
using hui::ui::InputPrompt;
using hui::ui::KeyBinder;
using hui::ui::Keyboard;
using hui::ui::KeyboardLayout;
using hui::ui::KeyboardShift;
using hui::ui::KeyPicker;
using hui::ui::KeyPickerKey;
using hui::ui::PinEntry;
using hui::ui::PinState;
using hui::ui::SearchField;
using hui::ui::Suggestion;

// Rows and keys of KeyboardLayout::letters().
constexpr int kQwertyRow = 1;
constexpr int kBottomRow = 4;
constexpr int kSpaceKey = 2;
constexpr int kBackspaceKey = 3;
constexpr int kDoneKey = 4;

class ComponentsEntry : public hui::testing::ComponentFixture
{
  protected:
    ComponentsEntry()
    {
        keys_.set_bounds({100.0f, 100.0f, 800.0f, 360.0f});
        keys_.on_text = [this](std::string_view utf8) { text_.append(utf8); };
        keys_.on_backspace = [this]() { ++erased_; };
        keys_.on_done = [this]() { ++done_; };
    }

    // One input to a component, then half a second of animation.
    template <typename Component> Event send(Component &component, const hui::InputFrame &input)
    {
        feedback_.clear();
        const Event event = component.handle(input, feedback_);
        for (int i = 0; i < 30; ++i)
            component.update(kFrame);
        return event;
    }
    Event send(const hui::InputFrame &input)
    {
        return send(keys_, input);
    }
    static hui::InputFrame repeat(Direction direction)
    {
        hui::InputFrame input = nav(direction);
        input.nav_repeat = true;
        return input;
    }
    static hui::InputFrame hold(Action action)
    {
        hui::InputFrame input = idle();
        input.held = action_bit(action);
        return input;
    }

    Keyboard keys_;
    std::string text_;
    int erased_ = 0;
    int done_ = 0;
};

TEST_F(ComponentsEntry, KeyboardSendsWhatItTypes)
{
    keys_.set_focus(kQwertyRow, 0);
    EXPECT_EQ(send(press(Action::confirm)), Event::changed);
    EXPECT_EQ(text_, "q");
    EXPECT_EQ(keys_.typed(), "q");
    EXPECT_TRUE(asked(Cue::type));

    keys_.set_focus(kBottomRow, kSpaceKey);
    EXPECT_EQ(send(press(Action::confirm)), Event::changed);
    EXPECT_EQ(text_, "q ");

    keys_.set_focus(kBottomRow, kBackspaceKey);
    EXPECT_EQ(send(press(Action::confirm)), Event::changed);
    EXPECT_EQ(erased_, 1);
    EXPECT_EQ(keys_.erased(), 1);
    EXPECT_TRUE(asked(Cue::erase));

    keys_.set_focus(kBottomRow, kDoneKey);
    EXPECT_EQ(send(press(Action::confirm)), Event::activated);
    EXPECT_EQ(done_, 1);
    EXPECT_TRUE(asked(Cue::select));
    EXPECT_EQ(send(press(Action::back)), Event::cancelled);
    EXPECT_TRUE(asked(Cue::back));
}

TEST_F(ComponentsEntry, KeyboardShiftIsSpentByALetterAndLocksOnASecondPress)
{
    keys_.toggle_shift(feedback_);
    EXPECT_EQ(keys_.shift(), KeyboardShift::once);
    EXPECT_TRUE(asked(Cue::toggle));
    // A digit does not spend it; a letter does.
    keys_.set_focus(0, 0);
    send(press(Action::confirm));
    EXPECT_EQ(keys_.shift(), KeyboardShift::once);
    keys_.set_focus(kQwertyRow, 0);
    send(press(Action::confirm));
    EXPECT_EQ(text_, "1Q");
    EXPECT_EQ(keys_.shift(), KeyboardShift::off);

    keys_.toggle_shift(feedback_);
    keys_.toggle_shift(feedback_);
    EXPECT_EQ(keys_.shift(), KeyboardShift::lock);
    send(press(Action::confirm));
    send(press(Action::confirm));
    EXPECT_EQ(text_, "1QQQ");
    EXPECT_EQ(keys_.shift(), KeyboardShift::lock);
    keys_.toggle_shift(feedback_);
    EXPECT_EQ(keys_.shift(), KeyboardShift::off);

    // Without the lock a second press turns it off again.
    keys_.style.shift_lock = false;
    keys_.toggle_shift(feedback_);
    keys_.toggle_shift(feedback_);
    EXPECT_EQ(keys_.shift(), KeyboardShift::off);

    // An empty text arms it once, by itself.
    keys_.style.auto_capital = true;
    keys_.set_length(0);
    send(idle());
    EXPECT_EQ(keys_.shift(), KeyboardShift::once);
}

TEST_F(ComponentsEntry, KeyboardWrapsSidewaysAndRemembersTheColumn)
{
    keys_.set_focus(kQwertyRow, 9);
    EXPECT_EQ(send(nav(Direction::right)), Event::moved);
    EXPECT_EQ(keys_.focus_key(), 0);
    EXPECT_TRUE(asked(Cue::focus));
    EXPECT_EQ(send(nav(Direction::left)), Event::moved);
    EXPECT_EQ(keys_.focus_key(), 9);
    // A held direction stops at the edge, silently.
    EXPECT_EQ(send(repeat(Direction::right)), Event::refused);
    EXPECT_TRUE(feedback_.cues.empty());
    keys_.style.wrap = false;
    EXPECT_EQ(send(nav(Direction::right)), Event::refused);
    EXPECT_TRUE(asked(Cue::error));

    // "n" is the sixth key; the space bar under it is three columns wide.
    keys_.set_focus(3, 5);
    EXPECT_EQ(send(nav(Direction::down)), Event::moved);
    EXPECT_EQ(keys_.focus_row(), kBottomRow);
    EXPECT_EQ(keys_.focus_key(), kSpaceKey);
    EXPECT_EQ(send(nav(Direction::up)), Event::moved);
    EXPECT_EQ(keys_.focus_key(), 5);
    // Without the memory, up from a wide key goes to what is over its middle.
    keys_.style.column_memory = false;
    send(nav(Direction::down));
    send(nav(Direction::up));
    EXPECT_EQ(keys_.focus_key(), 4);

    // The top and the bottom are edges, unless the screen takes the focus.
    keys_.set_focus(0, 3);
    EXPECT_EQ(send(nav(Direction::up)), Event::refused);
    keys_.style.exits.up = true;
    EXPECT_EQ(send(nav(Direction::up)), Event::none);
    EXPECT_EQ(keys_.exit(), Direction::up);
}

TEST_F(ComponentsEntry, KeyboardShortcutsAndHeldBackspace)
{
    // Nothing is bound until the screen says so: Square is not the keyboard's.
    keys_.set_length(12);
    EXPECT_EQ(send(press(Action::west)), Event::none);
    keys_.style.bindings = hui::ui::KeyboardBindings::standard();
    EXPECT_EQ(send(press(Action::north)), Event::changed);
    EXPECT_EQ(text_, " ");
    send(press(Action::jump_prev));
    EXPECT_EQ(keys_.shift(), KeyboardShift::once);
    send(press(Action::jump_next));
    EXPECT_EQ(keys_.layout(), 1);

    EXPECT_EQ(send(press(Action::west)), Event::changed);
    EXPECT_EQ(erased_, 1);
    // Held, it waits and then repeats.
    for (int i = 0; i < 60; ++i)
    {
        keys_.handle(hold(Action::west), feedback_);
        keys_.update(kFrame);
    }
    EXPECT_GE(erased_, 5);
    EXPECT_LE(erased_, 9);

    // An empty text refuses a press and stays silent on the repeats.
    keys_.set_length(0);
    EXPECT_EQ(send(press(Action::west)), Event::refused);
    EXPECT_TRUE(asked(Cue::error));
    feedback_.clear();
    const int before = erased_;
    for (int i = 0; i < 60; ++i)
    {
        keys_.handle(hold(Action::west), feedback_);
        keys_.update(kFrame);
    }
    EXPECT_EQ(erased_, before);
    EXPECT_TRUE(feedback_.cues.empty());
}

TEST_F(ComponentsEntry, KeyboardRefusesAtTheLimit)
{
    keys_.style.max_length = 3;
    keys_.set_length(2);
    keys_.set_focus(kQwertyRow, 2);
    EXPECT_EQ(send(press(Action::confirm)), Event::changed);
    EXPECT_EQ(keys_.length(), 3);
    EXPECT_EQ(send(press(Action::confirm)), Event::refused);
    EXPECT_TRUE(asked(Cue::error));
    EXPECT_EQ(text_, "e");
    EXPECT_TRUE(keys_.typed().empty());
}

TEST_F(ComponentsEntry, KeyboardLayoutsAreDataAndTapFindsTheKey)
{
    keys_.cycle_layout(feedback_);
    EXPECT_EQ(keys_.layout(), 1);
    EXPECT_TRUE(asked(Cue::tab));

    // A capital is on the letters: tap goes back there and arms shift.
    EXPECT_EQ(keys_.tap("A", feedback_), Event::changed);
    EXPECT_EQ(keys_.layout(), 0);
    EXPECT_EQ(keys_.typed(), "A");
    EXPECT_EQ(keys_.focused().text, "a");
    EXPECT_EQ(keys_.tap("#", feedback_), Event::changed);
    EXPECT_EQ(keys_.layout(), 1);
    EXPECT_EQ(keys_.tap("\b", feedback_), Event::changed);
    EXPECT_EQ(keys_.erased(), 1);
    EXPECT_EQ(text_, "A#");

    keys_.set_layouts({KeyboardLayout::numeric()});
    EXPECT_EQ(keys_.layouts()[0].columns, 3);
    EXPECT_EQ(keys_.tap("7", feedback_), Event::changed);
    EXPECT_EQ(keys_.tap("\n", feedback_), Event::activated);
    // One layout has nothing to switch to.
    feedback_.clear();
    keys_.cycle_layout(feedback_);
    EXPECT_TRUE(asked(Cue::error));

    keys_.set_layouts({KeyboardLayout::email(), KeyboardLayout::symbols()});
    EXPECT_EQ(keys_.tap(".com", feedback_), Event::changed);
    EXPECT_EQ(keys_.tap("@", feedback_), Event::changed);
    EXPECT_EQ(text_, "A#7.com@");

    // A layout of your own.
    KeyboardLayout vowels;
    vowels.columns = 5;
    vowels.add_row("aeiou");
    keys_.set_layouts({vowels});
    keys_.set_focus(0, 4);
    EXPECT_EQ(send(press(Action::confirm)), Event::changed);
    EXPECT_EQ(keys_.typed(), "u");
}

TEST_F(ComponentsEntry, PinEntrySpinsMovesAndSubmits)
{
    PinEntry pin;
    pin.set_bounds({100.0f, 100.0f, 400.0f, 160.0f});
    EXPECT_EQ(send(pin, nav(Direction::up)), Event::changed);
    EXPECT_EQ(pin.digit(0), 0);
    EXPECT_TRUE(asked(Cue::slider));
    send(pin, nav(Direction::down));
    EXPECT_EQ(pin.digit(0), 9); // 0 wraps to 9
    send(pin, nav(Direction::up));
    send(pin, nav(Direction::up));
    EXPECT_EQ(pin.digit(0), 1);

    EXPECT_EQ(send(pin, nav(Direction::left)), Event::refused);
    EXPECT_EQ(send(pin, nav(Direction::right)), Event::moved);
    EXPECT_EQ(pin.cursor(), 1);
    // Confirm is "next" on a set box and refused on an empty one.
    EXPECT_EQ(send(pin, press(Action::confirm)), Event::refused);
    send(pin, nav(Direction::down));
    EXPECT_EQ(pin.digit(1), 9);
    EXPECT_EQ(send(pin, press(Action::confirm)), Event::moved);
    EXPECT_EQ(pin.cursor(), 2);

    pin.set_value("123");
    EXPECT_FALSE(pin.complete());
    EXPECT_EQ(pin.cursor(), 3);
    send(pin, nav(Direction::up));
    EXPECT_TRUE(pin.complete());
    EXPECT_EQ(pin.value(), "1230");
    EXPECT_EQ(send(pin, press(Action::confirm)), Event::activated);
    EXPECT_TRUE(asked(Cue::select));
    EXPECT_EQ(send(pin, press(Action::back)), Event::cancelled);

    // Edges hand the focus on where the screen asks; without the spin, up too.
    pin.style.exits.right = true;
    pin.style.exits.up = true;
    EXPECT_EQ(send(pin, nav(Direction::right)), Event::none);
    EXPECT_EQ(pin.exit(), Direction::right);
    pin.style.spin = false;
    EXPECT_EQ(send(pin, nav(Direction::up)), Event::none);
    EXPECT_EQ(pin.exit(), Direction::up);
}

TEST_F(ComponentsEntry, PinEntryTypedDigitsAdvanceAndTheVerdictShows)
{
    PinEntry pin;
    pin.set_bounds({100.0f, 100.0f, 400.0f, 160.0f});
    EXPECT_EQ(pin.insert('2', feedback_), Event::changed);
    EXPECT_EQ(pin.cursor(), 1);
    EXPECT_TRUE(asked(Cue::type));
    EXPECT_EQ(pin.insert('x', feedback_), Event::refused);
    EXPECT_EQ(pin.insert("58", feedback_), Event::changed);
    EXPECT_EQ(pin.insert('0', feedback_), Event::activated);
    EXPECT_EQ(pin.value(), "2580");

    feedback_.clear();
    EXPECT_EQ(pin.backspace(feedback_), Event::changed);
    EXPECT_TRUE(asked(Cue::erase));
    EXPECT_EQ(pin.value(), "258");
    // On an empty box backspace takes the digit before it.
    pin.set_cursor(3);
    EXPECT_EQ(pin.backspace(feedback_), Event::changed);
    EXPECT_EQ(pin.value(), "25");
    EXPECT_EQ(pin.cursor(), 2);

    feedback_.clear();
    pin.reject(feedback_, "That is not the code");
    EXPECT_EQ(pin.state(), PinState::error);
    EXPECT_TRUE(pin.value().empty());
    EXPECT_EQ(pin.cursor(), 0);
    EXPECT_TRUE(asked(Cue::error));
    // The next edit answers the error.
    pin.insert('2', feedback_);
    EXPECT_EQ(pin.state(), PinState::neutral);

    pin.insert("580", feedback_);
    feedback_.clear();
    pin.accept(feedback_);
    EXPECT_EQ(pin.state(), PinState::success);
    EXPECT_TRUE(asked(Cue::saved));
    // An accepted code is settled.
    EXPECT_EQ(send(pin, nav(Direction::up)), Event::none);
    EXPECT_EQ(pin.insert('1', feedback_), Event::refused);
    EXPECT_EQ(pin.value(), "2580");
    pin.reset();
    EXPECT_EQ(pin.state(), PinState::neutral);
    EXPECT_TRUE(pin.value().empty());
}

TEST_F(ComponentsEntry, SearchFieldAsksTheProviderAndPicks)
{
    SearchField search;
    search.set_bounds({100.0f, 100.0f, 440.0f, 400.0f});
    search.provider = [](std::string_view query, std::vector<Suggestion> &out)
    {
        for (const char *title : {"Glass Orchard", "Neon Harbor", "Tidewater"})
        {
            if (std::string_view(title).find(query) != std::string_view::npos)
                out.push_back({title, "Game", 0});
        }
    };
    search.set_recent({"tide", "racing"});
    EXPECT_TRUE(search.showing_recent());
    EXPECT_EQ(search.suggestions().size(), 2u);

    // The provider is asked once the player stops typing.
    EXPECT_TRUE(search.insert("ar"));
    EXPECT_TRUE(search.searching());
    send(search, idle());
    EXPECT_FALSE(search.searching());
    EXPECT_FALSE(search.showing_recent());
    ASSERT_EQ(search.suggestions().size(), 2u);

    EXPECT_EQ(send(search, nav(Direction::down)), Event::moved);
    EXPECT_TRUE(search.in_list());
    EXPECT_EQ(send(search, nav(Direction::down)), Event::moved);
    EXPECT_EQ(search.focus(), 1);
    EXPECT_EQ(send(search, nav(Direction::down)), Event::refused);
    EXPECT_EQ(send(search, press(Action::confirm)), Event::activated);
    EXPECT_TRUE(search.has_pick());
    EXPECT_EQ(search.picked().text, "Neon Harbor");
    EXPECT_EQ(search.text(), "Neon Harbor");
    EXPECT_EQ(search.focus(), SearchField::kFocusField);
    EXPECT_EQ(search.recent().front(), "Neon Harbor");

    // Confirm on the field itself is "open the keyboard".
    EXPECT_EQ(send(search, press(Action::confirm)), Event::activated);
    EXPECT_FALSE(search.has_pick());

    // The clear button is one step to the right.
    EXPECT_EQ(send(search, nav(Direction::right)), Event::moved);
    EXPECT_EQ(search.focus(), SearchField::kFocusClear);
    EXPECT_EQ(send(search, press(Action::confirm)), Event::changed);
    EXPECT_TRUE(search.text().empty());
    EXPECT_TRUE(search.showing_recent());
    EXPECT_EQ(search.suggestions().size(), 3u);

    // Edges refuse, or hand the focus on.
    EXPECT_EQ(send(search, nav(Direction::left)), Event::refused);
    search.style.exits.left = true;
    EXPECT_EQ(send(search, nav(Direction::left)), Event::none);
    EXPECT_EQ(search.exit(), Direction::left);

    // The editing calls with a voice, the limit, and no debounce.
    search.style.max_length = 2;
    search.style.debounce = 0.0f;
    EXPECT_EQ(search.insert('T', feedback_), Event::changed);
    EXPECT_TRUE(asked(Cue::type));
    EXPECT_EQ(search.suggestions().size(), 1u);
    EXPECT_TRUE(search.insert("ide")); // as far as it fits
    EXPECT_EQ(search.text(), "Ti");
    EXPECT_TRUE(search.full());
    EXPECT_FALSE(search.insert('x'));
    EXPECT_EQ(search.insert('x', feedback_), Event::refused);
    EXPECT_EQ(search.backspace(feedback_), Event::changed);
    EXPECT_EQ(search.text(), "T");
    EXPECT_EQ(send(search, press(Action::back)), Event::cancelled);
}

TEST_F(ComponentsEntry, KeyBinderListensSwapsRefusesAndResets)
{
    enum
    {
        kJump = 1,
        kDodge,
        kAttack,
        kFixed,
    };
    KeyBinder binder;
    binder.set_bindings({{kJump, "Jump", Action::confirm, Action::count, false},
                         {kDodge, "Dodge", Action::back, Action::count, false},
                         {kAttack, "Attack", Action::west, Action::count, false},
                         {kFixed, "Pause", Action::r3, Action::count, true}});
    binder.set_bounds({100.0f, 100.0f, 420.0f, 400.0f});
    EXPECT_TRUE(binder.is_default());

    EXPECT_EQ(send(binder, press(Action::confirm)), Event::activated);
    EXPECT_TRUE(binder.listening());
    EXPECT_GT(binder.listen_left(), 3.0f);
    // A button the application keeps is refused; the row keeps listening.
    EXPECT_EQ(send(binder, press(Action::jump_next)), Event::refused);
    EXPECT_TRUE(binder.listening());
    EXPECT_EQ(send(binder, press(Action::north)), Event::changed);
    EXPECT_FALSE(binder.listening());
    EXPECT_EQ(binder.changed_id(), kJump);
    EXPECT_EQ(binder.action_of(kJump), Action::north);
    EXPECT_TRUE(asked(Cue::toggle));

    // A button another row has: the two rows swap.
    send(binder, nav(Direction::down));
    send(binder, press(Action::confirm));
    EXPECT_EQ(send(binder, press(Action::west)), Event::changed);
    EXPECT_EQ(binder.action_of(kDodge), Action::west);
    EXPECT_EQ(binder.action_of(kAttack), Action::back);

    // ... or the press is refused.
    binder.style.conflict = hui::ui::BindConflict::refuse;
    send(binder, press(Action::confirm));
    EXPECT_EQ(send(binder, press(Action::north)), Event::refused);
    EXPECT_TRUE(binder.listening());
    EXPECT_EQ(binder.action_of(kDodge), Action::west);
    // A D-pad direction is a button like any other.
    EXPECT_EQ(send(binder, nav(Direction::up)), Event::changed);
    EXPECT_EQ(binder.action_of(kDodge), Action::up);

    // Back keeps the old button; so does waiting.
    send(binder, press(Action::confirm));
    EXPECT_EQ(send(binder, press(Action::back)), Event::cancelled);
    EXPECT_FALSE(binder.listening());
    send(binder, press(Action::confirm));
    for (int i = 0; i < 230; ++i)
        binder.update(kFrame);
    EXPECT_EQ(send(binder, idle()), Event::cancelled);
    EXPECT_TRUE(asked(Cue::back));
    EXPECT_EQ(binder.action_of(kDodge), Action::up);

    // A locked row does not listen.
    binder.set_focus(3);
    EXPECT_EQ(send(binder, press(Action::confirm)), Event::refused);

    // The last row puts everything back, once.
    binder.set_focus(4);
    EXPECT_EQ(send(binder, press(Action::confirm)), Event::changed);
    EXPECT_EQ(binder.changed_id(), KeyBinder::kResetId);
    EXPECT_TRUE(binder.is_default());
    EXPECT_EQ(binder.action_of(kJump), Action::confirm);
    EXPECT_EQ(send(binder, press(Action::confirm)), Event::refused);
    EXPECT_EQ(send(binder, nav(Direction::down)), Event::refused);
}

TEST_F(ComponentsEntry, InputPromptComposesAFieldAndAKeyboard)
{
    KeyPicker picker;
    picker.set_bounds({100.0f, 520.0f, 900.0f, 420.0f});
    InputPrompt prompt;
    prompt.set_title("Name this save");
    prompt.style.max_length = 4;
    prompt.open(feedback_, "Ab");
    EXPECT_TRUE(prompt.is_open());
    EXPECT_TRUE(asked(Cue::modal_open));
    EXPECT_EQ(prompt.text(), "Ab");

    // It opens on "g", in the middle of the board.
    EXPECT_EQ(send(prompt, press(Action::confirm)), Event::changed);
    EXPECT_EQ(prompt.text(), "Abg");
    send(prompt, press(Action::confirm));
    EXPECT_EQ(send(prompt, press(Action::confirm)), Event::refused); // full
    EXPECT_EQ(prompt.text(), "Abgg");
    prompt.keyboard.set_focus(kBottomRow, kBackspaceKey);
    EXPECT_EQ(send(prompt, press(Action::confirm)), Event::changed);
    EXPECT_EQ(prompt.text(), "Abg");

    prompt.keyboard.set_focus(kBottomRow, kDoneKey);
    EXPECT_EQ(send(prompt, press(Action::confirm)), Event::activated);
    EXPECT_FALSE(prompt.is_open());
    EXPECT_EQ(prompt.text(), "Abg");
    EXPECT_TRUE(asked(Cue::select));

    // Done on an empty text is refused, and the field says why.
    prompt.open(feedback_);
    send(prompt, idle());
    prompt.keyboard.set_focus(kBottomRow, kDoneKey);
    EXPECT_EQ(send(prompt, press(Action::confirm)), Event::refused);
    EXPECT_TRUE(prompt.is_open());
    EXPECT_FALSE(prompt.field.error().empty());
    // A new text starts with a capital, and typing answers the error.
    prompt.keyboard.set_focus(kQwertyRow, 0);
    send(prompt, press(Action::confirm));
    EXPECT_EQ(prompt.text(), "Q");
    EXPECT_TRUE(prompt.field.error().empty());

    // Down from the last row of keys reaches Cancel / Done.
    prompt.keyboard.set_focus(kBottomRow, kSpaceKey);
    EXPECT_EQ(send(prompt, nav(Direction::down)), Event::moved);
    EXPECT_TRUE(prompt.on_buttons());
    EXPECT_EQ(send(prompt, nav(Direction::right)), Event::refused);
    EXPECT_EQ(send(prompt, nav(Direction::left)), Event::moved);
    EXPECT_EQ(send(prompt, press(Action::confirm)), Event::cancelled);
    EXPECT_FALSE(prompt.is_open());
    EXPECT_TRUE(asked(Cue::modal_close));

    prompt.open(feedback_, "Kept");
    EXPECT_EQ(send(prompt, press(Action::back)), Event::cancelled);
    EXPECT_FALSE(prompt.is_open());
    EXPECT_FALSE(prompt.visible());
}

TEST_F(ComponentsEntry, KeyPickerNamesAreWordsAndFillTheGrid)
{
    std::set<std::string_view> names;
    int columns[6] = {};
    for (const KeyPickerKey &key : hui::ui::kKeyPickerKeys)
    {
        EXPECT_TRUE(names.insert(key.name).second) << key.name;
        EXPECT_FALSE(key.name.empty());
        EXPECT_FALSE(key.label.empty()) << key.name;
        // Names go into text that separates with + , ; = and spaces.
        for (char c : key.name)
            EXPECT_TRUE((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) << key.name;
        ASSERT_LT(key.row, 6);
        columns[key.row] += key.span;
    }
    // Every row covers the 15 columns; the last leaves 2 for the Done key.
    for (int row = 0; row < 5; ++row)
        EXPECT_EQ(columns[row], 15) << row;
    EXPECT_EQ(columns[5], 13);
    for (std::string_view name : {"ctrl", "shift", "alt", "lwin"})
    {
        EXPECT_NE(hui::ui::find_picker_key(name), nullptr) << name;
        EXPECT_GE(hui::ui::picker_modifier_rank(name), 0) << name;
    }
    EXPECT_EQ(hui::ui::picker_modifier_rank("capslock"), -1);
    EXPECT_EQ(hui::ui::find_picker_key("mouse_left"), nullptr);
}

TEST_F(ComponentsEntry, KeyPickerKeepsModifiersFirstAndStopsAtFour)
{
    KeyPicker picker;
    picker.set_bounds({100.0f, 100.0f, 1320.0f, 560.0f});
    std::vector<std::string> seen;
    picker.on_change = [&](const std::vector<std::string> &keys) { seen = keys; };

    EXPECT_EQ(picker.tap("s", feedback_), Event::changed);
    EXPECT_EQ(picker.tap("ctrl", feedback_), Event::changed);
    EXPECT_EQ(picker.keys(), (std::vector<std::string>{"ctrl", "s"}));
    EXPECT_EQ(seen, picker.keys());
    EXPECT_EQ(picker.summary(), "Ctrl + S");
    EXPECT_TRUE(picker.picked("ctrl"));
    EXPECT_EQ(picker.focused_name(), "ctrl");

    feedback_.clear();
    EXPECT_EQ(picker.tap("lwin", feedback_), Event::changed);
    EXPECT_TRUE(asked(Cue::toggle));
    EXPECT_EQ(picker.tap("shift", feedback_), Event::changed);
    EXPECT_EQ(picker.keys(), (std::vector<std::string>{"ctrl", "shift", "lwin", "s"}));
    // A fifth key is refused; dropping one makes room again.
    feedback_.clear();
    EXPECT_EQ(picker.tap("a", feedback_), Event::refused);
    EXPECT_TRUE(asked(Cue::error));
    EXPECT_EQ(picker.keys().size(), 4u);
    EXPECT_EQ(picker.tap("lwin", feedback_), Event::changed);
    EXPECT_EQ(picker.tap("a", feedback_), Event::changed);
    // The other keys keep the order they were picked in.
    EXPECT_EQ(picker.keys(), (std::vector<std::string>{"ctrl", "shift", "s", "a"}));
    EXPECT_EQ(picker.summary(), "Ctrl + Shift + S + A");

    // Cross on the focused key drops it.
    picker.focus_key("s");
    EXPECT_EQ(send(picker, press(Action::confirm)), Event::changed);
    EXPECT_EQ(picker.keys(), (std::vector<std::string>{"ctrl", "shift", "a"}));
    EXPECT_EQ(picker.tap("nokey", feedback_), Event::refused);
}

TEST_F(ComponentsEntry, KeyPickerClearsAcceptsAndCancels)
{
    KeyPicker picker;
    picker.set_bounds({100.0f, 100.0f, 1320.0f, 560.0f});
    int accepted = 0;
    int cancelled = 0;
    std::vector<std::string> taken;
    picker.on_accept = [&](const std::vector<std::string> &keys)
    {
        ++accepted;
        taken = keys;
    };
    picker.on_cancel = [&]() { ++cancelled; };

    // Nothing picked: Triangle and Square are refused.
    EXPECT_EQ(send(picker, press(Action::north)), Event::refused);
    EXPECT_EQ(send(picker, press(Action::west)), Event::refused);
    EXPECT_EQ(accepted, 0);

    picker.tap("f5", feedback_);
    EXPECT_EQ(send(picker, press(Action::west)), Event::changed);
    EXPECT_TRUE(picker.keys().empty());

    picker.tap("space", feedback_);
    EXPECT_EQ(send(picker, press(Action::north)), Event::activated);
    EXPECT_TRUE(asked(Cue::select));
    EXPECT_EQ(accepted, 1);
    EXPECT_EQ(taken, (std::vector<std::string>{"space"}));

    // The Done key does the same.
    picker.set_focus(picker.key_count() - 1);
    EXPECT_TRUE(picker.focused_name().empty());
    EXPECT_EQ(send(picker, press(Action::confirm)), Event::activated);
    EXPECT_EQ(accepted, 2);

    EXPECT_EQ(send(picker, press(Action::back)), Event::cancelled);
    EXPECT_TRUE(asked(Cue::back));
    EXPECT_EQ(cancelled, 1);

    // An empty pick may be accepted where the screen allows it.
    picker.style.allow_empty = true;
    picker.clear(feedback_);
    EXPECT_EQ(send(picker, press(Action::north)), Event::activated);

    // Without the Done key the space bar takes its columns.
    picker.style.done_key = false;
    picker.update(kFrame);
    EXPECT_EQ(picker.key_count(), static_cast<int>(std::size(hui::ui::kKeyPickerKeys)));
    picker.focus_key("space");
    const hui::gfx::Rect space = picker.key_rect(picker.focus());
    picker.focus_key("right");
    EXPECT_GT(space.w, picker.key_rect(picker.focus()).w * 7.0f);
}

TEST_F(ComponentsEntry, KeyPickerSingleKeyReplacesAndSetKeysOrders)
{
    KeyPicker picker;
    EXPECT_TRUE(picker.set_keys({"e", "alt", "ctrl"}));
    EXPECT_EQ(picker.keys(), (std::vector<std::string>{"ctrl", "alt", "e"}));
    // Unknown names, repeats and names past the limit are dropped.
    EXPECT_FALSE(picker.set_keys(std::vector<std::string>{"q", "q", "x;", "w", "e", "r", "t"}));
    EXPECT_EQ(picker.keys(), (std::vector<std::string>{"q", "w", "e", "r"}));

    picker.style.max_keys = 1;
    picker.set_keys(std::vector<std::string_view>{"w"});
    EXPECT_EQ(picker.tap("up", feedback_), Event::changed);
    EXPECT_EQ(picker.keys(), (std::vector<std::string>{"up"}));
    EXPECT_EQ(picker.tap("up", feedback_), Event::changed);
    EXPECT_TRUE(picker.keys().empty());
}

TEST_F(ComponentsEntry, KeyPickerReachesEveryKeyAndRefusesAtEdges)
{
    KeyPicker picker;
    picker.set_bounds({100.0f, 100.0f, 1320.0f, 560.0f});
    // Walk the grid from every key in every direction: each key is reached.
    std::set<int> reached{0};
    std::vector<int> open{0};
    while (!open.empty())
    {
        const int from = open.back();
        open.pop_back();
        for (Direction direction :
             {Direction::up, Direction::down, Direction::left, Direction::right})
        {
            picker.set_focus(from);
            picker.handle(nav(direction), feedback_);
            if (reached.insert(picker.focus()).second)
                open.push_back(picker.focus());
        }
    }
    EXPECT_EQ(static_cast<int>(reached.size()), picker.key_count());

    // Up and down come back to the column the player left.
    picker.focus_key("g");
    send(picker, nav(Direction::down));
    send(picker, nav(Direction::down));
    EXPECT_EQ(picker.focused_name(), "space");
    send(picker, nav(Direction::up));
    send(picker, nav(Direction::up));
    EXPECT_EQ(picker.focused_name(), "g");

    // The top refuses; a held direction stops at a side instead of wrapping.
    picker.focus_key("escape");
    EXPECT_EQ(send(picker, nav(Direction::up)), Event::refused);
    EXPECT_EQ(send(picker, repeat(Direction::left)), Event::refused);
    EXPECT_EQ(send(picker, nav(Direction::left)), Event::moved);
    EXPECT_EQ(picker.focused_name(), "delete");
    picker.style.exits.right = true;
    EXPECT_EQ(send(picker, nav(Direction::right)), Event::none);
    EXPECT_EQ(picker.exit(), Direction::right);
}

TEST_F(ComponentsEntry, GalleryPageTourPicksAShortcut)
{
    const std::unique_ptr<hui::concepts::gallery::Page> page =
        hui::concepts::gallery::make_entry_page(context_);
    const auto frame = [&](const hui::InputFrame &input)
    {
        feedback_.clear();
        page->update(input, kFrame, feedback_);
        hui::ui::Canvas target = canvas();
        page->draw(target);
        page->draw_modal(target);
    };
    bool picker_seen = false;
    for (const hui::app::TourStep &step : page->tour())
    {
        for (int i = 0, frames = static_cast<int>(step.wait / kFrame); i < frames; ++i)
            frame(idle());
        if (step.capture != nullptr && std::string_view(step.capture) == "entry-keys")
        {
            // The picker is open over the page.
            ASSERT_GE(page->hints().size(), 2u);
            EXPECT_STREQ(page->hints()[1].label, "Pick or drop");
            expect_drawn(step.capture);
            picker_seen = true;
        }
        hui::InputFrame input = idle();
        input.pressed = step.press;
        input.held = step.press;
        input.nav = step.nav;
        frame(input);
    }
    EXPECT_TRUE(picker_seen);
    for (int i = 0; i < 60; ++i)
        frame(idle());
    // Accepted and closed: the page has the focus again.
    EXPECT_STRNE(page->hints()[1].label, "Pick or drop");
}

TEST_F(ComponentsEntry, DrawsInEveryThemeAndVariant)
{
    PinEntry pin;
    pin.set_label("Code");
    pin.set_message("Four digits");
    pin.set_bounds({100.0f, 500.0f, 500.0f, 180.0f});
    pin.set_value("12");
    SearchField search;
    search.set_bounds({1000.0f, 100.0f, 440.0f, 400.0f});
    search.set_placeholder("Search");
    search.set_recent({"tide", "racing"});
    search.provider = [](std::string_view, std::vector<Suggestion> &out)
    {
        out.push_back({"Starlane Courier", "Simulation", 0});
        out.push_back({"A title that is far too long for the row it is in", "Genre", 0});
    };
    KeyBinder binder;
    binder.set_bindings({{1, "Jump", Action::confirm, Action::count, false},
                         {2, "Map", Action::up, Action::count, false},
                         {3, "Unset", Action::count, Action::count, false},
                         {4, "Camera", Action::r3, Action::count, true}});
    binder.set_bounds({1000.0f, 520.0f, 420.0f, 200.0f}); // short: it scrolls
    KeyPicker picker;
    picker.set_bounds({100.0f, 520.0f, 900.0f, 420.0f});
    InputPrompt prompt;
    prompt.set_title("Name this save");
    prompt.field.set_label("Name");

    int index = 0;
    for (const hui::ui::Theme &theme : hui::ui::themes())
    {
        for (HighlightKind kind :
             {HighlightKind::ring, HighlightKind::fill, HighlightKind::tint, HighlightKind::bar,
              HighlightKind::underline, HighlightKind::glow, HighlightKind::none})
        {
            const bool odd = (index++ % 2) != 0;
            keys_.style.theme = theme;
            keys_.style.highlight.kind = kind;
            keys_.style.surfaces = !odd;
            keys_.style.panel = kind != HighlightKind::underline;
            keys_.style.reduced_motion = odd;
            keys_.style.bindings = hui::ui::KeyboardBindings::standard();
            keys_.set_layouts(kind == HighlightKind::bar
                                  ? std::vector<KeyboardLayout>{KeyboardLayout::numeric()}
                                  : std::vector<KeyboardLayout>{KeyboardLayout::email(),
                                                                KeyboardLayout::symbols()});
            // One frame after a wrap: the highlight is entering, its ghost leaving.
            keys_.set_focus(kQwertyRow, 9);
            keys_.handle(nav(Direction::right), feedback_);
            keys_.update(kFrame);

            pin.style.theme = theme;
            pin.style.reduced_motion = odd;
            pin.style.masked = odd;
            pin.style.group = odd ? 2 : 0;
            pin.set_active(true);
            if (kind == HighlightKind::fill)
                pin.accept(feedback_);
            else if (kind == HighlightKind::tint)
                pin.reject(feedback_, "Wrong");
            else if (kind == HighlightKind::bar)
                pin.set_value("1234");
            pin.update(kFrame);

            search.style.theme = theme;
            search.style.highlight.kind = kind;
            search.style.list_panel = !odd;
            search.style.pill = odd;
            search.style.reduced_motion = odd;
            search.set_active(true);
            search.set_text(kind == HighlightKind::glow ? "" : "ar");
            {
                // Just after an edit: the spinner is turning.
                search.update(kFrame);
                hui::ui::Canvas busy = canvas();
                search.draw(busy);
                expect_drawn(theme.id);
            }
            send(search, nav(Direction::down));

            binder.style.theme = theme;
            binder.style.reduced_motion = odd;
            binder.style.highlight.kind = kind;
            binder.style.panel = odd;
            binder.stop_listening();
            binder.set_focus(kind == HighlightKind::ring ? 4 : 0);
            if (kind == HighlightKind::tint)
                send(binder, press(Action::confirm)); // listening

            picker.style.theme = theme;
            picker.style.reduced_motion = odd;
            picker.style.highlight.kind = kind;
            picker.style.surfaces = !odd;
            picker.style.panel = kind != HighlightKind::underline;
            picker.style.done_key = kind != HighlightKind::bar;
            picker.style.summary_height = kind == HighlightKind::glow ? 0.0f : 56.0f;
            picker.set_keys(
                odd ? std::vector<std::string_view>{}
                    : std::vector<std::string_view>{"ctrl", "shift", "backspace", "up"});
            picker.focus_key(odd ? "up" : "space");
            send(picker, press(Action::confirm));

            prompt.style.theme = theme;
            prompt.style.reduced_motion = odd;
            prompt.style.buttons = !odd;
            prompt.keyboard.style.highlight.kind = kind;
            prompt.open(feedback_, odd ? "" : "Marlowe");
            send(prompt, nav(Direction::down));

            hui::ui::Canvas target = canvas();
            keys_.draw(target);
            pin.draw(target);
            search.draw(target);
            binder.draw(target);
            picker.draw(target);
            prompt.draw(target);
            expect_drawn(theme.id);
        }
    }
}

} // namespace
