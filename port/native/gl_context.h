// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
namespace silent_scope {
class GLContext {
    void* display_{};void* surface_{};void* context_{};void* window_{};void* config_{};
public:
    GLContext();~GLContext();
    GLContext(const GLContext&)=delete;
    void* display() const{return display_;}
    void* context() const{return context_;}
    void* config() const{return config_;}
};
}

