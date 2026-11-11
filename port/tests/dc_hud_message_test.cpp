#include "dreamcast/dc_hud_message.h"
#include "dreamcast/dc_ta_ui.h"
#include "dreamcast/dc_scope_hud.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <cstring>
#include <string>
#include <vector>
namespace message=silent_scope::dc_hud_message;
struct V{float x{},y{},z{},u{},v{};uint8_t col[4]{};};
struct P{uint32_t first{},count{};struct W{uint32_t full{};}isp,tsp,tcw,pcw;uint32_t tileclip{};};
struct Pass{bool autosort{};uint32_t tr_count{},sorted_tr_count{};};
struct Sorted{uint32_t polyIndex{},first{},count{};};
struct Ctx{bool isRTT{};unsigned framebufferWidth{640},framebufferHeight{480};
    struct{bool supported{true};uint64_t generation{3},submit_sequence{2696};}dcCamera;
    std::vector<V>verts;std::vector<uint32_t>idx;std::vector<P>global_param_op,global_param_pt,global_param_tr;
    std::vector<Pass>render_passes;std::vector<Sorted>sortedTriangles;
};
Ctx event(std::string_view first="Hurry up!",std::string_view second={},std::string_view third={},std::string_view fourth={}){Ctx c;
    const auto add=[&](float x,float y,float w,float h,bool plate,unsigned cell=0){P p;p.first=uint32_t(c.verts.size());p.count=4;
        p.isp.full=0xc8800000;p.tsp.full=plate?0x9491a4e4:0x9491a4ed;p.tcw.full=plate?0x3f308d56:0x500c0000;p.pcw.full=plate?0x828c0002:0x828c000a;p.tileclip=0x100200;
        const float u=(cell&15)/16.f,v=(cell>>4)/16.f,q=plate?.7246376872062683f:.7299270033836365f;
        const uint8_t color=plate?0:255,alpha=plate?112:255;
        for(unsigned j=0;j<4;++j)c.verts.push_back({x+(j&2?w:0),y+(j&1?h:0),q,u+(j&2?1.f/16:0),v+(j&1?1.f/16:0),{color,color,color,alpha}});
        const auto first=uint32_t(c.idx.size()),poly=uint32_t(c.global_param_tr.size());c.idx.insert(c.idx.end(),{p.first,p.first+1,p.first+2,p.first+2,p.first+1,p.first+3});
        c.sortedTriangles.push_back({poly,first,6});c.global_param_tr.push_back(p);
    };
    add(80,390,440,90,true);
    const std::array rows{first,second,third,fourth};
    for(unsigned row=0;row<rows.size();++row)
        for(unsigned i=0;i<rows[row].size();++i)add(90+10.f*i,390+20.f*row,10,20,false,unsigned(rows[row][i])-32);
    const auto count=uint32_t(c.global_param_tr.size());
    c.render_passes={{true,count,count}};return c;
}
uint32_t word(std::ifstream& f){uint32_t v{};f.read(reinterpret_cast<char*>(&v),4);assert(f);return v;}
void complete(Ctx& c,unsigned expected=20){message::HurryUpGroup group;assert(group.configure("T9507N",c));
    silent_scope::dc_ta_ui::DecodedUiMask decoded;assert(decoded.configure("T9507N",c));
    unsigned found=0;
    {silent_scope::dc_scope_hud::ScopedDraw scoped("T9507N",c,true);
    for(const auto& st:c.sortedTriangles)for(unsigned j=st.first;j+2<st.first+st.count;j+=3){const auto a=c.idx[j],b=c.idx[j+1],d=c.idx[j+2];
        const auto decision=group.classify(c,2,st.polyIndex,a,b,d);
        if(decision.layer==silent_scope::dc_hud::Layer::World)continue;
        assert(decoded.proven_ui_triangle(c,2,st.polyIndex,a,b,d));
        const auto gate=silent_scope::dc_hud::frame_gate("T9507N",c,true,2);
        assert(silent_scope::dc_scope_hud::classify(gate,c,c.global_param_tr[st.polyIndex],st.polyIndex,a,b,d).layer==decision.layer);++found;
    }}
    assert(found==expected);
}
int main(int argc,char**argv){auto c=event();message::HurryUpGroup group;assert(group.configure("T9507N",c));complete(c);
    for(const auto& warning:{event("Calm down!"),event("Bad luck!"),event("You've lost your touch."),
                             event("Watch out!","The enemy is going to shoot!"),
                             event("Take out all surviving sniper enemies","from the roof!"),
                             event("Terrorists have kidnapped the President's","daughter and are hiding in the stadium."),
                             event("We can't identify them from here.","Go and search for them! Over."),
                             event("I've found the President's daughter","in the black convertible with Cobra."),
                             event("Take him out and save the President's","daughter. Over."),
                             event("Some terrorists have kidnapped the","President's daughter and are escaping","by the highway.","Chase them and save her from harm. Over.")}){
        assert(group.configure("T9507N",warning));
        silent_scope::dc_ta_ui::DecodedUiMask mask;
        assert(mask.configure("T9507N",warning));
        for(const auto& batch:warning.sortedTriangles)
            for(unsigned i=batch.first;i<batch.first+batch.count;i+=3)
                assert(mask.proven_ui_triangle(warning,2,batch.polyIndex,warning.idx[i],warning.idx[i+1],warning.idx[i+2]));
    }
    auto wrong_warning=event("Watch out!","The enemy is going to shout!");
    assert(!group.configure("T9507N",wrong_warning));
    for(unsigned i=0;i<13;++i){auto bad=c;
        if(i==0){bad.global_param_tr.pop_back();bad.sortedTriangles.pop_back();bad.render_passes[0]={true,9,9};}
        if(i==1)bad.verts[39].col[3]=254;
        if(i==2)bad.verts[4].u+=1.f/16;
        if(i==3)bad.verts[0].z=.7246f;
        if(i==4)bad.verts[0].col[3]=111;
        if(i==5)bad.global_param_tr[1].tcw.full^=1;
        if(i==6)bad.global_param_tr[0].pcw.full^=8;
        if(i==7)bad.isRTT=true;
        if(i==8)bad.dcCamera.generation=0;
        if(i==9)bad.dcCamera.supported=false;
        if(i==10)for(auto& v:bad.verts){v.x=320+(v.x-320)/3;v.y=240+(v.y-240)/3;}
        if(i==11){bad.global_param_tr.push_back(bad.global_param_tr[0]);++bad.render_passes[0].tr_count;}
        if(i==12)bad.verts[4].x=std::numeric_limits<float>::quiet_NaN();
        assert(!group.configure("T9507N",bad));
        assert(group.classify(bad,2,0,0,1,2).layer==silent_scope::dc_hud::Layer::World);
    }
    auto ignored=c;ignored.global_param_tr[0].tcw.full=0xdeadbeef;assert(group.configure("T9507N",ignored)); // Texturefalse plate
    assert(!group.configure("T9505D",c));assert(group.configure("T9507N",c));
    assert(group.classify(c,2,1,36,37,38).layer==silent_scope::dc_hud::Layer::MessageFont); // merged font material ID
    auto other=c;other.verts.insert(other.verts.end(),c.verts.begin()+4,c.verts.begin()+8);assert(group.configure("T9507N",other));
    assert(group.classify(other,2,1,40,41,42).layer==silent_scope::dc_hud::Layer::World);
    other.verts[40].x=200;other.verts[40].y=200;other.verts[41].x=200;other.verts[41].y=220;other.verts[42].x=210;other.verts[42].y=200;
    const auto mixed_first=uint32_t(other.idx.size());other.idx.insert(other.idx.end(),{4,5,6,40,41,42});
    {silent_scope::dc_scope_hud::ScopedDraw draw("T9507N",other,true);std::vector<silent_scope::dc_hud::DrawRange> ranges;
        assert(silent_scope::dc_scope_hud::draw_ranges(silent_scope::dc_hud::frame_gate("T9507N",other,true,2),other,other.global_param_tr[1],1,
            mixed_first,6,silent_scope::dc_hud::Primitive::Triangles,[&](auto r){ranges.push_back(r);}));
        assert(ranges.size()==2&&ranges[0].layer==silent_scope::dc_hud::Layer::MessageFont&&ranges[1].layer==silent_scope::dc_hud::Layer::World);}
    const auto gate=silent_scope::dc_hud::frame_gate("T9507N",c,true,2);
    {silent_scope::dc_scope_hud::ScopedDraw outer("T9507N",c,true);
        assert(silent_scope::dc_scope_hud::classify(gate,c,c.global_param_tr[0],0,0,1,2).layer==silent_scope::dc_hud::Layer::MessagePlate);
        {silent_scope::dc_scope_hud::ScopedDraw original("T9507N",c,false);
            assert(silent_scope::dc_scope_hud::classify(gate,c,c.global_param_tr[0],0,0,1,2).layer==silent_scope::dc_hud::Layer::World);}
        assert(silent_scope::dc_scope_hud::classify(gate,c,c.global_param_tr[0],0,0,1,2).layer==silent_scope::dc_hud::Layer::MessagePlate);}
    assert(group.configure("T9507N",c));auto plane=silent_scope::dc_hud::placement(.1f,true,.5f,2.5f);
    const auto decision=group.classify(c,2,0,0,1,2);silent_scope::dc_hud::Hit hit;
    const std::array<float,3> ray{(100.f/320-1)*25*silent_scope::dc_hud::NativeTanX*.5f,(400.f/240-1)*25*silent_scope::dc_hud::NativeTanY*.5f,-25};
    assert(silent_scope::dc_hud::hit_decision(decision,c.verts[0],c.verts[1],c.verts[2],plane,{0,0,0},ray,silent_scope::dc_hud::NativeTanX,silent_scope::dc_hud::NativeTanY,hit));
    assert(std::abs(hit.x-100.f/640)<1e-6f&&std::abs(hit.y-400.f/480)<1e-6f);
    if(argc>1){std::ifstream f(argv[1],std::ios::binary);char magic[8]{};f.read(magic,8);assert(std::memcmp(magic,"SSDOM002",8)==0);const auto count=word(f);assert(count==1);
        Ctx ctx;const auto nv=word(f),ni=word(f),no=word(f),np=word(f),nt=word(f),passes=word(f),sorts=word(f);(void)word(f);
        assert(nv<=65536&&ni<=262144&&no+np+nt<=16384&&passes<=64&&sorts<=16384);ctx.verts.resize(nv);f.read(reinterpret_cast<char*>(ctx.verts.data()),nv*sizeof(V));ctx.idx.resize(ni);f.read(reinterpret_cast<char*>(ctx.idx.data()),ni*4);assert(f);
        const auto polys=[&](auto& list,unsigned size){for(unsigned i=0;i<size;++i){P p;p.first=word(f);p.count=word(f);p.tcw.full=word(f);p.pcw.full=word(f);p.isp.full=word(f);p.tsp.full=word(f);p.tileclip=word(f);list.push_back(p);}};
        polys(ctx.global_param_op,no);polys(ctx.global_param_pt,np);polys(ctx.global_param_tr,nt);
        for(unsigned i=0;i<passes;++i){(void)word(f);(void)word(f);auto tr=word(f),sorted=word(f),automatic=word(f);ctx.render_passes.push_back({bool(automatic),tr,sorted});}
        for(unsigned i=0;i<sorts;++i)ctx.sortedTriangles.push_back({word(f),word(f),word(f)});
        const unsigned expected=argc>2?unsigned(std::stoul(argv[2])):20u;
        complete(ctx,expected);std::cout<<"Verified "<<expected<<" native advisory triangles from the captured frame\n";
    }
    std::cout<<"Hurry up complete native advisory group passed\n";
}
