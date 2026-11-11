#include "cabinet_controls.h"
#include <iostream>
using namespace silent_scope;
#define REQUIRE(x) do { if(!(x)){std::cerr << "FAIL line " << __LINE__ << ": " #x "\n";return 1;} } while(false)
int main(){
    CabinetControls policy;CabinetButtons buttons;
    policy.update(0,true,buttons);
    buttons.menu=true;REQUIRE(policy.update(.1,true,buttons).menu_open);
    buttons.menu=false;policy.update(.2,true,buttons);
    buttons.start=true;auto resume=policy.update(.3,true,buttons);
    REQUIRE(resume.resumed && !resume.menu_open && !resume.coin && !resume.start);
    REQUIRE(!policy.update(.4,true,buttons).coin);
    buttons.start=false;policy.update(.5,true,buttons);
    buttons.start=true;REQUIRE(policy.update(.6,true,buttons).coin);
    REQUIRE(!policy.update(.75,true,buttons).coin);
    REQUIRE(policy.update(.81,true,buttons).coin);
    REQUIRE(policy.update(1.02,true,buttons).coin);
    REQUIRE(policy.update(1.32,true,buttons).start);
    REQUIRE(!policy.update(1.51,true,buttons).start);
    policy.update(1.6,false,buttons);REQUIRE(!policy.update(1.7,true,buttons).coin);
    buttons.start=false;policy.update(1.8,true,buttons);
    buttons.start=true;REQUIRE(policy.update(1.9,true,buttons).coin);
    REQUIRE(!policy.update(1.95,false,buttons).coin);
    REQUIRE(!policy.update(2,true,buttons).coin);
    std::cout<<"Resume consumes A/X; credit/start pulses and interruption cancellation passed\n";
}
