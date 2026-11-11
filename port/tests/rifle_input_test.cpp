#include "rifle_input.h"
#include <cmath>
#include <iostream>
using namespace silent_scope;
#define REQUIRE(x) do { if(!(x)){std::cerr << "FAIL line " << __LINE__ << ": " #x "\n";return 1;} } while(false)
int main() {
    RifleInput policy;Calibration calibration;InputContext context;
    HandSample left{true,0,0,false},right{true,0,0,false};
    auto center=policy.update(left,right,context,calibration);
    REQUIRE(center.yaw==1024 && center.pitch==1024 && !center.trigger);
    right.trigger=true;REQUIRE(policy.update(left,right,context,calibration).trigger);
    context.focused=false;REQUIRE(!policy.update(left,right,context,calibration).trigger);
    context.focused=true;REQUIRE(!policy.update(left,right,context,calibration).trigger);
    right.trigger=false;policy.update(left,right,context,calibration);
    right.trigger=true;REQUIRE(policy.update(left,right,context,calibration).trigger);
    context.menu=true;REQUIRE(!policy.update(left,right,context,calibration).trigger);
    context.menu=false;REQUIRE(!policy.update(left,right,context,calibration).trigger);
    context.active_hand=0;left.trigger=true;
    REQUIRE(!policy.update(left,right,context,calibration).trigger);
    left.trigger=false;policy.update(left,right,context,calibration);left.trigger=true;
    REQUIRE(policy.update(left,right,context,calibration).trigger);
    left.yaw_degrees=-calibration.yaw_half_range;left.pitch_degrees=calibration.pitch_half_range;
    auto limits=policy.update(left,right,context,calibration);
    REQUIRE(limits.yaw==0 && limits.pitch==2047);
    left.yaw_degrees=90;left.pitch_degrees=-80;
    limits=policy.update(left,right,context,calibration);
    REQUIRE(limits.yaw==2047 && limits.pitch==0);
    left.tracked=false;auto invalid=policy.update(left,right,context,calibration);
    REQUIRE(!invalid.trigger && invalid.yaw==1024 && invalid.pitch==1024);
    left.tracked=true;left.yaw_degrees=NAN;
    REQUIRE(!policy.update(left,right,context,calibration).trigger);
    left.yaw_degrees=0;calibration.yaw_half_range=0;
    REQUIRE(!policy.update(left,right,context,calibration).trigger);
    calibration=Calibration{};context.active_hand=99;
    REQUIRE(!policy.update(left,right,context,calibration).trigger);
    context.active_hand=0;left={true,-179,0,false};calibration.yaw_center=179;
    REQUIRE(policy.update(left,right,context,calibration).yaw>1024);
    left.yaw_degrees=179;calibration.yaw_center=-179;
    REQUIRE(policy.update(left,right,context,calibration).yaw<1024);
    std::cout << "Native potentiometer mapping and fresh-input hand/focus/menu guards passed\n";
}
