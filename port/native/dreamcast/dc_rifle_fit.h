// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "rifle_mesh.h"
#include "rifle_support.h"
#if __has_include("rifle_awp_mesh.h")
#include "rifle_awp_mesh.h"
#endif

#if __has_include("rifle_cabinet_mesh.h")
#include "rifle_cabinet_mesh.h"
#endif

namespace silent_scope::dc_rifle {
inline constexpr glm::vec3 awp_muzzle{0,.066f,-.8715f};
inline constexpr glm::vec3 awp_ocular{0,.12102f,.03347f};
inline constexpr RifleForegrip awp_foregrip{.025f,-.53f,-.30f};
inline RifleMesh awp_mesh() {
#if __has_include("rifle_awp_mesh.h")
    return make_awp_rifle_mesh();
#else
    return make_rifle_mesh();
#endif
}
#if __has_include("rifle_cabinet_mesh.h")
inline constexpr glm::vec3 muzzle{0, .043267f, -.541551f};
inline constexpr glm::vec3 ocular{0, .14007855f, .04332006f};
inline constexpr RifleForegrip foregrip{.010f, -.35f, -.20f};
inline RifleMesh mesh() { return make_cabinet_rifle_mesh(); }
#else
inline constexpr glm::vec3 muzzle{0, .005f, -.94f};
inline constexpr glm::vec3 ocular{0, .115f, .09f};
inline constexpr RifleForegrip foregrip{};
inline RifleMesh mesh() { return make_rifle_mesh(); }
#endif
}
