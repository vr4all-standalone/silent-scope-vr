#include <cassert>
#include <cmath>
#if __has_include("dreamcast/dc_view_runtime_probe.h")
#include "dreamcast/dc_view_runtime_probe.h"
#define HAS_VIEW_PROBE 1
#endif
int main(){
#ifndef HAS_VIEW_PROBE
 assert(false&&"Persistent view probe missing");
#else
 using namespace silent_scope::dc_view_probe;
 assert(!recipe(39.99,100).owns&&!recipe(NAN,100).owns);
 assert(recipe(57.99,100).wider&&!recipe(58,100).wider);
 assert(!recipe(59.99,100).wider&&recipe(60,100).wider);
 assert(recipe(76.99,100).wider&&!recipe(77,100).wider);
 assert(!recipe(80,100).drive);
 const double times[]{40,58,60,62,65,68,71,74,77,80};
 const double yaw[]{0,0,0,10000,-10000,0,0,0,0,0};
 const double pitch[]{0,0,0,0,0,9000,-9000,0,0,0};
 for(unsigned i=0;i<10;++i){const auto r=recipe(times[i],100);
  assert(r.owns&&!r.target.fire&&!r.target.start&&!r.target.ui_select&&r.target.direction_valid);
  const auto& d=r.target.native_direction;const double unit=65536./6.2831853071795864769;
  assert(std::abs(-std::atan2(d.x,-d.z)*unit-yaw[i])<1e-8);
  assert(std::abs(-std::atan2(d.y,std::hypot(d.x,d.z))*unit-pitch[i])<1e-8);
  assert(std::abs(std::hypot(d.x,d.y,d.z)-1)<1e-12);
 }
#endif
}
