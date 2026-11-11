#include "dreamcast/dc_scope_composite_probe.h"
#include <cassert>
#include <iostream>
#include <vector>
struct Vertex {float x{},y{},z{1},u{},v{};unsigned char col[4]{255,255,255,255};};
struct Poly {
    unsigned first{},count{4};
    struct {unsigned TexAddr{0x52c000/8},ScanOrder{1},StrideSel{},PixelFmt{1},full{0xabc};}tcw;
    struct {unsigned TexU{5},TexV{5},full{0xdef};}tsp;
    struct {unsigned Texture{1},full{1};}pcw;
    struct {unsigned full{0x123};}isp;
    unsigned tileclip{};
};
struct Context {
    bool isRTT{};unsigned framebufferWidth{640},framebufferHeight{480},fb_W_SOF1{0x600000};
    struct {uint64_t generation{3},submit_sequence{12};}dcCamera;
    std::vector<Vertex> verts{{100,100,1,0,0},{356,100,1,1,0},{100,356,1,0,1},{356,356,1,1,1}};
    std::vector<unsigned> idx{0,1,2,3};
    std::vector<Poly> global_param_op,global_param_pt,global_param_tr{Poly{}};
    struct Pass{bool autosort{true};unsigned tr_count{1},sorted_tr_count{1};};
    std::vector<Pass> render_passes{Pass{}};std::vector<int> sortedTriangles{0};
};
int main(){
    using namespace silent_scope::dc_scope;
    const auto directory=std::filesystem::absolute("scope-composite-probe-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(directory);CompositeProbe probe(directory);Context context;
    assert(!probe.record("OTHER",context,1));
    context.isRTT=true;assert(!probe.record("T9507N",context,1));context.isRTT=false;
    assert(probe.record("T9507N",context,1));
    assert(!probe.record("T9507N",context,1.5));
    for(unsigned i=2;i<20;++i)probe.record("T9507N",context,i);
    assert(probe.records()==8);
    std::ifstream file(probe.path());std::string all((std::istreambuf_iterator<char>(file)),{});
    assert(all.find("candidate_only_not_verified")!=std::string::npos);
    assert(all.find("\"list\":\"translucent\"")!=std::string::npos);
    assert(all.find("\"texture_address\":5423104")!=std::string::npos);
    assert(all.find("\"x\":100")!=std::string::npos);
    assert(all.find("\"storage\":\"vertices\"")!=std::string::npos);
    Context crowded;crowded.global_param_tr.assign(20,Poly{});
    crowded.verts.insert(crowded.verts.end(),{{0,0,1,0,0},{500,0,1,1,0},{0,480,1,0,1},{500,480,1,1,1}});
    crowded.idx.insert(crowded.idx.end(),{4,5,6,7});Poly large;large.first=4;crowded.global_param_tr.push_back(large);
    crowded.render_passes[0].tr_count=unsigned(crowded.global_param_tr.size());
    CompositeProbe late(directory);assert(late.record("T9507N",crowded,30));
    std::ifstream late_file(late.path());std::string late_data((std::istreambuf_iterator<char>(late_file)),{});
    assert(late_data.find("\"poly_index\":20")!=std::string::npos);
    std::cout<<"Scope candidate probe: bounded metadata only, source-address candidates, no suppression passed\n";
}
