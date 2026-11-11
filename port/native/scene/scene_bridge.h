// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "projection.h"
#include "snapshot_slot.h"
#include <atomic>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace silent_scope::scene {
struct TextureImage {
    const uint64_t id, generation;
    const uint32_t width, height, lod;
    const std::vector<uint32_t> pixels;
    TextureImage(uint64_t id_, uint64_t generation_, uint32_t width_, uint32_t height_,
                 std::vector<uint32_t> pixels_,uint32_t lod_=0)
        : id(id_), generation(generation_), width(width_), height(height_), lod(lod_), pixels(std::move(pixels_)) {
        if (!width || !height || width>4096 || height>4096 || lod>8 || uint64_t(width)*height!=pixels.size())
            throw std::invalid_argument("Invalid scene texture dimensions");
    }
};
struct SceneVertex { Vec3 position; double u{},v{}; uint32_t color{0xffffffff}; };
struct RenderState {
    uint32_t color_path{}, alpha_mode{}, depth_mode{}, fog_mode{}, texture_mode{};
    uint32_t color0{}, color1{}, fog_color{}, chroma_key{};
};
struct SceneTriangle {
    std::array<SceneVertex,3> vertices;
    std::shared_ptr<const TextureImage> texture;
    RenderState state;
    bool panel{};
};
struct SceneFrame {
    uint64_t sequence{};
    NativeLens lens;
    std::vector<SceneTriangle> triangles;
    std::vector<std::shared_ptr<const TextureImage>> textures;
};

// One latest-frame slot, with no producer backlog. Each eye pass holds the same reader.
// The frame is copied/moved into const storage, so the producer cannot mutate it after publish.
class SceneBridge {
    SnapshotSlot<const SceneFrame> latest_;
public:
    void publish(SceneFrame frame) {
        latest_.store(std::make_shared<const SceneFrame>(std::move(frame)),std::memory_order_release);
    }
    std::shared_ptr<const SceneFrame> acquire() const {
        return latest_.load(std::memory_order_acquire);
    }
};
}

