#include "rifle_recoil.h"
#include "rifle_steady_aim.h"
#include "rifle_simulation.h"
#include "rifle_smoothing.h"
#include "rifle_scope_stability.h"
#include <glm/gtc/matrix_transform.hpp>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace silent_scope;
namespace {
constexpr double Pi=3.14159265358979323846,Rate=72;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
glm::mat4 pose(double degrees,glm::vec3 grip={.1f,1.2f,-.3f}){
 return glm::translate(glm::mat4(1),grip)*glm::rotate(glm::mat4(1),glm::radians(float(degrees)),glm::vec3(0,1,0));
}
double yaw(const glm::mat4& m){return std::atan2(double(m[2].x),double(m[2].z))*180/Pi;}
double error(double a,double b){return std::remainder(a-b,360.);}
bool close(const glm::mat4&a,const glm::mat4&b,double epsilon=1e-5){
 for(int c=0;c<4;++c)for(int r=0;r<4;++r)if(!std::isfinite(a[c][r])||!std::isfinite(b[c][r])||std::abs(a[c][r]-b[c][r])>epsilon)return false;
 return true;
}
bool rigid(const glm::mat4& model){
 for(int c=0;c<4;++c)for(int r=0;r<4;++r)if(!std::isfinite(model[c][r]))return false;
 const auto basis=glm::mat3(model);if(std::abs(glm::determinant(basis)-1)>1e-4)return false;
 for(int c=0;c<3;++c)for(int r=0;r<3;++r)if(std::abs(glm::dot(basis[c],basis[r])-(c==r?1.f:0.f))>1e-4)return false;
 return true;
}
void weighted_step(){
 constexpr double first_low[]{0,11,5,2.5},first_high[]{0,14,8,4.2};
 constexpr double seven_low[]{0,46,31,18},seven_high[]{0,50,36,22};
 constexpr int expected_90[]{0,10,20,40};
 for(int mode=1;mode<=3;++mode){RifleSmoothing filter;filter.update(pose(0),{0,1,mode,false,true});
  int updates90=0;double previous=0,first=0,seven=0;
  for(int n=1;n<=180;++n){const auto out=filter.update(pose(60),{n/Rate,1,mode,false,true});const auto angle=yaw(out.model);
   require(out.valid&&rigid(out.model),"Step emitted an invalid rotation");
   require(angle>=previous-1e-4&&angle<=60.001,"Weighted step overshot or reversed");previous=angle;
   if(n==1)first=angle;
   if(n==7)seven=angle;
   if(!updates90&&angle>=54)updates90=n;
  }
  std::cout<<"Step mode="<<mode<<" first="<<first<<"deg after7="<<seven<<"deg 90%="<<updates90<<" updates\n";
  require(first>=first_low[mode]&&first<=first_high[mode],"Weighted step must not snap toward a fast turn");
  require(seven>=seven_low[mode]&&seven<=seven_high[mode],"Weighted step lost its noticeable level-specific lag");
  require(std::abs(updates90-expected_90[mode])<=1,"Weighted step convergence is outside its intended time window");
  require(std::abs(previous-60)<.02,"Weighted step never settles at the requested angle");
 }
}
std::array<double,4> wiggle(double amplitude,double hz){
 std::array<double,4> rms{};
 for(int mode=0;mode<4;++mode){RifleSmoothing filter;double sum=0;int count=0;
  for(int i=0;i<432;++i){const double seconds=i/Rate;const auto grip=glm::vec3(float(i)*.0001f,1.2f,-.3f);
   const auto raw=pose(amplitude*std::sin(2*Pi*hz*seconds),grip);
   const auto out=filter.update(raw,{seconds,1,mode,false,true});require(out.valid,"Wiggle pose rejected");
   require(glm::vec3(out.model[3])==grip,"Smoothing moved the raw primary grip");
   if(!mode)require(close(out.model,raw,0),"Off must be exact accepted-pose passthrough");
   if(i>=216){sum+=yaw(out.model)*yaw(out.model);++count;}
  }
  rms[mode]=std::sqrt(sum/count);
 }
 std::cout<<"Wiggle "<<amplitude<<"deg/"<<hz<<"Hz RMS Off/Low/Medium/High="<<rms[0]<<'/'<<rms[1]<<'/'<<rms[2]<<'/'<<rms[3]<<"deg\n";
 return rms;
}
void large_wiggle(){
 const auto hand=wiggle(5,5);
 require(hand[1]<1.8&&hand[2]<1.0&&hand[3]<.55,"Five-degree hand wiggle bypassed angular smoothing");
 const auto large=wiggle(15,3);
 require(large[1]<7.5&&large[2]<4.8&&large[3]<2.6,"Large wiggle erased the weighted response");
 for(const auto& rms:{hand,large})require(rms[2]<rms[1]*.7&&rms[3]<rms[2]*.7,"Smoothing levels are not meaningfully distinct");
 const auto tremor=wiggle(.2,10);require(tremor[1]<.045&&tremor[2]<.025&&tremor[3]<.015,"Fine tremor is insufficiently damped");
}
void tracking_and_settling(){
 constexpr double slow_low[]{0,4,9,18},slow_high[]{0,6.5,12,24};
 constexpr double fast_low[]{0,8,18,34},fast_high[]{0,12,25,48};
 for(int mode=1;mode<=3;++mode)for(double speed:{90.,180.}){RifleSmoothing filter;filter.update(pose(0),{0,1,mode,true,true});
  const int moving=int(speed==90?2*Rate:Rate);double lag=0;
  for(int n=1;n<=moving;++n){const double goal=speed*n/Rate;auto out=filter.update(pose(goal),{n/Rate,1,mode,true,true});
   lag=error(goal,yaw(out.model));require(out.valid&&lag>=-1e-3,"Moving rifle aim overshot the hand");
  }
  const auto lo=speed==90?slow_low[mode]:fast_low[mode],hi=speed==90?slow_high[mode]:fast_high[mode];
  std::cout<<"Tracking "<<speed<<"deg/s mode="<<mode<<" lag="<<lag<<"deg\n";
  require(lag>=lo&&lag<=hi,"Continuous aiming lost its deliberate weighted lag");
  double previous=lag;
  for(int n=1;n<=144;++n){auto out=filter.update(pose(180),{(moving+n)/Rate,1,mode,true,true});const double remaining=error(180,yaw(out.model));
   require(remaining>=-1e-3&&remaining<=previous+1e-3,"Weighted settling overshot or reversed");previous=remaining;
  }
  require(std::abs(previous)<.05,"Fast turn failed to settle after the hand stopped");
 }
}
void frame_periods(){
 for(int mode=1;mode<=3;++mode){double reference=-1;
  for(double rate:{60.,72.,90.,120.}){RifleSmoothing filter;filter.update(pose(0),{0,1,mode,false,true});double now=0;RifleSmoothingResult out;
   while(now<.3){now=std::min(.3,now+1/rate);out=filter.update(pose(60),{now,1,mode,false,true});}
   if(reference<0)reference=yaw(out.model);
   require(std::abs(yaw(out.model)-reference)<.005,"Equal elapsed time depends on XR frame rate");
  }
  RifleSmoothing uneven;uneven.update(pose(0),{0,1,mode,false,true});double now=0;int sample=0;RifleSmoothingResult out;
  const double intervals[]{.004,.025,.011,.016};
  while(now<.3){now=std::min(.3,now+intervals[sample++%4]);out=uneven.update(pose(60),{now,1,mode,false,true});}
  require(std::abs(yaw(out.model)-reference)<.005,"Uneven valid frame intervals changed the response");
 }
}
void lifecycle_and_rotation(){
 RifleSmoothing seam;seam.update(pose(179),{1,1,3,false,true});
 auto out=seam.update(pose(-179),{1+1/Rate,1,3,false,true});require(std::abs(yaw(out.model))>175,"Quaternion seam took the long route");
 for(int mode=0;mode<4;++mode)for(int kind=0;kind<8;++kind){RifleSmoothing filter;filter.update(pose(0),{2,1,mode,false,true});
  filter.update(pose(1),{2.01,1,mode,false,true});RifleSmoothingContext context{2.02,1,mode,false,true};
  if(kind==0)context.hand=0;
  if(kind==1)context.level=(mode+1)%4;
  if(kind==2)context.supported=true;
  if(kind==3)context.seconds=2.5;
  if(kind==4)context.seconds=1.5;
  if(kind==5)context.seconds=2.01;
  if(kind==6)filter.reset();
  if(kind==7){context.available=false;require(!filter.update(pose(90),context).valid,"Unavailable model remained valid");context.available=true;context.seconds=2.03;}
  require(close(filter.update(pose(90),context).model,pose(90)),"Lifecycle restart retained old aim lag");
 }
 for(int mode=0;mode<4;++mode){RifleSmoothing filter;
  for(int n=0;n<240;++n){const double now=n/Rate;const auto raw=pose(80*std::sin(now),{float(now),1.3f,float(-now)})*
    glm::rotate(glm::mat4(1),float(.6*std::sin(3*now)),glm::vec3(1,0,0))*glm::rotate(glm::mat4(1),float(2*std::sin(2*now)),glm::vec3(0,0,1));
   const auto mixed=filter.update(raw,{now,0,mode,true,true});require(mixed.valid&&rigid(mixed.model),"Mixed-axis rotation lost rigidity");
   require(mixed.model[3]==raw[3],"Mixed-axis filtering moved grip translation");
   if(!mode)require(close(mixed.model,raw,0),"Off changed an accepted supported pose");
  }
  auto malformed=pose(0);malformed[0].x=std::numeric_limits<float>::quiet_NaN();require(!filter.update(malformed,{4,0,mode,true,true}).valid,"Nonfinite pose accepted");
  require(close(filter.update(pose(80),{4.01,0,mode,true,true}).model,pose(80)),"Invalid pose failed to reset history");
 }
}
void restart_diagnostics(){
 for(int mode=0;mode<4;++mode){RifleSmoothing filter;
  const auto first=filter.update(pose(0),{1,1,mode,false,true});require(first.valid&&first.restarted,"First valid pose did not report history seeding");
  require(!filter.update(pose(5),{1.01,1,mode,false,true}).restarted,"Stable pose reported a spurious restart");
  require(filter.update(pose(10),{1.02,0,mode,false,true}).restarted,"Hand change did not report restart");
  require(filter.update(pose(15),{1.03,0,mode,true,true}).restarted,"Support change did not report restart");
  require(!filter.update(pose(20),{1.04,0,mode,true,true}).restarted,"Stable supported pose restarted");
  require(filter.update(pose(25),{1.31,0,mode,true,true}).restarted,"Long frame gap did not report restart");
  require(filter.update(pose(30),{1.30,0,mode,true,true}).restarted,"Reversed clock did not report restart");
  require(filter.update(pose(35),{1.30,0,mode,true,true}).restarted,"Duplicate timestamp did not report restart");
  const auto unavailable=filter.update(pose(40),{1.32,0,mode,true,false});
  require(!unavailable.valid&&!unavailable.restarted,"Unavailable sample must not count as a seeded valid frame");
  require(filter.update(pose(45),{1.33,0,mode,true,true}).restarted,"Return from unavailable did not report restart");
  require(!filter.update(pose(50),{1.34,0,mode,true,true}).restarted,"Restart diagnostic repeated on next frame");
  filter.reset();require(filter.update(pose(55),{1.35,0,mode,true,true}).restarted,"Explicit reset was not reported on valid return");
  require(filter.update(pose(60),{1.36,0,(mode+1)%4,true,true}).restarted,"Smoothing mode change did not report restart");
 }
 RifleSmoothing invalid;
 for(int kind=0;kind<7;++kind){invalid.update(pose(0),{2,1,1,false,true});auto raw=pose(5);RifleSmoothingContext context{2.01,1,1,false,true};
  if(kind==0)raw[0].x=std::numeric_limits<float>::infinity();
  if(kind==1)raw[0]*=-1.f;
  if(kind==2)raw[0]=glm::vec4(0);
  if(kind==3)raw[0].w=.1f;
  if(kind==4)context.hand=2;
  if(kind==5)context.seconds=std::numeric_limits<double>::quiet_NaN();
  if(kind==6)context.seconds=-1;
  const auto bad=invalid.update(raw,context);require(!bad.valid&&!bad.restarted,"Invalid sample reported a valid seeded frame");
  const auto restored=invalid.update(pose(25),{2.02,1,1,false,true});
  require(restored.valid&&restored.restarted&&close(restored.model,pose(25)),"Invalid sample left delayed or unreported history");
 }
}
}
void scoped_jitter(){
    const glm::vec3 head(0, .14f, .15f), ocular(0, .14f, .04f);
    const auto neutral = pose(0, glm::vec3(0));
    double previousPower = 1.;
    for(int level:{1,2,3,4,5}) {
        silent_scope::RifleScopeStability stock;
        const auto step = [&](const glm::mat4& model, double time, float alignment = 1.f) {
            return stock.update(model,head,ocular,time,alignment,level);
        };
        require(close(step(neutral,0),neutral,0),"Scoped support snapped on entry");
        double aimPower=0, rawPower=0, cheekPower=0;
        for(int i=1;i<=288;++i) {
            const double wave=std::sin(i*6.283185307179586/9);
            auto raw=pose(.16*wave,glm::vec3(float(.003*wave),0,0));
            auto output=step(raw,i/72.);
            if(i<72)continue;
            aimPower+=double(output[0].z)*output[0].z;
            rawPower+=double(raw[0].z)*raw[0].z;
            const auto cheek=glm::vec3(output*glm::vec4(ocular,1));
            cheekPower+=double(cheek.x)*cheek.x;
        }
        require(aimPower<rawPower*.23&&aimPower<previousPower,"Scoped aim tremor did not decrease");
        require(cheekPower<.0003,"Cheek position remains noisy");
        previousPower=aimPower;
        for(int i=1;i<=36;++i){
            const auto raw=pose(90.*i/72.,glm::vec3(0));
            const auto output=step(raw,(288+i)/72.);
            if(i>=12)require(std::abs(output[0].z-raw[0].z)<.045f,"Steady sweep dragged behind the hand");
        }
        const auto lowered=pose(30,glm::vec3(.2f,0,0));
        require(close(stock.update(lowered,head,ocular,4.6,0,0),lowered,0),"Scope stability Off changed the pose");
        require(close(step(lowered,5,0),lowered,0),"Reset retained an old shoulder position");
        std::cout<<"Scope level "<<level<<" angular RMS ratio "<<std::sqrt(aimPower/rawPower)<<'\n';
    }
}
void shot_recoil(){
    RifleRecoil recoil;
    const glm::mat4 rest(1.f);
    require(close(recoil.apply(rest,1),rest,0),"Idle rifle moved");
    recoil.shot(1,false);
    const auto single=recoil.apply(rest,1.025);
    require(single[3].z>0&&single[2].y<0,"Expected backwards travel and muzzle rise");
    recoil.shot(1,true);
    const auto braced=recoil.apply(rest,1.025);
    require(std::abs(braced[3].z-.018f)<.00001f&&
            std::abs(glm::degrees(std::asin(-braced[2].y))-4.5f)<.001f,"Braced kick should match the previous single-hand kick");
    require(std::abs(single[3].z-2*braced[3].z)<.00001f&&
            std::abs(std::asin(-single[2].y)-2*std::asin(-braced[2].y))<.00001f,"One-handed kick should be double");
    require(close(recoil.apply(rest,1.3),rest,0),"Kick did not settle");
    recoil.reset();
    require(close(recoil.apply(rest,1.025),rest,0),"Kick leaked across pose reset");
}
void steady_aim(){
    RifleSteadyAim steady;
    const glm::vec3 ocular(0,.14f,.04f);
    const auto rest=pose(32), moved=pose(42);
    require(close(steady.update(moved,ocular,0,true,true),moved,0),"Held grip armed after reset");
    steady.update(rest,ocular,.01,false,true);
    require(close(steady.update(rest,ocular,.02,true,true),rest,.0001f),"Grip press changed aim");
    const auto reduced=steady.update(moved,ocular,.03,true,true);
    require(std::abs(glm::degrees(std::asin(reduced[2].x))-34)<.001f,"Steady aim should move at one fifth gain");
    require(glm::length(glm::vec3((reduced-moved)*glm::vec4(ocular,1)))<.0001f,"Steady aim moved the eyepiece");
    require(close(steady.update(moved,ocular,.04,false,true),reduced,.0001f),"Grip release snapped aim");
    require(close(steady.update(moved,ocular,.04,true,true),reduced,.0001f),"Quick re-grip snapped aim");
    steady.update(moved,ocular,.045,false,true);
    require(close(steady.update(moved,ocular,.23,false,true),moved,0),"Grip release did not restore tracking");
    steady.reset();
    require(close(steady.update(moved,ocular,.24,true,true),moved,0),"Tracking reset retained a precision anchor");
}
void rifle_carry(){
    using Hold=RifleCarry::Hold;
    RifleCarry carry;
    RifleSupport support;
    const glm::mat4 chest=glm::translate(glm::mat4(1),glm::vec3(0,-.4f,-.3f));
    const auto rear=chest;
    const auto front=glm::translate(rear,glm::vec3(0,0,-.35f));
    require(carry.update(chest,rear,front,{},false,false)==chest,"Unheld rifle did not stow");
    require(carry.update(chest,rear,front,{},true,true)==rear&&carry.hold()==Hold::Rear,"Rear grip did not pick up the rifle");
    const auto settled=glm::translate(rear,glm::vec3(0,.04f,.025f));
    carry.remember_aimed_pose(settled,true);
    require(carry.update(chest,rear,front,{},false,true)==settled&&carry.hold()==Hold::Front,"Rear release moved the rifle out of the support hand");
    const auto moved=glm::translate(front,glm::vec3(.1f,0,0));
    const auto carried=carry.update(chest,rear,moved,{},false,true);
    require(glm::distance(glm::vec3(carried[3]),glm::vec3(settled[3])+glm::vec3(.1f,0,0))<.001f,"Rifle did not follow the support controller");
    const auto missed=glm::translate(carried,glm::vec3(.085f,0,0));
    carry.update(chest,missed,moved,{},true,true);
    require(carry.hold()==Hold::Front,"Rear grip attached outside the handle");
    const auto returned=glm::rotate(glm::translate(carried,glm::vec3(.055f,0,0)),.55f,glm::vec3(0,1,0));
    const auto grabbed=carry.update(chest,returned,moved,{},true,true);
    require(grabbed==carried&&carry.hold()==Hold::Rear,"Rear regrab failed to preserve rifle pose");
    std::array<RifleSupportHand,2> hands{{{moved,true,true},{grabbed,true,true}}};
    const auto joined=support.update(hands,{true,false,1,{},carry.rejoined_front()});
    require(joined.supported&&joined.model==carried,"Regrab dropped the support grip or moved the aim");
    hands[1].model=carry.update(chest,returned,moved,{},true,true);
    const auto steady=support.update(hands,{true,false,1,{},carry.rejoined_front()});
    require(steady.supported&&glm::distance(glm::vec3(steady.model[3]),glm::vec3(carried[3]))<.0001f
        &&glm::distance(glm::vec3(steady.model[2]),glm::vec3(carried[2]))<.0001f,"Stationary hands jumped after regrab");
    hands[0].grip=false;
    require(!support.update(hands,{true,false,1,{}}).supported,"Releasing support did not detach");
    require(carry.update(chest,rear,front,{},false,false)==chest&&carry.hold()==Hold::Chest,"Both grips released without stowing");
    require(carry.update(chest,rear,front,{},false,true)==chest&&carry.hold()==Hold::Front,"Front grip did not pick up from the chest");
    carry.reset();
    require(carry.hold()==Hold::Chest,"Tracking reset left the rifle in a hand");
}
void awp_native_ammo_and_bolt(){
    RifleSimulation awp;
    const auto bolt=RifleSimulation::BoltHandle;
    awp.sync_native_rounds(3);
    require(awp.rounds()==3&&awp.can_fire(),"Arcade-to-Simulation ammo did not carry over");
    awp.fired();
    require(awp.rounds()==2&&!awp.can_fire(),"Confirmed shot did not empty the chamber");
    awp.update(bolt,false,true,false);
    awp.update(bolt+glm::vec3(.12f,0,0),true,true,false);
    awp.update(bolt,true,true,false);
    require(awp.holding_bolt(),"Held trigger missed the bolt when approaching");
    awp.update(bolt+glm::vec3(0,.05f,.08f),true,true,false);
    awp.update(bolt+glm::vec3(0,.05f,.009f),false,true,false);
    require(awp.can_fire()&&!awp.holding_bolt(),"Forward travel on the release frame did not chamber");
    awp.sync_native_rounds(1);
    require(RifleSimulation::native_shot_spent(1,5)&&!RifleSimulation::native_shot_spent(3,3),
        "Last-shot refill or unchanged frame was misclassified");
    awp.fired();
    awp.sync_native_rounds(5);
    require(awp.rounds()==0,"Native refill bypassed the empty physical magazine");
}
void manual_awp(){
    RifleSimulation awp;
    const auto bolt=RifleSimulation::BoltHandle;
    const auto well=RifleSimulation::MagazineBase;
    const auto rack=[&]{
        awp.update(bolt,false,true,false);
        awp.update(bolt,true,true,false);
        require(awp.holding_bolt(),"Grabbed bolt reported a free support hand");
        awp.update(bolt+glm::vec3(0,0,.08f),true,true,false);
        require(awp.bolt()==0,"Locked bolt moved rearward");
        awp.update(bolt+glm::vec3(0,.05f,0),true,true,false);
        const auto previousEjections=awp.ejections();
        awp.update(bolt+glm::vec3(0,.05f,.08f),true,true,false);
        require(awp.feeding()==(awp.rounds()>0),"Feed visibility did not follow loaded magazine");
        awp.update(bolt+glm::vec3(0,.05f,.085f),true,true,false);
        require(awp.ejections()<=previousEjections+1,"Repeated rear samples duplicated the casing");
        require(!awp.can_fire(),"Open bolt allowed a shot");
        awp.update(bolt+glm::vec3(0,.05f,0),true,true,false);
        require(awp.can_fire()==(awp.rounds()>0),"Bolt did not chamber at full forward travel");
        require(!awp.feeding(),"Chambered cartridge stayed visible");
        awp.update(bolt,true,true,false);
    };
    require(awp.can_fire(),"Simulation did not start chambered");
    for(int round=RifleSimulation::Capacity;round>0;--round){
        if(round!=RifleSimulation::Capacity)rack();
        require(awp.can_fire(),"Complete bolt stroke did not chamber a round");
        awp.fired();
        require(!awp.can_fire()&&awp.rounds()==round-1,"Shot did not require a new bolt cycle");
    }
    rack();
    require(!awp.can_fire(),"Empty magazine allowed a shot");
    require(awp.ejections()==RifleSimulation::Capacity,"A confirmed shot failed to eject exactly one case");
    awp.update(well,false,true,false);
    awp.update(well,true,true,false);
    require(awp.magazine()==RifleSimulation::Magazine::EmptyInHand,"Empty magazine did not detach");
    awp.update(well,false,true,false);
    awp.update({0,-.5f,0},true,true,true);
    awp.update(well,true,true,false);
    require(awp.rounds()==RifleSimulation::Capacity&&!awp.can_fire(),"Fresh magazine bypassed chambering");
    rack();
    require(awp.can_fire(),"Reload and chamber sequence failed");
}
int main(){int failures=0;
 const auto run=[&](const char* name,auto test){try{test();std::cout<<"PASS "<<name<<'\n';}catch(const std::exception& e){++failures;std::cerr<<"FAIL "<<name<<": "<<e.what()<<'\n';}};
 run("weighted step",weighted_step);run("realistic wiggle",large_wiggle);run("tracking/settling",tracking_and_settling);
 run("frame periods",frame_periods);run("lifecycle/rotation",lifecycle_and_rotation);
 run("restart diagnostics",restart_diagnostics);
 run("scoped jitter and large motion",scoped_jitter);
 run("single-hand and braced shot recoil",shot_recoil);
 run("hold-to-steady aim",steady_aim);
 run("AWP hand and chest carry",rifle_carry);
 run("AWP manual bolt and reload",manual_awp);
 run("AWP native ammo and release-frame bolt",awp_native_ammo_and_bolt);
 return failures?1:0;
}
