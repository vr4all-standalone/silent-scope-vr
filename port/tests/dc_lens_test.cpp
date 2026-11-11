#include "dreamcast/dc_lens.h"
#include <cassert>
#include <cmath>
#include <map>
#include <vector>
using namespace silent_scope::dreamcast;
static uint32_t bits(float v){uint32_t b;std::memcpy(&b,&v,4);return b;}
static bool near(float a,float b){return std::abs(a-b)<0.0005f;}
struct TestVertex{float x,y,z;};
struct TestPolygon{uint32_t first,count;struct{uint32_t full;}tcw;};
struct TestContext{
    SubmittedLens dcLens;struct{uint64_t generation,submit_sequence;}dcCamera;
    bool isRTT{};std::vector<TestVertex> verts;std::vector<uint32_t> idx;
    std::vector<TestPolygon> global_param_op,global_param_pt;
};
int main(){
    std::map<uint32_t,uint32_t> memory;
    for(auto g:LensCodeGuard)memory[g.first]=g.second;
    memory[LensSourceMain]=bits(555);memory[LensMode]=0;
    for(auto a:{LensSdkBase,LensCacheBase,LensCacheBase+4})memory[a]=bits(555);
    memory[LensSdkBase+4]=bits(640);memory[LensSdkBase+8]=bits(480);
    memory[LensSdkBase+12]=memory[LensCacheBase+8]=bits(320);
    memory[LensSdkBase+16]=memory[LensCacheBase+12]=bits(240);
    memory[LensSdkBase+52]=memory[LensSdkBase+56]=bits(1);
    unsigned reads=0;auto read=[&](uint32_t a){++reads;return memory.at(a);};
    LensRequest request;
    LensOwnership ownership{true,7,9,555,555,320,240};
    auto capture=[&](bool rtt=false,const char* title="T9507N"){return capture_lens(title,rtt,640,480,7,9,request,ownership,read);};
    assert(!capture().valid&&reads==0); // capability defaults off, no RAM traffic
    request={true,true,false,10};
    assert(!capture(false,"OTHER").valid&&reads==0);
    assert(!capture(true).valid&&reads==0); // scope RTT never borrows main lens
    ownership.verified=false;assert(!capture().valid&&reads==0);ownership.verified=true;
    ownership.submit_sequence=8;assert(!capture().valid&&reads==0);ownership.submit_sequence=9;
    const auto stock=capture();assert(stock.valid&&stock.factor==1&&stock.fx==555&&stock.fy==555);
    assert(stock.generation==7&&stock.submit_sequence==9&&stock.request_revision==10);
    assert(lens_status(stock,request,7)==LensStatus::Original);
    ++request.revision;request.wider=true;
    assert(lens_status(stock,request,7)==LensStatus::Pending);
    assert(lens_status(stock,request,8)==LensStatus::Unavailable);
    for(auto a:{LensSdkBase,LensCacheBase,LensCacheBase+4})memory[a]=bits(185);
    assert(!capture().valid); // transition45: SDK185 but geometry still stock555
    ownership.fx=ownership.fy=185;
    const auto wide=capture();assert(wide.valid&&wide.factor==3&&wide.fx==185);
    assert(lens_status(wide,request,7)==LensStatus::Wider);
    const auto immutable=wide;memory[LensCacheBase]=bits(999);
    assert(!capture().valid&&immutable.fx==185);
    memory[LensCacheBase]=bits(185);
    request.wide_proven=false;assert(!capture().valid);request.wide_proven=true;
    for(auto bad:std::array<std::pair<uint32_t,uint32_t>,11>{{
        {LensSourceMain,bits(185)},{LensMode,1},{LensSdkBase,bits(296.767f)},
        {LensSdkBase,bits(277.5f)},{LensCacheBase+4,bits(555)},
        {LensSdkBase+4,bits(512)},{LensSdkBase+8,bits(384)},
        {LensSdkBase+12,bits(128)},{LensSdkBase+16,bits(128)},
        {LensSdkBase+52,bits(2)},{LensCacheBase+4,0x7fc00000}}}){
        const auto old=memory.at(bad.first);memory[bad.first]=bad.second;assert(!capture().valid);memory[bad.first]=old;
    }
    for(auto guard:LensCodeGuard){memory[guard.first]^=1;assert(!capture().valid);memory[guard.first]^=1;}
    assert(!capture_lens("T9507N",false,512,384,7,9,request,ownership,read).valid);
    assert(!capture_lens("T9507N",false,640,480,0,9,request,ownership,read).valid);
    // The same submitted lens reconstructs shader and picker positions, including
    // geometry outside the original camera cone. Projecting uses stock hit lens.
    for(float x:{-120.f,0.f,320.f,640.f,760.f})for(float y:{0.f,240.f,480.f}){
        EyePoint p{};assert(unproject(immutable,x,y,1.f/100.f,p));
        assert(near(p.x,(x-320)*100/185)&&near(p.y,(y-240)*100/185)&&near(p.z,-100));
        PixelPoint pixel{};assert(project_draw(immutable,p,pixel));assert(near(pixel.x,x)&&near(pixel.y,y));
        assert(project_stock(immutable,p,pixel));assert(near(pixel.x,320+(x-320)*3));
        assert(near(pixel.y,240+(y-240)*3));
    }
    EyePoint p{1,2,3};assert(!unproject(SubmittedLens{},320,240,1,p));assert(p.x==1);
    assert(!unproject(immutable,320,240,0,p));
    assert(!unproject(immutable,NAN,240,1,p));
    PixelPoint pixel{1,2};assert(!project_draw(immutable,{0,0,0},pixel));assert(pixel.x==1);
    request.wider=false;++request.revision;assert(lens_status(immutable,request,7)==LensStatus::Pending);
    for(auto a:{LensSdkBase,LensCacheBase,LensCacheBase+4})memory[a]=bits(555);
    assert(!capture().valid); // transition55: SDK555 but geometry still wide185
    ownership.fx=ownership.fy=555;
    const auto restored=capture();assert(lens_status(restored,request,7)==LensStatus::Original);
    request.enabled=false;assert(lens_status(restored,request,7)==LensStatus::Unavailable);
    TestContext ctx;ctx.dcLens=stock;ctx.dcCamera={7,9};
    ctx.verts={{10,10,.01f},{20,10,.01f},{10,20,.01f}};
    ctx.idx={0,1,2,UINT32_MAX,0,1,2,UINT32_MAX,0,1,2,UINT32_MAX,0,1,2};
    ctx.global_param_pt={{0,uint32_t(ctx.idx.size()),{0}}};
    assert(!has_world_geometry(ctx)); // inherited555 + four fixed-depth UI quads
    ctx.verts[2].z=.02f;assert(has_world_geometry(ctx));
    ctx.dcCamera.submit_sequence=10;assert(!has_world_geometry(ctx));ctx.dcCamera.submit_sequence=9;
    ctx.dcLens.valid=false;assert(!has_world_geometry(ctx));ctx.dcLens.valid=true;
    ctx.isRTT=true;assert(!has_world_geometry(ctx));ctx.isRTT=false;
    ctx.global_param_pt[0].tcw.full=0x0c0a5800;assert(!has_world_geometry(ctx));ctx.global_param_pt[0].tcw.full=0;
    ctx.verts[2].z=.8f;assert(!has_world_geometry(ctx)); // near-depth overlay/composite
    ctx.verts[2].z=NAN;assert(!has_world_geometry(ctx));ctx.verts[2].z=.02f;
    ctx.global_param_pt[0].count=11;assert(!has_world_geometry(ctx));ctx.global_param_pt[0].count=15;
    ctx.idx[14]=999;assert(!has_world_geometry(ctx));ctx.idx[14]=2;
    ctx.verts[0].x=ctx.verts[1].x=ctx.verts[2].x=-30;assert(!has_world_geometry(ctx));
    ctx.verts.clear();ctx.idx.clear();
    for(unsigned n=0;n<4;++n){
        const float q=.01f*(n+1);ctx.verts.insert(ctx.verts.end(),{{10,10,q},{20,10,q},{10,20,q}});
        ctx.idx.insert(ctx.idx.end(),{n*3,n*3+1,n*3+2,UINT32_MAX});
    }
    ctx.global_param_pt[0].count=uint32_t(ctx.idx.size());
    assert(!has_world_geometry(ctx)); // different fixed depths still remain UI
    ctx.verts={{-100,100,.01f},{100,-100,.02f},{-100,-100,.03f}};
    ctx.idx={0,1,2,UINT32_MAX,0,1,2,UINT32_MAX,0,1,2,UINT32_MAX,0,1,2};
    ctx.global_param_pt[0].count=uint32_t(ctx.idx.size());
    assert(!has_world_geometry(ctx)); // bounding box overlaps; triangle only touches origin
}
