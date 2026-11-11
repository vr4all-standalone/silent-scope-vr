#include "operator_service.h"
#include <cassert>
#include <map>
using namespace silent_scope;
int main(){
    std::map<uint32_t,uint32_t> words;for(auto [address,value]:OperatorCode)words[address]=value;
    words[0xada70]=0x800ae378;words[0xadb10]=0x800b3a30;uint8_t state=3,mode=14,game=1;
    auto read=[&]{return read_operator_state([&](uint32_t a){return words[a];},[&](uint32_t a){return a==0xae392?state:a==0xae3a2?mode:game;});};
    assert(read()==OperatorState::Normal);state=2;game=0;assert(read()==OperatorState::Service);
    state=3;mode=1;assert(read()==OperatorState::Service); // Native Gun Check / Gun Adjust
    mode=14;assert(read()==OperatorState::Unknown); // No guessed game transition
    game=1;mode=1;assert(read()==OperatorState::Unknown);
    game=0;for(auto internal:{12,13}){mode=uint8_t(internal);assert(read()==OperatorState::Unknown);state=2;assert(read()==OperatorState::Unknown);state=3;}
    for(auto unsupported:{0,1,4,255}){state=uint8_t(unsupported);assert(read()==OperatorState::Unknown);}
    state=2;mode=1;game=0;assert(read()==OperatorState::Service);
    for(auto [address,value]:OperatorCode){words[address]^=1;assert(read()==OperatorState::Unknown);words[address]=value;}
    words[0xada70]=0x800ae380;assert(read()==OperatorState::Unknown);
}
