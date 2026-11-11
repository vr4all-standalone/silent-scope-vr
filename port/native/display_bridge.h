// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

namespace silent_scope {
enum class Display : unsigned { Main, Scope };
struct RasterFrame {
    uint64_t generation{}, sequence{};
    double emulated_seconds{};
    uint32_t width{}, height{};
    double aspect{4.0/3.0};
    std::vector<uint32_t> pixels;
};

// Each reader owns a completed immutable snapshot. No queue grows behind either display.
class DisplayBridge {
    mutable std::mutex mutex_;
    uint64_t generation_{1};
    std::array<std::shared_ptr<const RasterFrame>,2> latest_;
public:
    uint64_t generation() const { std::lock_guard lock(mutex_); return generation_; }
    bool publish(Display display, RasterFrame frame) {
        auto index=static_cast<unsigned>(display);
        if(index>=latest_.size() || !frame.sequence || !frame.width || !frame.height ||
           frame.width>2048 || frame.height>2048 ||
           uint64_t(frame.width)*frame.height!=frame.pixels.size() ||
           !std::isfinite(frame.emulated_seconds) || frame.emulated_seconds<0 ||
           !std::isfinite(frame.aspect) || frame.aspect<.25 || frame.aspect>4) return false;
        auto snapshot=std::make_shared<const RasterFrame>(std::move(frame));
        std::shared_ptr<const RasterFrame> previous;
        {
            std::lock_guard lock(mutex_);
            if(snapshot->generation!=generation_) return false;
            auto& target=latest_[index];
            if(target && (snapshot->sequence<=target->sequence ||
                          snapshot->emulated_seconds<target->emulated_seconds)) return false;
            previous.swap(target);
            target=std::move(snapshot);
        }
        return true;
    }
    std::shared_ptr<const RasterFrame> acquire(Display display) const {
        auto index=static_cast<unsigned>(display);
        if(index>=latest_.size()) return {};
        std::lock_guard lock(mutex_);return latest_[index];
    }
    void reset() {
        std::array<std::shared_ptr<const RasterFrame>,2> previous;
        {std::lock_guard lock(mutex_);++generation_;previous.swap(latest_);}
    }
};
}
