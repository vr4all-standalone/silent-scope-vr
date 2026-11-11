#include <filesystem>
#include <fstream>
#include <chrono>
#include <cassert>
#include <iostream>
#include "atomic_file_io.h"
#include "dreamcast/dc_persistence.h"
int main(){
    namespace fs=std::filesystem;
    const auto dir=fs::absolute("private")/("atomic-native-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(dir);const auto path=dir/"emu.cfg";
    const auto read=[](const fs::path& p){std::ifstream in(p,std::ios::binary);return std::string((std::istreambuf_iterator<char>(in)),{});};
    const std::string original="[config]\nNativeSave = preserved\nvr.PrimaryHand = right\n";
    std::string error;assert(silent_scope::write_file_atomic(path,original,&error)&&error.empty());
    assert(read(path)==original);
    const auto blocked=dir/"blocked";fs::create_directory(blocked);
    assert(!silent_scope::dc_persistence::save_config(blocked.string(),"replacement"));
    assert(!silent_scope::dc_persistence::last_error().empty()&&fs::is_directory(blocked)&&read(path)==original);
    const std::string replacement="[config]\nNativeSave = preserved\nvr.PrimaryHand = left\nvr.FpsCounter = yes\n";
    assert(silent_scope::dc_persistence::save_config(path.string(),replacement));
    assert(silent_scope::dc_persistence::last_error().empty()&&read(path)==replacement);
    assert(!silent_scope::write_file_atomic(path/"child.cfg","bad",&error)&&!error.empty()&&read(path)==replacement);
    for(const auto& entry:fs::directory_iterator(dir))assert(entry.path().filename().string().find(".tmp.")==std::string::npos);
    std::cout<<"Atomic native settings: complete replacement, preservation on failure, cleanup and error acknowledgement passed\n";
}
