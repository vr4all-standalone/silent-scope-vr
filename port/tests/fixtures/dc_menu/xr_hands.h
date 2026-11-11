#pragma once
#include <string>
#include <vector>
namespace vr::xr {
struct GunChoice{int setting;const char* file;const char* label;bool made,complete;};
constexpr int OwnGuns=4;
inline std::vector<GunChoice> gunChoices(){return {{1,nullptr,"Other game gun",true,true}};}
inline void chosenGun(){}
}
