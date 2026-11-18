// ps5-homebrew-ui - DualSense input through scePad.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "platform/ps5/pad.hpp"

#include "platform/ps5/system.hpp"

#include <cstddef>
#include <cstdint>

namespace
{

// scePadRead sample layout (120 bytes), as typed in ProsperoLight.
struct RawPadSample
{
    std::uint32_t buttons;
    std::uint8_t left_x;
    std::uint8_t left_y;
    std::uint8_t right_x;
    std::uint8_t right_y;
    std::uint8_t l2;
    std::uint8_t r2;
    std::uint8_t reserved0[66];
    std::int32_t connected;
    std::uint64_t timestamp_us;
    std::uint8_t extension[16];
    std::uint8_t connected_count;
    std::uint8_t reserved1[15];
};
static_assert(sizeof(RawPadSample) == 120);
static_assert(offsetof(RawPadSample, connected) == 0x4c);
static_assert(offsetof(RawPadSample, timestamp_us) == 0x50);
static_assert(offsetof(RawPadSample, connected_count) == 0x68);

constexpr int kAlreadyInitialized = static_cast<int>(0x80960003);
// The classic two-motor rumble (the other mode is the DualSense's haptics).
constexpr int kVibrationModeCompatible = 2;

struct PadVibration
{
    std::uint8_t large_motor;
    std::uint8_t small_motor;
};

struct PadColor
{
    std::uint8_t r;
    std::uint8_t g;
    std::uint8_t b;
    std::uint8_t reserved;
};

} // namespace

extern "C"
{
    int sceUserServiceInitialize(const void *params);
    int sceUserServiceTerminate(void);
    int sceUserServiceGetInitialUser(int *user);
    int sceUserServiceGetForegroundUser(int *user);
    int sceUserServiceGetLoginUserIdList(int *users);
    int sceSystemServiceGetStatus(void *status);
    int scePadInit(void);
    int scePadOpen(int user, int type, int index, const void *parameters);
    int scePadRead(int handle, RawPadSample *samples, int count);
    int scePadClose(int handle);
    int scePadSetVibrationMode(int handle, int mode);
    int scePadSetVibration(int handle, const PadVibration *vibration);
    int scePadSetLightBar(int handle, const PadColor *color);
}

