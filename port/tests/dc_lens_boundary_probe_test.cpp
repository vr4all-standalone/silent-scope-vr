#include "dreamcast/dc_lens_boundary_probe.h"
#include <cassert>
#include <filesystem>
using namespace silent_scope::dreamcast;
int main(){
    const auto dir=std::filesystem::temp_directory_path()/"silent-scope-lens-boundary-host-test";
    std::filesystem::create_directories(dir);
    LensBoundaryProbe probe(dir,8);unsigned reads=0;auto read=[&](uint32_t a){++reads;return a;};
    assert(!probe.record("OTHER",LensBoundary::ListInit,45,0,0,0,0,0,false,read)&&!reads);
    assert(!probe.record("T9507N",LensBoundary::ListInit,39,0,0,0,0,0,false,read)&&!reads);
    assert(!probe.record("T9507N",LensBoundary::ListInit,60,0,0,0,0,0,false,read)&&!reads);
    assert(!probe.record("T9507N",LensBoundary::Dma,50,0,0,0,0,0,false,read)&&!reads);
    for(unsigned stage=0;stage<4;++stage)
        assert(probe.record("T9507N",LensBoundary(stage),45,7,0x4d5700,123,0xc123400,1024,false,read));
    assert(reads==4*14&&probe.records()==4);
    for(unsigned i=0;i<100;++i)probe.record("T9507N",LensBoundary::Dma,55,7,0,0,0,0,false,read);
    assert(probe.records()==8);const unsigned previous=reads;
    assert(!probe.record("T9507N",LensBoundary::Submit,56,0,0,0,0,0,false,read)&&reads==previous);
    std::filesystem::remove(probe.path()); // only this test's generated file
}
