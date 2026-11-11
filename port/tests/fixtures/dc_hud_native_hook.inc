// SPDX-License-Identifier: GPL-2.0-or-later
// Verbatim prepared production gldraw helpers; SHA256 15acc0197241e73241e5856ee454f49c42856803f2ed07c060f2765ef8d694fd
#if !defined(GLES2)
static silent_scope::dc_hud::Gate silentScopeHudGate(bool translucent)
{
    return silent_scope::dc_hud::frame_gate(settings.content.gameId, *gl.rendContext,
        vr::xr::currentEye() != nullptr && bool(config::VrReproject), translucent ? 2u : 3u);
}

static void silentScopeHudUniforms(silent_scope::dc_hud::Layer layer, u32 first, GLint overlay, GLint size,
    float nativeScale = 1.f, float worldSize = -1.f)
{
    glm::vec4 value = ShaderUniforms.vrOverlay;
    float xySize = worldSize;
    if (layer != silent_scope::dc_hud::Layer::World)
    {
        const auto plane = silent_scope::dc_hud::placement(vr::worldScale(),
            silent_scope::dc_hud::actual_wider(*gl.rendContext), float(config::VrHudSize), silent_scope::dc_settings::panel_distance_metres());
        if (plane)
        {
            value.x = silent_scope::dc_hud::NativeTanX;
            value.y = silent_scope::dc_hud::NativeTanY;
            value.z = plane.depth;
            value.w = 1.f / gl.rendContext->verts[gl.rendContext->idx[first]].z;
            xySize = silent_scope::dc_hud::render_size(layer, nativeScale, plane); // target-attached UI keeps its native bearing
        }
    }
    if (overlay != -1) glUniform4fv(overlay, 1, &value[0]);
    if (size != -1 && xySize > 0.f) glUniform1f(size, xySize);
}

template<u32 Type, bool SortingEnabled>
static bool drawSilentScopeHud(const PolyParam* params, u32 first, u32 count, bool triangles, std::size_t polygon = SIZE_MAX)
{
    namespace hud = silent_scope::dc_hud;
    const auto gate = silentScopeHudGate(Type == ListType_Translucent);
    return silent_scope::dc_scope_hud::draw_ranges(gate, *gl.rendContext, *params, polygon, first, count,
        triangles ? hud::Primitive::Triangles : hud::Primitive::Strip, [&](const hud::DrawRange& range) {
            SetGPState<Type, SortingEnabled>(params, range.flip ? 1u : 0u);
            if (range.layer == hud::Layer::Ammo && vr::xr::simulationMagazineEmpty()) return;
            if (range.layer != hud::Layer::World)
                silentScopeHudUniforms(range.layer, range.first, CurrentShader->vrOverlay, CurrentShader->vrHudSize, range.native_scale);
            glDrawElements(triangles ? GL_TRIANGLES : GL_TRIANGLE_STRIP, range.count, gl.index_type,
                (GLvoid*)(gl.get_index_size() * range.first));
            if (range.layer != hud::Layer::World)
                silentScopeHudUniforms(hud::Layer::World, 0, CurrentShader->vrOverlay,
                    CurrentShader->vrHudSize, 1.f, ShaderUniforms.vrHudSize);
        });
}
#endif

