// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "dc_menu_actions.h"
#include <cstdint>
namespace silent_scope { namespace dc_xr {
struct Point {double x{},y{},z{};}; // Native reconstructed camera coordinates, y down.
struct Target {
 Point native_point{},native_direction{}; // Direction must be finite and unit length.
 double yaw_units{},pitch_units{},host_seconds{}; // Signed native angle:65536 units/turn.
 // Native UI selection is a separate domain: no gameplay aim delta or shot.
 // point_valid is only an actual endpoint. A valid direction permits native aim
 // and a legitimate miss without inventing a laser/reticle endpoint.
 bool point_valid{},direction_valid{},native_ui{},ui_select{},fire{},start{},focused{},tracked{},menu{};
 int ui_navigation{}; // Native menu wheel intent: -1 Up,0 neutral,+1 Down; UI only.
 bool ui_main_menu{};int ui_pointer_item{-1}; // Complete verified native Main Menu geometry only.
 dc_menu::MenuAction ui_action; // Positive source/layout proof for post-Main native menus.
 int hand{1};uint64_t epoch{};
};
// Registration and submission are render-thread operations. The consumer owns
// user and must unregister before destroying it. No raw mouse/Maple writes here.
struct InputSink {void* user{};void(*submit)(void*,const Target&){};void(*reset)(void*,uint64_t){};};
class InputBridge {
 InputSink sink_{};uint64_t epoch_{1};bool armed_{},navigation_armed_{},have_previous_{};
 bool focused_{},tracked_{},menu_{},native_ui_{},main_menu_{};int hand_{-1};double previous_seconds_{-1};
 dc_menu::MenuKind menu_kind_{dc_menu::MenuKind::None};std::uint64_t menu_revision_{};
public:
 void set_sink(InputSink);
 void invalidate();
 bool submit(Target,Target* emitted=nullptr); // false without a registered consumer.
};
InputBridge& input_bridge();
void set_input_sink(InputSink);
// A completed XR-loop sample and distinct retained-main identities. Native Hz
// counts newly observed main frames, never RTT callbacks or repeated eye draws.
class FrameRates {
 bool have_{};double since_{},last_{};uint64_t generation_{},sequence_{},xr_{},native_{};
 double xr_hz_{},native_hz_{};
public:
 void reset();
 void sample(double host_seconds,uint64_t generation,uint64_t main_sequence);
 double xr_hz() const{return xr_hz_;}
 double native_hz() const{return native_hz_;}
};
}}
