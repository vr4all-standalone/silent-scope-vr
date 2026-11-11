#include "settings.h"
#include <sstream>
#include <iostream>
using namespace silent_scope;
#define REQUIRE(x) do { if(!(x)){std::cerr << "FAIL line " << __LINE__ << ": " #x "\n";return 1;} } while(false)
int main() {
    Settings initial;initial.active_hand=0;initial.panel_distance=2.2;initial.gun_angle=7;
    initial.calibration.yaw_half_range=50;initial.calibration.pitch_half_range=25;
    std::stringstream saved;initial.save(saved);auto loaded=Settings::load(saved);
    REQUIRE(loaded.active_hand==0 && loaded.panel_distance==2.2 && loaded.gun_angle==7);
    REQUIRE(loaded.calibration.yaw_half_range==50 && loaded.calibration.pitch_half_range==25);
    std::stringstream invalid("hand=9\npanel_distance=nan\ngun_angle=99\nyaw_half_range=0\npitch_half_range=inf\nunknown=1\n");
    loaded=Settings::load(invalid);Settings defaults;
    REQUIRE(loaded.active_hand==defaults.active_hand);
    REQUIRE(loaded.panel_distance==defaults.panel_distance && loaded.gun_angle==defaults.gun_angle);
    REQUIRE(loaded.calibration.yaw_half_range==defaults.calibration.yaw_half_range);
    REQUIRE(loaded.calibration.pitch_half_range==defaults.calibration.pitch_half_range);
    std::stringstream malformed("hand=0junk\npanel_distance=2junk\ngun_angle=\n");
    loaded=Settings::load(malformed);REQUIRE(loaded.active_hand==defaults.active_hand);
    REQUIRE(loaded.panel_distance==defaults.panel_distance);
    initial.reset();REQUIRE(initial.active_hand==defaults.active_hand && initial.gun_angle==0);
    std::stringstream reset_saved;initial.save(reset_saved);loaded=Settings::load(reset_saved);
    REQUIRE(loaded.panel_distance==defaults.panel_distance);
    REQUIRE(reset_saved.str().find("bolt")==std::string::npos);
    std::cout << "Settings round trip, invalid input defaults and reset passed; bolt option deferred\n";
}
