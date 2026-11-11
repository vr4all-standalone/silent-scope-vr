// SPDX-License-Identifier: GPL-2.0-or-later
// Verbatim prepared depth-loop insertion; SHA256 c5111a90e06d32b41f72e841995fcd6a3eb496499c083faf013e413359069106
// Test wrapper supplies the surrounding renderer loop and original draw fallback.
[[maybe_unused]] static void drawExtractedNativeDepth(const PolyParam* params,int first_batch=0){
for(int p=first_batch;p<first_batch+1;++p){
#if !defined(GLES2)
                // The depth-only pass must follow the exact same per-triangle plane.
                namespace hud = silent_scope::dc_hud;
                if (silent_scope::dc_scope_hud::draw_ranges(silentScopeHudGate(true), *gl.rendContext, *params, gl.rendContext->sortedTriangles[p].polyIndex,
                        gl.rendContext->sortedTriangles[p].first, gl.rendContext->sortedTriangles[p].count,
                        hud::Primitive::Triangles, [&](const hud::DrawRange& range) {
                            if (range.layer == hud::Layer::Ammo && vr::xr::simulationMagazineEmpty()) return;
                            if (range.layer != hud::Layer::World)
                                silentScopeHudUniforms(range.layer, range.first,
                                    gl.modvol_shader.vrOverlay, gl.modvol_shader.vrHudSize, range.native_scale, ShaderUniforms.vrHudSize);
                            SetCull(params->isp.CullMode ^ 1);
                            glDrawElements(GL_TRIANGLES, range.count, gl.index_type,
                                (GLvoid*)(gl.get_index_size() * range.first));
                            if (range.layer != hud::Layer::World)
                                silentScopeHudUniforms(hud::Layer::World, 0,
                                    gl.modvol_shader.vrOverlay, gl.modvol_shader.vrHudSize, 1.f, ShaderUniforms.vrHudSize);
                        }))
                    continue;
                silentScopeHudUniforms(hud::Layer::World, 0,
                    gl.modvol_shader.vrOverlay, gl.modvol_shader.vrHudSize, 1.f, ShaderUniforms.vrHudSize);
#endif
				SetCull(params->isp.CullMode ^ 1);
glDrawElements(GL_TRIANGLES,gl.rendContext->sortedTriangles[p].count,gl.index_type,
 (GLvoid*)(gl.get_index_size()*gl.rendContext->sortedTriangles[p].first));
}}
