#include "dreamcast/dc_scope_composite.h"
#include "dreamcast/dc_scope_hud.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
using namespace silent_scope::dc_scope;
struct V {float x{},y{},z{},u{},v{};uint8_t col[4]{};};
struct P {uint32_t first{},count{};struct W{uint32_t full{};}isp,tsp,tcw,pcw;uint32_t tileclip{};};
struct Pass {bool autosort{true};uint32_t tr_count{},sorted_tr_count{1};};
struct Sorted {uint32_t polyIndex{},first{},count{};};
struct Ctx {
    bool isRTT{};unsigned framebufferWidth{640},framebufferHeight{480};
    struct {uint64_t generation{3},submit_sequence{11};bool supported{true};}dcCamera;
    std::vector<V> verts;std::vector<uint32_t> idx;
    std::vector<P> global_param_op,global_param_pt,global_param_tr;
    std::vector<Pass> render_passes;std::vector<Sorted> sortedTriangles{{0,0,0}};
};
void append(Ctx& ctx,const std::vector<V>& vertices,bool rim){
    P p;p.first=uint32_t(ctx.verts.size());p.count=uint32_t(vertices.size());
    p.isp.full=0xc8800000;p.tsp.full=rim?0x9491a4db:0x9491a4ed;
    p.tcw.full=rim?0x5004c800:0x0c0a5800;p.pcw.full=0x828c002a;p.tileclip=0x100200;
    ctx.verts.insert(ctx.verts.end(),vertices.begin(),vertices.end());ctx.global_param_tr.push_back(p);
}
Ctx synthetic(float cx=320,float cy=240){
    Ctx ctx;const auto point=[&](int i){
        const double angle=i*6.2831853071795864769/64;
        return V{float(cx+120.5890197753906*std::cos(angle)),float(cy+112.6999816894531*std::sin(angle)),.625f,
            float(std::clamp(.5+.5*std::cos(angle),1./256,255./256)),
            float(std::clamp(127./256+.5*std::sin(angle),0.,254./256)),{254,254,254,255}};
    };
    const V center{cx,cy,.625f,.5f,127.f/256,{254,254,254,255}};
    append(ctx,{point(0),point(1),center},false);
    for(int i=0;i<21;++i){int a=1+i*3;append(ctx,{point(a),point(a+1),center,point(a+2),point(a+3)},false);}
    const int counts[]{8,9,9,10};int quadrant=0;
    for(int sx:{-1,1})for(int sy:{-1,1}){
        std::vector<V> rim;
        const float xy[][2]{{123.05f,0},{116,6},{123.05f,57.5f},{80,74},{61.525f,115},{7.21f,107.18f},{0,115},{-.17f,107.18f},{7.21f,107.18f},{61.525f,115}};
        for(int i=0;i<counts[quadrant];++i)rim.push_back({cx+sx*xy[i][0],cy+sy*xy[i][1],.6289308071136475f,.5f,.5f,{176,176,176,255}});
        ++quadrant;append(ctx,rim,true);
    }
    ctx.render_passes.push_back({true,uint32_t(ctx.global_param_tr.size()),1});return ctx;
}
TextureView image(){return {12,256,256,1,10,3,1,77,10,true,true};}
bool configured(CompositeMask& mask,const Ctx& ctx,bool vr=true,TextureView view=image(),double now=10.03){
    return mask.configure("T9507N",ctx,vr,view,77,now);
}
void check(Ctx ctx){
    CompositeMask mask;assert(configured(mask,ctx));
    {silent_scope::dc_scope_hud::ScopedDraw fallback("T9507N",ctx,true);
        const auto& p=ctx.global_param_tr[0];const auto gate=silent_scope::dc_hud::frame_gate("T9507N",ctx,true,2);
        const auto decision=silent_scope::dc_scope_hud::classify(gate,ctx,p,0,p.first,p.first+1,p.first+2);
        assert(decision.layer==silent_scope::dc_hud::Layer::ScopeFallback);
        {silent_scope::dc_scope_hud::ScopedDraw original("T9507N",ctx,false);
            assert(silent_scope::dc_scope_hud::classify(gate,ctx,p,0,p.first,p.first+1,p.first+2).layer==silent_scope::dc_hud::Layer::World);}
        assert(silent_scope::dc_scope_hud::classify(gate,ctx,p,0,p.first,p.first+1,p.first+2).layer==silent_scope::dc_hud::Layer::ScopeFallback);
    }
    for(std::size_t i=0;i<ctx.global_param_tr.size();++i)assert(mask.selected(ctx,i));
    assert(!configured(mask,ctx,false));assert(!mask.selected(ctx,0)); // Original/menu/native draw
    assert(!mask.configure("T9505D",ctx,true,image(),77,10.03));
    auto invalid=image();invalid.texture=0;assert(!configured(mask,ctx,true,invalid));
    invalid=image();++invalid.native_generation;assert(!configured(mask,ctx,true,invalid));
    invalid=image();invalid.content_top_at_v0=false;assert(!configured(mask,ctx,true,invalid));
    invalid=image();invalid.submit_sequence=12;assert(!configured(mask,ctx,true,invalid));
    assert(!configured(mask,ctx,true,image(),10.3));
    assert(configured(mask,ctx));++ctx.dcCamera.submit_sequence;assert(!mask.selected(ctx,0));
    --ctx.dcCamera.submit_sequence;ctx.isRTT=true;assert(!configured(mask,ctx));ctx.isRTT=false;
    auto damaged=ctx;damaged.verts[damaged.global_param_tr.front().first].z*=.999f;assert(!configured(mask,damaged));
    damaged=ctx;damaged.global_param_tr.back().tsp.full^=1;assert(!configured(mask,damaged));
    assert(configured(mask,ctx));ctx.global_param_tr.pop_back();ctx.render_passes[0].tr_count--;assert(!configured(mask,ctx));
}
uint32_t u32(std::istream& stream){uint32_t n{};stream.read(reinterpret_cast<char*>(&n),4);assert(stream);return n;}
int main(int argc,char** argv){
    static_assert(sizeof(V)==24);
    assert(!rifle_draw_authorized());{ScopedRifleDrawAuthorization yes(true);assert(rifle_draw_authorized());
        {ScopedRifleDrawAuthorization no(false);assert(!rifle_draw_authorized());}assert(rifle_draw_authorized());}
    assert(!rifle_draw_authorized());
    check(synthetic());check(synthetic(324.2569f,244.2569f));
    CompositeMask mask;auto ctx=synthetic();
    auto broken=ctx;broken.verts[0].z=.624f;assert(!configured(mask,broken));
    broken=ctx;broken.verts[0].col[0]=253;assert(!configured(mask,broken));
    broken=ctx;broken.verts[0].u=NAN;assert(!configured(mask,broken));
    broken=ctx;broken.global_param_tr[0].tcw.full^=1;assert(!configured(mask,broken));
    broken=ctx;broken.global_param_tr[1].first=broken.global_param_tr[2].first;assert(!configured(mask,broken));
    broken=ctx;broken.render_passes.clear();assert(!configured(mask,broken));
    broken=ctx;broken.render_passes[0].autosort=false;assert(!configured(mask,broken));
    broken=ctx;broken.render_passes.push_back({true,1000,1});assert(!configured(mask,broken));
    // Unsorted paths have index ranges; the same verified26raw strips remain safe.
    auto indexed=ctx;indexed.sortedTriangles.clear();indexed.render_passes[0].autosort=false;indexed.render_passes[0].sorted_tr_count=0;
    for(auto& p:indexed.global_param_tr){const auto first=p.first;p.first=uint32_t(indexed.idx.size());for(uint32_t i=0;i<p.count;++i)indexed.idx.push_back(first+i);}
    assert(configured(mask,indexed));
    // Actual 185-focal UI shrinks the entire native fan+rim by3, independently
    // of the main world's lens at the transition frame. The full tuple must agree.
    auto shrunk=ctx;for(auto& v:shrunk.verts){v.x=320+(v.x-320)/3;v.y=240+(v.y-240)/3;}
    assert(configured(mask,shrunk));
    auto mixed=shrunk;for(unsigned n=22;n<26;++n){const auto& p=mixed.global_param_tr[n];
        for(unsigned i=p.first;i<p.first+p.count;++i){mixed.verts[i].x=320+(mixed.verts[i].x-320)*3;mixed.verts[i].y=240+(mixed.verts[i].y-240)*3;}}
    assert(!configured(mask,mixed));
    auto half=ctx;for(auto& v:half.verts){v.x=320+(v.x-320)/2;v.y=240+(v.y-240)/2;}assert(!configured(mask,half));
    // A complete group can be referenced by fewer merged sorted draw commands.
    assert(configured(mask,ctx));assert(mask.selected(ctx,0)&&mask.selected(ctx,22));
    const auto create=+[]()->Target{return {12,34};};
    const auto copy=+[](Target,unsigned)->CopyResult{return {true,0};};
    const auto destroy=+[](Target){};
    auto& gpu=gpu_scope();assert(gpu.initialize({create,copy,destroy},77));gpu.set_physical_rifle_active(true);
    Source source{"T9507N",true,true,640,480,0x52c000,1,512,0,0,256,256,10,3,55};
    assert(gpu.capture(source,77,monotonic_seconds()));
    {ScopedCompositeDraw draw("T9507N",ctx,true,77);assert(!should_skip_scope_composite(ctx,0));}
    {ScopedRifleDrawAuthorization rifle(true);
        {ScopedCompositeDraw draw("T9507N",ctx,true,77);assert(should_skip_scope_composite(ctx,0));
            {ScopedCompositeDraw original("T9507N",ctx,false,77);assert(!should_skip_scope_composite(ctx,0));}
            assert(should_skip_scope_composite(ctx,0));
            source.submit_sequence=12;assert(gpu.capture(source,77,monotonic_seconds()));
            assert(should_skip_scope_composite(ctx,0)); // prior compatible view retained
            gpu.invalidate();assert(!should_skip_scope_composite(ctx,0));
        }
        assert(!should_skip_scope_composite(ctx,0));
    }
    assert(gpu.release(77));
    // A frame that starts just before expiry must make one consistent scope
    // decision for its world polygons, rifle and both eyes. Time passing while
    // drawing must not restore half the widget or black the optic mid-frame.
    assert(gpu.initialize({create,copy,destroy},77));gpu.set_physical_rifle_active(true);
    source.submit_sequence=10;
    assert(gpu.capture(source,77,monotonic_seconds()-.24));
    {ScopedRifleDrawAuthorization presentation(false); // immediately after native drain
    std::this_thread::sleep_for(std::chrono::milliseconds(30)); // slow controller/world picking
    {ScopedRifleDrawAuthorization rifle(true);
        ScopedCompositeDraw draw("T9507N",ctx,true,77);
        assert(should_skip_scope_composite(ctx,0));
        const auto first_eye=gpu.acquire(11,3,77,presentation_seconds());assert(first_eye);
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        assert(should_skip_scope_composite(ctx,0));
        const auto optic=gpu.acquire(11,3,77,presentation_seconds());
        assert(optic&&optic.texture==first_eye.texture&&optic.sequence==first_eye.sequence);
        {ScopedCompositeDraw second_eye("T9507N",ctx,true,77);
            assert(should_skip_scope_composite(ctx,0));
        }
        assert(!gpu.acquire(11,3,77,monotonic_seconds())); // real clock crossed expiry
    }
    }
    {ScopedRifleDrawAuthorization rifle(true);
        ScopedCompositeDraw draw("T9507N",ctx,true,77);
        assert(!should_skip_scope_composite(ctx,0)); // expiry still applies next frame
    }
    assert(gpu.release(77));
    unsigned actual_frames=0;
    if(argc>1){std::ifstream input(argv[1],std::ios::binary);char magic[8]{};input.read(magic,8);assert(std::memcmp(magic,"SSSCMP01",8)==0);
        const auto count=u32(input);assert(count<=100);
        for(unsigned f=0;f<count;++f){Ctx native;const auto nv=u32(input),np=u32(input);assert(nv<2000&&np==26);
            native.verts.resize(nv);input.read(reinterpret_cast<char*>(native.verts.data()),nv*sizeof(V));assert(input);
            for(unsigned p=0;p<np;++p){P item;item.first=u32(input);item.count=u32(input);item.isp.full=u32(input);item.tsp.full=u32(input);item.tcw.full=u32(input);item.pcw.full=u32(input);item.tileclip=u32(input);native.global_param_tr.push_back(item);}
            native.render_passes.push_back({true,np,1});check(native);++actual_frames;
        }
    }
    std::cout<<"Scope composite: complete26group, geometry topology, gates, retained frames, Original and storage modes passed; actual fixtures="<<actual_frames<<'\n';
}
