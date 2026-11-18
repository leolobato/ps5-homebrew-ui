// ps5-homebrew-ui - Which of the signed-in users' controllers drives the app.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "core/input.hpp"

namespace hui
{

// A sample in which the controller is being used: a button down or a stick
// pushed well off centre, on a connected controller the system's overlay does
// not hold. Idle sticks drift a little, so a small deflection does not count.
inline bool pad_sample_used(const PadSample &sample)
{
    if (!sample.connected || (sample.buttons & pad_bits::kIntercepted) != 0)
        return false;
    if ((sample.buttons & ~pad_bits::kIntercepted) != 0)
        return true;
    constexpr int kDeadZone = 64;
    const auto off = [](std::uint8_t axis)
    {
        const int d = static_cast<int>(axis) - 128;
        return d > kDeadZone || d < -kDeadZone;
    };
    return off(sample.left_x) || off(sample.left_y) || off(sample.right_x) || off(sample.right_y);
}

// The app reads every signed-in user's controller, as the console's own menus
// do, and takes its input from the one used last. Following the console's
// foreground user is not enough: it does not move to a user whose controller
// is not a DualSense (a DualShock 4 bridged in by a payload), and it lags
// behind a second player picking up their controller. Only one controller
// drives at a time, so the presses and releases of two are never mixed.
//
//   hui::PadPick pick;
//   pick.prefer(slot_of_foreground_user);
//   ...each frame, with used[i] = a sample just read from slot i was used:
//   const int active = pick.update(used, open);
class PadPick
{
  public:
    static constexpr int kSlots = 4;

    // The slot to start with, until a controller is used.
    void prefer(int slot)
    {
        if (slot >= 0 && slot < kSlots)
            active_ = slot;
    }

    // Once a frame: the active controller keeps the app while it is used;
    // otherwise the first other one that is used takes it over. A closed
    // slot never drives. Returns the active slot, or -1 when none is open.
    int update(const bool (&used)[kSlots], const bool (&open)[kSlots])
    {
        if (active_ >= 0 && !open[active_])
            active_ = -1;
        if (active_ >= 0 && used[active_])
            return active_;
        for (int slot = 0; slot < kSlots; ++slot)
            if (open[slot] && used[slot])
                return active_ = slot;
        if (active_ < 0)
            for (int slot = 0; slot < kSlots; ++slot)
                if (open[slot])
                    return active_ = slot;
        return active_;
    }

    int active() const
    {
        return active_;
    }

  private:
    int active_ = -1;
};

} // namespace hui
