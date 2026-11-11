// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdint>
#include <string>
namespace silent_scope { namespace dc_settings {
enum class Setting {GunAngle,Hand,Laser,Reticle,ReticleMinimum,ReticleMaximum,ReticleDistance,Haptics,WorldSize,PanelDistance,HudSize,WiderView,FpsCounter,AimSmoothing,ScopeStability,ScopeZoom,ScopeImageDepth,ScopeImageSize,RifleMode,GunSize};
struct RuntimeCapabilities {
    bool wider_available{},wider_applied{};
    // Explicitly supplied from the applied world transform. Zero/invalid means
    // unknown: retain saved metres but do not guess a native HUD-plane depth.
    double game_units_per_metre{};
};
bool is_silent_scope() noexcept;
// Call after title options load; this only applies to the verified US product.
// Valid explicit choices survive; missing/malformed/out-of-range use SS defaults.
void sanitize_loaded();
// Emits preference writes without changing autosave. Caller batches this with
// native menu defaults and restores autosave on both success and exception.
void reset_defaults();
void set_runtime_capabilities(RuntimeCapabilities);
RuntimeCapabilities runtime_capabilities() noexcept;
int primary_hand() noexcept; //0 Left,1 Right. Root owns actual XR hand routing.
bool fps_counter() noexcept; // preference only; root supplies real measured rates.
int aim_smoothing_level() noexcept; //0 Off,1 Low(default),2 Medium,3 High.
int scope_stability_level() noexcept;
float scope_magnification() noexcept;
void reset_scope_zoom() noexcept;
bool simulation_mode() noexcept;
float rifle_user_scale() noexcept;
float rifle_model_scale() noexcept;
float rifle_user_scale(bool simulation) noexcept;
float rifle_model_scale(bool simulation) noexcept;
float scope_image_offset() noexcept;
float scope_image_scale() noexcept;
float panel_distance_metres() noexcept;
int laser_choice() noexcept;
bool select_laser_choice(int choice);
float laser_half_width() noexcept;
int wider_view_mode() noexcept;
bool toggle_aiming_aids();
bool set_primary_hand(int);
bool set_fps_counter(bool);
uint64_t settings_revision() noexcept;
bool enabled(Setting) noexcept;
bool change(Setting,int direction);
std::string value(Setting);
}}
