// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "dc_mouse_input.h"
#include <cstdint>

namespace silent_scope { namespace dc_mouse {
// Startup thread only, before emulation/EventManager broadcasts begin.
// Registers events and clears pending input. Call before native_mouse_sink.
void initialize_native_mouse();
// Render thread: returning this sink explicitly enables the adapter; default
// is inactive. Before initialization this returns an empty, disabled sink.
// This function never registers EventManager callbacks.
// Idempotent while enabled: repeated per-frame queries do not reset input.
// Target.host_seconds uses steady_clock::now().time_since_epoch() seconds.
dc_xr::InputSink native_mouse_sink();
void disable_native_mouse();

// Emulation thread only, at port A / main device / player 0 GetMouseInput.
// Caller must also verify the actual device is MDT_Mouse (not just config).
// true: owned Left/Right/Middle and relative axes were replaced, including
// neutral invalid/stale states. false: this adapter did not touch the packet.
// Explicit fresh Start requests map to native Middle (Start / pause), even
// when aim gain is unsupported. Native UI pointing remains unverified.
bool poll_native_mouse(std::uint8_t& buttons,int& x,int& y,int& wheel);

// Emulation thread only, AFTER a separately validated limits patch is applied.
// Checks live guarded code/literals before acknowledging caps. Default stock;
// a load/start/disk/terminate event returns this acknowledgement to stock.
bool acknowledge_native_mouse_limits(int yaw_limit,int pitch_limit);
// Render/host-safe copy of emulation-thread menu readback. No render RAM reads.
// context_revision changes for lifecycle/source-family changes, never each frame.
// Bind geometry actions to this revision AND their actual originating page.
dc_menu::MenuFacts native_menu_snapshot();
}}
