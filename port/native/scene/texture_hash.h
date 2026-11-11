// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "scene_bridge.h"
#include <unordered_map>

namespace silent_scope::scene {
inline uint64_t texture_content_hash(const TextureImage& image) {
    uint64_t hash=1469598103934665603ull;
    for(auto pixel:image.pixels){hash^=pixel;hash*=1099511628211ull;}
    for(auto value:{image.width,image.height,image.lod}){hash^=value;hash*=1099511628211ull;}
    return hash;
}

// RawFrame retains immutable images. Reset before preparing another frame:
// snapshot addresses can be recycled once its previous owner is released.
class FrameTextureHashes {
    std::unordered_map<const TextureImage*,uint64_t> hashes_;
    size_t hashed_pixels_{};
public:
    void reset(){hashes_.clear();hashed_pixels_=0;}
    uint64_t get(const TextureImage& image) {
        if(auto found=hashes_.find(&image);found!=hashes_.end())return found->second;
        auto hash=texture_content_hash(image);hashed_pixels_+=image.pixels.size();
        hashes_.emplace(&image,hash);return hash;
    }
    size_t hashed_pixels() const{return hashed_pixels_;}
};
}

