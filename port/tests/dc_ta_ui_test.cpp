#include "dreamcast/dc_ta_ui.h"
#include "dreamcast/dc_lens_ownership.h"
#include <cassert>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>
using silent_scope::dc_ta_ui::DecodedUiMask;
struct V {float x{},y{},z{},u{},v{};uint8_t col[4]{};};
struct P {uint32_t first{},count{};struct W{uint32_t full{};}isp,tsp,tcw,pcw;uint32_t tileclip{};};
struct Pass {bool autosort{};uint32_t tr_count{},sorted_tr_count{};};
struct S {uint32_t polyIndex{},first{},count{};};
struct Ctx {bool isRTT{};unsigned framebufferWidth{640},framebufferHeight{480};
    struct {bool supported{true};uint64_t generation{3},submit_sequence{11};}dcCamera;
    std::vector<V> verts;std::vector<uint32_t> idx;std::vector<P> global_param_op,global_param_pt,global_param_tr;
    std::vector<Pass> render_passes;std::vector<S> sortedTriangles;
};
P font(){return {0,3,{0xc8800000},{0x9491a4ed},{0x500c0000},{0x828c000a},0x100200};}
Ctx text(){Ctx c;c.verts={{220,30,.7299270034f,0,0,{255,255,255,255}},{240,30,.7299270034f,0,0,{255,255,255,255}},{220,50,.7299270034f,0,0,{255,255,255,255}}};
    c.global_param_tr={font()};c.idx={0,1,2};c.render_passes={{true,1,1}};c.sortedTriangles={{0,0,3}};return c;}
