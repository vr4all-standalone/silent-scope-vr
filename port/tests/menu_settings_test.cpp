#include "settings.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#if __has_include("settings_io.h")
#include "settings_io.h"
#define HAS_SETTINGS_IO 1
#endif
using namespace silent_scope;
#define REQUIRE(x) do { if(!(x)){std::cerr << "FAIL line " << __LINE__ << ": " #x "\n";return 1;} } while(false)
int main() {
    Settings s;s.calibration.yaw_center=12;s.calibration.pitch_center=-7;
    s.calibration.yaw_half_range=50;s.calibration.pitch_half_range=25;s.reset();
    REQUIRE(s.calibration.yaw_center==12 && s.calibration.pitch_center==-7);
    REQUIRE(s.calibration.yaw_half_range==50 && s.calibration.pitch_half_range==25);
#ifndef HAS_SETTINGS_IO
    REQUIRE(false && "atomic menu settings persistence has not been implemented");
#else
    REQUIRE(s.wider_view && s.laser_on && !s.fps_counter);
    REQUIRE(s.reticle==Reticle::Custom && s.haptics==Haptics::Medium);
    REQUIRE(s.reticle_min_size==.25 && s.reticle_max_size==1 && s.reticle_shrink_distance==6);
    REQUIRE(s.world_size==.1 && s.hud_size==1 && s.panel_distance==1.8);
    std::stringstream legacy("hand=0\ngun_angle=-8\npanel_distance=3\nyaw_center=12\npitch_center=-7\nyaw_half_range=50\npitch_half_range=25\n");
    auto migrated=Settings::load(legacy);
    REQUIRE(migrated.active_hand==0 && migrated.gun_angle==-8 && migrated.panel_distance==3);
    REQUIRE(migrated.calibration.pitch_center==-7 && migrated.wider_view);
    s.active_hand=0;s.gun_angle=27;s.panel_distance=10;s.world_size=.375;s.hud_size=1.7;
    s.reticle=Reticle::None;s.reticle_min_size=.37;s.reticle_max_size=2.35;s.reticle_shrink_distance=9.3;
    s.haptics=Haptics::Strong;s.wider_view=false;s.laser_on=false;s.fps_counter=true;
    const auto dir=std::filesystem::temp_directory_path()/"silent-scope-menu-settings-test";
    std::filesystem::create_directories(dir);const auto file=dir/"settings.ini";
    std::string error;REQUIRE(save_settings_atomic(file,s,&error));REQUIRE(error.empty());
    auto saved=load_settings_file(file);
    REQUIRE(saved.active_hand==0 && saved.panel_distance==10 && saved.gun_angle==27);
    REQUIRE(saved.world_size==.375 && saved.hud_size==1.7 && !saved.wider_view);
    REQUIRE(saved.reticle==Reticle::None && saved.reticle_min_size==.37 && saved.reticle_max_size==2.35);
    REQUIRE(saved.reticle_shrink_distance==9.3 && saved.haptics==Haptics::Strong && saved.fps_counter && !saved.laser_on);
    REQUIRE(saved.calibration.pitch_center==-7);
    s.laser_on=true;REQUIRE(save_settings_atomic(file,s));REQUIRE(load_settings_file(file).laser_on);
    const auto original_size=std::filesystem::file_size(file);
    REQUIRE(!save_settings_atomic(file/"invalid",s,&error));REQUIRE(!error.empty());
    REQUIRE(std::filesystem::file_size(file)==original_size && load_settings_file(file).reticle==Reticle::None);
    REQUIRE(!save_settings_atomic(dir,s,&error) && std::filesystem::is_directory(dir));
    for(const auto& entry:std::filesystem::directory_iterator(dir))REQUIRE(entry.path()==file);
    saved.reset();REQUIRE(save_settings_atomic(file,saved));saved=load_settings_file(file);
    REQUIRE(saved.world_size==.1 && saved.hud_size==1 && saved.calibration.pitch_center==-7 && saved.wider_view);
    std::stringstream invalid("hand=.5\npanel_distance=10.01\nworld_size=0.009\nhud_size=2.01\nreticle_min_size=.049\nreticle_max_size=3.01\nreticle_shrink_distance=nan\nhaptics=1.5\nreticle=2\nwider_view=2\nlaser_on=-1\nfps_counter=inf\ngun_angle=31\n");
    saved=Settings::load(invalid);
    REQUIRE(saved.active_hand==1 && saved.panel_distance==1.8 && saved.world_size==.1 && saved.hud_size==1);
    REQUIRE(saved.reticle_min_size==.25 && saved.reticle_max_size==1 && saved.reticle_shrink_distance==6);
    REQUIRE(saved.haptics==Haptics::Medium && saved.reticle==Reticle::Custom && saved.wider_view && saved.laser_on && !saved.fps_counter && saved.gun_angle==0);
    std::stringstream bounds("world_size=.01\nhud_size=.5\nreticle_min_size=.05\nreticle_max_size=.25\nreticle_shrink_distance=.2\npanel_distance=.5\n");
    saved=Settings::load(bounds);REQUIRE(saved.world_size==.01 && saved.hud_size==.5 && saved.reticle_min_size==.05 && saved.reticle_max_size==.25 && saved.reticle_shrink_distance==.2 && saved.panel_distance==.5);
    std::stringstream upper("world_size=1\nhud_size=2\nreticle_min_size=1\nreticle_max_size=3\nreticle_shrink_distance=20\npanel_distance=10\nwider_view=0\n");
    saved=Settings::load(upper);REQUIRE(saved.world_size==1 && saved.hud_size==2 && saved.reticle_min_size==1 && saved.reticle_max_size==3 && saved.reticle_shrink_distance==20 && saved.panel_distance==10 && !saved.wider_view);
    std::stringstream junk("world_size=.2junk\nhud_size=NaN\nlaser_on=0junk\nwider_view=0 #comment\n");
    saved=Settings::load(junk);REQUIRE(saved.world_size==.1 && saved.hud_size==1 && saved.laser_on && saved.wider_view);
    std::filesystem::remove(file);std::filesystem::remove(dir);
#endif
    std::cout<<"Menu settings migration, bounds, reset and atomic persistence passed\n";
}
