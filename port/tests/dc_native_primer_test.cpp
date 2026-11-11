#include "dreamcast/dc_native_primer.h"
#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <vector>
namespace primer=silent_scope::dc_native_primer;
struct V{float x{},y{},z{},u{},v{};uint8_t col[4]{};};
struct P{uint32_t first{},count{};struct W{uint32_t full{};}isp,tsp,tcw,pcw;uint32_t tileclip{};};
struct Pass{bool autosort{};uint32_t tr_count{},sorted_tr_count{};};
struct Sorted{uint32_t polyIndex{},first{},count{};};
struct Ctx{bool isRTT{};unsigned framebufferWidth{640},framebufferHeight{480};
    struct{bool supported{true};uint64_t generation{3},submit_sequence{11};}dcCamera;
    std::vector<V> verts;std::vector<uint32_t> idx;std::vector<P>global_param_op,global_param_pt,global_param_tr;
    std::vector<Pass>render_passes;std::vector<Sorted>sortedTriangles;
};
Ctx native(){Ctx c;const float nan=std::numeric_limits<float>::quiet_NaN();
    c.verts={{-1.5f,-.5f,.1f,nan,nan,{}},{-.5f,-.5f,.1f,nan,nan,{}},{-1.5f,-1,.1f,nan,nan,{}},{-.5f,-1,.1f,nan,nan,{}}};
    c.idx={0,1,2,3};c.global_param_pt={{0,4,{0x09000000},{0x00800440},{0},{0xa0800000},0x100200}};return c;
}
uint32_t word(std::ifstream& f){uint32_t v{};f.read(reinterpret_cast<char*>(&v),4);assert(f);return v;}
int main(int argc,char**argv){auto c=native();
    assert(primer::proven_triangle("T9507N",c,1,0,0,1,2));
    assert(primer::proven_triangle("T9507N",c,1,0,3,2,1));
    for(unsigned i=0;i<11;++i){auto bad=c;
        if(i==0)bad.verts[3].x=0; // the unsubmitted fourth corner must still prove the full sprite
        if(i==1)bad.verts[0].col[3]=1;
        if(i==2)bad.verts[0].z=.10001f;
        if(i==3)bad.global_param_pt[0].tcw.full=1;
        if(i==4)bad.global_param_pt[0].pcw.full^=8;
        if(i==5)bad.global_param_pt[0].count=3;
        if(i==6)bad.idx[3]=UINT32_MAX;
        if(i==7)bad.dcCamera.generation=0;
        if(i==8)bad.dcCamera.supported=false;
        if(i==9)bad.isRTT=true;
        if(i==10)bad.framebufferWidth=320;
        assert(!primer::proven_triangle("T9507N",bad,1,0,0,1,2));}
    assert(!primer::proven_triangle("T9505D",c,1,0,0,1,2));
    assert(!primer::proven_triangle("T9507N",c,1,0,0,1,3)); // wrong sprite diagonal
    assert(!primer::proven_triangle("T9507N",c,1,0,0,0,2));
    auto tr=c;tr.global_param_tr=tr.global_param_pt;tr.global_param_pt.clear();tr.render_passes={{true,1,1}};tr.sortedTriangles={{0,0,6}};tr.idx={0,1,2,1,2,3};
    assert(primer::proven_triangle("T9507N",tr,2,0,0,1,2));
    assert(primer::proven_triangle("T9507N",tr,2,0,1,2,3));
    // A merged sorted batch uses the first material ID for another native sprite.
    tr.verts.insert(tr.verts.end(),c.verts.begin(),c.verts.end());auto other=tr.global_param_tr[0];other.first=4;tr.global_param_tr.push_back(other);tr.render_passes[0].tr_count=2;
    assert(primer::proven_triangle("T9507N",tr,2,0,4,5,6));
    tr.global_param_tr.pop_back();tr.render_passes[0].tr_count=1;
    assert(!primer::proven_triangle("T9507N",tr,2,0,4,5,6)); // geometry not in an actual native range
    std::vector<primer::Range> ranges;
    assert(primer::draw_filtered("T9507N",c,1,0,0,4,false,true,[&](auto r){ranges.push_back(r);}));assert(ranges.empty());
    assert(!primer::draw_filtered("T9507N",c,1,0,0,4,false,false,[&](auto r){ranges.push_back(r);})); // Original unchanged
    auto mixed=c;mixed.verts.insert(mixed.verts.end(),c.verts.begin(),c.verts.end());
    mixed.verts[7].x=0;mixed.idx={0,1,2,3,UINT32_MAX,4,5,6,7};mixed.global_param_pt[0].count=9;
    assert(primer::draw_filtered("T9507N",mixed,1,0,0,9,false,true,[&](auto r){ranges.push_back(r);}));
    assert(ranges.size()==2&&ranges[0].first==5&&!ranges[0].flip&&ranges[1].first==6&&ranges[1].flip); // complete damaged quad survives with parity
    ranges.clear();mixed.idx.back()=99999;
    assert(!primer::draw_filtered("T9507N",mixed,1,0,0,9,false,true,[&](auto r){ranges.push_back(r);}));assert(ranges.empty());
    auto unsorted=tr;unsorted.idx={0,1,2,3};unsorted.sortedTriangles.clear();unsorted.render_passes={{false,1,0}};
    assert(primer::proven_triangle("T9507N",unsorted,2,0,0,1,2));
    unsorted.idx={0,1,2,3,4};unsorted.global_param_tr[0].count=5;assert(!primer::proven_triangle("T9507N",unsorted,2,0,0,1,2));
    if(argc>1){std::ifstream f(argv[1],std::ios::binary);char magic[8]{};f.read(magic,8);assert(std::memcmp(magic,"SSDOM002",8)==0);const auto count=word(f);assert(count&&count<=100);
        unsigned total=0;
        for(unsigned n=0;n<count;++n){Ctx ctx;const auto nv=word(f),ni=word(f),no=word(f),np=word(f),nt=word(f),passes=word(f),sorts=word(f);(void)word(f);
            assert(nv<=65536&&ni<=262144&&no+np+nt<=16384&&passes<=64&&sorts<=16384);
            ctx.verts.resize(nv);f.read(reinterpret_cast<char*>(ctx.verts.data()),nv*sizeof(V));ctx.idx.resize(ni);f.read(reinterpret_cast<char*>(ctx.idx.data()),ni*4);assert(f);
            const auto polys=[&](auto& list,unsigned size){for(unsigned i=0;i<size;++i){P p;p.first=word(f);p.count=word(f);p.tcw.full=word(f);p.pcw.full=word(f);p.isp.full=word(f);p.tsp.full=word(f);p.tileclip=word(f);list.push_back(p);}};
            polys(ctx.global_param_op,no);polys(ctx.global_param_pt,np);polys(ctx.global_param_tr,nt);
            for(unsigned i=0;i<passes;++i){(void)word(f);(void)word(f);auto tr_count=word(f),sorted=word(f),automatic=word(f);ctx.render_passes.push_back({bool(automatic),tr_count,sorted});}
            for(unsigned i=0;i<sorts;++i)ctx.sortedTriangles.push_back({word(f),word(f),word(f)});
            unsigned proven[3]{};
            auto triangle=[&](unsigned list,unsigned poly,uint32_t a,uint32_t b,uint32_t d){if(primer::proven_triangle("T9507N",ctx,list,poly,a,b,d))++proven[list];};
            for(unsigned list=0;list<2;++list){const auto& ps=list?ctx.global_param_pt:ctx.global_param_op;
                for(unsigned i=0;i<ps.size();++i)for(unsigned j=ps[i].first+2;j<ps[i].first+ps[i].count;++j)triangle(list,i,ctx.idx[j-2],ctx.idx[j-1],ctx.idx[j]);}
            for(const auto& draw:ctx.sortedTriangles)for(unsigned j=draw.first;j+2<draw.first+draw.count;j+=3)triangle(2,draw.polyIndex,ctx.idx[j],ctx.idx[j+1],ctx.idx[j+2]);
            assert(proven[0]==12&&proven[1]==12&&proven[2]==12);total+=proven[0]+proven[1]+proven[2];
        }
        std::cout<<"Actual complete native list-primer triangles="<<total<<" frames="<<count<<'\n';
    }
    std::cout<<"Native list-primer full-quad/range/material/frame proof passed\n";
}
