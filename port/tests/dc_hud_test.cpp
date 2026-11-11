#include "dreamcast/dc_hud.h"
#include <array>
#include <cassert>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <vector>
using namespace silent_scope::dc_hud;
struct V {float x{},y{},z{},u{},v{};uint8_t col[4]{};};
struct P {struct W{uint32_t full{};}isp,tsp,tcw,pcw;uint32_t tileclip{};};
struct Ctx {
    std::vector<V> verts;std::vector<uint32_t> idx;
    unsigned framebufferWidth{640},framebufferHeight{480};bool isRTT{};
    struct {uint64_t generation{3},submit_sequence{11};}dcCamera;
    struct Lens {bool supported{},valid{};float factor{},width{},height{},fx{},fy{},cx{},cy{};uint64_t generation{},submit_sequence{};}dcLens;
};
Gate gate(){return {"T9507N",640,480,false,true,3,11,2};}
P font(){return {{0xc8800000},{0x9491a4ed},{0x500c0000},{0x828c000a},0x100200};}
V v(float x,float y){return {x,y,.7299270033836365f,0,0,{255,255,255,255}};}
int main(int argc,char**argv){
    auto a=v(220,30),b=v(240,30),c=v(220,50);auto p=font();auto g=gate();
    // Missing measured-plane classification leaves the font 13.7cm from the camera.
    assert(classify(g,p,a,b,c)==Layer::Font);
    // Indoor Quest capture: white score glyph at the right edge.
    V score_tl{620,50,.7299270033836365f,0,.0625f,{255,255,255,255}};
    V score_bl=score_tl,score_tr=score_tl,score_br=score_tl;
    score_bl.y=score_br.y=70;score_bl.v=score_br.v=.125f;
    score_tr.x=score_br.x=630;score_tr.u=score_br.u=.0625f;
    assert(classify(g,p,score_tl,score_bl,score_tr)==Layer::Font);
    assert(classify(g,p,score_tr,score_bl,score_br)==Layer::Font);
    auto wrong_uv=score_tr;wrong_uv.u=.125f;
    assert(classify(g,p,score_tl,score_bl,wrong_uv)==Layer::World);
    const auto ranking_cell=[&](float x,float y,char glyph){
        auto top_left=v(x,y);
        const unsigned cell=unsigned(glyph)-32;
        top_left.u=(cell%16)/16.f;
        top_left.v=(cell/16)/16.f;
        auto bottom_left=top_left,top_right=top_left;
        bottom_left.y+=20;bottom_left.v+=.0625f;
        top_right.x+=10;top_right.u+=.0625f;
        return classify(g,p,top_left,bottom_left,top_right);
    };
    assert(ranking_cell(490,70,'3')==Layer::Font);
    assert(ranking_cell(520,130,'.')==Layer::Font);
    assert(ranking_cell(570,110,'\'')==Layer::Font);
    assert(ranking_cell(600,130,'"')==Layer::Font);
    assert(ranking_cell(570,110,'3')==Layer::World); // Wrong atlas cell for this field.
    assert(ranking_cell(490,150,'3')==Layer::World); // Beyond the fifth row.
    assert(ranking_cell(491,70,'3')==Layer::World); // Misaligned geometry.
    auto tl=v(30,30),bl=v(30,50),tr=v(40,30),br=v(40,50);
    tl.u=bl.u=.1875f;tr.u=br.u=.25f;
    tl.v=tr.v=.1875f;bl.v=br.v=.25f;
    for(auto* glyph:{&tl,&bl,&tr,&br}){glyph->col[1]=0;glyph->col[2]=0;}
    assert(classify(g,p,tl,bl,tr)==Layer::Font);
    assert(classify(g,p,tr,bl,br)==Layer::Font);
    ++br.y;
    assert(classify(g,p,tr,bl,br)==Layer::World);
    const P life{{0xc8800000},{0x9491a4e4},{0x500c1200},{0x828c000a},0x100200};
    auto life_tl=v(550,70),life_bl=v(550,100),life_tr=v(580,70),life_br=v(580,100);
    life_tl.u=life_bl.u=.5625f;life_tr.u=life_br.u=.75f;
    life_tl.v=life_tr.v=.1875f;life_bl.v=life_br.v=.375f;
    assert(classify(g,life,life_tl,life_bl,life_tr)==Layer::Life);
    assert(classify(g,life,life_tr,life_bl,life_br)==Layer::Life);
    life_br.x=581;
    assert(classify(g,life,life_tr,life_bl,life_br)==Layer::World);
    for(unsigned i=0;i<7;++i){auto bad=g;
        if(i==0)bad.product="T9505D";
        if(i==1)bad.width=320;
        if(i==2)bad.height=240;
        if(i==3)bad.is_rtt=true;
        if(i==4)bad.vr_world=false;
        if(i==5)bad.generation=0;
        if(i==6)bad.submit=0;
        assert(classify(bad,p,a,b,c)==Layer::World);}
    auto wrong=p;wrong.tcw.full^=1;assert(classify(g,wrong,a,b,c)==Layer::World);
    wrong=p;wrong.pcw.full^=0x20;assert(classify(g,wrong,a,b,c)==Layer::World);
    auto bad=a;bad.z=.333333343f;assert(classify(g,p,bad,b,c)==Layer::World);
    bad=a;bad.col[0]=254;assert(classify(g,p,bad,b,c)==Layer::World);
    bad=a;bad.y=200;assert(classify(g,p,bad,b,c)==Layer::World);
    bad=a;bad.x=std::numeric_limits<float>::quiet_NaN();assert(classify(g,p,bad,b,c)==Layer::World);
    const auto native=placement(.1f,false,.5f);assert(native&&std::abs(native.depth*.1f-1.8f)<1e-6f&&native.size==1.f);
    const auto wide=placement(.1f,true,.5f);assert(wide&&wide.size==.5f);
    const auto farther=placement(.1f,false,1.f,2.5f);assert(farther&&std::abs(farther.depth-25.f)<1e-5f);
    assert(std::abs(placement(.1f,false,1.f,.5f).depth-5.f)<1e-5f);
    assert(std::abs(placement(.1f,false,1.f,10.f).depth-100.f)<1e-5f);
    assert(std::abs(placement(.1f,false,1.f,std::numeric_limits<float>::quiet_NaN()).depth-18.f)<1e-5f);
    assert(!placement(0,true,1));assert(!placement(std::numeric_limits<float>::infinity(),true,1));
    // Picking the displayed font must use its physical 1.8m plane and inverse HUD sizing.
    Hit hit;const std::array<float,3> origin{0,0,0};
    const std::array<float,3> direction{(230.f/320-1)*18*(320.f/555), (40.f/240-1)*18*(240.f/555),-18};
    assert(hit_triangle(g,p,a,b,c,native,origin,direction,320.f/555,240.f/555,hit));
    assert(std::abs(hit.x-230.f/640)<1e-6f&&std::abs(hit.y-40.f/480)<1e-6f&&std::abs(hit.distance-1)<1e-6f);
    auto away=direction;away[0]=30;assert(!hit_triangle(g,p,a,b,c,native,origin,away,320.f/555,240.f/555,hit));
    auto doubled=direction;doubled[0]*=.5f;doubled[1]*=.5f;
    assert(hit_triangle(g,p,a,b,c,wide,origin,doubled,320.f/555,240.f/555,hit));
    assert(std::abs(hit.x-230.f/640)<1e-6f);
    P ammo{{0xc8800000},{0x9491a4e4},{0x500c7900},{0x828c002a},0x100200};
    V aa{10,390,.6451612114906311f,0,0,{176,176,176,255}},ab=aa,ac=aa;ab.x=30;ac.y=410;
    for(auto* vv:{&aa,&ab,&ac}){vv->x=320+(vv->x-320)/3;vv->y=240+(vv->y-240)/3;}
    assert(classify(g,ammo,aa,ab,ac)==Layer::Ammo);
    assert(classify_detailed(g,ammo,aa,ab,ac).native_scale==3);
    const std::array<float,3> ammoRay{(15.f/320-1)*18*NativeTanX,(395.f/240-1)*18*NativeTanY,-18};
    assert(hit_triangle(g,ammo,aa,ab,ac,native,origin,ammoRay,NativeTanX,NativeTanY,hit));
    assert(std::abs(hit.x-(320+(15.f-320)/3)/640)<1e-6f&&std::abs(hit.y-(240+(395.f-240)/3)/480)<1e-6f);
    auto halfA=aa,halfB=ab,halfC=ac;for(auto* vv:{&halfA,&halfB,&halfC}){vv->x=320+(vv->x-320)*1.5f;vv->y=240+(vv->y-240)*1.5f;}
    assert(classify(g,ammo,halfA,halfB,halfC)==Layer::World);
    P marker{{0xc8800000},{0x9491a4e4},{0x100b0a00},{0x828c002a},0x100200};
    V ma{100,200,.333333343267f,16.5f/128,1.5f/128,{176,176,176,255}};
    V mb{105.6376953125f,209.39616394043f,.333333343267f,17.f/128,0,{176,176,176,255}};
    V mc{111.275390625f,200,.333333343267f,18.f/128,1.5f/128,{176,176,176,255}};
    assert(classify(g,marker,ma,mb,mc)==Layer::TargetMarker);
    auto target_negative=[&](V x,V y,V z){assert(classify(g,marker,x,y,z)==Layer::World);};
    bad=ma;bad.u=32.f/128;target_negative(bad,mb,mc); // neighbouring red atlas artwork
    bad=ma;bad.col[3]=254;target_negative(bad,mb,mc); // unmeasured native fades
    bad=ma;bad.z+=.00001f;target_negative(bad,mb,mc);
    bad=mb;bad.x+=.01f;target_negative(ma,bad,mc);
    bad=mb;bad.y-=18.79232788086f;target_negative(ma,bad,mc); // upward triangle
    bad=mc;bad.x+=.01f;target_negative(ma,mb,bad);
    bad=mc;bad.u=ma.u;bad.v=ma.v;target_negative(ma,mb,bad); // degenerate UV
    auto target_wide=std::array<V,3>{ma,mb,mc};
    for(auto& vv:target_wide){vv.x=320+(vv.x-320)/3;vv.y=240+(vv.y-240)/3;}
    assert(classify_detailed(g,marker,target_wide[0],target_wide[1],target_wide[2]).native_scale==3);
    auto shifted=std::array<V,3>{ma,mb,mc};for(auto& vv:shifted){vv.x+=500;vv.y+=260;}
    assert(classify(g,marker,shifted[0],shifted[1],shifted[2])==Layer::TargetMarker); // moving, near raster edge
    for(auto& vv:shifted)vv.x+=50;
    assert(classify(g,marker,shifted[0],shifted[1],shifted[2])==Layer::World);
    V la{100,100,.333333343267f,.5f,0,{176,176,176,255}},lb=la,lc=la,ld=la;
    lb.x+=37.584656f;lb.u=1;lc.x=lb.x;lc.y+=15.033867f;lc.u=1;lc.v=15.5f/128;ld.y=lc.y;ld.v=lc.v;
    assert(classify(g,marker,la,lb,lc)==Layer::TargetLabel&&classify(g,marker,la,ld,lc)==Layer::TargetLabel);
    bad=la;bad.v=16.f/128;target_negative(bad,lb,lc); // another atlas label
    bad=lb;bad.y+=.01f;target_negative(la,bad,lc);
    bad=lb;bad.u=.5f;target_negative(la,bad,lc);
    for(auto* vv:{&la,&lb,&lc}){vv->x=320+(vv->x-320)/3;vv->y=240+(vv->y-240)/3;}
    assert(classify_detailed(g,marker,la,lb,lc).native_scale==3);
    assert(render_size(Layer::TargetMarker,3,wide)==3&&render_size(Layer::TargetLabel,1,wide)==1);
    assert(render_size(Layer::Ammo,3,wide)==1.5f);
    const float target_x=(ma.x+mb.x+mc.x)/3,target_y=(ma.y+mb.y+mc.y)/3;
    const std::array<float,3> target_ray{(target_x/320-1)*18*NativeTanX,(target_y/240-1)*18*NativeTanY,-18};
    assert(hit_triangle(g,marker,target_wide[0],target_wide[1],target_wide[2],wide,origin,target_ray,NativeTanX,NativeTanY,hit));
    assert(std::abs(hit.x-(320+(target_x-320)/3)/640)<1e-6f); // HUD size .5 cannot move a target's bearing
    // Same material and consecutive indices can contain measured HUD and world triangles.
    Ctx ctx;ctx.verts={a,b,c,v(240,50),v(260,50),v(240,70)};ctx.idx={0,1,2,3,4,5};
    assert(enabled(frame_gate("T9507N",ctx,true,2))&&!enabled(frame_gate("T9507N",ctx,false,2))&&!enabled(frame_gate("T9507N",ctx,true,0)));
    assert(!actual_wider(ctx));ctx.dcLens={true,true,3,640,480,185,185,320,240,3,11};assert(actual_wider(ctx));
    ctx.dcLens.submit_sequence=12;assert(!actual_wider(ctx));ctx.dcLens.submit_sequence=11;
    ctx.dcLens.fx=555;assert(!actual_wider(ctx));ctx.dcLens.fx=185;
    ctx.verts[4].z=.2f;ctx.verts[5].z=.2f;std::vector<DrawRange> runs;
    assert(draw_ranges(g,ctx,p,0,6,Primitive::Triangles,[&](auto r){runs.push_back(r);}));
    assert(runs.size()==2&&runs[0].first==0&&runs[0].count==3&&runs[0].layer==Layer::Font&&runs[1].layer==Layer::World);
    // Splitting a strip on its odd triangle must preserve culling parity; restart resets parity.
    ctx.idx={0,1,2,4,0xffffffff,0,1,2};runs.clear();
    assert(draw_ranges(g,ctx,p,0,8,Primitive::Strip,[&](auto r){runs.push_back(r);}));
    assert(runs.size()==3&&runs[0].first==0&&runs[0].count==3&&!runs[0].flip);
    assert(runs[1].first==1&&runs[1].count==3&&runs[1].flip&&runs[1].layer==Layer::World);
    assert(runs[2].first==5&&runs[2].count==3&&!runs[2].flip);
    ctx.idx={0,1,99};runs.clear();assert(!draw_ranges(g,ctx,p,0,3,Primitive::Triangles,[&](auto r){runs.push_back(r);}));assert(runs.empty());
    if(argc>1){std::ifstream f(argv[1],std::ios::binary);char magic[8]{};uint32_t count=0;f.read(magic,8);f.read(reinterpret_cast<char*>(&count),4);
        assert(std::string(magic,8)=="SSHUD001"&&count<1000000);std::array<unsigned,10> counts{};
        for(uint32_t i=0;i<count;++i){uint32_t words[6]{};std::array<V,3> vs;uint32_t want=0;
            f.read(reinterpret_cast<char*>(words),sizeof words);f.read(reinterpret_cast<char*>(vs.data()),sizeof vs);f.read(reinterpret_cast<char*>(&want),4);assert(f);
            const unsigned want_scale=want>>8?want>>8:1;want&=255;assert(want<counts.size());
            P pp{{words[0]},{words[1]},{words[2]},{words[3]},words[4]};auto fixture_gate=g;fixture_gate.list_id=words[5];
            auto decision=classify_detailed(fixture_gate,pp,vs[0],vs[1],vs[2]);auto got=decision.layer;
            if(unsigned(got)!=want||(want&&decision.native_scale!=want_scale)){std::cerr<<"fixture "<<i<<" expected "<<want<<" scale "<<want_scale<<" got "<<unsigned(got)<<" scale "<<decision.native_scale<<"\n";return 1;}++counts[want];
        }const unsigned frame_count=argc>2?unsigned(std::stoul(argv[2])):18;
        assert(frame_count>0&&frame_count<=100&&counts[0]>0&&counts[1]==frame_count*104&&counts[2]==frame_count*6&&counts[3]==frame_count*120&&counts[4]==frame_count*6&&counts[5]==frame_count*2&&counts[6]==frame_count*10);
        assert(counts[7]==0&&counts[8]>0&&counts[9]>0); // native target counts/positions are dynamic
        std::cout<<"Verified "<<count<<" actual native triangles\n";
    }
    std::cout<<"HUD classification, placement and split ranges passed\n";
}