uint32_t word(std::ifstream& f){uint32_t v{};f.read(reinterpret_cast<char*>(&v),4);assert(f);return v;}
void scope_checks(Ctx c){
    DecodedUiMask mask;assert(mask.configure("T9507N",c));
    silent_scope::dc_scope::GeometryGroup geometry;assert(geometry.configure("T9507N",c));
    unsigned triangles=0;
    for(unsigned n=0;n<c.global_param_tr.size();++n){const auto& p=c.global_param_tr[n];
        for(unsigned i=2;i<p.count;++i){const auto a=p.first+i-2,b=p.first+i-1,d=p.first+i;
            assert(mask.proven_ui_triangle(c,2,n,a,b,d));assert(mask.proven_ui_triangle(c,2,n,d,a,b));++triangles;}}
    assert(triangles==92);
    auto indexed=c;indexed.idx={UINT32_MAX,UINT32_MAX,UINT32_MAX};indexed.sortedTriangles.clear();indexed.render_passes={{false,26,0}};
    for(auto& p:indexed.global_param_tr){const auto first=p.first;p.first=uint32_t(indexed.idx.size());for(unsigned i=0;i<p.count;++i)indexed.idx.push_back(first+i);}
    assert(mask.configure("T9507N",indexed));
    for(unsigned n=0;n<indexed.global_param_tr.size();++n){const auto& p=indexed.global_param_tr[n];for(unsigned j=2;j<p.count;++j)
        assert(mask.proven_ui_triangle(indexed,2,n,indexed.idx[p.first+j-2],indexed.idx[p.first+j-1],indexed.idx[p.first+j]));}
    assert(mask.configure("T9507N",c));
    // sortTriangles can merge native polygon material IDs. Only a triangle
    // belonging to one of the validated strips may borrow that material ID.
    const auto& another=c.global_param_tr[1];assert(mask.proven_ui_triangle(c,2,0,another.first,another.first+1,another.first+2));
    const auto old_size=uint32_t(c.verts.size());c.verts.push_back({320,240,.625f,.5f,.5f,{254,254,254,255}});
    assert(!mask.proven_ui_triangle(c,2,0,0,1,old_size));
    // Nonadjacent points from a known strip do not prove an actual UI triangle.
    assert(!mask.proven_ui_triangle(c,2,1,another.first,another.first+1,another.first+4));
    assert(!mask.proven_ui_triangle(c,0,0,0,1,2));
    auto broken=c;broken.verts[0].z=.624f;assert(mask.configure("T9507N",broken));assert(!mask.proven_ui_triangle(broken,2,0,0,1,2));
    auto partial=c;partial.global_param_tr.pop_back();partial.render_passes[0].tr_count=25;
    assert(mask.configure("T9507N",partial));assert(!mask.proven_ui_triangle(partial,2,0,0,1,2));
}
int main(int argc,char** argv){
    static_assert(sizeof(V)==24);DecodedUiMask mask;auto c=text();
    assert(mask.configure("T9507N",c));assert(mask.proven_ui_triangle(c,2,0,0,1,2));
    c.isRTT=true;assert(!mask.proven_ui_triangle(c,2,0,0,1,2));c.isRTT=false;
    assert(!mask.proven_ui_triangle(c,0,0,0,1,2));assert(!mask.proven_ui_triangle(c,2,4,0,1,2));
    assert(!mask.configure("T9505D",c));assert(!mask.proven_ui_triangle(c,2,0,0,1,2));
    c.isRTT=true;assert(!mask.configure("T9507N",c));c.isRTT=false;
    c.framebufferWidth=512;assert(!mask.configure("T9507N",c));c.framebufferWidth=640;
    c.dcCamera.supported=false;assert(!mask.configure("T9507N",c));c.dcCamera.supported=true;
    c.dcCamera.generation=0;assert(!mask.configure("T9507N",c));c.dcCamera.generation=3;
    // A flat world quad at this very same q is not a UI exemption.
    c.verts[0].y=200;assert(mask.configure("T9507N",c));assert(!mask.proven_ui_triangle(c,2,0,0,1,2));
    c=text();assert(mask.configure("T9507N",c));++c.dcCamera.submit_sequence;assert(!mask.proven_ui_triangle(c,2,0,0,1,2));
    c=text();c.global_param_tr[0].tsp.full=0x9491a4e4;c.global_param_tr[0].tcw.full=0x100b0a00;c.global_param_tr[0].pcw.full=0x828c002a;
    c.verts={{100,200,.333333343267f,16.5f/128,1.5f/128,{176,176,176,255}},
        {105.6376953125f,209.39616394043f,.333333343267f,17.f/128,0,{176,176,176,255}},
        {111.275390625f,200,.333333343267f,18.f/128,1.5f/128,{176,176,176,255}}};
    assert(mask.configure("T9507N",c)&&mask.proven_ui_triangle(c,2,0,0,1,2));
    for(auto& v:c.verts){v.x=320+(v.x-320)/3;v.y=240+(v.y-240)/3;}
    assert(mask.configure("T9507N",c)&&mask.proven_ui_triangle(c,2,0,0,1,2));
    c.verts[0].u=32.f/128;assert(!mask.proven_ui_triangle(c,2,0,0,1,2)); // red/other atlas regions are not proven
    // Stadium's five-letter caption persists after the green dossier clears.
    // These are the first captured glyph triangles from each draw.
    c=text();
    constexpr float q=.7299270034f;
    c.verts={{280,320,q,.1875f,.125f,{255,255,255,255}},
        {280,340,q,.1875f,.1875f,{255,255,255,255}},
        {290,320,q,.25f,.125f,{255,255,255,255}}};
    assert(mask.configure("T9507N",c)&&mask.proven_ui_triangle(c,2,0,0,1,2));
    for(auto& v:c.verts)v.x+=10;
    assert(!mask.proven_ui_triangle(c,2,0,0,1,2));
    c.verts={{180,90,q,.1875f,.125f,{0,255,0,191}},
        {180,110,q,.1875f,.1875f,{0,255,0,191}},
        {190,90,q,.25f,.125f,{0,255,0,191}}};
    assert(mask.proven_ui_triangle(c,2,0,0,1,2));
    c.verts[0].col[3]=192;
    assert(!mask.proven_ui_triangle(c,2,0,0,1,2));
    c.verts={{500,130,q,0,0,{0,255,0,191}},
        {500,150,q,0,.0625f,{0,255,0,191}},
        {510,130,q,.0625f,0,{0,255,0,191}}};
    assert(mask.proven_ui_triangle(c,2,0,0,1,2));
    // Unknown constant-q WORLD still participates in source-lens coherence.
    // A proven UI triangle alone may bypass ownership; arbitrary flat q cannot.
    silent_scope::dreamcast::SubmittedLens lens;lens.valid=lens.supported=true;lens.generation=3;lens.request_revision=7;
    lens.fx=lens.fy=555;lens.cx=320;lens.cy=240;lens.width=640;lens.height=480;lens.factor=1;
    silent_scope::dreamcast::NativeLensAccumulator accumulator;accumulator.reset();
    const auto flat=[&](const auto& owner,bool ui){accumulator.vertex(220,200,.7299270034f,owner,false,ui);accumulator.vertex(240,200,.7299270034f,owner,false,ui);accumulator.vertex(220,220,.7299270034f,owner,true,ui);};
    for(unsigned i=0;i<4;++i)flat(lens,false);
    assert(accumulator.owned().valid);
    c=text();assert(mask.configure("T9507N",c));assert(mask.proven_ui_triangle(c,2,0,0,1,2));
    flat(silent_scope::dreamcast::SubmittedLens{},true);assert(accumulator.owned().valid);
    flat(silent_scope::dreamcast::SubmittedLens{},false);assert(accumulator.rejected()&&!accumulator.owned().valid);
    unsigned frames=0;
    if(argc>1){std::ifstream f(argv[1],std::ios::binary);char magic[8]{};f.read(magic,8);
      if(std::memcmp(magic,"SSDOM002",8)==0){const auto count=word(f);assert(count<=100);unsigned total_scope=0,total_hud=0,total_target=0,total_unknown=0;
        for(unsigned n=0;n<count;++n){Ctx native;const auto nv=word(f),ni=word(f),no=word(f),np=word(f),nt=word(f),passes=word(f),sorts=word(f);(void)word(f);
            assert(nv<=65536&&ni<=262144&&no+np+nt<=16384&&passes<=64&&sorts<=16384);
            native.verts.resize(nv);f.read(reinterpret_cast<char*>(native.verts.data()),nv*sizeof(V));native.idx.resize(ni);f.read(reinterpret_cast<char*>(native.idx.data()),ni*4);assert(f);
            const auto polys=[&](auto& list,unsigned size){for(unsigned i=0;i<size;++i){P p;p.first=word(f);p.count=word(f);p.tcw.full=word(f);p.pcw.full=word(f);p.isp.full=word(f);p.tsp.full=word(f);p.tileclip=word(f);list.push_back(p);}};
            polys(native.global_param_op,no);polys(native.global_param_pt,np);polys(native.global_param_tr,nt);
            for(unsigned i=0;i<passes;++i){(void)word(f);(void)word(f);const auto tr=word(f),sorted=word(f),automatic=word(f);native.render_passes.push_back({bool(automatic),tr,sorted});}
            for(unsigned i=0;i<sorts;++i)native.sortedTriangles.push_back({word(f),word(f),word(f)});
            assert(mask.configure("T9507N",native));unsigned scope=0,hud=0,targets=0,unknown_flat=0;
            for(const auto& draw:native.sortedTriangles){assert(draw.polyIndex<native.global_param_tr.size());const auto& p=native.global_param_tr[draw.polyIndex];
                for(unsigned j=draw.first;j+2<draw.first+draw.count;j+=3){const auto a=native.idx[j],b=native.idx[j+1],c=native.idx[j+2];
                    if(a==b||a==c||b==c)continue;
                    const bool known=mask.proven_ui_triangle(native,2,draw.polyIndex,a,b,c);
                    if(p.tcw.full==0x100b0a00&&p.tsp.full==0x9491a4e4){assert(known);++targets;}
                    else if(known){if(p.tcw.full==0x0c0a5800||p.tcw.full==0x5004c800)++scope;else ++hud;}
                    else if(native.verts[a].z==native.verts[b].z&&native.verts[a].z==native.verts[c].z)++unknown_flat;
                }
            }
            assert(scope==92&&hud==248&&targets>0&&targets<=100);total_scope+=scope;total_hud+=hud;total_target+=targets;total_unknown+=unknown_flat;++frames;
        }
        std::cout<<"Full native contexts="<<frames<<", scope UI="<<total_scope<<", HUD UI="<<total_hud<<", target UI="<<total_target<<", unknown flat surfaces="<<total_unknown<<"\n";
      }else if(std::memcmp(magic,"SSHUD001",8)==0){const auto count=word(f);assert(count<1000000);unsigned positives=0;
        auto triangle=text();
        for(unsigned i=0;i<count;++i){auto& p=triangle.global_param_tr[0];p.isp.full=word(f);p.tsp.full=word(f);p.tcw.full=word(f);p.pcw.full=word(f);p.tileclip=word(f);const auto list=word(f);
            f.read(reinterpret_cast<char*>(triangle.verts.data()),3*sizeof(V));assert(f);const bool want=(word(f)&255)!=0;
            assert(mask.configure("T9507N",triangle));const bool got=mask.proven_ui_triangle(triangle,list,0,0,1,2);
            if(got!=want){std::cerr<<"HUD fixture "<<i<<" expected "<<want<<" got "<<got<<'\n';return 1;}positives+=got;
        }
        std::cout<<"Actual decoded HUD triangles="<<count<<", proven UI="<<positives<<'\n';
      }else{
        assert(std::memcmp(magic,"SSSCMP01",8)==0);
        const auto count=word(f);assert(count<=100);
        for(unsigned n=0;n<count;++n){Ctx native;const auto nv=word(f),np=word(f);assert(nv<=2000&&np==26);
            native.verts.resize(nv);f.read(reinterpret_cast<char*>(native.verts.data()),nv*sizeof(V));assert(f);
            for(unsigned i=0;i<np;++i){P p;p.first=word(f);p.count=word(f);p.isp.full=word(f);p.tsp.full=word(f);p.tcw.full=word(f);p.pcw.full=word(f);p.tileclip=word(f);native.global_param_tr.push_back(p);}
            native.render_passes={{true,np,1}};native.sortedTriangles={{0,0,0}};scope_checks(native);++frames;
        }
      }
    }
    std::cout<<"Decoded native UI exceptions: exact HUD, whole26scope, merged triangle identity, unknown-flat rejection; actual scope frames="<<frames<<'\n';
}
