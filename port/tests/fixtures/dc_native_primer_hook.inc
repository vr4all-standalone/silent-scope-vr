// Verbatim proposed0017 renderer helper; SHA256 0dad0bf423d1d0040708677a6e556ff26a5079b1f92b1570cbf6e9dbbfc09cd8
#if !defined(GLES2)
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

