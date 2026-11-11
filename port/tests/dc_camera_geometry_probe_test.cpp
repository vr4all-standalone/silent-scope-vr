#include "dreamcast/dc_camera_geometry_probe.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <vector>
using namespace silent_scope::dreamcast;
struct Word{uint32_t full{};};
struct Vertex {float x{},y{},z{},u{},v{};unsigned char col[4]{};};
struct Poly {uint32_t first{},count{};Word isp,tsp,tcw,pcw;uint32_t tileclip{};};
struct Pass {bool autosort{};uint32_t op_count{},pt_count{},tr_count{},sorted_tr_count{};};
struct Sorted {uint32_t polyIndex{},first{},count{};};
struct Context {CameraContext dcCamera;std::vector<Vertex> verts;std::vector<uint32_t> idx;std::vector<Poly> global_param_op,global_param_pt,global_param_tr;std::vector<Pass> render_passes;std::vector<Sorted> sortedTriangles;};
int main(){
    const auto directory=std::filesystem::absolute("ports/silent-scope-vr/private/flycast-research/geometry-unit-scratch");
    std::filesystem::create_directories(directory);
    auto limits=camera_geometry_probe_limits(false,"80");assert(limits.end==80&&limits.records==80);
    assert(camera_geometry_probe_limits(false,nullptr).end==60&&camera_geometry_probe_limits(false,nullptr).records==40);
    assert(camera_geometry_probe_limits(true,"80").end==20);
    assert(camera_geometry_probe_limits(false,"81").end==60);
    Context context;context.dcCamera.supported=true;context.dcCamera.candidates_captured=true;context.dcCamera.code_guard_valid=true;
    context.dcCamera.generation=2;context.dcCamera.submit_sequence=123;context.dcCamera.native_seconds=45;
    context.dcCamera.sdk_words[0]=0x43390000;
    context.dcCamera.cache_captured=true;context.dcCamera.cache_fx=185;context.dcCamera.cache_fy=185;context.dcCamera.cache_cx=320;context.dcCamera.cache_cy=240;
    context.verts={{320,240,0.01f,0,0,{255,255,255,255}},{400,240,0.01f,1,0,{}},{400,300,0.01f,1,1,{}}};
    context.idx={0,1,2,0xffffffff,0,1,2};context.global_param_op={{0,7,{},{},{},{},{}}};
    context.render_passes={{false,1,0,0,0}};
    CameraGeometryProbe early(directory,40,3,20);
    context.dcCamera.native_seconds=3;assert(early.record(context));
    ++context.dcCamera.submit_sequence;context.dcCamera.native_seconds=20;assert(!early.record(context));
    for(const auto& path:early.paths())std::filesystem::remove(path);
    context.dcCamera.native_seconds=45;
    CameraGeometryProbe probe(directory,2);
    assert(probe.record(context));
    assert(!probe.record(context)); // render calls/eyes may repeat a submitted context
    context.dcCamera.is_rtt=true;context.dcCamera.submit_sequence=124;
    assert(probe.record(context));assert(probe.records()==2);
    ++context.dcCamera.submit_sequence;assert(!probe.record(context)); // hardcap
    for(const auto& path:probe.paths()){
        std::ifstream input(path,std::ios::binary);char magic[8];input.read(magic,8);assert(std::string(magic,8)=="SSDCG001");
        const auto word=[&](){unsigned char bytes[4];input.read(reinterpret_cast<char*>(bytes),4);return uint32_t(bytes[0])|(uint32_t(bytes[1])<<8)|(uint32_t(bytes[2])<<16)|(uint32_t(bytes[3])<<24);};
        const uint32_t metadataSize=word();assert(word()==3&&word()==7&&word()==1);
        std::string metadata(metadataSize,' ');input.read(metadata.data(),metadataSize);
        assert(metadata.find("\"43390000\"")!=std::string::npos);
        assert(metadata.find("\"native_seconds\":45")!=std::string::npos);
        assert(metadata.find("\"render_passes\":[[0,1,0,0,0]]")!=std::string::npos);
        assert(metadata.find("\"cache_fx\":185")!=std::string::npos&&metadata.find("\"cache_cy\":240")!=std::string::npos);
        assert(word()==0x43a00000); // bit-exact320px first vertex
        input.seekg(std::streamoff(24+metadataSize+3*24+3*4));assert(word()==0xffffffff);
        input.seekg(0,std::ios::end);assert(input.tellg()==std::streamoff(24+metadataSize+3*24+7*4+32));input.close();std::filesystem::remove(path);
    }
    CameraGeometryProbe guard(directory);
    context.dcCamera.candidates_captured=false;assert(!guard.record(context));
    context.dcCamera.candidates_captured=true;context.dcCamera.code_guard_valid=false;assert(!guard.record(context));
    context.dcCamera.code_guard_valid=true;context.dcCamera.native_seconds=60;assert(!guard.record(context));
    context.dcCamera.native_seconds=45;context.idx[2]=99;assert(!guard.record(context));
    context.idx[2]=2;context.global_param_op[0].count=8;assert(!guard.record(context));
    context.global_param_op[0].count=3;context.verts.resize(65537);assert(!guard.record(context));
    context.verts.resize(3);context.global_param_tr={{3,1,{},{},{},{},{}}};
    context.render_passes={{true,1,0,1,1}};context.sortedTriangles={{0,0,3}};
    assert(!guard.record(context)); // sorted TR is a vertex range, not an index range
    context.global_param_tr[0]={0,3,{},{},{},{},{}};
    CameraGeometryProbe sortedProbe(directory,1);assert(sortedProbe.record(context));
    std::filesystem::remove(sortedProbe.paths().front());
    context.global_param_tr[0]={3,4,{},{},{},{},{}};context.render_passes={{false,1,0,1,0}};context.sortedTriangles.clear();
    CameraGeometryProbe unsortedProbe(directory,1);assert(unsortedProbe.record(context)); // index range beyond verts is valid
    std::filesystem::remove(unsortedProbe.paths().front());
    assert(guard.records()==0);
    CameraGeometryProbe extended(directory,80,40,80);context.dcCamera.native_seconds=79.9;
    assert(extended.record(context));std::filesystem::remove(extended.paths().front());
    ++context.dcCamera.submit_sequence;context.dcCamera.native_seconds=80;assert(!extended.record(context));
    std::filesystem::remove(directory);
}
