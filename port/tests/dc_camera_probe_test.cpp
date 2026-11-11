#include <iostream>
#if !__has_include("dreamcast/dc_camera_probe.h")
int main(){std::cerr<<"Missing guarded console camera submission probe\n";return 1;}
#else
#include "dreamcast/dc_camera_probe.h"
#include <map>
#include <filesystem>
#include <cstdlib>
using namespace silent_scope::dreamcast;
void require(bool ok,const char* why){if(!ok){std::cerr<<why<<'\n';std::exit(1);}}
int main(){
 auto early=ui_camera_probe_limits("1");require(early.begin_native==3&&early.end_native==20&&early.max_records==40,"Explicit UI probe has bounded early window");
 auto extended=ui_camera_probe_limits(nullptr,"80");require(extended.end_native==80&&extended.max_records==128,"Explicit lifecycle probe remains bounded");
 require(ui_camera_probe_limits(nullptr,"81").end_native==70&&ui_camera_probe_limits(nullptr,"nan").end_native==70,"Unknown end override retains default");
 auto defaults=ui_camera_probe_limits(nullptr);require(defaults.begin_native==30&&defaults.end_native==70&&defaults.max_records==128,"Default camera probe policy unchanged");
 require(ui_camera_probe_limits("true").begin_native==30,"Only exact UI probe1 enables early capture");
 std::map<uint32_t,uint32_t> ram;for(auto pair:CameraSampler::CodeGuard)ram[pair.first]=pair.second;
 for(unsigned i=0;i<44;i++)ram[CameraSampler::SdkBase+i*4]=0x1000+i;
 for(unsigned i=0;i<12;i++)ram[CameraSampler::ObjectBase+i*4]=0x2000+i;
 ram[0x8cd206d4]=0x43390000;ram[0x8cd206d8]=0x43390000;ram[0x8cd206dc]=0x43a00000;ram[0x8cd206e0]=0x43700000;
 CameraSampler sampler;unsigned reads=0;auto read=[&](uint32_t a){++reads;require(a>=0x8c000000&&a<0x8d000000,"Only native Dreamcast RAM is read");return ram[a];};
 auto wrong=sampler.capture("T9505D",1,false,0,true,read);require(!wrong.supported&&!wrong.submit_sequence&&reads==0,"Other product has no probe or RAM access");
 auto off=sampler.capture("T9507N",2,false,0,false,read);require(off.supported&&off.submit_sequence==1&&!off.candidates_captured&&!off.cache_captured&&reads==0,"Probe-off submission ownership has no RAM reads");
 auto main=sampler.capture("T9507N",3,false,0x300000,true,read);
 require(main.code_guard_valid&&main.candidates_captured&&main.sdk_words[0]==0x1000&&main.object_words[11]==0x200b,"Proven code gates bounded candidate blocks");
 require(main.cache_captured&&main.cache_fx==185&&main.cache_fy==185&&main.cache_cx==320&&main.cache_cy==240,"Four actual cached projection fields are distinct and typed");
 require(reads==CameraSampler::CodeGuard.size()+44+12+4,"Read budget includes only four extra cache words");
 auto saved=main;ram[CameraSampler::SdkBase]=0xdead;
 ram[0x8cd206d4]=0x43946231;
 auto rtt=sampler.capture("T9507N",4,true,0x52c000,true,read);
 require(rtt.is_rtt&&rtt.submit_sequence==main.submit_sequence+1&&main.sdk_words[0]==saved.sdk_words[0],"Independent main/RTT stamps retain immutable submission data");
 require(rtt.cache_fx!=main.cache_fx&&main.cache_fx==185,"Main cached lens survives later native RTT changes");
 sampler.reset_generation();auto fresh=sampler.capture("T9507N",5,false,0,false,read);
 require(fresh.generation>main.generation&&fresh.submit_sequence>rtt.submit_sequence,"Load/reset generation changes without recycling sequence identities");
 ram[CameraSampler::CodeGuard[0].first]^=1;reads=0;
 auto invalid=sampler.capture("T9507N",6,false,0,true,read);require(!invalid.code_guard_valid&&!invalid.candidates_captured&&reads==1,"Unknown native code never reads proposed SDK blocks");
 ram[CameraSampler::CodeGuard[0].first]^=1;ram[CameraSampler::CodeGuard[16].first]^=1;reads=0;
 auto wrongCacheCode=sampler.capture("T9507N",7,false,0,true,read);
 require(!wrongCacheCode.cache_captured&&!wrongCacheCode.candidates_captured&&reads==17,"Unknown native cache-refresh code rejects before all SDK/cache data reads");
 auto dir=std::filesystem::temp_directory_path()/"silent-scope-dc-camera-policy-test";std::filesystem::create_directories(dir);
 CameraProbe probe(dir,CameraProbeLimits{4,30,70,1});
 require(!probe.record(main,29)&&probe.record(main,30)&&!probe.record(main,30.5),"Main diagnostic sampling has a bounded native-time cadence");
 require(probe.record(rtt,30.5)&&probe.record(main,31)&&probe.record(rtt,31.5)&&!probe.record(main,32),"Main/RTT metadata budgets cannot grow unbounded");
 require(probe.records()==4&&std::filesystem::file_size(probe.path())<16000,"No raw full-RAM dump or unbounded file growth");
 std::ifstream input(probe.path());std::string first;std::getline(input,first);
 require(first.find("\"cache_fx\":185")!=std::string::npos&&first.find("\"cache_cx\":320")!=std::string::npos,"Named cache metadata fields retain actual sampled numbers");
 std::cout<<"Console title/code guards, bounded read-only SDK samples, submitted main/RTT ownership and diagnostic budgets passed\n";
}
#endif
