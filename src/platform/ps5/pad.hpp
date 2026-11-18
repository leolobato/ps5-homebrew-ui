// ps5-homebrew-ui - DualSense input through scePad.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "core/input.hpp"
#include "core/pad_pick.hpp"

#include <cstddef>
#include <span>

namespace hui::ps5
{

// Reads every buffered sample of each signed-in user's controller each frame
// (scePadRead batches up to 64), so short taps survive long frames, and hands
// out those of the one used last (core/pad_pick.hpp), as the console's own
// menus do. It starts on the foreground user's controller (the initial user's
// when there is none), and picks up users who sign in or out while it runs.
// Follows ProsperoLight's radio_input.cpp.
class Pad
{
  public:
    Pad() = default;
    Pad(const Pad &) = delete;
    Pad &operator=(const Pad &) = delete;
    ~Pad();

    bool open();
    void close();
    // Fills out with the samples read since the last call from the active
    // controller; returns the count. Returns one disconnected sample if no
    // controller is open.
    std::size_t read(std::span<PadSample> out);
    bool is_open() const
    {
        return pick_.active() >= 0;
    }
    // A system menu covered the app at the last read(): its samples then come
    // out intercepted, whatever the controllers do.
    bool system_menu_open() const
    {
        return overlaid_;
    }

    // Colours the active controller's light bar (for example with the active
    // design's accent). Repeated calls with the same colour do nothing.
    void set_light_bar(std::uint8_t r, std::uint8_t g, std::uint8_t b);
    // Starts a short rumble: strength 0..1, for seconds. tick() stops it.
    void rumble(float strength, float seconds);
    // Call once per frame: ends a rumble whose time is up, and opens or
    // closes controllers as users sign in or out.
    void tick(float dt);

  private:
    struct Slot
    {
        int user = -1;
        int handle = -1;
    };

    // Opens the controller of every signed-in user that has none open yet and
    // closes those of users who signed out; tries attempts times 50 ms apart
    // (the pad service can lag behind the app's start).
    void refresh(int attempts);
    int active_handle() const;
    void stop_rumble(int handle);
    void apply_light_bar(int handle);

    Slot slots_[PadPick::kSlots];
    PadPick pick_;
    bool owns_user_service_ = false;
    bool pad_ready_ = false;
    // The active controller went away: the next read starts with an idle
    // sample, as on a switch.
    bool restart_input_ = false;
    bool overlaid_ = false;
    // Per slot: a system menu just closed, and the controller has not been
    // neutral since.
    bool wait_neutral_[PadPick::kSlots] = {};
    float rumble_left_ = 0.0f;
    float refresh_left_ = 0.0f;
    std::uint32_t light_bar_ = 0xffffffffu;
};

} // namespace hui::ps5
