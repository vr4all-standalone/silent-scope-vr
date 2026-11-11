#include <cassert>
#include <map>
#include <limits>
#if __has_include("dreamcast/dc_view_runtime.h")
#include "dreamcast/dc_view_runtime.h"
#define HAS_VIEW_RUNTIME 1
#endif
int main(){
#ifndef HAS_VIEW_RUNTIME
 assert(false&&"View runtime policy missing");
#else
 using namespace silent_scope::dc_view;
 using silent_scope::dreamcast::LensStatus;
 RequestPolicy requests;
 assert(requests.sync(true,true,false,false));auto r=requests.request();
 assert(r.enabled&&r.wider&&!r.wide_proven&&r.revision==1);
 assert(!requests.sync(true,true,false,false)&&requests.request().revision==1);
 assert(requests.sync(true,true,false,true)&&requests.request().wide_proven);
 const auto revision=requests.request().revision;
 auto caps=capabilities(requests.request(),false,true,LensStatus::Wider,.1,true);
 assert(caps.wider_available&&caps.wider_applied&&caps.game_units_per_metre==10);
 assert(!capabilities(requests.request(),false,false,LensStatus::Wider,.1,true).wider_applied);
 assert(!capabilities(requests.request(),false,true,LensStatus::Pending,.1,true).wider_applied);
 assert(!capabilities(requests.request(),false,true,LensStatus::Wider,.1,false).wider_available);
 assert(capabilities(requests.request(),false,true,LensStatus::Wider,NAN,true).game_units_per_metre==0);
 assert(requests.sync(true,true,true,true));r=requests.request();assert(!r.wide_proven&&r.revision==revision+1);
 assert(!capabilities(r,true,true,LensStatus::Wider,.1,true).wider_applied);
 assert(requests.sync(false,true,true,true)&&!requests.request().enabled);
 std::map<unsigned short,unsigned short> unused; (void)unused;
 std::map<std::uint32_t,std::uint16_t> ram;
 const std::uint16_t code[]={0x902f,0x9430,0x03ed,0x3347,0x8b00,0x0e45,0x03ed,0x942b,0x3343,0x8900,0x0e45,0x9025,0x9427,0x03ed,0x3347,0x8b00,0x0e45,0x03ed,0x9422,0x3343,0x8900,0x0e45,0x4f26};
 auto fill=[&](bool wide){for(unsigned i=0;i<23;++i)ram[0x8c073e7eu+2*i]=code[i];ram[0x8c073ee0]=0x06fe;ram[0x8c073ee2]=0x06fc;
  const auto v=silent_scope::dc_input::limits_for_factor(wide?3:1);ram[0x8c073ee4]=v.yaw;ram[0x8c073ee6]=std::uint16_t(-v.yaw);ram[0x8c073ee8]=v.pitch;ram[0x8c073eea]=std::uint16_t(-v.pitch);};
 auto read=[&](std::uint32_t a){return ram[a];};int writes=0,acks=0;
 auto write=[&](std::uint32_t a,std::uint16_t v){++writes;ram[a]=v;};
 auto ack=[&](silent_scope::dc_input::AimLimits){++acks;return true;};
 NativeLimitsPolicy limits;
 auto result=limits.synchronize("T9507N",0,false,read,write,ack); // Early boot: not code-ready.
 assert(!result.known&&!result.failure&&!writes&&!acks);
 fill(false);result=limits.synchronize("T9507N",0,false,read,write,ack);
 assert(result.known&&result.acknowledged&&!result.wide&&!writes&&acks==1);
 result=limits.synchronize("T9507N",0,true,read,write,ack);assert(acks==1&&!writes);
 result=limits.synchronize("T9507N",1,true,read,write,ack);
 assert(result.wide&&result.changed&&result.acknowledged&&writes==4&&acks==2);
 limits.new_epoch();result=limits.synchronize("T9507N",-1,false,read,write,ack); // Load known wide, adopt without stores.
 assert(result.wide&&result.acknowledged&&!result.changed&&writes==4&&acks==3);
 result=limits.synchronize("T9507N",0,false,read,write,ack);assert(!result.wide&&writes==8&&acks==4);
 ram[0x8c073ee4]=123;result=limits.synchronize("T9507N",0,false,read,write,ack);
 assert(result.failure&&!result.acknowledged&&writes==8&&acks==4); // Formerly verified mismatch.
 limits.new_epoch();result=limits.synchronize("T9507N",0,false,read,write,ack);
 assert(!result.failure&&!result.known&&writes==8); // Unknown post-load state is never overwritten.
 result=limits.synchronize("T9507N",1,true,read,write,ack);assert(result.failure&&writes==8); // Recognized SDK requires real clamp guards.
 fill(true);silent_scope::dc_input::AimLimitPatch adopted;assert(adopted.adopt("T9507N",read)&&adopted.wider());
 ram[0x8c073e80]^=1;assert(!adopted.adopt("T9507N",read));ram[0x8c073e80]^=1;
 NativeLimitsPolicy failed_ack;auto noack=[&](silent_scope::dc_input::AimLimits){return false;};
 result=failed_ack.synchronize("T9507N",1,true,read,write,noack);assert(result.failure&&!result.acknowledged);
 NativeLimitsPolicy failed_store;fill(false);auto dropwrite=[&](std::uint32_t,std::uint16_t){++writes;};
 const int before=acks;result=failed_store.synchronize("T9507N",1,true,read,dropwrite,ack);
 assert(result.failure&&acks==before); // Actual postwrite guard gates acknowledgement.
#endif
}