namespace hui::ps5
{

namespace
{

constexpr int kNoUser = -1;
// Seconds between two looks at who is signed in.
constexpr float kRefreshInterval = 1.0f;

// Whether a system menu (the PS button's, a system dialog) covers the app:
// SceSystemServiceStatus.isSystemUiOverlaid, the byte after its int32 event
// count. The console marks its own controllers' samples intercepted then, but
// not a controller it does not know is connected, such as a DualShock 4 a
// payload bridges in; their input would reach the app behind the menu.
bool system_ui_overlaid()
{
    std::uint8_t status[256] = {};
    if (sceSystemServiceGetStatus(status) != 0)
        return false;
    return status[4] != 0;
}

PadSample convert(const RawPadSample &r)
{
    PadSample s;
    s.buttons = r.buttons;
    s.left_x = r.left_x;
    s.left_y = r.left_y;
    s.right_x = r.right_x;
    s.right_y = r.right_y;
    s.l2 = r.l2;
    s.r2 = r.r2;
    s.connected = r.connected != 0;
    s.timestamp_us = r.timestamp_us;
    return s;
}

} // namespace

Pad::~Pad()
{
    close();
}

bool Pad::open()
{
    const int init = sceUserServiceInitialize(nullptr);
    owns_user_service_ = init == 0;
    if (init != 0 && init != kAlreadyInitialized)
        sys::log("[HUI] pad sceUserServiceInitialize=0x%08x", static_cast<unsigned>(init));
    const int pad_init = scePadInit();
    if (pad_init != 0)
        sys::log("[HUI] pad scePadInit=0x%08x", static_cast<unsigned>(pad_init));
    pad_ready_ = true;
    refresh(20);
    // Start on the foreground user's controller, else the initial user's.
    int first = kNoUser;
    if (sceUserServiceGetForegroundUser(&first) != 0 || first == kNoUser)
        if (sceUserServiceGetInitialUser(&first) != 0)
            first = kNoUser;
    for (int slot = 0; slot < PadPick::kSlots; ++slot)
        if (slots_[slot].handle >= 0 && slots_[slot].user == first)
            pick_.prefer(slot);
    const bool none[PadPick::kSlots] = {};
    bool open[PadPick::kSlots] = {};
    for (int slot = 0; slot < PadPick::kSlots; ++slot)
        open[slot] = slots_[slot].handle >= 0;
    const int active = pick_.update(none, open);
    if (active < 0)
    {
        sys::log("[HUI] pad no controller opened");
        return false;
    }
    sys::log("[HUI] pad active user=0x%08x", static_cast<unsigned>(slots_[active].user));
    return true;
}

void Pad::refresh(int attempts)
{
    int users[PadPick::kSlots] = {kNoUser, kNoUser, kNoUser, kNoUser};
    if (sceUserServiceGetLoginUserIdList(users) != 0)
    {
        // No list: the initial user alone, as before.
        users[0] = kNoUser;
        if (sceUserServiceGetInitialUser(&users[0]) != 0)
            users[0] = kNoUser;
    }
    const auto signed_in = [&](int user)
    {
        for (const int id : users)
            if (id != kNoUser && id == user)
                return true;
        return false;
    };
    // Users who signed out give their controller back.
    for (Slot &slot : slots_)
        if (slot.handle >= 0 && !signed_in(slot.user))
        {
            sys::log("[HUI] pad closed user=0x%08x (signed out)", static_cast<unsigned>(slot.user));
            // The next sample the app gets comes from another controller.
            if (&slot - slots_ == pick_.active())
                restart_input_ = true;
            stop_rumble(slot.handle);
            scePadClose(slot.handle);
            slot = Slot{};
        }
    for (const int user : users)
    {
        if (user == kNoUser)
            continue;
        bool have = false;
        for (const Slot &slot : slots_)
            have = have || (slot.handle >= 0 && slot.user == user);
        if (have)
            continue;
        Slot *empty = nullptr;
        for (Slot &slot : slots_)
            if (empty == nullptr && slot.handle < 0)
                empty = &slot;
        if (empty == nullptr)
            break;
        for (int attempt = 0; attempt < attempts; ++attempt)
        {
            if (attempt > 0)
                sys::sleep_us(50000);
            const int handle = scePadOpen(user, 0, 0, nullptr);
            if (handle >= 0)
            {
                const int mode = scePadSetVibrationMode(handle, kVibrationModeCompatible);
                sys::log("[HUI] pad open user=0x%08x handle=%d attempts=%d vibration_mode=0x%x",
                         static_cast<unsigned>(user), handle, attempt + 1,
                         static_cast<unsigned>(mode));
                *empty = Slot{user, handle};
                apply_light_bar(handle);
                break;
            }
        }
    }
}

int Pad::active_handle() const
{
    const int active = pick_.active();
    return active >= 0 ? slots_[active].handle : -1;
}

void Pad::stop_rumble(int handle)
{
    if (handle < 0)
        return;
    const PadVibration stop{0, 0};
    scePadSetVibration(handle, &stop);
}

void Pad::apply_light_bar(int handle)
{
    if (handle < 0 || light_bar_ == 0xffffffffu)
        return;
    const PadColor color{static_cast<std::uint8_t>(light_bar_ >> 16),
                         static_cast<std::uint8_t>(light_bar_ >> 8),
                         static_cast<std::uint8_t>(light_bar_), 0};
    const int result = scePadSetLightBar(handle, &color);
    if (result != 0)
        sys::log("[HUI] pad scePadSetLightBar=0x%08x", static_cast<unsigned>(result));
}

void Pad::set_light_bar(std::uint8_t r, std::uint8_t g, std::uint8_t b)
{
    const std::uint32_t packed =
        (static_cast<std::uint32_t>(r) << 16) | (static_cast<std::uint32_t>(g) << 8) | b;
    if (packed == light_bar_)
        return;
    light_bar_ = packed;
    // Every open controller wears the app's colour, so the one picked up next
    // already shows it.
    for (const Slot &slot : slots_)
        apply_light_bar(slot.handle);
}

void Pad::rumble(float strength, float seconds)
{
    const int handle = active_handle();
    if (handle < 0 || strength <= 0.0f || seconds <= 0.0f)
        return;
    const float clamped = strength > 1.0f ? 1.0f : strength;
    // The small motor gives the crisp tick a UI wants; the large one adds
    // weight only to strong effects.
    const PadVibration vibration{
        static_cast<std::uint8_t>(clamped > 0.6f ? (clamped - 0.6f) * 2.5f * 255.0f : 0.0f),
        static_cast<std::uint8_t>(clamped * 255.0f)};
    scePadSetVibration(handle, &vibration);
    rumble_left_ = seconds;
}

void Pad::tick(float dt)
{
    refresh_left_ -= dt;
    if (pad_ready_ && refresh_left_ <= 0.0f)
    {
        refresh_left_ = kRefreshInterval;
        refresh(1);
    }
    if (rumble_left_ <= 0.0f)
        return;
    rumble_left_ -= dt;
    if (rumble_left_ <= 0.0f)
        for (const Slot &slot : slots_)
            stop_rumble(slot.handle);
}

void Pad::close()
{
    for (Slot &slot : slots_)
    {
        if (slot.handle >= 0)
        {
            stop_rumble(slot.handle);
            scePadClose(slot.handle);
        }
        slot = Slot{};
    }
    pick_ = PadPick{};
    pad_ready_ = false;
    if (owns_user_service_)
        sceUserServiceTerminate();
    owns_user_service_ = false;
}

std::size_t Pad::read(std::span<PadSample> out)
{
    if (out.empty())
        return 0;
    const bool overlaid = system_ui_overlaid();
    if (overlaid != overlaid_)
    {
        overlaid_ = overlaid;
        sys::log("[HUI] pad system menu %s", overlaid ? "over the app" : "gone");
        // What the controllers do around the menu's going was meant for the
        // menu: the Cross that closed it may still be down, or a press may sit
        // in the buffers. Each controller counts again once it is neutral.
        if (!overlaid)
            for (bool &wait : wait_neutral_)
                wait = true;
    }
    // 30 KiB of samples: kept off the stack (an app has one Pad).
    static RawPadSample raw[PadPick::kSlots][64];
    int counts[PadPick::kSlots] = {};
    int starts[PadPick::kSlots] = {};
    bool used[PadPick::kSlots] = {};
    bool open[PadPick::kSlots] = {};
    const int limit = static_cast<int>(out.size() < 64 ? out.size() : 64);
    for (int slot = 0; slot < PadPick::kSlots; ++slot)
    {
        if (slots_[slot].handle < 0)
            continue;
        open[slot] = true;
        const int count = scePadRead(slots_[slot].handle, raw[slot], limit);
        counts[slot] = count > 0 ? count : 0;
        // A controller still waiting to be neutral after a menu gives the
        // whole batch to the menu; the newest sample decides whether the next
        // one is the app's. An older neutral sample doesn't count, or the
        // press that closed the menu would follow it.
        int first = 0;
        if (wait_neutral_[slot] && counts[slot] > 0)
        {
            first = counts[slot];
            if (!pad_sample_used(convert(raw[slot][counts[slot] - 1])))
                wait_neutral_[slot] = false;
        }
        starts[slot] = first;
        for (int index = first; index < counts[slot] && !used[slot]; ++index)
            used[slot] = pad_sample_used(convert(raw[slot][index]));
    }
    if (overlaid)
    {
        // Everything is released while the menu has the controllers, and a
        // press meant for the menu does not pick a controller.
        PadSample held;
        held.connected = true;
        held.buttons = pad_bits::kIntercepted;
        out[0] = held;
        return 1;
    }
    const int before = pick_.active();
    const int active = pick_.update(used, open);
    if (active < 0)
    {
        out[0] = PadSample{};
        return 1;
    }
    std::size_t used_out = 0;
    if ((active != before && before >= 0) || restart_input_)
    {
        restart_input_ = false;
        sys::log("[HUI] pad active user=0x%08x", static_cast<unsigned>(slots_[active].user));
        if (before >= 0)
            stop_rumble(slots_[before].handle);
        rumble_left_ = 0.0f;
        // One idle sample first: what the old controller held comes up, so a
        // button the new one presses is a press even when it is the same one,
        // and a held direction does not carry its repeat across.
        PadSample idle;
        idle.connected = true;
        out[used_out++] = idle;
    }
    for (int index = starts[active]; index < counts[active] && used_out < out.size(); ++index)
        out[used_out++] = convert(raw[active][index]);
    return used_out;
}

} // namespace hui::ps5
