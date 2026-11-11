// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string_view>
namespace silent_scope::dc_native_menu {
struct Bounds{float x0{},y0{},x1{},y1{};};
class MainMenuProof {
 const void* frame_{};uint64_t generation_{},submission_{};int selected_{-1};std::array<Bounds,4> bounds_{};
 template<class C> static auto no_modifiers(const C& c,int)->decltype(c.modtrig.empty()){return c.modtrig.empty();}
 template<class C> static bool no_modifiers(const C&,long){return true;}
 template<class P> static bool state(const P& p,uint32_t tcw){
  return p.isp.full==0xc8800000&&p.tsp.full==0x9491a4ed&&p.tcw.full==tcw&&p.pcw.full==0x828c000a&&p.tileclip==0x001c09c0;
 }
 template<class P> static bool same_state(const P& a,const P& b){return a.isp.full==b.isp.full&&a.tsp.full==b.tsp.full&&
  a.tcw.full==b.tcw.full&&a.pcw.full==b.pcw.full&&a.tileclip==b.tileclip;}
 template<class C> static bool background(const C& c){const auto& p=c.global_param_op[0];
  if(p.first!=0||p.count!=4||p.isp.full!=0xe0800000||p.tsp.full!=0x208004c0||p.tcw.full||p.pcw.full!=2||p.tileclip)return false;
  for(unsigned i=0;i<4;++i){const auto& v=c.verts[i];if(c.idx[i]!=i||v.x!=(i&1?896.f:-256.f)||v.y!=(i&2?480.f:0.f)||
    v.z!=8.999988494906574e-6f||v.u!=0||v.v!=0||v.col[0]||v.col[1]||v.col[2]||v.col[3]!=255)return false;}
  return true;
 }
 template<class C,class P> static bool quad(const C& c,const P& p,unsigned first,float x,float y,float w,float h,
    float q,float u,float v,float uw,float vh,unsigned color){
  if(p.first!=first||p.count!=4||uint64_t(first)+4>c.verts.size())return false;
  for(unsigned i=0;i<4;++i){const auto& a=c.verts[first+i];
   if(a.x!=x+(i&2?w:0)||a.y!=y+(i&1?h:0)||a.z!=q||a.u!=u+(i&2?uw:0)||a.v!=v+(i&1?vh:0)||
     a.col[0]!=color||a.col[1]!=color||a.col[2]!=color||a.col[3]!=255)return false;}
  return true;
 }
public:
 // Exact captured Main Menu: native table8c0e1a48, four strings drawn by
 //8c086ae0..c38. Whole-frame proof, never a generic equal-q or overscan exception.
 template<class C> bool configure(std::string_view title,const C& c){
  *this=MainMenuProof{};
  if(title!="T9507N"||c.isRTT||c.framebufferWidth!=640||c.framebufferHeight!=480||!c.dcCamera.supported||
    !c.dcCamera.generation||!c.dcCamera.submit_sequence||c.verts.size()!=180||c.idx.size()!=268||
    c.global_param_op.size()!=1||!c.global_param_pt.empty()||c.global_param_tr.size()!=44||
    c.render_passes.size()!=1||c.sortedTriangles.size()!=7||!no_modifiers(c,0)||!background(c))return false;
  const auto& pass=c.render_passes[0];
  if(!pass.autosort||pass.op_count!=1||pass.pt_count||pass.tr_count!=44||pass.sorted_tr_count!=7)return false;
  constexpr uint32_t tiles[]{0x08063558,0x080e51ac,0x08067558,0x080e91ac,0x0806b558,0x080ed1ac};
  for(unsigned n=0;n<6;++n){const auto& p=c.global_param_tr[n];
   if(!state(p,tiles[n])||!quad(c,p,4+4*n,256.f*(n%3),256.f*(n/3),256,256,.1f,0,0,1,1,255))return false;}
  constexpr std::string_view labels[]{"ARCADE MODE","TRAINING MODE","OPTIONS","RANKING"};
  unsigned polygon=6;int highlighted=-1;
  for(unsigned row=0;row<4;++row){const unsigned color=c.verts[4+4*polygon].col[0];
   if(color!=128&&color!=255)return false;
   if(color==255){if(highlighted>=0)return false;highlighted=int(row);}
   for(unsigned ch=0;ch<labels[row].size();++ch,++polygon){const auto& p=c.global_param_tr[polygon];const unsigned cell=unsigned(labels[row][ch])-32;
    if(!state(p,0x400c0900)||!quad(c,p,4+4*polygon,60+20.f*ch,70+30.f*row,20,30,.7299270033836365f,
       (cell&15)/16.f,(cell>>4)*3.f/32,1.f/16,3.f/32,color))return false;}
   const auto& first=c.verts[4+4*(polygon-labels[row].size())];const auto& last=c.verts[4+4*(polygon-1)+3];
   bounds_[row]={first.x,first.y,last.x,last.y};
  }
  if(polygon!=44||highlighted<0)return false;
  std::array<unsigned,44> triangles{};std::array<bool,268> used{};for(unsigned i=0;i<4;++i)used[i]=true;
  for(const auto& draw:c.sortedTriangles){
   if(draw.polyIndex>=44||draw.first<4||draw.first>268||draw.count>268-draw.first||!draw.count||draw.count%3)return false;
   for(uint64_t j=draw.first;j<uint64_t(draw.first)+draw.count;j+=3){
    const auto a=c.idx[j],b=c.idx[j+1],d=c.idx[j+2];if(a<4||b<4||d<4||a>=180||b>=180||d>=180||a==b||a==d||b==d)return false;
    const unsigned least=std::min({a,b,d}),n=(least-4)/4,base=4+4*n;
    if(a>=base+4||b>=base+4||d>=base+4||!same_state(c.global_param_tr[n],c.global_param_tr[draw.polyIndex]))return false;
    const unsigned mask=(1u<<(a-base))|(1u<<(b-base))|(1u<<(d-base)),bit=mask==7?1:mask==14?2:0;
    if(!bit||(triangles[n]&bit))return false;
    triangles[n]|=bit;
    for(unsigned k=0;k<3;++k){if(used[j+k])return false;used[j+k]=true;}
   }
  }
  for(auto t:triangles)if(t!=3)return false;
  for(bool value:used)if(!value)return false;
  frame_=&c;generation_=c.dcCamera.generation;submission_=c.dcCamera.submit_sequence;selected_=highlighted;return true;
 }
 template<class C> bool matches(const C& c)const{return frame_==&c&&selected_>=0&&!c.isRTT&&c.framebufferWidth==640&&c.framebufferHeight==480&&
  c.dcCamera.supported&&generation_==c.dcCamera.generation&&submission_==c.dcCamera.submit_sequence;}
 // These are verified glyph-quad bounds, including spaces, not guessed row bands.
 template<class C> int hit(const C& c,float x,float y)const{
  if(!matches(c)||!std::isfinite(x)||!std::isfinite(y))return -1;
  for(unsigned i=0;i<4;++i){const auto& b=bounds_[i];if(x>=b.x0&&x<b.x1&&y>=b.y0&&y<b.y1)return int(i);}return -1;
 }
 int highlighted_item()const{return selected_;} // Presentation evidence, never native selection acknowledgment.
};
}
