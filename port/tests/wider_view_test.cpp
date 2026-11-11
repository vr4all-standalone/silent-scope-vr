#include <iostream>
#if !__has_include("scene/wider_view.h")
int main(){std::cerr<<"Missing guarded native WiderView policy\n";return 1;}
#else
#include "scene/wider_view.h"
#include "scene/sharc_sram.h"
#include <map>
#include <bit>
#include <cstdlib>
#include <fstream>
#include <vector>
using namespace silent_scope::scene;
void require(bool ok,const char* text){if(!ok){std::cerr<<text<<'\n';std::exit(1);}}
struct Fixture{
 std::map<uint32_t,uint64_t> code;
 std::map<uint32_t,uint32_t> data;
 WiderView view;
 Fixture(){for(auto [a,v]:WiderView::ProgramSignatures)code[a]=v;
  for(auto [a,v]:std::initializer_list<std::pair<uint32_t,float>>{{0x28232,4096},{0x28233,3072},{0x28234,-7088},{0x28235,443},{0x28236,-7088},{0x28237,443},{0x28238,-32},{0x28239,-600000},{0x2823a,256.f/443},{0x2823b,1+(256.f/443)*(256.f/443)},{0x2823c,192.f/443},{0x2823d,1+(192.f/443)*(192.f/443)},{0x2823e,-16}})data[a]=std::bit_cast<uint32_t>(v);
 }
 auto state(bool gameplay=true){return view.frame_state([&](auto a){return code[a];},[&](auto a){return data[a];},512,384,gameplay);}
 auto sync(bool gameplay=true){return view.synchronize_at_setup([&](auto a){return code[a];},[&](auto a){return data[a];},[&](auto a,auto v){data[a]=v;},512,384,gameplay,0x20153,0x2823d);}
 void incoming(uint32_t a,float v,uint32_t mask=0xffffffff){data[a]=view.intercept_write(a,std::bit_cast<uint32_t>(v),mask,true);}
};
int main(int argc,char** argv){
 std::vector<uint32_t> bank0(32768),bank1(32768);SharcSram sram{bank0.data(),bank1.data()};
 for(auto [address,opcode]:WiderView::ProgramSignatures){(void)opcode;
  for(auto alias:WiderView::program_data_aliases(address)){
   require(alias>=0x20000&&alias<0x28000,"Guard DM alias must remain in main code bank");
   bank0[alias-0x20000]=UINT32_MAX;uint64_t changed{};
   require(sram.program_read(address,changed)&&changed!=0,"Each guarded DM alias really changes its PM instruction");
   bank0[alias-0x20000]=0;
  }
 }
 // Exercise actual captured DSP words as well as synthetic transition cases.
 for(int arg=1;arg+1<argc;arg+=2){
  Fixture real;std::ifstream program(argv[arg],std::ios::binary),data(argv[arg+1],std::ios::binary);
  require(bool(program)&&bool(data),"Real DSP fixture files must exist");
  uint64_t instruction{};uint32_t word{},address=0x20000;
  while(program.read(reinterpret_cast<char*>(&instruction),sizeof(instruction)))real.code[address++]=instruction;
  require(address==0x30000,"Program fixture covers20000..2ffff");address=0x20000;
  while(data.read(reinterpret_cast<char*>(&word),sizeof(word)))real.data[address++]=word;
  require(address==0x30000,"Data fixture covers20000..2ffff");
  const auto untouched=real.data;
  for(double factor:{1.1,1.5,2.,3.,4.}){
   real.view.request(true,factor);auto actual=real.sync();require(actual.applied&&std::abs(actual.lens.fx-443/factor)<1e-9,"Captured DSP tuple supports bounded factor transform");
   real.view.request(false,factor);real.sync();require(real.data==untouched,"Captured data restores byte-exactly, all unrelated words preserved");
  }
 }
 Fixture f;auto original=f.data;
 require(!f.state(false).lens.valid(),"Original camera in Service/Unknown remains flat before any widening ownership");
 require(f.state().requested&&!f.state().applied,"New request defaultsOn but application is separate");
 f.view.request(true,2);auto s=f.sync();
 require(s.applied&&s.factor==2&&s.lens.fx==221.5&&s.lens.fy==221.5,"Applied lens must match actual half-focal tuple");
 require(f.data[0x28234]==original[0x28234]&&f.data[0x28236]==original[0x28236]&&f.data[0x2823e]==original[0x2823e]&&f.data[0x28238]==original[0x28238]&&f.data[0x28239]==original[0x28239],"Raw lens and near/far/depth scale stay native");
 require(std::abs(std::bit_cast<float>(f.data[0x2823a])-512.f/443)<1e-6,"Native horizontal culling widens");
 auto once=f.data;require(f.sync().applied&&f.data==once,"Repeated sync must not compound");
 for(int i=0;i<5;i++){f.incoming(0x28235,443);f.incoming(0x28237,443);require(f.state().applied&&f.state().lens.fx==221.5,"Incoming native rewrites never compound");}
 // Native depth scale changes regenerate projected focal from untouched raw lens.
 f.data[0x2823e]=std::bit_cast<uint32_t>(-8.f);f.incoming(0x28235,886);f.incoming(0x28237,886);
 require(f.state().applied&&f.state().lens.fx==221.5,"Scale command changes reciprocal depth but keeps physical lens");
 f.view.request(false,2);require(!f.sync().applied,"Original view disables applied state");
 require(f.data[0x28235]==std::bit_cast<uint32_t>(886.f)&&f.data[0x2823a]==original[0x2823a],"Restore latest genuine native tuple exactly");
 f.view.request(true,1.5);require(f.sync().applied,"Factor1.5 applies after Original");
 require(!f.state(false).applied&&!f.state(false).lens.valid(),"Service cannot publish applied or mismatched 3D lens");
 f.sync(false);require(f.data[0x28235]==std::bit_cast<uint32_t>(886.f),"Service transition restores native projection");
 require(f.sync().applied,"Gameplay can reapply");f.code[0x2013b]=0;
 require(!f.state().applied&&!f.state().lens.valid(),"Unknown microcode cannot claim a camera");f.sync();
 require(f.data[0x28235]!=std::bit_cast<uint32_t>(886.f),"Unknown program cannot prove a safe SDK boundary; it performs no restore writes");
 Fixture transition;transition.view.request(true,3);transition.sync();auto pending=transition.data;
 transition.view.request(false,3);
 for(int i=0;i<8;i++){auto observed=transition.state();require(observed.applied&&!observed.requested&&observed.factor==3&&transition.data==pending,"Pending Original request/poll preserves actual camera until safe setup");}
 auto restore_before=transition.view.restore_generation();
 auto wrong=transition.view.synchronize_at_setup([&](auto a){return transition.code[a];},[&](auto a){return transition.data[a];},[&](auto a,auto v){transition.data[a]=v;},512,384,true,0x22006,0x2823d);
 require(wrong.applied&&transition.data==pending&&transition.view.restore_generation()==restore_before,"Arbitrary projection PC cannot apply or restore even with a valid tuple");
 transition.sync();require(!transition.state().applied&&transition.view.restore_generation()==restore_before+1,"Actual setup Original restore gets exactly one receipt");
 transition.view.request(true,2);pending=transition.data;require(!transition.state().applied&&transition.data==pending,"Pending On cannot widen from a poll");
 transition.sync();pending=transition.data;require(transition.state().applied&&transition.state().factor==2,"Next safe setup applies pending On");
 transition.view.request(true,3);require(transition.state().factor==2&&transition.data==pending,"Pending factor change preserves the old native projection");
 transition.sync();require(transition.state().factor==3,"Safe setup applies factor change atomically");
 pending=transition.data;require(!transition.state(false).lens.valid()&&transition.data==pending,"Service forces flat observation without mutating in-progress native geometry");
 transition.sync(false);require(!transition.state().applied,"Service SDK setup safely restores Original");
 Fixture same_original;auto cached_original=same_original.state();const auto original_bits=same_original.data[0x28235];
 auto unchanged=same_original.view.intercept_write_cached(0x28235,original_bits,UINT32_MAX,true,original_bits,cached_original);
 require(unchanged==original_bits&&cached_original.lens.valid()&&!cached_original.applied,"Identical Original SDK rewrite retains proven unchanged camera metadata");
 Fixture same_wide;same_wide.view.request(true,3);same_wide.sync();auto cached_wide=same_wide.state();const auto wide_bits=same_wide.data[0x28235];
 auto transformed=same_wide.view.intercept_write_cached(0x28235,std::bit_cast<uint32_t>(443.f),UINT32_MAX,true,wide_bits,cached_wide);
 require(transformed==wide_bits&&cached_wide.lens.valid()&&cached_wide.applied&&cached_wide.factor==3,"Native443 rewrite intercepted back to the same widened bits retains its actual camera");
 transformed=same_wide.view.intercept_write_cached(0x28235,std::bit_cast<uint32_t>(444.f),UINT32_MAX,true,wide_bits,cached_wide);
 require(transformed!=wide_bits&&!cached_wide.lens.valid(),"Actual transformed camera change invalidates metadata until complete setup");
 auto unknown=cached_original;same_original.view.intercept_write_cached(0x28235,original_bits,UINT32_MAX,false,original_bits,unknown);
 require(!unknown.lens.valid(),"Unknown code or eligibility invalidates cache even when native data bits match");
 auto masked=cached_original;same_original.view.intercept_write_cached(0x28235,original_bits^0xffff0000,0xffff,true,original_bits,masked);
 require(masked.lens.valid(),"Ignored unmasked incoming bits do not pretend that hardware camera changed");
 same_original.view.intercept_write_cached(0x28235,original_bits^1,0xffff,true,original_bits,masked);
 require(!masked.lens.valid(),"A genuine masked native bit change invalidates cache");
 auto unrelated=cached_original;same_original.view.intercept_write_cached(0x2823f,0,UINT32_MAX,false,123,unrelated);
 require(unrelated.lens.valid(),"Unrelated non-camera fields cannot change camera ownership metadata");
 Fixture external;external.view.request(true,2);external.sync();external.data[0x28235]=std::bit_cast<uint32_t>(777.f);
 require(!external.state().applied&&!external.state().lens.valid(),"Partial external changes suppress geometric presentation");
 external.view.request(false,2);external.sync();require(external.data[0x28235]==std::bit_cast<uint32_t>(777.f),"Restore preserves unrelated replacement");
 Fixture changed_cull;changed_cull.code[0x20e2c]=0;auto before_cull=changed_cull.data;require(!changed_cull.sync().applied&&changed_cull.data==before_cull,"Changed culling consumer rejects native patch");
 Fixture rewrite;rewrite.view.request(true,2);rewrite.sync();auto genuine=rewrite.data[0x28235];
 rewrite.data[0x28235]=rewrite.view.intercept_write(0x28235,genuine,UINT32_MAX,false);
 rewrite.view.request(false,2);rewrite.sync();require(rewrite.data[0x28235]==genuine,"A genuine unrecognized native write supersedes ownership even when bits equal our last output");
 Fixture invalid;invalid.data[0x2823a]=std::bit_cast<uint32_t>(NAN);auto bad=invalid.data;
 invalid.view.request(true,2);require(!invalid.sync().applied&&invalid.data==bad,"Nonfinite tuple is never patched");
 Fixture inconsistent;inconsistent.data[0x2823b]=std::bit_cast<uint32_t>(7.f);bad=inconsistent.data;
 require(!inconsistent.sync().applied&&inconsistent.data==bad,"Incoherent culling tuple is never patched");
 Fixture unsupported;
 for(auto [a,v]:std::initializer_list<std::pair<uint32_t,float>>{{0x28234,-8192},{0x28236,-8192},{0x28235,512},{0x28237,512},{0x2823a,.5},{0x2823b,1.25},{0x2823c,.375},{0x2823d,1.140625}})unsupported.data[a]=std::bit_cast<uint32_t>(v);
 bad=unsupported.data;require(!unsupported.sync().applied&&unsupported.data==bad,"Same microcode with an unproven coherent lens stays native");
 Fixture disabled;disabled.view.request(true,5);bad=disabled.data;require(!disabled.sync().applied&&disabled.data==bad,"Out-of-probe-range factor is rejected");
 Fixture partial;partial.view.request(true,2);partial.sync();partial.incoming(0x28235,443,0xffff);
 require(!partial.state().applied,"Partial native writes invalidate application");
 // Same camera-space target produces stock native cabinet aim regardless of draw FOV.
 auto target=s.lens.unproject(300,220,1000);NativeLens stock{512,384,443,443,256,192};
 auto aim=stock.project_point(target);require(aim.valid&&std::abs(aim.u-(256+88)/511.)<1e-10&&std::abs(aim.v-(192+56)/383.)<1e-10,"Main native aim uses stock lens while wider draw uses applied lens");
 require(target.y<0,"Accepted rasterY inversion is preserved");
 std::cout<<"Wider native tuple, guards, restore, scale transitions and stock aim invariants passed\n";
}
#endif

