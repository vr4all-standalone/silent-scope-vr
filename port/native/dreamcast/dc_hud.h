// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <algorithm>
#include <cmath>
#include <limits>
#include <array>
#include <iterator>
namespace silent_scope::dc_hud {
constexpr float NativeTanX=320.f/555.f,NativeTanY=240.f/555.f;
enum class Layer : uint8_t { World, Font, Life, Countdown, TimerDetail, AmmoPlate, Ammo, ScopeFallback, TargetMarker, TargetLabel, MessageFont, MessagePlate };
struct Gate { std::string_view product; unsigned width{},height{}; bool is_rtt{},vr_world{}; uint64_t generation{},submit{}; unsigned list_id{3}; };
struct Placement { float depth{},size{1}; explicit operator bool()const{return depth>0;} };
inline bool enabled(const Gate& g){return g.product=="T9507N"&&g.width==640&&g.height==480&&!g.is_rtt&&g.vr_world&&g.generation&&g.submit&&g.list_id==2;}
template<class Context> Gate frame_gate(std::string_view title,const Context& ctx,bool vr_world,unsigned list_id){
    return {title,ctx.framebufferWidth,ctx.framebufferHeight,ctx.isRTT,vr_world,ctx.dcCamera.generation,ctx.dcCamera.submit_sequence,list_id};
}
template<class Context> bool actual_wider(const Context& ctx){
    const auto& lens=ctx.dcLens;
    return lens.supported&&lens.valid&&lens.factor==3.f&&lens.width==640&&lens.height==480&&
        lens.fx==185.f&&lens.fy==185.f&&lens.cx==320.f&&lens.cy==240.f&&
        lens.generation!=0&&lens.generation==ctx.dcCamera.generation&&lens.submit_sequence!=0&&lens.submit_sequence==ctx.dcCamera.submit_sequence;
}
// vrMat consumes game units. This keeps the physical plane at saved metres as
// world scale changes. Saved XY sizing is allowed only by actual Wider evidence.
inline Placement placement(float world_scale,bool actual_wider,float saved_size,float panel_metres=1.8f){
    if(!std::isfinite(world_scale)||world_scale<1e-6f)return {};
    if(!std::isfinite(panel_metres)||panel_metres<.5f||panel_metres>10.f)panel_metres=1.8f;
    return {panel_metres/world_scale,actual_wider&&std::isfinite(saved_size)?std::clamp(saved_size,.5f,2.f):1.f};
}
struct Decision {Layer layer{Layer::World};float native_scale{1};};
// Target-attached artwork must keep its game-camera bearing when HUD size is
// changed. Restore the measured /3 projection, but do not move it toward/away
// from the screen centre with the fixed scoreboard's user sizing.
inline float render_size(Layer layer,float native_scale,Placement plane){
    return native_scale*((layer==Layer::TargetMarker||layer==Layer::TargetLabel)?1.f:plane.size);
}
template<class Polygon,class Vertex> Decision target_indicator(const Polygon& p,const Vertex& a,const Vertex& b,const Vertex& c){
    if(p.isp.full!=0xc8800000||p.tileclip!=0x00100200||p.tsp.full!=0x9491a4e4||p.tcw.full!=0x100b0a00||p.pcw.full!=0x828c002a)return {};
    const std::array<const Vertex*,3> vs{&a,&b,&c};
    float x0=a.x,x1=a.x,y0=a.y,y1=a.y;
    for(const auto* v:vs){
        if(!std::isfinite(v->x)||!std::isfinite(v->y)||!std::isfinite(v->z)||!std::isfinite(v->u)||!std::isfinite(v->v)||
           std::abs(v->z-.3333333432674408f)>2e-7f||v->col[0]!=176||v->col[1]!=176||v->col[2]!=176||v->col[3]!=255)return {};
        x0=std::min(x0,v->x);x1=std::max(x1,v->x);y0=std::min(y0,v->y);y1=std::max(y1,v->y);
    }
    const float width=x1-x0,height=y1-y0;
    // Moving target markers may intersect an edge; their measured artwork size
    // bounds the overscan. No surveyed fixed screen-position box is assumed.
    if(width<=0||height<=0||x1<0||x0>640||y1<0||y0>480||x0 < -width||x1>640+width||y0 < -height||y1>480+height)return {};
    for(float scale:{1.f,3.f}){
        if(std::abs(width*scale-11.275390625f)<.001f&&std::abs(height*scale-9.39616394043f)<.001f){
            unsigned bases=0,tips=0;float base_left=640,base_right=-640,tip=0;
            for(const auto* v:vs){
                if(std::abs((v->y-y0)*scale)<.0004f){++bases;base_left=std::min(base_left,v->x);base_right=std::max(base_right,v->x);}
                else if(std::abs((v->y-y1)*scale)<.0004f){++tips;tip=v->x;}
                // The verified blue gradient chip ends before red artwork at
                // Utex32. Other messages/colours in this same atlas are unknown.
                if(v->u*128<.5f||v->u*128>30.5f||v->v*128<0||v->v*128>7.f){bases=99;break;}
            }
            const double uv_area=double(b.u-a.u)*(c.v-a.v)-double(b.v-a.v)*(c.u-a.u);
            if(bases==2&&tips==1&&std::abs((tip-(base_left+base_right)*.5f)*scale)<.0004f&&std::abs(uv_area)>1e-10)
                return {Layer::TargetMarker,scale};
        }
        if(std::abs(width*scale-37.584656f)<.001f&&std::abs(height*scale-15.033867f)<.001f){
            unsigned corners=0;bool matched=true;
            for(const auto* v:vs){
                const float u=v->u*128,w=v->v*128;
                const bool right=std::abs(u-128.f)<.00001f,bottom=std::abs(w-15.5f)<.00001f;
                if((!right&&std::abs(u-64.f)>.00001f)||(!bottom&&std::abs(w)>.00001f)||
                   std::abs((v->x-(right?x1:x0))*scale)>.0004f||std::abs((v->y-(bottom?y1:y0))*scale)>.0004f){matched=false;break;}
                corners|=1u<<((right?1:0)+(bottom?2:0));
            }
            // Native TARGET is the TL/TR/BR and TL/BL/BR diagonal, with no
            // winding assumption. The UV label identifies its semantic region.
            if(matched&&(corners==0xbu||corners==0xdu))return {Layer::TargetLabel,scale};
        }
    }
    return {};
}
// The native range draws five ranking rows at (490,50), stepping 20 px.
// Match the authored rank, initials and m'ss"cc fields on their font grid.
inline bool range_ranking_glyph(float x,float y,float u,float v){
    if(!(x>=490.f&&x<=620.f&&y>=50.f&&y<=130.f&&u>=0.f&&u<=.9375f&&v>=0.f&&v<=.9375f))return false;
    const float column=(x-490.f)/10.f,row=(y-50.f)/20.f;
    if(column!=std::floor(column)||row!=std::floor(row)||
       u*16!=std::floor(u*16)||v*16!=std::floor(v*16))return false;
    const int glyph=32+int(u*16)+16*int(v*16);
    const bool digit=glyph>='0'&&glyph<='9';
    switch(int(column)){
    case 0: return (glyph>='1'&&glyph<='8')||glyph=='-';
    case 1: case 5: return glyph==' ';
    case 2: case 3: case 4:
        return (glyph>='A'&&glyph<='Z')||digit||glyph=='.'||glyph==' ';
    case 6: return digit||glyph==' ';
    case 8: return glyph=='\'';
    case 11: return glyph=='"';
    default: return digit;
    }
}
inline bool stadium_caption_glyph(float x,float y,int glyph){
    constexpr std::string_view name="Cobra";
    if(y!=320.f||x<280.f||x>320.f)return false;
    const float column=(x-280.f)/10.f;
    return column==std::floor(column)&&glyph==name[std::size_t(column)];
}
inline bool story_dossier_glyph(float x,float y,int glyph){
    if((y!=90.f&&y!=110.f&&y!=130.f)||x<180.f||x>630.f)return false;
    const float column=(x-180.f)/10.f;
    if(column!=std::floor(column))return false;
    static constexpr std::string_view rows[][3]{
        {"SCORPION THE BUTCHER ", "WEAPON-ROCKET LAUNCHER ", "HOSTAGE-THE FIRST LADY "},
        {"SCORPION THE BUTCHER ", "WEAPON-FIGHTER PLANE ", {}},
        {"COBRA THE IRON MAN ", "WEAPON-SUBMACHINE GUN ", "HOSTAGE-THE PRESIDENT'S DAUGHTER "},
        {"COBRA THE IRON MAN ", "WEAPON-SUBMACHINE GUN AND HAND GRENADES ", "HOSTAGE-THE PRESIDENT'S DAUGHTER "},
        {"COBRA THE IRON MAN ", "WEAPON-THE HUGE TRAILER ", {}},
        {"HORNET THE SNIPER ", "WEAPON-SNIPER RIFLE ", "HOSTAGE-THE FIRST LADY "},
        {"TOM & JERRY THE MEN WITH NIGHT VISION GOGGLES ", "WEAPON-CROSSBOW AND KNIFE ", {}},
        {"MONICA THE ARMED SECRETARY ", "WEAPON-SUBMACHINE GUN ", "HOSTAGE-THE PRESIDENT "},
    };
    const auto index=std::size_t(column);
    // The native typewriter draws its cursor in the next character cell.
    const auto row=std::size_t((y-90.f)/20.f);
    return std::any_of(std::begin(rows),std::end(rows),[&](const auto& dossier){
        const auto text=dossier[row];
        return !text.empty()&&(glyph==127?index<=text.size():index<text.size()&&glyph==text[index]);
    });
}
template<class Polygon,class Vertex> Decision classify_detailed(const Gate& g,const Polygon& p,const Vertex& a,const Vertex& b,const Vertex& c){
    if(!enabled(g)||p.isp.full!=0xc8800000||p.tileclip!=0x00100200)return {};
    // These native 10x20 font cells have no world lens owner.
    if(p.tcw.full==0x500c0000&&p.tsp.full==0x9491a4ed&&p.pcw.full==0x828c000a){
        const float x=std::min({a.x,b.x,c.x}),y=std::min({a.y,b.y,c.y});
        const float u=std::min({a.u,b.u,c.u}),v=std::min({a.v,b.v,c.v});
        const bool atlas_cell=u>=0&&u<=.9375f&&v>=0&&v<=.9375f&&u*16==std::floor(u*16)&&v*16==std::floor(v*16);
        const int glyph=atlas_cell?32+int(u*16)+16*int(v*16):0;
        // Indoor emits extra score digits at the end of the second HUD row.
        // Accept only digit atlas cells in the two rightmost native slots.
        const bool indoor_digit=(x==620.f||x==630.f)&&y==50.f&&v==.0625f&&u<=9.f/16.f;
        const bool ranking=range_ranking_glyph(x,y,u,v);
        const bool caption=stadium_caption_glyph(x,y,glyph);
        const bool dossier=a.col[0]==0&&story_dossier_glyph(x,y,glyph);
        const bool white=(indoor_digit||ranking||caption)&&a.col[0]==255&&a.col[1]==255;
        const uint8_t red=dossier?0:255,green=dossier||white?255:0,blue=white?255:0,alpha=dossier?191:255;
        unsigned corners=0;
        for(const auto* vertex:{&a,&b,&c}){
            const bool right=vertex->x==x+10,bottom=vertex->y==y+20;
            if((!right&&vertex->x!=x)||(!bottom&&vertex->y!=y)||
               vertex->u!=u+(right?.0625f:0.f)||vertex->v!=v+(bottom?.0625f:0.f)||
               std::abs(vertex->z-.729927003384f)>2e-7f||!std::isfinite(vertex->z)||
               vertex->col[0]!=red||vertex->col[1]!=green||vertex->col[2]!=blue||vertex->col[3]!=alpha)break;
            corners|=1u<<((right?1:0)+(bottom?2:0));
        }
        if(x>=0&&x<=630&&y>=0&&y<=460&&atlas_cell&&(corners==7||corners==14))
            return {Layer::Font,1};
    }
    struct Signature {Layer layer;uint32_t tcw,tsp,pcw;float q,x0,y0,x1,y1;uint8_t r,g,b,a;};
    static constexpr Signature signatures[]{
        {Layer::Font,0x500c0000,0x9491a4ed,0x828c000a,.729927003384f,220,30,630,450,255,255,255,255},
        // The fourth life icon appears at x=550..580 after a bonus life.
        {Layer::Life,0x500c1200,0x9491a4e4,0x828c000a,.729927003384f,460,70,580,100,255,255,255,255},
        {Layer::Countdown,0x500c7600,0x9491a4e4,0x828c002a,.714284956455f,233.1821f,56.0571f,395.2334f,111.5572f,0,176,0,255},
        {Layer::TimerDetail,0,0x9491a4e4,0x828c0022,.689656615257f,332.8885f,98.6291f,339.6720f,107.8021f,0,176,0,168},
        {Layer::AmmoPlate,0x10047a00,0x9491a4dc,0x828c002a,.666666686535f,-.366f,386.4899f,71.8293f,480.0549f,176,176,176,255},
        {Layer::Ammo,0x500c7900,0x9491a4e4,0x828c002a,.645161211491f,5.6503f,385.8942f,77.0936f,449.0652f,176,176,176,255}
    };
    for(const auto& s:signatures){
        const bool textured=(s.pcw&8)!=0;
        if(p.tsp.full!=s.tsp||p.pcw.full!=s.pcw||(textured&&p.tcw.full!=s.tcw))continue;
        // Font/life use native pixel mode. The measured projected timer/ammo
        // groups also have an exact centered /3 native variant, independently
        // of the main world's lens at transition frames. Do not widen a box.
        for(float factor:{1.f,3.f}){
        if(factor==3&&(s.layer==Layer::Font||s.layer==Layer::Life))continue;
        bool matches=true;
        for(const auto* v:{&a,&b,&c}){
            const float x=320+(v->x-320)*factor,y=240+(v->y-240)*factor;
            if(!std::isfinite(v->x)||!std::isfinite(v->y)||!std::isfinite(v->z)||std::abs(v->z-s.q)>2e-7f||
               x<s.x0-.02f||x>s.x1+.02f||y<s.y0-.02f||y>s.y1+.02f||
               v->col[0]!=s.r||v->col[1]!=s.g||v->col[2]!=s.b||v->col[3]!=s.a){matches=false;break;}
            // Font is two measured bands, not the entire rectangle between them.
            if(s.layer==Layer::Font&&!((v->y<=70.02f&&v->x<=620.02f)||(v->x>=529.98f&&v->y>=429.98f))){matches=false;break;}
            if(textured&&(!std::isfinite(v->u)||!std::isfinite(v->v))){matches=false;break;}
        }
        const double area=double(b.x-a.x)*(c.y-a.y)-double(b.y-a.y)*(c.x-a.x);
        if(matches&&std::isfinite(area)&&std::abs(area)>1e-9)return {s.layer,factor};
        }
    }
    return target_indicator(p,a,b,c);
}
template<class Polygon,class Vertex> Layer classify(const Gate& g,const Polygon& p,const Vertex& a,const Vertex& b,const Vertex& c){return classify_detailed(g,p,a,b,c).layer;}
struct Hit {float x{},y{},distance{};};
// Same triangle decision as rendering. Output is native normalized screen
// position, undoing only the HUD's XY size, with distance in the ray's units.
template<class Vertex> bool hit_decision(Decision decision,const Vertex& a,const Vertex& b,const Vertex& c,
        Placement plane,const std::array<float,3>& origin,const std::array<float,3>& direction,float tan_x,float tan_y,Hit& out){
    if(decision.layer==Layer::World||!plane||!std::isfinite(plane.size)||plane.size<=0||!std::isfinite(decision.native_scale)||decision.native_scale<=0||
       !std::isfinite(tan_x)||!std::isfinite(tan_y)||tan_x<=0||tan_y<=0)return false;
    for(unsigned i=0;i<3;++i)if(!std::isfinite(origin[i])||!std::isfinite(direction[i]))return false;
    if(direction[2]>=-1e-6f)return false;
    const float distance=(-plane.depth-origin[2])/direction[2];
    if(!std::isfinite(distance)||distance<=0)return false;
    const float size=render_size(decision.layer,decision.native_scale,plane);
    const float x=((origin[0]+direction[0]*distance)/(tan_x*plane.depth*size)+1)*320;
    const float y=((origin[1]+direction[1]*distance)/(tan_y*plane.depth*size)+1)*240;
    const auto side=[&](const auto& v,const auto& w){return (w.x-v.x)*(y-v.y)-(w.y-v.y)*(x-v.x);};
    const float ab=side(a,b),bc=side(b,c),ca=side(c,a);
    if(!std::isfinite(x)||!std::isfinite(y)||!((ab>=-.001f&&bc>=-.001f&&ca>=-.001f)||(ab<=.001f&&bc<=.001f&&ca<=.001f)))return false;
    out={x/640,y/480,distance};return true;
}
template<class Polygon,class Vertex> bool hit_triangle(const Gate& g,const Polygon& p,const Vertex& a,const Vertex& b,const Vertex& c,
        Placement plane,const std::array<float,3>& origin,const std::array<float,3>& direction,float tan_x,float tan_y,Hit& out){
    return hit_decision(classify_detailed(g,p,a,b,c),a,b,c,plane,origin,direction,tan_x,tan_y,out);
}
enum class Primitive { Triangles, Strip };
struct DrawRange { uint32_t first{},count{}; bool flip{}; Layer layer{Layer::World}; float native_scale{1}; };
// Classify the actual submitted indices, not PolyParam.first/count: sorted
// batches may combine different original polygons with equal material state.
// The caller falls back to its original draw if a range is malformed. Validation
// precedes every callback so it cannot emit half a range and then draw it again.
template<class Context,class Polygon,class Classify,class Emit> bool draw_ranges_with(const Gate& g,const Context& ctx,const Polygon&,
        uint32_t first,uint32_t count,Primitive mode,Classify classify_triangle,Emit emit){
    if(!enabled(g)||first>ctx.idx.size()||count>ctx.idx.size()-first||
       (mode==Primitive::Triangles&&count%3))return false;
    const uint64_t end=uint64_t(first)+count;
    for(uint64_t j=first;j<end;++j){const uint32_t index=ctx.idx[j];
        if(index==UINT32_MAX&&mode==Primitive::Strip)continue;
        if(index>=ctx.verts.size())return false;
    }
    DrawRange run{};bool pending=false;
    auto flush=[&]{if(pending){emit(run);pending=false;}};
    uint32_t segment=first;
    for(uint64_t j=first;j<end;){
        if(mode==Primitive::Strip&&ctx.idx[j]==UINT32_MAX){flush();segment=uint32_t(j+1);++j;continue;}
        if(mode==Primitive::Strip&&j<uint64_t(segment)+2){++j;continue;}
        if(mode==Primitive::Triangles&&j+2>=end)break;
        const uint32_t start=uint32_t(mode==Primitive::Strip?j-2:j);
        const auto decision=classify_triangle(ctx.idx[start],ctx.idx[start+1],ctx.idx[start+2]);
        if(!pending||run.layer!=decision.layer||run.native_scale!=decision.native_scale){
            flush();run={start,3,mode==Primitive::Strip&&((start-segment)&1)!=0,decision.layer,decision.native_scale};pending=true;}
        else run.count+=mode==Primitive::Strip?1:3;
        j+=mode==Primitive::Strip?1:3;
    }
    flush();return true;
}
template<class Context,class Polygon,class Emit> bool draw_ranges(const Gate& g,const Context& ctx,const Polygon& p,
        uint32_t first,uint32_t count,Primitive mode,Emit emit){
    return draw_ranges_with(g,ctx,p,first,count,mode,[&](uint32_t a,uint32_t b,uint32_t c){
        return classify_detailed(g,p,ctx.verts[a],ctx.verts[b],ctx.verts[c]);
    },emit);
}
}
