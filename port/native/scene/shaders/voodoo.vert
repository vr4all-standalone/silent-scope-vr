#version 330
in vec3 in_position;
in vec3 in_tex;
in vec4 in_color;
uniform vec2 u_size;
uniform int u_y_flip;
uniform float u_eye_shift;
uniform bool u_spatial;
uniform mat4 u_view_projection;
uniform vec4 u_lens;
uniform float u_world_scale;
uniform bool u_panel;
uniform float u_panel_distance;
uniform float u_hud_size;
out vec3 v_tex;
out vec4 v_color;
out float v_depth;
out vec2 v_raster;
void main() {
    float depth=1.0/max(in_position.z,0.00001);
    float x=in_position.x-u_eye_shift*in_position.z;
    float y=(u_y_flip!=0)?u_size.y-in_position.y:in_position.y;
    vec2 ndc=vec2(x/u_size.x*2.0-1.0,1.0-y/u_size.y*2.0);
    gl_Position=vec4(ndc*depth,depth*1.0000002-0.02,depth);
    if(u_spatial) {
        float metres=u_panel?u_panel_distance:depth*u_world_scale;
        vec3 camera=vec3((in_position.x-u_lens.z)/u_lens.x*metres,
                         (u_lens.w-y)/u_lens.y*metres,-metres);
        camera.xy*=u_hud_size;
        gl_Position=u_view_projection*vec4(camera,1.0);
    }
    v_tex=in_tex*depth;
    // Voodoo's iterated colors are affine in original screen space.
    v_color=in_color*depth;
    v_depth=depth;
    v_raster=vec2(x,y)*depth;
}
