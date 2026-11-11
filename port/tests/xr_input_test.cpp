#include "xr_input.h"
#include <iostream>
using namespace silent_scope;
#define REQUIRE(x) do { if(!(x)){std::cerr << "FAIL line " << __LINE__ << ": " #x "\n";return 1;} } while(false)
int main(){
    auto valid=XR_SPACE_LOCATION_POSITION_VALID_BIT|XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
    auto tracked=valid|XR_SPACE_LOCATION_POSITION_TRACKED_BIT|XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT;
    REQUIRE(!tracked_hand(valid,{0,0,-1},true).tracked);
    REQUIRE(tracked_hand(tracked,{0,0,-1},true).tracked);
    REQUIRE(!tracked_hand(tracked,{0,0,0},true).tracked);
    REQUIRE(tracked_hand(tracked,{1,0,-1},false).yaw_degrees>0);
    REQUIRE(tracked_hand(tracked,{-1,0,-1},false).yaw_degrees<0);
    RifleInput policy;Calibration calibration;InputContext context;
    HandSample unused;
    policy.update(unused,tracked_hand(tracked,{0,0,-1},false),context,calibration);
    REQUIRE(policy.update(unused,tracked_hand(tracked,{0,0,-1},true),context,calibration).trigger);
    REQUIRE(!policy.update(unused,tracked_hand(valid,{0,0,-1},true),context,calibration).trigger);
    REQUIRE(!policy.update(unused,tracked_hand(tracked,{0,0,-1},true),context,calibration).trigger);
    REQUIRE(!view_tracking_good(XR_VIEW_STATE_POSITION_VALID_BIT|XR_VIEW_STATE_ORIENTATION_VALID_BIT));
    REQUIRE(view_tracking_good(XR_VIEW_STATE_POSITION_VALID_BIT|XR_VIEW_STATE_ORIENTATION_VALID_BIT|XR_VIEW_STATE_POSITION_TRACKED_BIT|XR_VIEW_STATE_ORIENTATION_TRACKED_BIT));
    std::cout<<"OpenXR valid-but-untracked poses disarm and require a fresh trigger; yaw direction passed\n";
}
