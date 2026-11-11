// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "dc_xr_bridge.h"
#include <cmath>
#include <cstdint>
namespace silent_scope { namespace dc_view_probe {
struct Recipe {bool owns{},drive{},wider{};unsigned phase{};dc_xr::Target target;};
inline Recipe recipe(double native,double host)noexcept{
 Recipe r;if(!std::isfinite(native)||!std::isfinite(host)||native<40||host<0)return r;
 r.owns=true;r.drive=native<80;r.wider=native<58||(native>=60&&native<77);
 r.phase=native<58?0:native<60?1:native<62?2:native<65?3:native<68?4:native<71?5:native<74?6:native<77?7:native<80?8:9;
 const double yaw=r.phase==3?10000:r.phase==4?-10000:0,pitch=r.phase==5?9000:r.phase==6?-9000:0;
 constexpr double rad=6.2831853071795864769/65536.;const double a=yaw*rad,b=pitch*rad;
 r.target.host_seconds=host;r.target.focused=r.target.tracked=r.target.direction_valid=true;r.target.hand=1;
 r.target.native_direction={-std::sin(a)*std::cos(b),-std::sin(b),-std::cos(a)*std::cos(b)};return r;
}
bool environment_requested()noexcept;
// Only explicit Windows !LIBRETRO !USE_OPENXR diagnostic builds link these.
void initialize_runtime_probe();
bool poll_runtime_probe(std::uint8_t& buttons,int& x,int& y,int& wheel);
}}
