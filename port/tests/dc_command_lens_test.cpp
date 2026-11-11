#include "dreamcast/dc_command_lens.h"
#include <cassert>
#include <map>
#include <thread>
#include <string>
using namespace silent_scope::dreamcast;
int main(){
    std::map<uint32_t,uint32_t> memory{{0x8cd134e4,0x8cd134cc},{0x8cd134cc,0x8ce3fea0},{0x8cd134e0,0x8c000000},
        {0x8c100028,0x8c272580},{0x8c10002c,0},{0x8c100000,0x8c200000},{0x8c100030,0x40}};
    auto read=[&](uint32_t a){return memory.at(a);};
    bool packet_matches=true;auto verify=[&](uint32_t,uint32_t length){return packet_matches&&length==32;};
    memory[0x8c1ffffc]=0x00010000; // original count/UFO atpayload-2
    auto span=read_command_commit(0x8c1124b0,0x8c100000,0xe0e3ffa0,1,5,read);
    assert(span.valid&&span.begin==0xe3fea0&&span.length==256&&span.scratch==0x272580&&span.near_count==0);
    SubmittedLens stock;stock.valid=stock.supported=true;stock.generation=7;stock.fx=stock.fy=555;
    stock.width=640;stock.height=480;stock.cx=320;stock.cy=240;stock.factor=1;
    ProjectionRangeLedger<> ledger;ledger.reset(7);ledger.write(0x272580,96,stock);
    memory[0x8c200000]=3;memory[0x8c200004]=0x00020001; // length3, indices0,1,2
    auto lens=resolve_command_lens(span,ledger,read,verify);assert(lens.valid&&span.references==3&&span.scratch_length==96);
    packet_matches=false;assert(!resolve_command_lens(span,ledger,read,verify).valid);packet_matches=true;
    ledger.write(0x2725c0,32,{});assert(!resolve_command_lens(span,ledger,read,verify).valid);
    ledger.write(0x2725c0,32,stock);
    auto word=[&](unsigned n,uint16_t value){const uint32_t address=0x8c200000+(n*2&~3u),shift=(n&1)*16;
        memory[address]=(memory[address]&~(0xffffu<<shift))|(uint32_t(value)<<shift);};
    const uint16_t uv[]{uint16_t(-3),0,123,456,1,123,456,2,123,456,0xbeef};
    for(unsigned n=0;n<11;++n)word(n,uv[n]);
    for(uint32_t type:{0x41u,0x42u}){
        memory[0x8c1ffffc]=0x40010000;
        memory[0x8c100030]=type;span=read_command_commit(0x8c1124b0,0x8c100000,0xe0e3ffa0,0x4001,12,read);
        assert(resolve_command_lens(span,ledger,read,verify).valid&&span.references==3);
        span.poly_words=10;assert(!resolve_command_lens(span,ledger,read,verify).valid);
    }
    memory[0x8c1ffffc]=0x00010000;memory[0x8c100030]=0x40;
    word(0,3);word(1,0);word(2,1);word(3,2);word(4,0);
    span=read_command_commit(0x8c1124b0,0x8c100000,0xe0e3ffa0,0x8c0fad24,6,read);
    assert(span.strip_flags==1&&resolve_command_lens(span,ledger,read,verify).valid); // real overwrittenR13
    word(4,99);assert(!resolve_command_lens(span,ledger,read,verify).valid);
    span=read_command_commit(0,0,0xe0e3ffa0,0,0,read);assert(span.valid&&span.scratch_length==0);
    span=read_command_commit(0x8c1124b0,0x8c100000,0xe0e3fe80,1,5,read);assert(!span.valid);
    memory[0x8cd134e0]=0x10000000;span=read_command_commit(0,0,0xe0e3ffa0,0,0,read);assert(!span.valid);
    // Native source callbacks and render finalization share one diagnostic sink.
    const auto directory=std::filesystem::temp_directory_path()/(
        "silent-scope-lens-log-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    assert(std::filesystem::create_directory(directory));
    {
        CommandLensProbe probe(directory);assert(probe.enabled());
        std::atomic<bool> begin{};
        auto writer=[&](unsigned stage){while(!begin.load(std::memory_order_acquire))std::this_thread::yield();
            for(unsigned i=0;i<1000;++i)probe.record(stage,45.01,7,8,0,span,stock);};
        std::thread source(writer,0),render(writer,1);begin.store(true,std::memory_order_release);
        source.join();render.join();
    }
    {
        CommandLensProbe all(directory,true,80);assert(all.wants(3,79.99,false)&&!all.wants(3,80,false));
        assert(!all.wants(3,39.99,false));
        for(unsigned i=0;i<2001;++i)all.record(3,61,7,8,0,span,stock);
        assert(!all.wants(3,61,false)); // hard2000-row bound evenatunchangednative time
        CommandLensProbe defaults(directory);assert(!defaults.wants(3,60,false));
    }
    unsigned lines=0;
    for(const auto& entry:std::filesystem::directory_iterator(directory)){
        std::ifstream input(entry.path());std::string line;
        while(std::getline(input,line)){
            assert(line.rfind("{\"stage\":",0)==0&&line.back()=='}');
            assert(line.find("\"stage\":",2)==std::string::npos);++lines;
        }
        input.close();assert(std::filesystem::remove(entry.path()));
    }
    assert(lines==4000);assert(std::filesystem::remove(directory));

}

