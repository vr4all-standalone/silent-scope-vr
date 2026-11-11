#include <iostream>
#if !__has_include("scene/native_camera.h")
int main(){std::cerr<<"Missing immutable native camera capture\n";return 1;}
#else
#include "scene/native_camera.h"
#include "scene/voodoo_capture.h"
#include "scene/frame_composition.h"
#include <cstdlib>
using namespace silent_scope::scene;
void require(bool ok,const char* why){if(!ok){std::cerr<<why<<'\n';std::exit(1);}}
int main(){
 WiderViewState stock{false,false,1,{512,384,443,443,256,192}},wide{true,true,2,{512,384,221.5,221.5,256,192}};
 auto current=stock;int polls=0;std::vector<std::string> issues;set_native_camera_provider([&]{return current;},[&]{++polls;},[&](const char* reason){issues.emplace_back(reason);});
 require(native_camera_state()->lens.fx==443,"Provider exposes actual source lens");poll_native_camera();require(polls==1,"Paused host can reach policy restore");
 RawTriangle triangle;for(auto& v:triangle.vertex)v.q=.01;
 VoodooCapture capture;capture.clear(10,0);capture.triangle(10,triangle,&stock);auto first=capture.present(10,512,384,0,0);
 require(first->complete&&first->lens.fx==443&&!first->wider_applied,"First native triangle owns buffer camera");
 current=wide;auto composed=compose_frame(first,wide.lens,512,384,{},{});
 require(composed&&composed->lens.fx==443&&!composed->wider_applied,"Live camera must not relabel previous native swap");
 capture.clear(11,0);capture.triangle(11,triangle,&wide);auto second=capture.present(11,512,384,0,0);
 require(second->complete&&second->lens.fx==221.5&&second->wider_applied&&second->wider_factor==2,"Wide lens and applied state travel with buffer");
 require(first->lens.fx==443&&!first->wider_applied,"Later settings cannot mutate old immutable capture");
 capture.clear(12,0);capture.triangle(12,triangle,&stock);capture.triangle(12,triangle,&wide);auto mixed=capture.present(12,512,384,0,0);
 require(!mixed->complete&&!compose_frame(mixed,wide.lens,512,384,{},{}),"Mixed projection buffer falls back to native raster");
 WiderViewState invalid{true,false,1,{}};capture.clear(13,0);capture.triangle(13,triangle,&invalid);
 require(!capture.present(13,512,384,0,0)->complete,"Unrecognized lens cannot leak into geometric output");
 require(issues==std::vector<std::string>{"capture_mixed","capture_invalid"},"Only first invalidation per buffer reports its precise diagnostic reason");
 clear_native_camera_provider();require(!native_camera_state(),"Exit removes device lifetime callback");poll_native_camera();
 std::cout<<"Immutable camera per native buffer, toggle/mixed fallback and paused restore callback passed\n";
}
#endif
