#version 330
// Voodoo combine rules follow MAME's BSD-licensed voodoo_render.cpp (Aaron Giles).
in vec3 v_tex;
in vec4 v_color;
in float v_depth;
in vec2 v_raster;
uniform highp usampler2D u_native_background_depth;
uniform bool u_test_background_depth;
uniform uint u_native_depth_constant;
uniform vec2 u_size;
uniform vec4 u_native_clip;
uniform sampler2D u_texture;
uniform vec2 u_texture_size;
uniform bool u_textured;
uniform uint u_texture_mode;
uniform uint u_color_path;
uniform uint u_depth_mode;
uniform uint u_alpha_mode;
uniform uint u_fog_mode;
uniform uint u_fog_delta_mask;
uniform uint u_fog_table[64];
uniform vec4 u_fog_color;
uniform vec4 u_color0;
uniform vec4 u_color1;
uniform vec4 u_chroma_key;
out vec4 output_color;
uint bits(uint v,uint start,uint n) { return (v>>start)&((1u<<n)-1u); }
bool bit(uint v,uint at) { return (v&(1u<<at))!=0u; }
vec4 combine_texture(vec4 local) {
    uint m=u_texture_mode;
    vec4 other=vec4(0.0);
    vec3 rgb=bit(m,12u)?vec3(0.0):other.rgb;
    float alpha=bit(m,21u)?0.0:other.a;
    if(bit(m,13u))rgb-=local.rgb;
    if(bit(m,22u))alpha-=local.a;
    uint factor=bits(m,14u,3u);
    vec3 f=factor==1u?local.rgb:factor==2u?vec3(other.a):factor==3u?vec3(local.a):vec3(0.0);
    uint afactor=bits(m,23u,3u);
    float af=afactor==1u||afactor==3u?local.a:afactor==2u?other.a:0.0;
    if(!bit(m,17u))f=255.0-f;
    if(!bit(m,26u))af=255.0-af;
    uint add=bits(m,18u,2u);
    vec3 a=add==1u?local.rgb:add==2u?vec3(local.a):vec3(0.0);
    float aa=bits(m,27u,2u)!=0u?local.a:0.0;
    vec4 result=clamp(floor(vec4(rgb,alpha)*(vec4(f,af)+1.0)/256.0+vec4(a,aa)),0.0,255.0);
    if(bit(m,20u))result.rgb=255.0-result.rgb;
    if(bit(m,29u))result.a=255.0-result.a;
    return result;
}
bool compare_alpha(float a,float b,uint op) {
    if(op==0u)return false;
    if(op==1u)return a<b;
    if(op==2u)return a==b;
    if(op==3u)return a<=b;
    if(op==4u)return a>b;
    if(op==5u)return a!=b;
    if(op==6u)return a>=b;
    return true;
}
uint encode_w(float q) {
    if(q>=1.0)return 0u;
    if(q<1.0/65536.0)return 65535u;
    int exponent=-int(floor(log2(q)))-1;
    return ((uint(exponent)<<12u)|(uint(floor(q*exp2(float(13+exponent))))^8191u))+1u;
}
void main() {
    vec2 native_pixel=v_raster/v_depth;
    if(any(lessThan(native_pixel,u_native_clip.xy))||any(greaterThanEqual(native_pixel,u_native_clip.zw)))discard;
    if(u_test_background_depth) {
        vec2 pixel=v_raster/v_depth;
        if(all(greaterThanEqual(pixel,vec2(0)))&&all(lessThan(pixel,u_size))) {
            uint dest=texelFetch(u_native_background_depth,ivec2(pixel),0).r;
            if(dest!=65535u) {
                int source=int(encode_w(1.0/v_depth));
                int bias=int(u_native_depth_constant)-(u_native_depth_constant>=32768u?65536:0);
                if(bit(u_depth_mode,16u))source=clamp(source+bias,0,65535);
                if(bit(u_depth_mode,20u))source=int(u_native_depth_constant);
                if(!compare_alpha(float(source),float(dest),bits(u_depth_mode,5u,3u)))discard;
            }
        }
    }
    vec4 iterated=floor(v_color/v_depth);
    if(bit(u_color_path,28u)) iterated=clamp(iterated,0.0,255.0);
    else {
        ivec4 raw=ivec4(iterated)&ivec4(4095);
        for(int i=0;i<4;i++) { if(raw[i]==4095)raw[i]=0; if(raw[i]==256)raw[i]=255; }
        iterated=vec4(raw&ivec4(255));
    }
    vec4 texel=vec4(0.0);
    if(u_textured) {
        vec2 st=bit(u_texture_mode,0u)?v_tex.xy/v_tex.z:(v_tex.xy/v_depth);
        texel=combine_texture(floor(texture(u_texture,st/u_texture_size)*255.0+0.001));
    }
    uint cp=u_color_path;
    uint select_rgb=bits(cp,0u,2u),select_alpha=bits(cp,2u,2u);
    vec4 other=select_rgb==0u?iterated:select_rgb==1u?texel:select_rgb==2u?u_color1:vec4(0.0);
    if(bit(u_depth_mode,1u)&&all(equal(ivec3(other.rgb),ivec3(u_chroma_key.rgb))))discard;
    other.a=select_alpha==0u?iterated.a:select_alpha==1u?texel.a:select_alpha==2u?u_color1.a:0.0;
    if(bit(u_depth_mode,13u)&&((int(other.a)&1)==0))discard;
    bool local_constant=bit(cp,7u)?texel.a>=128.0:bit(cp,4u);
    vec4 local=local_constant?u_color0:iterated;
    uint local_alpha=bits(cp,5u,2u);
    local.a=local_alpha==0u?iterated.a:local_alpha==1u?u_color0.a:0.0;
    vec3 rgb=bit(cp,8u)?vec3(0.0):other.rgb;
    float alpha=bit(cp,17u)?0.0:other.a;
    if(bit(cp,9u))rgb-=local.rgb;
    if(bit(cp,18u))alpha-=local.a;
    uint select_factor=bits(cp,10u,3u);
    vec3 factor=select_factor==1u?local.rgb:select_factor==2u?vec3(other.a):
        select_factor==3u?vec3(local.a):select_factor==4u?vec3(texel.a):select_factor==5u?texel.rgb:vec3(0.0);
    uint select_afactor=bits(cp,19u,3u);
    float afactor=select_afactor==1u||select_afactor==3u?local.a:select_afactor==2u?other.a:select_afactor==4u?texel.a:0.0;
    if(!bit(cp,13u))factor=255.0-factor;
    if(!bit(cp,22u))afactor=255.0-afactor;
    uint select_add=bits(cp,14u,2u);
    vec3 add=select_add==1u?local.rgb:select_add==2u?vec3(local.a):vec3(0.0);
    float aadd=bits(cp,23u,2u)!=0u?local.a:0.0;
    vec4 result=clamp(floor(vec4(rgb,alpha)*(vec4(factor,afactor)+1.0)/256.0+vec4(add,aadd)),0.0,255.0);
    if(bit(cp,16u))result.rgb=255.0-result.rgb;
    if(bit(cp,25u))result.a=255.0-result.a;
    if(bit(u_alpha_mode,0u)&&!compare_alpha(result.a,float(u_alpha_mode>>24u),bits(u_alpha_mode,1u,3u)))discard;
    if(bit(u_fog_mode,0u)) {
        vec3 fog=u_fog_color.rgb;
        if(bit(u_fog_mode,5u)) {
            if(!bit(u_fog_mode,2u))fog=clamp(fog+result.rgb,0.0,255.0);
        } else {
            uint d=encode_w(1.0/v_depth);
            if(bit(u_depth_mode,16u))d=uint(clamp(int(d)+int(int(u_native_depth_constant<<16u)>>16),0,65535));
            uint entry=u_fog_table[d>>10u];
            int delta=int(entry&255u);
            int value=int(uint(delta)&u_fog_delta_mask)*int((d>>2u)&255u);
            if(bit(u_fog_mode,7u)&&(delta&2)!=0)value=-value;
            value>>=6;value>>=4;
            int factor=int(entry>>8u)+value+1;
            if(bit(u_fog_mode,1u))fog=vec3(0.0);
            if(!bit(u_fog_mode,2u))fog-=result.rgb;
            // Match pinned MAME's signed scale_imm_and_clamp, then add.
            fog=clamp(floor(fog*float(factor)/256.0),0.0,255.0);
            if(!bit(u_fog_mode,2u))fog=clamp(fog+result.rgb,0.0,255.0);
        }
        result.rgb=fog;
    }
    output_color=result/255.0;
}
