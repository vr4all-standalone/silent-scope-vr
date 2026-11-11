#pragma once
namespace primer_gpu_fixture {
inline constexpr char Helper[]=R"PRIMER(#if !defined(GLES2)
template<u32 Type, bool SortingEnabled>
static bool drawSilentScopeNativePrimers(const PolyParam* params, u32 first, u32 count,
    bool triangles, std::size_t polygon)
{
    if (Type != ListType_Opaque && Type != ListType_Punch_Through && Type != ListType_Translucent)
        return false;
    const unsigned list = Type == ListType_Opaque ? 0u : Type == ListType_Punch_Through ? 1u : 2u;
    return silent_scope::dc_native_primer::draw_filtered(settings.content.gameId, *gl.rendContext,
        list, polygon, first, count, triangles, vr::xr::currentEye() != nullptr && bool(config::VrReproject),
        [&](const silent_scope::dc_native_primer::Range& range) {
            SetGPState<Type, SortingEnabled>(params, range.flip ? 1u : 0u);
            glDrawElements(triangles ? GL_TRIANGLES : GL_TRIANGLE_STRIP, range.count, gl.index_type,
                (GLvoid*)(gl.get_index_size() * range.first));
        });
}
#endif

)PRIMER";
inline constexpr char Depth[]=R"PRIMER(#if !defined(GLES2)
                if (silent_scope::dc_native_primer::draw_filtered(settings.content.gameId, *gl.rendContext,
                        2u, gl.rendContext->sortedTriangles[p].polyIndex,
                        gl.rendContext->sortedTriangles[p].first, gl.rendContext->sortedTriangles[p].count,
                        true, vr::xr::currentEye() != nullptr && bool(config::VrReproject),
                        [&](const silent_scope::dc_native_primer::Range& range) {
                            silentScopeHudUniforms(silent_scope::dc_hud::Layer::World, 0,
                                gl.modvol_shader.vrOverlay, gl.modvol_shader.vrHudSize, 1.f, ShaderUniforms.vrHudSize);
                            SetCull(params->isp.CullMode ^ 1);
                            glDrawElements(GL_TRIANGLES, range.count, gl.index_type,
                                (GLvoid*)(gl.get_index_size() * range.first));
                        }))
                    continue;
#endif
)PRIMER";
}
