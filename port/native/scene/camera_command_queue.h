// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "wider_view.h"
#include <cstddef>
#include <deque>
#include <optional>
namespace silent_scope::scene {
struct AcceptedCommandCamera {
    bool accepted{};
    std::optional<WiderViewState> camera;
};
// A side queue of triangle-only metadata follows the native Voodoo FIFO order.
// Ordinary register/texture/LFB writes are not entries. An explicit no-provider
// legacy stamp differs from underflow; neither may borrow a later live camera.
class CameraCommandQueue {
    const size_t capacity_;
    std::deque<std::optional<WiderViewState>> pending_;
    bool poisoned_{};
public:
    // Native PCI+framebuffer FIFOs hold fewer than65536 triangle commands.
    explicit CameraCommandQueue(size_t capacity=65536):capacity_(capacity){}
    bool accept(std::optional<WiderViewState> camera){
        if(poisoned_)return false;
        if(pending_.size()>=capacity_){pending_.clear();poisoned_=true;return false;}
        pending_.push_back(std::move(camera));return true;
    }
    AcceptedCommandCamera consume(){
        if(poisoned_||pending_.empty())return {};
        auto camera=std::move(pending_.front());pending_.pop_front();return {true,std::move(camera)};
    }
    void reset(){pending_.clear();poisoned_=false;}
    size_t pending()const{return pending_.size();}
    bool poisoned()const{return poisoned_;}
};
// Execution scope carries an already accepted stamp through the synchronous
// triangle setup/capture call, even if FIFO execution occurs much later.
inline const std::optional<WiderViewState>*& executing_camera_stamp(){
    static thread_local const std::optional<WiderViewState>* stamp=nullptr;return stamp;
}
class CameraCommandExecution {
    std::optional<WiderViewState> camera_;
    const std::optional<WiderViewState>* previous_;
    bool enabled_;
public:
    explicit CameraCommandExecution(std::optional<WiderViewState> camera,bool enabled=true)
        :camera_(std::move(camera)),previous_(executing_camera_stamp()),enabled_(enabled){
        if(enabled_)executing_camera_stamp()=&camera_;
    }
    ~CameraCommandExecution(){if(enabled_)executing_camera_stamp()=previous_;}
    CameraCommandExecution(const CameraCommandExecution&)=delete;
    CameraCommandExecution& operator=(const CameraCommandExecution&)=delete;
};
inline AcceptedCommandCamera executing_command_camera(){
    const auto* stamp=executing_camera_stamp();return stamp?AcceptedCommandCamera{true,*stamp}:AcceptedCommandCamera{};
}
}
