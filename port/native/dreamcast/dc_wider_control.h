// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "dc_lens.h"
#include "dc_camera_focal_probe.h"
namespace silent_scope { namespace dreamcast {
// Root flips this single audited capability only after strict source→TA→world
// receipts and normal/wide/restore/scope controls pass. Desired settings alone
// cannot activate native writes, source ownership or immersive rendering.
inline constexpr bool ProductionCameraCertified=true;
inline bool production_camera_enabled(const LensRequest& request,bool certified=ProductionCameraCertified){return certified&&request.enabled;}
struct MainWiderDecision{FocalSetterDecision store;float factor{1};uint64_t revision{};bool main_call{};};
inline MainWiderDecision decide_main_wider(const LensRequest& request,uint32_t loaded,uint32_t caller,uint32_t mode,
        uint32_t fpscr,bool guard,bool fpu_disabled,bool certified=ProductionCameraCertified){
    MainWiderDecision out;out.store.effective_bits=loaded;out.revision=request.revision;
    out.main_call=caller==FocalSetterProbe::CallerReturn&&mode==0;
    out.store.eligible=production_camera_enabled(request,certified)&&guard&&!fpu_disabled&&out.main_call
        &&loaded==0x440ac000u&&(fpscr&((1u<<19)|(1u<<20)))==0;
    out.store.applied=out.store.eligible&&request.wider&&request.wide_proven;
    if(out.store.applied){out.store.effective_bits=0x43390000u;out.factor=3;}
    return out;
}
struct NativeWiderApplicationTracker{
    bool initialized{},known{};float factor{1};uint64_t request_revision{};
    void reset(){initialized=false;known=false;factor=1;request_revision=0;}
    bool changed(bool next_known,float next_factor,uint64_t next_revision){
        const bool different=!initialized||known!=next_known||factor!=next_factor||request_revision!=next_revision;
        initialized=true;known=next_known;factor=next_factor;request_revision=next_revision;return different;
    }
};
// Called on the SH4 thread only after an actual recognized SDK store (and on
// recognized-main fallback to unknown). Root binds guarded native input clamp
// acknowledgement here; this callback never acknowledges a rendered frame.
using NativeWiderObserver=void(*)(bool known,float factor,uint64_t request_revision);
void set_native_wider_observer(NativeWiderObserver);
}}
