#include "dreamcast/dc_menu_stick.h"
#include <cassert>
#include <limits>
int main(){using silent_scope::dc_mouse::MenuStickDirection;MenuStickDirection stick;
    assert(stick.update(.8f,true,1)==0); // held through native UI entry needs neutral
    assert(stick.update(0,true,1)==0);assert(stick.update(.59f,true,1)==0);
    assert(stick.update(.61f,true,1)==-1);assert(stick.update(.45f,true,1)==-1);
    assert(stick.update(.3f,true,1)==0);assert(stick.update(-.61f,true,1)==1);
    assert(stick.update(.8f,true,0)==0);assert(stick.update(.4f,true,0)==0);
    assert(stick.update(0,true,0)==0);assert(stick.update(.8f,true,0)==-1);
    assert(stick.update(.8f,false,0)==0);assert(stick.update(.8f,true,0)==0);
    assert(stick.update(0,true,0)==0);assert(stick.update(-.8f,true,0)==1);
    assert(stick.update(std::numeric_limits<float>::quiet_NaN(),true,0)==0);
    assert(stick.update(-.8f,true,0)==0);assert(stick.update(0,true,0)==0);
    assert(stick.update(-.8f,true,0)==1);
    for(int unavailable=0;unavailable<3;++unavailable){
        stick.reset();assert(stick.update_sample({true,true,0,0},true,1)==0);
        silent_scope::dc_mouse::MenuStickSample sample{true,true,0,0};
        if(unavailable==0)sample.queried=false;if(unavailable==1)sample.active=false;
        if(unavailable==2)sample.y=std::numeric_limits<float>::quiet_NaN();
        assert(stick.update_sample(sample,true,1)==0);
        assert(stick.update_sample({true,true,0,.8f},true,1)==0);
        assert(stick.update_sample({true,true,0,0},true,1)==0);
        assert(stick.update_sample({true,true,0,.8f},true,1)==-1);
    }
}
