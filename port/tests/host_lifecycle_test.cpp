#include "shared_runtime.h"
#include <windows.h>
#include <chrono>
#include <thread>
#include <iostream>
int main(int count,char** args) {
    if(count!=2)return 1;
    auto module=LoadLibraryA(args[1]);if(!module){std::cerr<<"Host library not built/loaded\n";return 1;}
    auto start=reinterpret_cast<bool(*)(silent_scope::SharedRuntime*)>(GetProcAddress(module,"silent_scope_xr_start"));
    auto stop=reinterpret_cast<void(*)()>(GetProcAddress(module,"silent_scope_xr_stop"));
    if(!start||!stop)return 1;
    silent_scope::SharedRuntime runtime;runtime.abi=0;
    if(start(nullptr)||start(&runtime))return 1;
    runtime.abi=silent_scope::SharedRuntime::Abi;
    for(int run=0;run<2;++run) {
        if(!start(&runtime)||start(&runtime))return 1;
        for(int i=0;i<200&&!runtime.ready&&!runtime.quit;++i)std::this_thread::sleep_for(std::chrono::milliseconds(10));
        if(!runtime.ready){std::cerr<<"Headless graphics host failed to become ready\n";stop();return 1;}
        stop();stop();if(runtime.ready)return 1;
    }
    FreeLibrary(module);
    std::cout<<"Host ABI validation, repeated startup, shutdown and duplicate-start rejection passed\n";
}
