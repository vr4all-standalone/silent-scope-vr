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
    source.kind=MenuKind::Options;source.source_context=0;source.page_count=6;source.page=0;
    source.row_count=4;source.row=4;source.value=5;source.interactive=true;
    assert(options_panel(source,true,30.00));
    for(unsigned i=0;i<10;++i){auto bad=source;
        if(i==0)bad.verified=false;if(i==1)bad.context_active=false;if(i==2)bad.interactive=false;
        if(i==3)bad.context_revision=0;if(i==4)bad.source_context=2;if(i==5)bad.row_count=9;
        if(i==6)bad.row=5;if(i==7)bad.page=6;if(i==8)bad.native_seconds=31;if(i==9)bad.kind=MenuKind::Dialog;
        assert(!options_panel(bad,true,30.00));
    }
    assert(!options_panel(source,false,30.00));
    source.page=5;source.row_count=source.row=0;assert(options_panel(source,true,30.00));
    source.kind=MenuKind::ModeSelect;source.source_context=StoryRouteContext;
    assert(selector_panel(source,true,30.00));
    source.mode_type=6;
    assert(!selector_panel(source,true,30.00));
    source.source_context=OutdoorStageContext;
    assert(selector_panel(source,true,30.00));
    source.interactive=false;
    assert(!selector_panel(source,true,30.00));
}
