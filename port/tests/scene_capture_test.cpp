#include "scene/voodoo_hooks.h"
#include <iostream>
using namespace silent_scope::scene;
int main(){
    enable_capture(true);if(!capture_device(":voodoo0")||capture_device(":voodoo1")||capture_device(":unknown"))return 1;
    auto& capture=voodoo_capture();capture.reset();
    RawTriangle near;near.vertex[0].q=near.vertex[1].q=near.vertex[2].q=.5;
    capture.clear(10,0xff000000);capture.triangle(10,near);auto first=capture.present(10,512,384,0,0);
    if(first->triangles.size()!=1||first->triangles[0].vertex[0].q!=.5)return 1;
    near.vertex[0].q=.25;capture.triangle(10,near);auto second=capture.present(10,512,384,0,0);
    if(first->triangles[0].vertex[0].q!=.5||second->triangles.size()!=1)return 1;
    capture.reset();if(capture.acquire())return 1;
    enable_capture(false);if(capture_device(":voodoo0"))return 1;
    std::cout<<"Main-board ownership, immutable native geometry and reset passed; scope board excluded\n";
}
