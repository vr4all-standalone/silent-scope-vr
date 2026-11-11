#include <iostream>
#if !__has_include("scene/camera_command_queue.h")
int main(){std::cerr<<"Missing accepted-command camera association\n";return 1;}
#else
#include "scene/camera_command_queue.h"
#include "scene/voodoo_capture.h"
#include "scene/voodoo_hooks.h"
#include <cstdlib>
using namespace silent_scope::scene;
void require(bool ok,const char* message){if(!ok){std::cerr<<message<<'\n';std::exit(1);}}
int main(){
 WiderViewState stock{false,false,1,{512,384,443,443,256,192}},wide{true,true,3,{512,384,443./3,443./3,256,192}};
 CameraCommandQueue queue(4);auto live=stock;
 require(queue.accept(live),"Native command accepts original camera");
 live={}; // DSP starts its NEXT camera while Voodoo executes previous work.
 auto first=queue.consume();require(first.accepted&&first.camera&&first.camera->lens.fx==443,"FIFO execution retains camera from command acceptance");
 RawTriangle triangle;for(auto& v:triangle.vertex)v.q=.01;
 VoodooCapture capture;capture.clear(1,0);capture.triangle(1,triangle,&*first.camera);
 require(capture.present(1,512,384,0,0)->complete,"Next camera's incomplete builder cannot invalidate prior accepted geometry");
 queue.accept(stock);queue.accept(wide);live=stock;
 auto before=queue.consume(),after=queue.consume();
 require(before.camera&&!before.camera->applied&&after.camera&&after.camera->applied&&after.camera->factor==3,"FIFO keeps Original/Wider transition ordering despite later live camera");
 capture.clear(2,0);capture.triangle(2,triangle,&*before.camera);capture.triangle(2,triangle,&*after.camera);
 require(!capture.present(2,512,384,0,0)->complete,"An actual mixed-projection native buffer still falls back safely");
 require(!queue.consume().accepted,"Missing command metadata never fabricates a camera");
 queue.accept(std::nullopt);auto legacy=queue.consume();require(legacy.accepted&&!legacy.camera,"Explicit legacy no-provider capture remains distinct from missing metadata");
 for(int i=0;i<4;i++)require(queue.accept(stock),"Bounded queue accepts its documented capacity");
 require(!queue.accept(wide)&&queue.poisoned()&&!queue.consume().accepted,"Overflow suppresses geometry until native FIFO reset");
 queue.reset();require(!queue.poisoned()&&queue.pending()==0&&queue.accept(wide),"Native reset clears pending/stale ownership");
 auto reset=queue.consume();require(reset.camera&&reset.camera->applied,"Reset starts a fresh ordered stream");
 int device=0;set_native_camera_provider([&]{return live;},[]{});enable_capture(true);
 live=stock;capture_accept_triangle(&device);live={};
 auto accepted=capture_consume_triangle(&device);require(accepted&&accepted->lens.fx==443,"Production hooks snapshot at command acceptance before delayed execution");
 {
  CameraCommandExecution execution(accepted);auto executing=executing_command_camera();
  require(executing.accepted&&executing.camera&&executing.camera->lens.fx==443,"Raster setup uses scoped accepted stamp");
  {CameraCommandExecution nested(wide);require(executing_command_camera().camera->applied,"Nested scope installs its own accepted stamp");}
  require(executing_command_camera().camera->lens.fx==443,"Nested execution restores prior stamp");
 }
 require(!executing_command_camera().accepted,"Execution camera cannot leak to later commands");
 live={};capture_accept_triangle(&device);auto invalid=capture_consume_triangle(&device);
 require(invalid&&!invalid->lens.valid(),"Acceptance during an incomplete camera stays explicitly invalid");
 live=wide;capture_accept_triangle(&device);capture_reset_commands(&device);
 auto missing=capture_consume_triangle(&device);require(missing&&!missing->lens.valid(),"Native soft reset removes stale queued camera metadata");
 capture_reset_all_commands();clear_native_camera_provider();enable_capture(false);
 std::cout<<"Native command acceptance camera, delayed FIFO, true mixed buffers, underflow/overflow/reset passed\n";
}
#endif
