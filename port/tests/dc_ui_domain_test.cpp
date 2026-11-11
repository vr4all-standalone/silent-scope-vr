#include "dreamcast/dc_ui_domain.h"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <vector>
using namespace silent_scope::dc_ui;
struct V {float x{},y{},z{},u{},v{};uint8_t col[4]{0,0,0,255};};
struct P {uint32_t first{},count{};struct W{uint32_t full{};}tcw{},pcw{},isp{},tsp{};uint32_t tileclip{};};
struct Pass {uint32_t op_count{},pt_count{},tr_count{},sorted_tr_count{};bool autosort{};};
struct Sorted {uint32_t polyIndex{},first{},count{};};
struct Ctx {
    bool isRTT{};unsigned framebufferWidth{640},framebufferHeight{480};
    struct {bool supported{true};uint64_t generation{3},submit_sequence{10};}dcCamera;
    struct {bool supported{true},valid{};uint64_t generation{3},submit_sequence{10};}dcLens;
    std::vector<V> verts;std::vector<uint32_t> idx;std::vector<P> global_param_op,global_param_pt,global_param_tr;
    std::vector<Pass> render_passes;std::vector<Sorted> sortedTriangles;
};
void quad(Ctx& ctx,std::vector<P>& list,float x,float y,float w,float h,float q){
    const uint32_t start=uint32_t(ctx.verts.size()),first=uint32_t(ctx.idx.size());
    ctx.verts.insert(ctx.verts.end(),{{x,y,q},{x+w,y,q},{x,y+h,q},{x+w,y+h,q}});
    ctx.idx.insert(ctx.idx.end(),{start,start+1,start+2,start+3});list.push_back({first,4,{0x10012345},{8}});
}
Ctx warning(){Ctx c;quad(c,c.global_param_op,0,0,640,480,.1f);quad(c,c.global_param_pt,100,100,440,280,1.f);
    c.render_passes.push_back({1,1,0,0,false});return c;}
