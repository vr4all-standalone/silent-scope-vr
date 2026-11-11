#include "rifle_holsters.h"
#include "rifle_simulation.h"
#include <iostream>
#include <stdexcept>
using namespace silent_scope;
static void check(bool yes,const char* why){if(!yes)throw std::runtime_error(why);}
static bool near(glm::vec3 a,glm::vec3 b){return glm::distance(a,b)<.0001f;}
int main(){try{
    const auto belt=glm::translate(glm::mat4(1),glm::vec3(0,1.6f,0));
    std::array<glm::mat4,2> slots{RifleHolsters::pose(belt,0),RifleHolsters::pose(belt,1)};
    check(slots[0][3].x<0&&slots[1][3].x>0,"Rifles lost left/right separation");
    for(const auto& slot:slots){
        check(near(glm::vec3(slot*glm::vec4(0,0,-1,0)),{0,-1,0}),"Holstered barrel is not straight down");
        check(slot[3].y>=1.15f&&slot[3].y<=1.3f&&slot[3].z<-.35f&&std::abs(slot[3].x)<.23f,
            "Grip is not in front within easy chest-height reach");
    }
    const auto turned=glm::rotate(belt,1.2f,glm::vec3(0,1,0));
    check(near(glm::vec3(RifleHolsters::pose(turned,0)[3]),glm::vec3(turned*glm::vec4(-.18f,-.40f,-.45f,1))),"Rack did not follow body yaw");
    std::array<RifleForegrip,2> fronts{{{.01f,-.35f,-.20f},{.0275f,-.583f,-.33f}}};
    for(int primary:{0,1}){
        RifleHolsters rack;
        std::array<RifleSupportHand,2> h;
        h[primary]={slots[0],true,true,true};h[1-primary]={glm::mat4(1),true,false,true};
        check(rack.pick(h,slots,fronts,{1,1},primary,false,true)==-1,"Startup held Grip picked up a rifle");
        h[primary].grip=false;rack.pick(h,slots,fronts,{1,1},primary,false,true);
        h[primary].grip=true;
        check(rack.pick(h,slots,fronts,{1,1},primary,false,true)==0,"Rear Grip failed to pick Arcade for either firing hand");
        h[primary].model=slots[1];
        check(rack.pick(h,slots,fronts,{1,1},primary,true,true)==-1,"Holding one rifle allowed a second pickup");
        check(rack.pick(h,slots,fronts,{1,1},primary,false,true)==-1,"Held Grip replayed as a new pickup after drop");
        h[primary].grip=false;rack.pick(h,slots,fronts,{1,1},primary,false,true);h[primary].grip=true;
        check(rack.pick(h,slots,fronts,{1,1},primary,false,true)==1,"Drop and fresh Grip failed to switch to Simulation");
        h[primary].grip=false;rack.pick(h,slots,fronts,{1,1},primary,false,true);h[primary].grip=true;
        h[primary].tracked=false;
        check(rack.pick(h,slots,fronts,{1,1},primary,false,true)==-1,"Untracked hand picked up a rifle");
        h[primary].tracked=true;
        check(rack.pick(h,slots,fronts,{1,1},primary,false,true)==-1,"Tracking recovery replayed Grip");
        h[primary].grip=false;h[1-primary].grip=false;rack.pick(h,slots,fronts,{1,1},primary,false,true);
        h[1-primary].model=glm::translate(slots[1],glm::vec3(0,.0275f,-.45f));h[1-primary].grip=true;
        check(rack.pick(h,slots,fronts,{1,1},primary,false,true)==1,"Support Grip failed to pick the actual fore-end");
        rack.reset();h[1-primary].grip=false;rack.pick(h,slots,fronts,{1,1},primary,false,true);
        h[1-primary].grip=true;h[1-primary].model=glm::translate(slots[1],glm::vec3(.2f,.0275f,-.45f));
        check(rack.pick(h,slots,fronts,{1,1},primary,false,true)==-1,"Off-gun Grip picked up a rifle");
        h[1-primary].model=glm::translate(slots[1],glm::vec3(0,.0275f,-.45f));
        check(rack.pick(h,slots,fronts,{1,1},primary,false,false)==-1,"Inactive/menu rack accepted pickup");
        check(rack.pick(h,slots,fronts,{1,1},primary,false,true)==-1,"Closing menu replayed held Grip");
    }
    for(int slot=0;slot<2;++slot){
        RifleCarry carry;
        const auto rear=slots[slot];
        const auto front=glm::translate(rear,glm::vec3(0,fronts[slot].height,-.30f));
        carry.update(slots[slot],rear,front,fronts[slot],true,false);
        check(carry.hold()==RifleCarry::Hold::Rear,"Rear pickup did not enter carried state");
        carry.remember_aimed_pose(rear,true);
        carry.update(slots[slot],rear,front,fronts[slot],false,true);
        check(carry.hold()==RifleCarry::Hold::Front,"Releasing rear hand dropped supported rifle");
        const auto returned=carry.update(slots[slot],rear,front,fronts[slot],false,false);
        check(carry.hold()==RifleCarry::Hold::Chest&&near(glm::vec3(returned[3]),glm::vec3(slots[slot][3])),
            "Fully released rifle failed to return to its own hip");
    }
    RiflePickupFireGate fire;
    check(!fire.update(true,true),"Held Trigger fired on initial pickup");
    check(fire.update(true,false)&&fire.update(true,true),"Fresh Trigger could not fire after neutral");
    check(!fire.update(false,true),"Support-only carry allowed a shot");
    check(!fire.update(true,true),"Rear regrab replayed held Trigger as a new shot");
    check(fire.update(true,false)&&fire.update(true,true),"Rear regrab failed to rearm after Trigger release");
    fire.reset();check(!fire.update(true,true),"Focus reset replayed Trigger");
    std::cout<<"Hip holsters: down-facing, body-relative, both hands, pickup, drop/switch, focus, reach and safe rear regrab pass\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
