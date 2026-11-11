#include "rifle_support.h"
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <iostream>
#include <limits>
using namespace silent_scope;
#define REQUIRE(x) do {if(!(x)){std::cerr<<"FAIL line "<<__LINE__<<": " #x "\n";return 1;}}while(false)
static bool near(glm::vec3 a,glm::vec3 b,float tolerance=1e-5f){return glm::length(a-b)<tolerance;}
static bool near(glm::mat4 a,glm::mat4 b){for(int i=0;i<4;i++)for(int j=0;j<4;j++)if(!std::isfinite(a[i][j]-b[i][j])||std::abs(a[i][j]-b[i][j])>1e-5f)return false;return true;}
static bool finite(glm::mat4 m){for(int i=0;i<4;i++)for(int j=0;j<4;j++)if(!std::isfinite(m[i][j]))return false;return true;}
static std::array<RifleSupportHand,2> hands(){
 std::array<RifleSupportHand,2> h{};
 h[1]={glm::translate(glm::mat4(1),glm::vec3(.25f,1.2f,.6f)),true,false};
 h[0]={glm::translate(glm::mat4(1),glm::vec3(.25f,1.175f,.26f)),true,false};
 return h;
}
static int roll_stability(){
 for(int primary:{0,1})for(const auto grab:{RifleSupport::foregrip_anchor,glm::vec3(.119f,-.025f,-.18f)}){
  RifleSupport support;RifleSupportContext context{true,false,primary};auto h=hands();
  const auto base=glm::translate(glm::mat4(1),glm::vec3(1,2,3))*glm::rotate(glm::mat4(1),.4f,glm::vec3(0,1,0))*glm::rotate(glm::mat4(1),-.2f,glm::vec3(1,0,0));
  h[primary]={base,true,false};h[1-primary]={glm::translate(glm::mat4(1),glm::vec3(base*glm::vec4(grab,1))),true,false};
  support.update(h,context);h[1-primary].grip=true;
  REQUIRE(near(support.update(h,context).model,base));
  const auto forward=glm::vec3(base*glm::vec4(0,0,-1,0));
  for(float degrees:{90.f,180.f,-179.f,359.f}){
   h[primary].model=base*glm::rotate(glm::mat4(1),glm::radians(degrees),glm::vec3(0,0,1));
   const auto result=support.update(h,context);
   REQUIRE(result.supported&&near(glm::vec3(result.model*glm::vec4(0,0,-1,0)),forward));
   REQUIRE(near(glm::vec3(result.model[3]),glm::vec3(base[3])));
  }
 }
 return 0;
}
static int grip_hysteresis(){
 GripLatch grip;
 REQUIRE(!grip.valid());
 REQUIRE(grip.update(.9f)&&grip.update(.5f)); // no startup neutral inferred from midrange
 REQUIRE(grip.valid());
 REQUIRE(!grip.update(.35f)&&!grip.update(.64f));
 REQUIRE(grip.update(.65f)&&grip.update(.5f)&&grip.update(.3501f));
 REQUIRE(!grip.update(.35f)&&!grip.update(.4f));
 grip.reset();REQUIRE(!grip.valid());REQUIRE(grip.update(.5f)&&grip.update(.64f));
 REQUIRE(!grip.update(.34f)&&grip.update(.7f));
 for(float invalid:{-1.f,2.f,std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()}){
  REQUIRE(grip.update(invalid)&&!grip.valid());
  REQUIRE(grip.update(.5f)&&grip.valid());
  REQUIRE(!grip.update(0));
 }
 return 0;
}
static int grip_validity_composition(){
 RifleSupport support;GripLatch grip;RifleSupportContext context;auto h=hands();
 const auto sample=[&](float value){h[0].grip=grip.update(value);h[0].grip_valid=grip.valid();return support.update(h,context);};
 REQUIRE(!sample(0).supported); // real neutral arms support
 REQUIRE(!sample(std::numeric_limits<float>::quiet_NaN()).supported); // invalid cannot capture an armed helper
 REQUIRE(!sample(.9f).supported); // recovering held cannot invent another neutral
 REQUIRE(!sample(0).supported&&sample(.9f).supported);
 const auto detached=sample(std::numeric_limits<float>::infinity());
 REQUIRE(!detached.supported&&detached.valid&&near(detached.model,h[1].model));
 REQUIRE(!sample(.9f).supported);
 REQUIRE(!sample(0).supported&&sample(.9f).supported);
 // Primary grip validity is irrelevant to the tracked one-hand pose/fire ray.
 h[1].grip_valid=false;REQUIRE(sample(.9f).supported);
 h[0].grip=false;h[0].grip_valid=false;
 const auto base=support.update(h,context);REQUIRE(base.valid&&!base.supported&&near(base.model,h[1].model));
 return 0;
}
static int capsule_capture_release(){
 for(int primary:{0,1})for(float z:{-.49f,-.18f}){
  RifleSupport support;RifleSupportContext context{true,false,primary};auto h=hands();
  const auto base=glm::translate(glm::mat4(1),glm::vec3(-2,1,3))*glm::rotate(glm::mat4(1),.6f,glm::vec3(0,1,0));
  h[primary]={base,true,false};h[1-primary]={glm::translate(glm::mat4(1),glm::vec3(base*glm::vec4(.121f,-.025f,z,1))),true,false};
  support.update(h,context);h[1-primary].grip=true;REQUIRE(!support.update(h,context).supported);
  h[1-primary].model[3]=base*glm::vec4(.119f,-.025f,z,1);
  const auto capture=support.update(h,context);REQUIRE(capture.supported&&near(capture.model,base));
 }
 RifleSupport support;RifleSupportContext context;auto h=hands();support.update(h,context);h[0].grip=true;support.update(h,context);
 const auto initial_delta=glm::vec3(h[0].model[3]-h[1].model[3]);
 h[0].model[3]=h[1].model[3]+glm::vec4(initial_delta*2.f,0);REQUIRE(support.update(h,context).supported);
 h[0].model[3]=h[1].model[3]+glm::vec4(initial_delta*2.15f,0);REQUIRE(!support.update(h,context).supported);
 h[0].model=hands()[0].model;REQUIRE(!support.update(h,context).supported); // exit requires fresh neutral
 h[0].grip=false;support.update(h,context);h[0].grip=true;REQUIRE(support.update(h,context).supported);
 h[0].model[3].x+=.3f;
 REQUIRE(support.update(h,context).supported); // exit is measured in solved rifle, not raw wrist frame
 support.reset();h=hands();support.update(h,context);h[0].grip=true;support.update(h,context);
 h[1].model=glm::rotate(h[1].model,glm::radians(90.f),glm::vec3(0,1,0));
 REQUIRE(!support.update(h,context).supported); // retained barrel-swing plausibility guard
 // A supported player turning around is not the offhand moving behind an unchanged wrist.
 support.reset();h=hands();h[0].model[3].y=h[1].model[3].y;
 support.update(h,context);h[0].grip=true;support.update(h,context);
 const auto origin=glm::vec3(h[1].model[3]);
 for(float angle:{179.f,180.f}){
  const auto turn=glm::rotate(glm::mat4(1),glm::radians(angle),glm::vec3(0,1,0));
  h[1].model=glm::translate(glm::mat4(1),origin)*turn;h[0].model[3]=glm::vec4(origin+glm::vec3(turn*glm::vec4(0,0,-.34f,0)),1);
  const auto result=support.update(h,context);REQUIRE(result.supported&&result.valid&&finite(result.model));
 }
 return 0;
}
int main(){
 REQUIRE(roll_stability()==0);
 REQUIRE(grip_hysteresis()==0);
 REQUIRE(grip_validity_composition()==0);
 REQUIRE(capsule_capture_release()==0);
 RifleSupport support;RifleSupportContext context;auto h=hands();
 // An armed squeeze outside the fore-end waits for approach without being consumed.
 support.update(h,context);h[0].model[3].x+=.25f;h[0].grip=true;
 REQUIRE(!support.update(h,context).supported);
 h[0].model=hands()[0].model;
 REQUIRE(support.update(h,context).supported);
 support.reset();h=hands();
 // Held on first appearance, or after invalid separation, still needs neutral.
 h[0].grip=true;REQUIRE(!support.update(h,context).supported);
 h[0].grip=false;support.update(h,context);
 h[0].model[3].x=2;h[0].grip=true;REQUIRE(!support.update(h,context).supported);
 h[0]=hands()[0];h[0].grip=true;REQUIRE(!support.update(h,context).supported);
 // A released grip followed by a nearby fresh press latches without a pose jump.
 h[0].grip=false;auto free=support.update(h,context);REQUIRE(free.valid&&near(free.model,h[1].model));
 h[0].grip=true;auto held=support.update(h,context);REQUIRE(held.supported&&near(held.model,h[1].model));
 h[0].model[3].x+=.10f;held=support.update(h,context);
 REQUIRE(held.supported&&held.valid&&near(glm::vec3(held.model[3]),glm::vec3(h[1].model[3])));
 const auto visible_forward=glm::vec3(held.model*glm::vec4(0,0,-1,0));
 REQUIRE(visible_forward.x>.2f&&visible_forward.z<-.9f);
 // Native aim can use this exact model's -Z, preserving the visible gun direction.
 REQUIRE(std::abs(glm::length(visible_forward)-1)<1e-5f);
 h[1].model[3].x+=.015f;held=support.update(h,context);
 REQUIRE(held.supported&&near(glm::vec3(held.model[3]),glm::vec3(.265f,1.2f,.6f)));
 h[0].grip=false;REQUIRE(near(support.update(h,context).model,h[1].model));
 // Each interruption cancels and requires neutral input before reattachment.
 for(int interruption=0;interruption<5;++interruption){
  h=hands();context={};support.reset();support.update(h,context);h[0].grip=true;
  REQUIRE(support.update(h,context).supported);
  if(interruption==0)context.focused=false;
  if(interruption==1)context.menu=true;
  if(interruption==2)h[0].tracked=false;
  if(interruption==3)h[1].tracked=false;
  if(interruption==4)support.reset();
  REQUIRE(!support.update(h,context).supported);
  context={};h[0].tracked=h[1].tracked=true;
  REQUIRE(!support.update(h,context).supported);
  h[0].grip=false;support.update(h,context);h[0].grip=true;
  REQUIRE(support.update(h,context).supported);
 }
 // Hand changes cannot inherit a held grip; left-hand ownership works equally.
 h=hands();context={};support.reset();support.update(h,context);h[0].grip=true;
 REQUIRE(support.update(h,context).supported);
 context.active_hand=0;h[1].grip=true;REQUIRE(!support.update(h,context).supported);
 h[0].model=glm::translate(glm::mat4(1),glm::vec3(-.3f,1.1f,.5f));
 h[1].model=glm::translate(glm::mat4(1),glm::vec3(-.3f,1.075f,.16f));
 REQUIRE(!support.update(h,context).supported);
 h[1].grip=false;support.update(h,context);h[1].grip=true;
 held=support.update(h,context);REQUIRE(held.supported&&near(glm::vec3(held.model[3]),glm::vec3(-.3f,1.1f,.5f)));
 // A malformed primary cannot emit NaN; malformed support only releases support.
 for(float bad:{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()}){
  h=hands();context={};support.reset();support.update(h,context);h[0].grip=true;support.update(h,context);
  h[0].model[3].x=bad;auto result=support.update(h,context);REQUIRE(!result.supported&&result.valid&&finite(result.model));
  h=hands();h[1].model[0].x=bad;result=support.update(h,context);REQUIRE(!result.supported&&!result.valid&&finite(result.model));
 }
 h=hands();h[1].model[0].x=-1;REQUIRE(!support.update(h,context).valid); // reflected basis
 h=hands();h[1].model[0]=glm::vec4(0);REQUIRE(!support.update(h,context).valid); // collapsed basis
 h=hands();h[1].model[0].w=.2f;REQUIRE(!support.update(h,context).valid); // nonaffine pose
 h=hands();support.reset();support.update(h,context);h[0].grip=true;support.update(h,context);
 h[0].model[3]=h[1].model[3];REQUIRE(!support.update(h,context).supported);
 h=hands();support.reset();support.update(h,context);h[0].grip=true;support.update(h,context);
 h[0].model[3].z+=.7f;auto behind=support.update(h,context);
 REQUIRE(!behind.supported&&finite(behind.model)&&near(behind.model,h[1].model));
 // Roll and nonzero origins remain stable through a sweep near the grabbed point.
 h=hands();support.reset();
 h[1].model=glm::translate(glm::mat4(1),glm::vec3(2,3,4))*glm::rotate(glm::mat4(1),1.2f,glm::vec3(0,0,1));
 h[0].model=glm::translate(glm::mat4(1),glm::vec3(h[1].model*glm::vec4(0,-.025f,-.34f,1)));
 support.update(h,context);h[0].grip=true;held=support.update(h,context);
 REQUIRE(held.supported&&near(held.model,h[1].model));
 for(int step=-20;step<=20;++step){
  h[0].model[3].x=2+step*.005f;held=support.update(h,context);
  REQUIRE(held.supported&&finite(held.model)&&near(glm::vec3(held.model[3]),glm::vec3(2,3,4)));
  REQUIRE(glm::dot(glm::vec3(held.model[2]),glm::vec3(h[1].model[2]))>.9f);
 }
 context.active_hand=99;REQUIRE(!support.update(h,context).valid);
 std::cout<<"Foregrip held-approach/capsules, grip validity/hysteresis composition, position-driven aim/wrist twist, ownership and interruption guards passed\n";
}
