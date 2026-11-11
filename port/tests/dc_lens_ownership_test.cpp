#include "dreamcast/dc_lens_ownership.h"
#include <cassert>
using namespace silent_scope::dreamcast;
static SubmittedLens lens(float factor,uint64_t epoch=1){SubmittedLens v;v.valid=v.supported=true;v.generation=7;v.request_revision=epoch;
    v.fx=v.fy=555/factor;v.cx=320;v.cy=240;v.width=640;v.height=480;v.factor=factor;return v;}
int main(){
    ProjectionRangeLedger<4> ledger;ledger.reset(7);
    auto stock=lens(1),wide=lens(3,2);
    assert(!ledger.lookup(0x100,32).valid);
    assert(ledger.write(0x100,64,stock));assert(ledger.lookup(0x100,64).fx==555);
    assert(ledger.write(0x140,64,stock));assert(ledger.ranges()==1); // adjacent equal ownership coalesces
    assert(ledger.lookup(0x110,96).valid);assert(!ledger.lookup(0xff,32).valid);
    assert(ledger.write(0x120,32,wide));assert(ledger.ranges()==3);
    assert(ledger.lookup(0x120,32).fx==185);assert(!ledger.lookup(0x100,128).valid); // mixed lenses
    assert(ledger.write(0x120,32,{}));assert(!ledger.lookup(0x120,1).valid); // unknown overwrite erases ownership
    assert(ledger.lookup(0x100,32).valid&&ledger.lookup(0x140,64).valid);
    assert(!ledger.lookup(UINT32_MAX-3,8).valid);
    assert(!ledger.write(UINT32_MAX-3,8,stock));
    auto wrong_generation=stock;wrong_generation.generation=8;
    assert(!ledger.write(0x100,32,wrong_generation));
    assert(!ledger.lookup(0x100,32).valid); // a genuine unsupported overwrite supersedes old ownership
    ledger.reset(8);assert(!ledger.lookup(0x100,32).valid);assert(!ledger.write(0x100,32,stock));
    ledger.reset(7);
    for(unsigned n=0;n<4;++n)assert(ledger.write(n*64,32,stock));
    assert(!ledger.write(4*64,32,stock)&&ledger.overflowed());assert(!ledger.lookup(0,32).valid);
    assert(!ledger.write(0,32,stock));ledger.reset(7);assert(!ledger.overflowed());
    auto receipt=publish_owned_lens(wide,7,90,640,480,false,true);
    assert(receipt.valid&&receipt.fx==185&&receipt.submit_sequence==90&&receipt.request_revision==2);
    assert(!publish_owned_lens(wide,7,90,640,480,false,false).valid);
    assert(!publish_owned_lens(wide,8,90,640,480,false,true).valid);
    assert(!publish_owned_lens(wide,7,90,640,480,true,true).valid);
    assert(!publish_owned_lens(wide,7,90,512,384,false,true).valid);
    // No live SDK argument: complete old555/185 ownership is retained at toggle.
    assert(publish_owned_lens(stock,7,91,640,480,false,true).fx==555);
    NativeLensAccumulator frame;
    auto triangle=[&](const SubmittedLens& a,const SubmittedLens& b,const SubmittedLens& c){
        frame.vertex(10,10,.01f,a,false);frame.vertex(20,10,.02f,b,false);frame.vertex(10,20,.03f,c,true);
    };
    for(unsigned n=0;n<4;++n)triangle(wide,wide,wide);
    assert(frame.owned().valid&&frame.owned().fx==185);
    frame.reset();for(unsigned n=0;n<4;++n){frame.vertex(0,0,.7f,{},false);frame.vertex(5,0,.7f,{},false);frame.vertex(0,5,.7f,{},true,true);}
    assert(!frame.owned().valid&&!frame.rejected()); // explicitly proven native HUD domain
    frame.reset();frame.vertex(0,0,.7f,{},false);frame.vertex(5,0,.7f,{},false);frame.vertex(0,5,.7f,{},true);
    assert(frame.rejected()); // unknown constant-q can also be a real world plane
    frame.reset();for(unsigned n=0;n<4;++n)triangle(wide,wide,wide);
    frame.vertex(0,0,.01f,stock,false);frame.vertex(5,0,.01f,stock,false);frame.vertex(0,5,.01f,stock,true);
    assert(frame.rejected()&&!frame.owned().valid); // known555 flat world mixed with185 sloped world
    frame.reset();
    for(unsigned n=0;n<4;++n)triangle(stock,stock,stock);
    assert(frame.owned().fx==555);
    triangle(stock,{},stock);assert(!frame.owned().valid&&frame.rejected());
    frame.reset();for(unsigned n=0;n<4;++n)triangle(stock,stock,stock);
    triangle(wide,wide,wide);assert(!frame.owned().valid&&frame.rejected());
    ledger.reset(7);assert(ledger.append(0,32,stock));assert(ledger.append(32,32,stock));assert(ledger.ranges()==1);
    assert(ledger.append(64,32,{}));assert(ledger.append(96,32,stock));assert(ledger.ranges()==2);
    assert(!ledger.lookup(0,128).valid&&ledger.lookup(96,32).valid);
    assert(ledger.append(0,32,{}));assert(!ledger.lookup(0,32).valid); // nonmonotonic overwrite still erases
}
