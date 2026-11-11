// Verbatim proposed0017 depth filter; SHA256 2ce6772ccafd18a22099488c75e48ed125b95d9c1d51c11b2a53a86f3b9cc701
static void drawExtractedNativePrimerDepth(const PolyParam* params){
for(int p=0;p<1;++p){
#if !defined(GLES2)
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

drawExtractedNativeDepth(params,p);
}}
