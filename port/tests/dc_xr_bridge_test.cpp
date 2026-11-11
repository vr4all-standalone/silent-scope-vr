#include "dreamcast/dc_xr_bridge.h"
#include <cassert>
#include <cmath>
#include <limits>
using namespace silent_scope::dc_xr;
struct Receiver {Target last{};int submits{},resets{};};
void submit(void* p,const Target& v){auto& r=*static_cast<Receiver*>(p);r.last=v;++r.submits;}
void reset(void* p,uint64_t){++static_cast<Receiver*>(p)->resets;}
int main(){
 InputBridge bridge; Target t; t.focused=t.tracked=t.point_valid=true;t.hand=1;t.host_seconds=1;t.native_point={1,1,-1};
 assert(!bridge.submit(t)); // No unregistered/fake input success.
 Receiver r; bridge.set_sink({&r,submit,reset});
 t.fire=t.start=true;bridge.submit(t);assert(!r.last.fire&&!r.last.start);
 t.fire=t.start=false;t.host_seconds=2;bridge.submit(t);
 t.fire=t.start=true;t.host_seconds=3;assert(bridge.submit(t)&&r.last.fire&&r.last.start);
 const auto resets=r.resets;bridge.set_sink({&r,submit,reset});assert(r.resets==resets);
 t.host_seconds=3.1;assert(bridge.submit(t)&&r.last.fire); // Idempotent native sink query cannot disarm every frame.
 assert(std::abs(r.last.yaw_units+8192)<.001);
 assert(r.last.pitch_units<-6000&&r.last.pitch_units>-7000); // Native Y down is negative pitch.
 auto epoch=r.last.epoch;
 t.menu=true;t.host_seconds=4;bridge.submit(t);assert(!r.last.fire&&!r.last.start&&r.last.epoch>epoch);
 t.menu=false;t.host_seconds=5;bridge.submit(t);assert(!r.last.fire&&!r.last.start);
 t.fire=t.start=false;t.host_seconds=6;bridge.submit(t);
 t.hand=0;t.fire=true;t.host_seconds=7;bridge.submit(t);assert(!r.last.fire);
 t.fire=false;t.host_seconds=8;bridge.submit(t);
 t.fire=true;t.host_seconds=9;bridge.submit(t);assert(r.last.fire);
 t.tracked=false;t.host_seconds=10;bridge.submit(t);assert(!r.last.fire);
 t.tracked=true;t.host_seconds=11;bridge.submit(t);assert(!r.last.fire);
 t.point_valid=false;t.fire=false;t.start=false;t.host_seconds=12;bridge.submit(t);
 t.start=true;t.host_seconds=13;bridge.submit(t);assert(r.last.start&&!r.last.fire);
 t.point_valid=true;t.native_point.x=std::numeric_limits<float>::quiet_NaN();t.host_seconds=14;bridge.submit(t);assert(!r.last.point_valid&&!r.last.fire);
 t.native_point={0,0,-2};t.point_valid=true;t.native_ui=true;t.ui_select=true;t.fire=false;t.start=false;t.host_seconds=15;bridge.submit(t);assert(!r.last.ui_select);
 t.ui_select=false;t.host_seconds=16;bridge.submit(t);t.ui_select=true;t.host_seconds=17;bridge.submit(t);assert(r.last.ui_select&&!r.last.fire);
 t.native_ui=false;t.ui_select=false;t.fire=true;t.host_seconds=18;bridge.submit(t);assert(!r.last.fire);
 t.fire=false;t.host_seconds=19;bridge.submit(t);
 t.native_point={0,-1,-1};t.fire=true;t.host_seconds=20;bridge.submit(t);assert(r.last.fire&&std::abs(r.last.pitch_units-8192)<.001);
 t.native_point={0,1,-1};t.host_seconds=21;bridge.submit(t);assert(r.last.fire&&std::abs(r.last.pitch_units+8192)<.001);
 // Empty sky still supplies a finite native direction and permits a real miss.
 t.point_valid=false;t.direction_valid=true;t.native_direction={0,-std::sqrt(.5),-std::sqrt(.5)};
 t.host_seconds=22;bridge.submit(t);assert(r.last.fire&&!r.last.point_valid&&r.last.direction_valid&&std::abs(r.last.pitch_units-8192)<.001);
 t.native_direction.y=std::sqrt(.5);t.host_seconds=23;bridge.submit(t);assert(r.last.fire&&std::abs(r.last.pitch_units+8192)<.001);
 // Prefer the actual scene point over the camera's parallel barrel direction.
 t.point_valid=true;t.native_point={1,0,-1};t.host_seconds=24;bridge.submit(t);assert(r.last.fire&&std::abs(r.last.yaw_units+8192)<.001);
 t.point_valid=false;t.native_direction={0,0,0};t.host_seconds=25;bridge.submit(t);assert(!r.last.direction_valid&&!r.last.fire);
 t.native_direction={0,0,-2};t.host_seconds=26;bridge.submit(t);assert(!r.last.direction_valid&&!r.last.fire);
 t.native_direction={0,0,std::numeric_limits<double>::quiet_NaN()};t.host_seconds=27;bridge.submit(t);assert(!r.last.direction_valid&&!r.last.fire);
 t.native_direction={0,0,-1};t.native_ui=true;t.ui_select=true;t.host_seconds=28;bridge.submit(t);assert(!r.last.fire&&!r.last.ui_select);
 t.ui_select=false;t.fire=false;t.host_seconds=29;bridge.submit(t);t.ui_select=true;t.host_seconds=30;bridge.submit(t);assert(!r.last.ui_select); // UI needs a real panel hit.
 bridge.invalidate();assert(r.resets>=4);
 FrameRates rates;rates.sample(0,1,1);rates.sample(.5,1,1);rates.sample(1,1,2);
 assert(rates.xr_hz()==2&&rates.native_hz()==1);
 rates.sample(1.5,2,1);assert(rates.xr_hz()==0&&rates.native_hz()==0);
}
