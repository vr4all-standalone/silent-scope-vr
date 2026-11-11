#include "dreamcast/dc_menu_presentation.h"
#include <cassert>
#include <initializer_list>
using namespace silent_scope::dc_menu;
int main() {
    MenuFacts source{}; source.kind=MenuKind::Pause; source.verified=true;
    source.context_active=true; source.source_context=0;
    assert(raster_menu(source,MenuKind::Pause,-1,0,true,true,false));
    for (unsigned i=0;i<7;++i) {
        auto changed=source;
        if(i==0) changed.verified=false;
        if(i==1) changed.context_active=false;
        if(i==2) changed.kind=MenuKind::Main;
        assert(!raster_menu(changed,i==3?MenuKind::Training:MenuKind::Pause,-1,
            i==4?1:0,i!=5,i!=6,false));
    }
    assert(!raster_menu(source,MenuKind::Pause,-1,0,true,true,true));
    for(auto kind:{MenuKind::Options,MenuKind::Ranking}) {
        source.kind=kind; source.page=2;
        assert(raster_menu(source,kind,2,0,true,true,false));
        assert(!raster_menu(source,kind,1,0,true,true,false));
        assert(!raster_menu(source,kind,-1,0,true,true,false));
    }
    source.kind=MenuKind::ModeSelect; source.source_context=0x100;
    assert(raster_menu(source,MenuKind::ModeSelect,-1,0x100,true,true,false));
    assert(!raster_menu(source,MenuKind::ModeSelect,-1,0,true,true,false));
    source.mode_type=0; source.interactive=true; source.context_revision=3; source.native_seconds=30.05;
    assert(selector_panel(source,true,30.00));
    assert(!selector_panel(source,false,30.00));
    assert(!selector_panel(source,true,29.80));
    assert(!selector_panel(source,true,30.25));
    source.mode_type=1;
    assert(!selector_panel(source,true,30.00));
    source.source_context=StoryRouteContext;
    assert(selector_panel(source,true,30.00));
    source.mode_type=6;
    assert(!selector_panel(source,true,30.00));
    source.source_context=OutdoorStageContext;
    assert(selector_panel(source,true,30.00));
    source.interactive=false;
    assert(!selector_panel(source,true,30.00));
}
