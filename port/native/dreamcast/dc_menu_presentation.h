// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "dc_menu_actions.h"
namespace silent_scope::dc_menu {
// These source-verified selectors have a fixed 640x480 pointer plane.
// Limit source ownership to a nearby main frame from the same native clock.
inline bool selector_panel(const MenuFacts& source,bool native_canvas,double frame_seconds){
    if(!native_canvas||!source.verified||!source.interactive||!source.context_active||
       !source.context_revision||source.kind!=MenuKind::ModeSelect||
       !std::isfinite(frame_seconds)||frame_seconds<0||
       !std::isfinite(source.native_seconds)||source.native_seconds<0||
       std::abs(source.native_seconds-frame_seconds)>.15)return false;
    return (source.mode_type==0&&source.source_context==ArcadeModeContext)||
           (source.mode_type==1&&source.source_context==StoryRouteContext)||
           (source.mode_type==4&&source.source_context==InvasionRouteContext)||
           (source.mode_type==6&&source.source_context==OutdoorStageContext);
}
// Other menus require both a current native art proof and the source snapshot.
inline bool raster_menu(const MenuFacts& source, MenuKind rendered_kind,
                        int rendered_page, unsigned rendered_context,
                        bool current_geometry, bool xr_eye, bool rtt) {
    if (!xr_eye || rtt || !current_geometry || !source.verified ||
        !source.context_active || source.kind == MenuKind::None ||
        source.kind == MenuKind::Main || source.kind != rendered_kind ||
        source.source_context != rendered_context) return false;
    return (source.kind != MenuKind::Options && source.kind != MenuKind::Ranking) ||
        (rendered_page >= 0 && rendered_page == source.page);
}
}
