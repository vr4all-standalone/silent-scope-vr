"""Apply only Silent Scope's adapter to its dedicated clean pinned checkout."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess


NATIVE_CAMERA_METHODS=r'''
void hornet_state::silent_scope_camera_request()
{
    auto& runtime=silent_scope::shared_runtime();
    const char* diagnostic=std::getenv("SILENT_SCOPE_NATIVE_WIDER");
    const char* scale=std::getenv("SILENT_SCOPE_WIDER_FACTOR");
    const double factor=scale?std::strtod(scale,nullptr):3.0;
    auto boundary=[](const char* name,double fallback){
        const char* text=std::getenv(name);if(!text)return fallback;
        char* end=nullptr;const double value=std::strtod(text,&end);
        return end!=text&&!*end&&std::isfinite(value)&&value>=0?value:-1.0;
    };
    const double begin=boundary("SILENT_SCOPE_WIDER_BEGIN_NATIVE",0),end=boundary("SILENT_SCOPE_WIDER_END_NATIVE",1e100);
    const double time=machine().time().as_double();
    const bool window=begin>=0&&end>begin&&time>=begin&&time<end;
    m_vr_wider.request(runtime.wider_requested.load()&&(!diagnostic||std::strcmp(diagnostic,"0")!=0)&&window,factor);
}

bool hornet_state::silent_scope_camera_gameplay() const
{
    const auto& runtime=silent_scope::shared_runtime();
    return runtime.native_service_state.load()==int(silent_scope::OperatorState::Normal)
        &&!runtime.service_active.load();
}

silent_scope::scene::WiderViewState hornet_state::silent_scope_camera_state()
{
    // Called for every triangle: no DSP dispatch/read work in this callback.
    return m_vr_camera_state;
}

void hornet_state::silent_scope_camera_diagnostic(const char* event)
{
    if(!m_vr_camera_diagnostic_path)return;
    const double time=machine().time().as_double();
    auto boundary=[](const char* name,double fallback){
        const char* text=std::getenv(name);if(!text)return fallback;
        char* end=nullptr;const double value=std::strtod(text,&end);
        return end!=text&&!*end&&std::isfinite(value)&&value>=0?value:-1.0;
    };
    const double begin=boundary("SILENT_SCOPE_CAMERA_DIAGNOSTIC_BEGIN_NATIVE",90);
    const double end=boundary("SILENT_SCOPE_CAMERA_DIAGNOSTIC_END_NATIVE",111);
    if(begin<0||end<=begin||time<begin||time>=end||m_vr_camera_diag_count>=256)return;
    const unsigned category=std::strcmp(event,"poll")==0?0:std::strcmp(event,"staged")==0?1:
        std::strcmp(event,"staged_restore")==0?2:std::strcmp(event,"staged_apply")==0?3:
        std::strcmp(event,"command_accept_invalid")==0?4:std::strcmp(event,"capture_invalid")==0?5:
        std::strcmp(event,"capture_mixed")==0?6:7;
    const bool priority=category==2||category==3;
    const s64 second=s64(std::floor(time));
    // Routine setup spam cannot exhaust issue visibility or the reserved64
    // priority apply/restore slots. Each issue category is limited to1/second.
    if(!priority){
        if(m_vr_camera_diag_count>=192||m_vr_camera_diag_last[category]==second)return;
        m_vr_camera_diag_last[category]=second;
    }
    ++m_vr_camera_diag_count;
    std::ofstream out(m_vr_camera_diagnostic_path,std::ios::app);
    out<<event<<" time="<<time<<" pc="<<std::hex<<m_dsp[0]->state_int(STATE_GENPC)<<std::dec
       <<" requested="<<m_vr_camera_state.requested<<" applied="<<m_vr_camera_state.applied
       <<" lens="<<m_vr_camera_state.lens.valid()<<" factor="<<m_vr_camera_state.factor
       <<" fx="<<m_vr_camera_state.lens.fx<<" fy="<<m_vr_camera_state.lens.fy
       <<" operator="<<silent_scope::shared_runtime().native_service_state.load()
       <<" service="<<silent_scope::shared_runtime().service_active.load()
       <<" pause="<<silent_scope::shared_runtime().pause_requested.load()<<'\n';
    for(u32 a=0x28232;a<=0x2823e;++a){const u32 bits=m_dsp[0]->space(AS_DATA).read_dword(a);
        out<<std::hex<<a<<'='<<bits<<std::dec<<'/'<<std::bit_cast<float>(bits)<<' ';}out<<'\n';
    for(auto [a,expected]:silent_scope::scene::WiderView::ProgramSignatures){
        const u64 actual=m_dsp[0]->space(AS_PROGRAM).read_qword(a);
        if(actual!=expected)out<<"PROGRAM_MISMATCH "<<std::hex<<a<<'='<<actual<<" expected="<<expected<<std::dec<<'\n';
    }
}

void hornet_state::silent_scope_camera_sync()
{
    auto& runtime=silent_scope::shared_runtime();
    const auto operator_state=silent_scope::read_operator_state(
        [this](u32 a){return m_maincpu->space(AS_PROGRAM).read_dword(a);},
        [this](u32 a){return m_maincpu->space(AS_PROGRAM).read_byte(a);});
    runtime.native_service_state=int(operator_state);
    if(operator_state!=silent_scope::OperatorState::Unknown)runtime.service_active=operator_state==silent_scope::OperatorState::Service;
    silent_scope_camera_request();
    auto program=[this](u32 a){return m_dsp[0]->space(AS_PROGRAM).read_qword(a);};
    auto data=[this](u32 a){return m_dsp[0]->space(AS_DATA).read_dword(a);};
    // Polls can run between native projected vertices. They only observe the
    // actually applied tuple; pending On/Off/factor waits for the next SDK store.
    m_vr_camera_state=m_vr_wider.frame_state(program,data,512,384,silent_scope_camera_gameplay());
    silent_scope_camera_diagnostic("poll");
    // Availability describes recognized capability; applied belongs to each
    // immutable captured buffer and is published with that buffer by the host.
    silent_scope::shared_runtime().wider_available=silent_scope::scene::WiderView::recognized(program,512,384)
        &&silent_scope::scene::read_silent_scope_lens(program,data,512,384).valid();
}

void hornet_state::silent_scope_camera_setup()
{
    if(!silent_scope::scene::capture_enabled()||std::string_view(machine().system().name)!="sscope")return;
    for(size_t i=0;i<silent_scope::scene::WiderView::Fields.size();++i){
        const auto address=silent_scope::scene::WiderView::Fields[i];
        m_vr_wider_taps[i]=m_dsp[0]->space(AS_DATA).install_write_tap(address,address,
            "silent-scope-main-wider",[this](offs_t address,u32& value,u32 mask){
                if(m_vr_wider.internal_write())return;
                const u32 old_word=m_dsp[0]->space(AS_DATA).read_dword(address);
                auto program=[this](u32 a){return m_dsp[0]->space(AS_PROGRAM).read_qword(a);};
                const bool guard=silent_scope::scene::WiderView::recognized(program,512,384)
                    &&m_dsp[0]->space(AS_DATA).read_dword(0x28232)==0x45800000
                    &&m_dsp[0]->space(AS_DATA).read_dword(0x28233)==0x45400000
                    &&m_dsp[0]->space(AS_DATA).read_dword(0x28234)==0xc5dd8000
                    &&m_dsp[0]->space(AS_DATA).read_dword(0x28236)==0xc5dd8000;
                value=m_vr_wider.intercept_write_cached(address,value,mask,guard&&silent_scope_camera_gameplay(),old_word,m_vr_camera_state);
                const u32 pc=m_dsp[0]->state_int(STATE_GENPC);
                // The current word is not committed yet. A staged accessor
                // exposes the complete native camera at its final setup store,
                // transforms all six outputs, then commits this store on return.
                if(mask==~u32(0)&&guard&&((address==0x2823d&&pc==0x20153)||(address==0x28237&&pc==0x20176))){
                    silent_scope_camera_request();
                    auto data=[this,address,&value](u32 offset){return offset==address?value:m_dsp[0]->space(AS_DATA).read_dword(offset);};
                    auto write=[this,address,&value](u32 a,u32 word){if(a==address)value=word;else m_dsp[0]->space(AS_DATA).write_dword(a,word);};
                    const auto restores=m_vr_wider.restore_generation();
                    m_vr_camera_state=m_vr_wider.synchronize_at_setup(program,data,write,512,384,silent_scope_camera_gameplay(),pc,address);
                    if(m_vr_camera_diagnostic_path){
                        const bool first_apply=m_vr_camera_state.applied&&(!m_vr_camera_diag_applied||m_vr_camera_diag_factor!=m_vr_camera_state.factor);
                        m_vr_camera_diag_applied=m_vr_camera_state.applied;m_vr_camera_diag_factor=m_vr_camera_state.factor;
                        if(m_vr_wider.restore_generation()!=restores)silent_scope_camera_diagnostic("staged_restore");
                        else if(first_apply)silent_scope_camera_diagnostic("staged_apply");
                        else silent_scope_camera_diagnostic("staged");
                    }
                }
            });
    }
    // The20 instruction words prove the camera layout. Direct PM/boot-DMA
    // writes go through generic program dispatch, so invalidate cached state
    // immediately on code upload rather than trusting it until next screen poll.
    for(size_t i=0;i<silent_scope::scene::WiderView::ProgramSignatures.size();++i){
        const auto address=silent_scope::scene::WiderView::ProgramSignatures[i].first;
        m_vr_wider_code_taps[i]=m_dsp[0]->space(AS_PROGRAM).install_write_tap(address,address,
            "silent-scope-camera-code",[this](offs_t,u64&,u64){m_vr_camera_state={};});
        const auto aliases=silent_scope::scene::WiderView::program_data_aliases(address);
        for(size_t word=0;word<aliases.size();++word)
            m_vr_wider_code_data_taps[i*2+word]=m_dsp[0]->space(AS_DATA).install_write_tap(aliases[word],aliases[word],
                "silent-scope-camera-code-dm",[this](offs_t,u32&,u32){m_vr_camera_state={};});
    }
    m_vr_camera_diagnostic_path=std::getenv("SILENT_SCOPE_CAMERA_DIAGNOSTIC");
    silent_scope::scene::set_native_camera_provider([this]{return silent_scope_camera_state();},[this]{silent_scope_camera_sync();},
        m_vr_camera_diagnostic_path?std::function<void(const char*)>([this](const char* reason){silent_scope_camera_diagnostic(reason);}):std::function<void(const char*)>{});
}
'''

def driver_source(original):
    source=original
    replacements=[
        ('#include "screen.h"','#include "screen.h"\n#include "silent_scope/engine_adapter.h"\n#include "silent_scope/operator_service.h"\n#include "silent_scope/scene/native_camera.h"'),
        ('void init_sscope() ATTR_COLD;','void init_sscope() ATTR_COLD;\n\tvoid silent_scope_exit();\n\tvoid silent_scope_camera_setup();\n\tvoid silent_scope_camera_request();\n\tbool silent_scope_camera_gameplay() const;\n\tsilent_scope::scene::WiderViewState silent_scope_camera_state();\n\tvoid silent_scope_camera_sync();\n\tvoid silent_scope_camera_diagnostic(const char* event);'),
        ('\tm_k037122[Which]->tile_draw(screen, bitmap, cliprect);','''
    if constexpr (Which==0) {
        if (silent_scope::scene::capture_enabled() && std::string_view(machine().system().name)=="sscope") {
            silent_scope_camera_sync();
            if(m_vr_overlay.width()!=bitmap.width()||m_vr_overlay.height()!=bitmap.height())m_vr_overlay.allocate(bitmap.width(),bitmap.height());
            m_vr_overlay.fill(0);m_k037122[Which]->tile_draw(screen,m_vr_overlay,cliprect);
            for(int y=cliprect.min_y;y<=cliprect.max_y;++y)for(int x=cliprect.min_x;x<=cliprect.max_x;++x)
                if(m_vr_overlay.pix(y,x))bitmap.pix(y,x)=m_vr_overlay.pix(y,x);
            const auto visible=screen.visible_area();
            auto lens=silent_scope::scene::read_silent_scope_lens([this](u32 a){return m_dsp[0]->space(AS_PROGRAM).read_qword(a);},[this](u32 a){return m_dsp[0]->space(AS_DATA).read_dword(a);},visible.width(),visible.height());
            silent_scope::publish_screen<Which>(machine(),screen,bitmap,lens,&m_vr_overlay);
        } else m_k037122[Which]->tile_draw(screen,bitmap,cliprect);
    } else {
        m_k037122[Which]->tile_draw(screen,bitmap,cliprect);
        silent_scope::publish_screen<Which>(machine(),screen,bitmap);
    }
    '''),
        ('\tbool m_sound_irq_enabled = false;','\tbitmap_rgb32 m_vr_overlay;\n\tsilent_scope::scene::WiderView m_vr_wider;\n\tsilent_scope::scene::WiderViewState m_vr_camera_state;\n\tconst char* m_vr_camera_diagnostic_path=nullptr;\n\tu32 m_vr_camera_diag_count=0;\n\tstd::array<s64,8> m_vr_camera_diag_last{{-1,-1,-1,-1,-1,-1,-1,-1}};\n\tbool m_vr_camera_diag_applied=false;\n\tdouble m_vr_camera_diag_factor=1;\n\tstd::array<memory_passthrough_handler,6> m_vr_wider_taps;\n\tstd::array<memory_passthrough_handler,20> m_vr_wider_code_taps;\n\tstd::array<memory_passthrough_handler,40> m_vr_wider_code_data_taps;\n\tbool m_sound_irq_enabled = false;'),
        ('\tm_konppc->set_num_boards(2);','''\tm_konppc->set_num_boards(2);
    m_dsp[0]->enable_fast_sram();m_dsp[1]->enable_fast_sram();
    if(const char* drc=std::getenv("SILENT_SCOPE_SHARC_DRC")) {
        if(std::strcmp(drc,"1")==0||std::strcmp(drc,"main")==0)m_dsp[0]->enable_recompiler();
        if(std::strcmp(drc,"1")==0||std::strcmp(drc,"scope")==0)m_dsp[1]->enable_recompiler();
    }'''),
        ('void hornet_state::machine_start()\n{','void hornet_state::silent_scope_exit() {silent_scope::scene::clear_native_camera_provider();silent_scope::stop_engine();}\n\nvoid hornet_state::machine_start()\n{\n\tsilent_scope_camera_setup();\n\tif (std::string_view(machine().system().name)=="sscope")\n\t\tmachine().add_notifier(MACHINE_NOTIFY_EXIT,machine_notify_delegate(&hornet_state::silent_scope_exit,this));'),
        ('void hornet_state::machine_reset()\n{','void hornet_state::machine_reset()\n{\n\tif (silent_scope::vr_requested() && std::string_view(machine().system().name)=="sscope") silent_scope::reset_engine();\n\tm_vr_wider=silent_scope::scene::WiderView{};\n\tm_vr_camera_state={};\n\tm_vr_camera_diag_count=0;\n\tm_vr_camera_diag_last.fill(-1);\n\tm_vr_camera_diag_applied=false;\n\tm_vr_camera_diag_factor=1;\n\tif (std::string_view(machine().system().name)=="sscope") silent_scope::scene::capture_reset_all_commands();'),
    ]
    for old,new in replacements:
        if source.count(old)!=1:
            raise SystemExit('Pinned source anchor differs: '+old)
        source=source.replace(old,new,1)
    source=source.replace("void hornet_state::machine_start()",NATIVE_CAMERA_METHODS+"\nvoid hornet_state::machine_start()",1)
    return source

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('engine',type=Path)
    args=parser.parse_args()
    root=Path(__file__).parents[1]
    engine=args.engine.resolve()
    if engine.name!='mame-source' or engine.parent.name!='VR4ALL-SilentScope-Build':
        raise SystemExit('Use the dedicated VR4ALL-SilentScope-Build/mame-source checkout')
    lock=json.loads((root/'engine-lock.json').read_text())
    revision=subprocess.check_output(['git','-C',str(engine),'rev-parse','HEAD'],text=True).strip()
    if revision!=lock['commit']:
        raise SystemExit('MAME revision differs from engine lock')
    for name in ('0002-mingw-wasapi-guids.patch','0003-android-sdl-cross-build.patch','0004-enable-android-arm64-drc.patch','0005-main-scene-capture.patch','0006-native-cpu-profile.patch','0007-cache-line-probe-once.patch','0008-sharc-live-sram-reads.patch','0009-paused-host-poll.patch','0011-native-command-camera.patch'):
        compatibility=root/'patches'/name
        checked=subprocess.run(['git','-C',str(engine),'apply','--check',str(compatibility)],capture_output=True)
        if checked.returncode==0:
            subprocess.run(['git','-C',str(engine),'apply',str(compatibility)],check=True)
        else:
            reverse=subprocess.run(['git','-C',str(engine),'apply','--reverse','--check',str(compatibility)],capture_output=True)
            if reverse.returncode:
                raise SystemExit(name+' conflicts with existing source; refusing changes')
    target=engine/'src/mame/konami/hornet.cpp'
    original=subprocess.check_output(['git','-C',str(engine),'show','HEAD:src/mame/konami/hornet.cpp'],text=True)
    source=driver_source(original)
    current=target.read_text(encoding='utf-8')
    if current not in (original,source):
        previous_patch=root/'patches/0001-silent-scope-bridge.patch'
        current_patch=subprocess.check_output(['git','-C',str(engine),'diff','--','src/mame/konami/hornet.cpp'])
        if not previous_patch.exists() or current_patch!=previous_patch.read_bytes():
            raise SystemExit('Unexpected existing driver modifications; refusing overwrite')
    if current!=source:
        target.write_text(source,encoding='utf-8',newline='\n')
    destination=engine/'src/lib/silent_scope'
    destination.mkdir(parents=True,exist_ok=True)
    for name in ('display_bridge.h','rifle_input.h','shared_runtime.h','engine_adapter.h','host_poll.h','operator_service.h'):
        shutil.copyfile(root/'native'/name,destination/name)
    scene=destination/'scene';scene.mkdir(exist_ok=True)
    for header in (root/'native/scene').glob('*.h'):
        shutil.copyfile(header,scene/header.name)
    patch=subprocess.check_output(['git','-C',str(engine),'diff','--','src/mame/konami/hornet.cpp'])
    (root/'patches').mkdir(exist_ok=True)
    (root/'patches/0001-silent-scope-bridge.patch').write_bytes(patch)
    print('Prepared exact pinned driver and isolated Silent Scope headers')


if __name__=='__main__':main()
