// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "dc_xr_bridge.h"
#include "dc_menu_pointer.h"
#include <cstdint>
#include <memory>
namespace silent_scope { namespace dc_mouse {
constexpr std::uint8_t Left=1u<<2,Right=1u<<1,Middle=1u<<3,OwnedButtons=Left|Middle|Right;
// Source branch073b80: real mouse override /16, speed0.5/1/2, normal
// scoped factor40, then sign and native integer accumulation. Other paths are
// explicitly unsupported until attested; measured acknowledgement refines gain.
struct SourceContext {
 bool code_verified{},mouse_override{},digital_override{},alternate_a{},alternate_b{},reverse_x{},reverse_y{};
 unsigned speed_index{},mode{},scope_branch{};int input_sign{};
};
struct GainHint {bool verified{};std::uint64_t signature{};double yaw{},pitch{};};
GainHint source_hint(const SourceContext&)noexcept;
struct Feedback {
 bool code_verified{},mouse_device{},menu_navigation_verified{},main_menu_verified{};
 int menu_selected{-1},menu_count{},menu_native_wheel{};
 std::uint64_t generation{},poll_sequence{};
 double native_seconds{},yaw{},pitch{};
 double yaw_min{-5500},yaw_max{5500},pitch_min{-4300},pitch_max{4300};
 GainHint gain;
 dc_menu::MenuFacts menu;
};
enum class Status {NoTarget,InvalidContext,StaleTarget,Unreachable,UnsupportedGain,WaitingAck,AckTimeout,Ready,UiClickOnly,DuplicatePoll,ExplicitStart};
struct Packet {
 int dx{},dy{},wheel{};std::uint8_t pressed{};
 // fresh_fire acknowledges a fresh native Left packet, not alignment, a hit,
 // guest ammunition consumption or physical/visual shot confirmation.
 Status status{Status::NoTarget};bool aligned{},reachable{},fresh_fire{},ui_pointing_unverified{};
 bool learned_yaw{},learned_pitch{};double yaw_gain{},pitch_gain{};
 bool main_menu_context{};int menu_target{-1};MenuPointerStatus menu_pointer_status{MenuPointerStatus::Inactive};
 dc_menu::PolicyStatus menu_action_status{dc_menu::PolicyStatus::Inactive};
};
// submit/reset run on the XR producer thread, poll on the emulation thread.
// The mailbox latches transitions and physical edges between native polls.
// Unregister sink before destroying Consumer. No emulator/GL/RAM API is owned.
class Consumer {
 struct Impl;std::unique_ptr<Impl> impl_;
public:
 Consumer();~Consumer();Consumer(const Consumer&)=delete;Consumer& operator=(const Consumer&)=delete;
 void submit(const dc_xr::Target&);
 void reset(std::uint64_t epoch);
 dc_xr::InputSink sink()noexcept;
 Packet poll(double host_now,const Feedback&);
};
}}