void captured_background(Ctx& c){
    c.verts[0]={-256,0,8.999988494906574e-6f};c.verts[1]={896,0,8.999988494906574e-6f};
    c.verts[2]={-256,480,8.999988494906574e-6f};c.verts[3]={896,480,8.999988494906574e-6f};
    auto& p=c.global_param_op[0];p.tcw.full=0;p.pcw.full=2;p.isp.full=0xe0800000;p.tsp.full=0x208004c0;
}
Domain domain(const Ctx& c,bool owned=false){return classify("T9507N",&c,owned);}
uint32_t word(std::ifstream& f){uint32_t v{};f.read(reinterpret_cast<char*>(&v),4);assert(f);return v;}
int main(int argc,char** argv){
    // Prior fallback was !native3d: missing/invalid lens must not authorize Left.
    auto c=warning();assert(domain(c)==Domain::NativeUi);
    auto actual_background=c;captured_background(actual_background);assert(domain(actual_background)==Domain::NativeUi);
    auto mutated=actual_background;mutated.global_param_op[0].tsp.full^=1;assert(domain(mutated)==Domain::Unknown);
    mutated=actual_background;mutated.verts[0].x=-255;assert(domain(mutated)==Domain::Unknown);
    mutated=actual_background;mutated.verts[0].col[0]=1;assert(domain(mutated)==Domain::Unknown);
    assert(classify<Ctx>("T9507N",nullptr,false)==Domain::Unknown);
    assert(classify("OTHER",&c,false)==Domain::Unknown);
    auto bad=c;bad.isRTT=true;assert(domain(bad)==Domain::Unknown);
    bad=c;bad.framebufferWidth=512;assert(domain(bad)==Domain::Unknown);
    bad=c;bad.dcCamera.supported=false;assert(domain(bad)==Domain::Unknown);
    bad=c;bad.dcCamera.generation=0;assert(domain(bad)==Domain::Unknown);
    bad=c;bad.verts[4].z=.8f;assert(domain(bad)==Domain::Unknown);
    bad=c;bad.verts[4].z=std::numeric_limits<float>::quiet_NaN();assert(domain(bad)==Domain::Unknown);
    bad=c;bad.idx.back()=5000;assert(domain(bad)==Domain::Unknown);
    bad=c;bad.render_passes.back().tr_count=1;assert(domain(bad)==Domain::Unknown);
    bad=c;bad.global_param_pt[0].count=UINT32_MAX;assert(domain(bad)==Domain::Unknown);
    bad=c;bad.global_param_pt[0].tcw.full=0x0c0a5800;assert(domain(bad)==Domain::Unknown);
    bad.global_param_pt[0].pcw.full=0;assert(domain(bad)==Domain::NativeUi); // Stale TCW on an untextured primitive is irrelevant.
    bad=c;bad.global_param_pt[0].tcw.full=0x5004c800;assert(domain(bad)==Domain::Unknown);
    bad=c;bad.global_param_pt.clear();bad.render_passes[0].pt_count=0;assert(domain(bad)==Domain::Unknown); // Background alone.
    // The title can have several flat layers at different W without being 3D.
    quad(c,c.global_param_tr,200,50,240,80,.5f);c.render_passes[0].tr_count=1;
    assert(domain(c)==Domain::NativeUi);
    // Same actual TR triangles, now expressed as an autosorted batch's index range.
    c.global_param_tr[0].first=8;c.global_param_tr[0].count=4;
    c.idx={0,1,2,3,4,5,6,7,8,9,10,9,10,11};c.sortedTriangles={{0,8,6}};
    c.render_passes[0].sorted_tr_count=1;c.render_passes[0].autosort=true;
    assert(domain(c)==Domain::NativeUi);
    bad=c;bad.sortedTriangles[0].polyIndex=8;assert(domain(bad)==Domain::Unknown);
    bad=c;bad.sortedTriangles[0].count=5;assert(domain(bad)==Domain::Unknown);
    bad=c;bad.render_passes[0].autosort=false;assert(domain(bad)==Domain::Unknown);
    // A known owned world has a separate capability, never implied by !UI.
    c.dcLens.valid=true;assert(domain(c,true)==Domain::World);
    bad=c;bad.dcLens.submit_sequence=9;assert(domain(bad,true)!=Domain::World);
    bad=c;bad.dcLens.valid=false;bad.verts[4].z=.8f;assert(domain(bad,true)==Domain::Unknown);
    if(argc>1){std::ifstream f(argv[1],std::ios::binary);char magic[8]{};f.read(magic,8);const bool full=std::string(magic,8)=="SSDOM002";assert(full||std::string(magic,8)=="SSDOM001");
        const auto frames=word(f);assert(frames>0&&frames<=100);
        unsigned positive=0;
        for(uint32_t n=0;n<frames;++n){Ctx native;const auto nv=word(f),ni=word(f),no=word(f),np=word(f),nt=word(f),passes=word(f),sorts=word(f);
            const auto want=full?word(f):0;assert(want<=1);
            assert(nv<=65536&&ni<=262144&&no+np+nt<=16384&&passes<=64&&sorts<=262144);
            native.verts.resize(nv);for(auto& v:native.verts)f.read(reinterpret_cast<char*>(&v),full?sizeof(V):3*sizeof(float));
            native.idx.resize(ni);f.read(reinterpret_cast<char*>(native.idx.data()),ni*4);assert(f);
            const auto read_polys=[&](std::vector<P>& polys,unsigned count){for(unsigned i=0;i<count;++i){P p;p.first=word(f);p.count=word(f);p.tcw.full=word(f);p.pcw.full=word(f);if(full){p.isp.full=word(f);p.tsp.full=word(f);p.tileclip=word(f);}polys.push_back(p);}};
            read_polys(native.global_param_op,no);read_polys(native.global_param_pt,np);read_polys(native.global_param_tr,nt);
            for(unsigned i=0;i<passes;++i){Pass p;p.op_count=word(f);p.pt_count=word(f);p.tr_count=word(f);p.sorted_tr_count=word(f);p.autosort=word(f)!=0;native.render_passes.push_back(p);}
            for(unsigned i=0;i<sorts;++i)native.sortedTriangles.push_back({word(f),word(f),word(f)});
            const auto expected=want?Domain::NativeUi:Domain::Unknown;
            if(domain(native)!=expected){std::cerr<<"Native fixture "<<n<<" expected "<<want<<" got "<<int(domain(native))<<'\n';return 1;}
            native.dcLens.valid=true;assert(domain(native)==expected); // Valid lens alone never labels a frame World.
            if(!want)assert(domain(native,true)==Domain::World);
            positive+=want;
        }std::cout<<"Verified "<<frames<<" actual native frames: "<<positive<<" NativeUi, "<<frames-positive<<" Unknown without owned world\n";
    }
    std::cout<<"Native UI/World/Unknown domain tests passed\n";
}
