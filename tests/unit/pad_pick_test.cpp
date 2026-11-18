// ps5-homebrew-ui - Which of the signed-in users' controllers drives the app.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/pad_pick.hpp"

#include <gtest/gtest.h>

namespace
{

hui::PadSample idle()
{
    hui::PadSample sample;
    sample.connected = true;
    return sample;
}

TEST(PadPick, AButtonOrAPushedStickIsUse)
{
    EXPECT_FALSE(hui::pad_sample_used(idle()));
    hui::PadSample cross = idle();
    cross.buttons = hui::pad_bits::kCross;
    EXPECT_TRUE(hui::pad_sample_used(cross));
    hui::PadSample stick = idle();
    stick.left_x = 128 + 100;
    EXPECT_TRUE(hui::pad_sample_used(stick));
    stick.left_x = 128 + 20; // drift
    EXPECT_FALSE(hui::pad_sample_used(stick));
}

TEST(PadPick, NoUseWhileDisconnectedOrHeldByTheSystem)
{
    hui::PadSample gone = idle();
    gone.connected = false;
    gone.buttons = hui::pad_bits::kCross;
    EXPECT_FALSE(hui::pad_sample_used(gone));
    hui::PadSample overlay = idle();
    overlay.buttons = hui::pad_bits::kIntercepted | hui::pad_bits::kCross;
    EXPECT_FALSE(hui::pad_sample_used(overlay));
}

TEST(PadPick, StartsOnThePreferredControllerThenFollowsUse)
{
    hui::PadPick pick;
    const bool open[4] = {true, true, false, false};
    const bool none[4] = {};
    pick.prefer(0);
    EXPECT_EQ(pick.update(none, open), 0);
    // The second user picks up their controller: it takes over.
    const bool second[4] = {false, true, false, false};
    EXPECT_EQ(pick.update(second, open), 1);
    // and keeps the app when nobody touches anything.
    EXPECT_EQ(pick.update(none, open), 1);
}

TEST(PadPick, TheActiveControllerKeepsTheAppWhileItIsUsed)
{
    hui::PadPick pick;
    const bool open[4] = {true, true, false, false};
    pick.prefer(0);
    const bool both[4] = {true, true, false, false};
    EXPECT_EQ(pick.update(both, open), 0);
}

TEST(PadPick, AClosedControllerNeverDrives)
{
    hui::PadPick pick;
    pick.prefer(2);
    const bool open[4] = {false, true, false, false};
    const bool used[4] = {true, false, true, false};
    EXPECT_EQ(pick.update(used, open), 1);
    const bool closed[4] = {};
    const bool none[4] = {};
    EXPECT_EQ(pick.update(none, closed), -1);
}

// On a switch the platform layer hands out one idle sample before the new
// controller's: the old controller's held Cross comes up, so the new one's
// Cross is a press, not a continuation.
TEST(PadPick, ASwitchKeepsTheNewControllersPress)
{
    hui::PadSample cross = idle();
    cross.buttons = hui::pad_bits::kCross;
    hui::InputTracker tracker;
    const hui::PadSample first[] = {cross};
    EXPECT_TRUE(tracker.update(first, 1000).is_pressed(hui::Action::confirm));
    const hui::PadSample without[] = {cross};
    EXPECT_FALSE(tracker.update(without, 2000).is_pressed(hui::Action::confirm));
    const hui::PadSample switched[] = {idle(), cross};
    EXPECT_TRUE(tracker.update(switched, 3000).is_pressed(hui::Action::confirm));
}

} // namespace
