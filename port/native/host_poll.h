// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "shared_runtime.h"
#include "scene/native_camera.h"
#include "operator_service.h"
#include <cstdlib>
#include <string_view>
namespace silent_scope {
// Called on the emulation thread, including OSD updates while native time is
// paused. A screen-update-only resume hook would deadlock the settings menu.
template<class Machine> void poll_host(Machine& machine) {
    if(!std::getenv("SILENT_SCOPE_VR")||std::getenv("SILENT_SCOPE_SIMULATION")||std::string_view(machine.system().name)!="sscope")return;
    auto& runtime=shared_runtime();
    if(runtime.quit){machine.schedule_exit();return;}
    if(!runtime.ready)return;
    scene::poll_native_camera();
    const auto command=runtime.host_command();
    const bool pause=command.pause;
    if(pause&&!machine.paused())machine.pause();
    else if(!pause&&machine.paused())machine.resume();
    const auto request=command.service_request;
    if(request!=runtime.service_consumed){
        runtime.service_consumed=request;
        runtime.service_pulse_until=!pause&&runtime.native_service_state==int(OperatorState::Normal)?machine.time().as_double()+.25:-1;
    }
    if(pause)runtime.service_pulse_until=-1;
    CabinetInput input;bool start,coin,service;
    {std::lock_guard lock(runtime.input_mutex);input=runtime.input;start=runtime.start;coin=runtime.coin;service=runtime.service;}
    service=machine.time().as_double()<runtime.service_pulse_until;
    if(pause){input={};start=coin=service=false;}
    auto set=[&](const char* tag,uint32_t mask,uint32_t value){
        if(auto* port=machine.root_device().ioport(tag))if(auto* field=port->field(mask))field->set_value(value);
    };
    set(":ANALOG1",0x7ff,input.yaw);set(":ANALOG2",0x7ff,input.pitch);
    set(":IN0",0x04,input.trigger);set(":IN0",0x80,start);
    set(":IN2",0x80,coin);set(":IN2",0x10,service);
}
}
