// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "voodoo_capture.h"
#include <bit>
#include <ostream>
#include <unordered_map>

namespace silent_scope::scene {
// Explicit little-endian CVTRC005 records; never dump pointer-bearing C++ structures.
inline bool write_trace(std::ostream& out,const RawFrame& frame) {
    static_assert(std::endian::native==std::endian::little);
    auto write=[&](const auto& value) { out.write(reinterpret_cast<const char*>(&value),sizeof(value)); };
    std::vector<std::shared_ptr<const TextureImage>> textures;
    std::unordered_map<const TextureImage*,uint32_t> index;
    for (const auto& tri:frame.triangles) for (const auto& texture:tri.texture) {
        if (texture && !index.contains(texture.get())) {
            index[texture.get()]=uint32_t(textures.size()); textures.push_back(texture);
        }
    }
    out.write("CVTRC005",8); write(frame.sequence); write(frame.width); write(frame.height);
    write(frame.clear_color); write(frame.x_offset); write(frame.y_offset);
    write(uint32_t(frame.complete)); write(uint32_t(frame.triangles.size())); write(uint32_t(textures.size()));
    write(uint32_t(frame.background.size()));
    out.write(reinterpret_cast<const char*>(frame.background.data()),std::streamsize(frame.background.size()*4));
    write(uint32_t(frame.background_depth.size()));
    out.write(reinterpret_cast<const char*>(frame.background_depth.data()),std::streamsize(frame.background_depth.size()*2));
    for (const auto& tri:frame.triangles) {
        for (const auto& v:tri.vertex) {
            for (double value:{v.x,v.y,v.z,v.q,v.s[0],v.s[1],v.t[0],v.t[1],v.tq[0],v.tq[1],v.r,v.g,v.b,v.a}) write(value);
        }
        const auto& s=tri.state;
        for (uint32_t value:{s.color_path,s.alpha_mode,s.depth_mode,s.fog_mode,s.texture_mode,
            s.color0,s.color1,s.fog_color,s.chroma_key,tri.texture_mode1,tri.alpha_reference,tri.depth_constant,
            tri.clip_left,tri.clip_right,tri.clip_top,tri.clip_bottom}) write(value);
        for (const auto& texture:tri.texture) write(texture?index.at(texture.get()):UINT32_MAX);
        for(auto value:tri.fog_table){write(value);}write(tri.fog_delta_mask);write(uint32_t(tri.overlay));
    }
    for (const auto& texture:textures) {
        write(texture->id); write(texture->generation); write(texture->width); write(texture->height);
        write(texture->lod);
        out.write(reinterpret_cast<const char*>(texture->pixels.data()),std::streamsize(texture->pixels.size()*4));
    }
    return bool(out);
}
}

