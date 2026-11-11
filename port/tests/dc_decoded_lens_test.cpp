#include "dreamcast/dc_decoded_lens.h"
#include <cassert>
#include <vector>
using namespace silent_scope::dreamcast;
struct V{float x,y,z;uint8_t col[4]{};};struct P{uint32_t first,count;struct W{uint32_t full{0xe0000000};}isp{};struct Z{uint32_t full{};}tsp{},tcw{},pcw{};uint32_t tileclip{};};
struct Pass{bool autosort;uint32_t tr_count,sorted_tr_count;};struct Batch{uint32_t polyIndex,first,count;};
struct Ctx{
    bool isRTT{};unsigned framebufferWidth{640},framebufferHeight{480};
    struct{bool supported{true};uint64_t generation{7},submit_sequence{90};}dcCamera;
    std::vector<V> verts;std::vector<SubmittedLens> dcVertexLens;std::vector<uint32_t> idx;
    std::vector<P> global_param_op,global_param_pt,global_param_tr;std::vector<Pass> render_passes;std::vector<Batch> sortedTriangles;
    std::vector<int> modtrig;
};
struct Ui{bool known{};bool proven_ui_triangle(const Ctx&,unsigned list,size_t poly,uint32_t,uint32_t,uint32_t)const{return known&&list==2&&poly==0;}};
int main(){
    SubmittedLens wide;wide.supported=wide.valid=true;wide.generation=7;wide.fx=wide.fy=185;
    wide.cx=320;wide.cy=240;wide.width=640;wide.height=480;wide.factor=3;
    auto stock=wide;stock.fx=stock.fy=555;stock.factor=1;
    Ctx c;c.verts={{10,10,.01f},{20,10,.02f},{10,20,.03f}};c.dcVertexLens={wide,wide,wide};
    c.idx={0,1,2,UINT32_MAX,0,1,2,UINT32_MAX,0,1,2,UINT32_MAX,0,1,2};
    c.global_param_op={{0,0},{0,15}};
    auto r=finalize_decoded_lens(c,Ui{});assert(r.lens.valid&&r.lens.fx==185&&r.world_triangles==4&&r.lens.submit_sequence==90);
    c.verts.insert(c.verts.end(),{{0,0,.7f},{5,0,.7f},{0,5,.7f}});c.dcVertexLens.resize(6);
    c.idx.insert(c.idx.end(),{3,4,5});c.global_param_tr={{15,3}};c.render_passes={{false,1,0}};
    r=finalize_decoded_lens(c,Ui{});assert(!r.lens.valid&&r.unknown_triangles==1);
    r=finalize_decoded_lens(c,Ui{true});assert(r.lens.valid&&r.ui_triangles==1); // explicit full-domain UI proof
    c.global_param_op.push_back({15,3,{0x09000000}});
    r=finalize_decoded_lens(c,Ui{true});assert(r.lens.valid&&r.skipped_never_polygons==1);c.global_param_op.pop_back();
    c.global_param_tr[0].isp.full=0x09000000;
    r=finalize_decoded_lens(c,Ui{});assert(r.lens.valid); // native unsortedTR Never is skipped
    c.global_param_pt.push_back({15,3,{0x09000000}});
    r=finalize_decoded_lens(c,Ui{});assert(!r.lens.valid);c.global_param_pt.clear(); // PT forces depth function
    c.render_passes[0].autosort=true;
    r=finalize_decoded_lens(c,Ui{});assert(!r.lens.valid);c.render_passes[0].autosort=false; // presortedstripTR still draws
    c.global_param_tr[0].isp.full=0xe0000000;
    c.dcVertexLens[3]=c.dcVertexLens[4]=c.dcVertexLens[5]=stock;
    r=finalize_decoded_lens(c,Ui{});assert(!r.lens.valid&&r.mixed_triangles==1); // flat555 vs sloped185
    c.dcVertexLens[3]=c.dcVertexLens[4]=c.dcVertexLens[5]=wide;
    c.sortedTriangles={{0,15,3}};c.render_passes={{true,1,1}};c.global_param_tr={{3,3}};
    r=finalize_decoded_lens(c,Ui{});assert(r.lens.valid&&r.world_triangles==5); // sortedTR uses idx batch, notpp range
    c.sortedTriangles[0].polyIndex=1;assert(!finalize_decoded_lens(c,Ui{}).lens.valid);c.sortedTriangles[0].polyIndex=0;
    c.dcVertexLens.pop_back();assert(!finalize_decoded_lens(c,Ui{}).lens.valid);c.dcVertexLens.push_back(wide);
    c.modtrig.push_back(1);assert(!finalize_decoded_lens(c,Ui{}).lens.valid);c.modtrig.clear();
    c.dcCamera.generation=8;assert(!finalize_decoded_lens(c,Ui{}).lens.valid);c.dcCamera.generation=7;
    c.isRTT=true;assert(!finalize_decoded_lens(c,Ui{}).lens.valid);c.isRTT=false;
    // Fixed native list primers are neither world nor HUD. Require the complete
    // exact unprojected sprite, even when only one triangle is being considered.
    const uint32_t first_vertex=uint32_t(c.verts.size()),first_index=uint32_t(c.idx.size());
    c.verts.insert(c.verts.end(),{{-1.5f,-.5f,.1f},{-.5f,-.5f,.1f},{-1.5f,-1,.1f},{-.5f,-1,.1f}});
    c.dcVertexLens.resize(c.verts.size());
    c.idx.insert(c.idx.end(),{first_vertex,first_vertex+1,first_vertex+2,first_vertex+3});
    c.global_param_pt.push_back({first_index,4,{0x09000000},{0x00800440},{0},{0xa0800000},0x100200});
    r=finalize_decoded_lens(c,Ui{});assert(r.lens.valid&&r.skipped_primers==2&&r.ui_triangles==0);
    c.verts.back().col[3]=1;r=finalize_decoded_lens(c,Ui{});assert(!r.lens.valid&&r.unknown_triangles==2&&r.skipped_primers==0);

}
