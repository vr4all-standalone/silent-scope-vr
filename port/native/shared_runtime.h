// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "display_bridge.h"
#include "rifle_input.h"
#include "scene/voodoo_capture.h"
#include <atomic>
#include <mutex>

namespace silent_scope {
struct SharedRuntime {
    static constexpr uint32_t Abi=0x53435304;
    uint32_t abi{Abi};
    DisplayBridge displays;
    scene::SnapshotSlot<const scene::RawFrame> main_scene;
    std::mutex input_mutex;
    CabinetInput input;
    bool start{},coin{},service{};
    std::atomic<bool> ready{},quit{};
    std::atomic<bool> pause_requested{},service_active{},wider_requested{true},wider_applied{},wider_available{};
    std::atomic<double> native_scene_hz{};
    std::atomic<int> native_service_state{};
    std::atomic<uint64_t> service_request{};
    uint64_t service_consumed{};double service_pulse_until{-1}; // emulation thread only
    struct HostCommand {bool pause;uint64_t service_request;};
    void publish_host_command(bool pause,bool enter_service=false){
        std::lock_guard lock(input_mutex);pause_requested=pause;if(enter_service)++service_request;
    }
    HostCommand host_command(){std::lock_guard lock(input_mutex);return {pause_requested.load(),service_request.load()};}
    void release_inputs() {std::lock_guard lock(input_mutex);input={};start=coin=service=false;}
    void reset() {displays.reset();main_scene.store({});release_inputs();quit=false;pause_requested=false;service_active=false;wider_applied=false;wider_available=false;native_scene_hz=0;native_service_state=0;service_request=0;service_consumed=0;service_pulse_until=-1;}
};
inline SharedRuntime& shared_runtime(){static SharedRuntime runtime;return runtime;}
}
