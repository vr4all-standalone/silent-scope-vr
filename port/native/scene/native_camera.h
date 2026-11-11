// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "wider_view.h"
#include <functional>
#include <optional>
namespace silent_scope::scene {
// Installed/used only by the emulation thread. Clear before the driver dies.
struct NativeCameraProvider {
    std::function<WiderViewState()> state;
    std::function<void()> poll;
    std::function<void(const char*)> diagnostic;
};
inline NativeCameraProvider& native_camera_provider(){static NativeCameraProvider provider;return provider;}
inline void set_native_camera_provider(std::function<WiderViewState()> state,std::function<void()> poll,std::function<void(const char*)> diagnostic={}){native_camera_provider()={std::move(state),std::move(poll),std::move(diagnostic)};}
inline void clear_native_camera_provider(){native_camera_provider()={};}
inline std::optional<WiderViewState> native_camera_state(){const auto& provider=native_camera_provider();return provider.state?std::optional<WiderViewState>(provider.state()):std::nullopt;}
inline void report_native_camera_issue(const char* reason){if(const auto& diagnostic=native_camera_provider().diagnostic)diagnostic(reason);}
inline void poll_native_camera(){if(const auto& poll=native_camera_provider().poll)poll();}
}
