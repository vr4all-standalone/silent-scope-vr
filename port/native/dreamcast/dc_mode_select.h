// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string_view>
#include "dc_menu_actions.h"
namespace silent_scope::dc_native_menus {
class ModeSelectProof {
 struct Point{float x,y,q,u,v;};
 struct Cell{uint32_t tcw,tsp;std::array<std::array<Point,4>,2> pose;};
 // ActualUS017 settled dim/active native art poses. Unknownanimation/localization
 // remains unclickable; these signatures do not authorize worldownership.
 inline static constexpr std::array<Cell,6> cells_{{
  {0x50049700,0x9491a4e4,{{{{{482.5060425f,302.4247742f,0.677297473f,1.f,0.74609375f},{482.5060425f,177.5752106f,0.677297473f,1.f,0.50390625f},{152.4021912f,302.4247742f,0.677297473f,0.f,0.74609375f},{152.4021912f,177.5752106f,0.677297473f,0.f,0.50390625f}}},{{{488.6967773f,304.8028564f,0.02380952612f,1.f,0.74609375f},{488.6967773f,175.197113f,0.02380952612f,1.f,0.50390625f},{146.0174866f,304.8028564f,0.02380952612f,0.f,0.74609375f},{146.0174866f,175.197113f,0.02380952612f,0.f,0.50390625f}}}}}},
  {0x100c9280,0x9491a4e3,{{{{{157.437088f,152.512558f,0.6757475138f,0.00390625f,0.98828125f},{309.8397827f,152.512558f,0.6757475138f,0.50390625f,0.98828125f},{157.4371033f,130.9221191f,0.6757475138f,0.00390625f,0.50390625f},{309.8397827f,130.9221191f,0.6757475138f,0.50390625f,0.50390625f}}},{{{151.2588959f,149.1876068f,0.02375297062f,0.00390625f,0.98828125f},{309.4536438f,149.1876068f,0.02375297062f,0.50390625f,0.98828125f},{151.2589111f,126.7766418f,0.02375297062f,0.00390625f,0.50390625f},{309.4536438f,126.7766418f,0.02375297062f,0.50390625f,0.50390625f}}}}}},
  {0x100c9280,0x9491a4e3,{{{{{330.1138916f,152.9111328f,0.6726689935f,0.00390625f,0.48828125f},{481.8223572f,152.9111328f,0.6726689935f,0.62890625f,0.48828125f},{330.1138916f,131.4191132f,0.6726689935f,0.00390625f,0.00390625f},{481.8223572f,131.4191132f,0.6726689935f,0.62890625f,0.00390625f}}},{{{330.4964294f,149.6169891f,0.02364066243f,0.00390625f,0.48828125f},{487.9432678f,149.6169891f,0.02364066243f,0.62890625f,0.48828125f},{330.4964294f,127.3120346f,0.02364066243f,0.00390625f,0.00390625f},{487.9432678f,127.3120346f,0.02364066243f,0.62890625f,0.00390625f}}}}}},
  {0x50049700,0x9491a4e4,{{{{{157.437088f,402.5629272f,0.6757475138f,0.f,0.99609375f},{253.9588165f,402.5629272f,0.6757475138f,0.5f,0.99609375f},{157.4371033f,367.0022888f,0.6757475138f,0.f,0.74609375f},{253.9588165f,367.0022888f,0.6757475138f,0.5f,0.74609375f}}},{{{151.2588959f,408.7410889f,0.02375297062f,0.f,0.99609375f},{251.4489136f,408.7410889f,0.02375297062f,0.5f,0.99609375f},{151.2589111f,371.8289795f,0.02375297062f,0.f,0.74609375f},{251.4489136f,371.8289795f,0.02375297062f,0.5f,0.74609375f}}}}}},
  {0x50049700,0x9491a4e4,{{{{{274.3835754f,402.1917725f,0.6742047668f,0.5f,0.99609375f},{365.6164246f,402.1917725f,0.6742047668f,1.f,0.99609375f},{274.3835754f,366.7123413f,0.6742047668f,0.5f,0.74609375f},{365.6164246f,366.7123413f,0.6742047668f,1.f,0.74609375f}}},{{{272.6540222f,408.341217f,0.02369668335f,0.5f,0.99609375f},{367.3459778f,408.341217f,0.02369668335f,1.f,0.99609375f},{272.6540222f,371.516571f,0.02369668335f,0.5f,0.74609375f},{367.3459778f,371.516571f,0.02369668335f,1.f,0.74609375f}}}}}},
  {0x50049400,0x9491a4e4,{{{{{385.7403259f,401.8223267f,0.6726689935f,0.f,0.7421875f},{481.8223572f,401.8223267f,0.6726689935f,0.5f,0.7421875f},{385.7403259f,366.4237061f,0.6726689935f,0.f,0.49609375f},{481.8223572f,366.4237061f,0.6726689935f,0.5f,0.49609375f}}},{{{388.2269592f,407.9432678f,0.02364066243f,0.f,0.7421875f},{487.9432678f,407.9432678f,0.02364066243f,0.5f,0.7421875f},{388.2269592f,371.2056885f,0.02364066243f,0.f,0.49609375f},{487.9432678f,371.2056885f,0.02364066243f,0.5f,0.49609375f}}}}}},
 }};
 inline static constexpr std::array<std::array<Point,4>,11> heading_{{
  {{{15.90563297f,37.26031494f,0.02283105068f,0.04296875f,0.f},{15.90563297f,77.80825043f,0.02283105068f,0.04296875f,0.24609375f},{46.3135376f,37.26031494f,0.02283105068f,0.20703125f,0.f},{46.31356049f,77.80825043f,0.02283105068f,0.20703125f,0.24609375f}}},
  {{{46.3135376f,37.26031494f,0.02283105068f,0.29296875f,0.f},{46.31356049f,77.80825043f,0.02283105068f,0.29296875f,0.24609375f},{76.72144318f,37.26029205f,0.02283105068f,0.45703125f,0.f},{76.72144318f,77.80825043f,0.02283105068f,0.45703125f,0.24609375f}}},
  {{{76.72144318f,37.26029205f,0.02283105068f,0.54296875f,0.f},{76.72144318f,77.80825043f,0.02283105068f,0.54296875f,0.24609375f},{107.1293945f,37.26029205f,0.02283105068f,0.70703125f,0.f},{107.1293945f,77.80825043f,0.02283105068f,0.70703125f,0.24609375f}}},
  {{{107.1293945f,37.26029205f,0.02283105068f,0.79296875f,0.f},{107.1293945f,77.80825043f,0.02283105068f,0.79296875f,0.24609375f},{137.5372925f,37.26029205f,0.02283105068f,0.95703125f,0.f},{137.5372925f,77.80822754f,0.02283105068f,0.95703125f,0.24609375f}}},
  {{{259.1689758f,37.26029205f,0.02283105068f,0.54296875f,0.24609375f},{259.1689758f,77.80825043f,0.02283105068f,0.54296875f,0.49609375f},{289.5768738f,37.26029205f,0.02283105068f,0.70703125f,0.24609375f},{289.5768738f,77.80825043f,0.02283105068f,0.70703125f,0.49609375f}}},
  {{{319.9848022f,77.80825043f,0.02283105068f,0.953125f,0.49609375f},{319.9848022f,37.26031494f,0.02283105068f,0.953125f,0.24609375f},{289.5768738f,77.80825043f,0.02283105068f,0.7890625f,0.49609375f},{289.5768738f,37.26029205f,0.02283105068f,0.7890625f,0.24609375f}}},
  {{{137.5372925f,77.80822754f,0.02283105068f,0.04296875f,0.49609375f},{167.9452209f,77.80825043f,0.02283105068f,0.20703125f,0.49609375f},{137.5372925f,37.26029205f,0.02283105068f,0.04296875f,0.24609375f},{167.9452209f,37.26031494f,0.02283105068f,0.20703125f,0.24609375f}}},
  {{{198.3531342f,37.26029205f,0.02283105068f,0.2890625f,0.24609375f},{198.3531342f,77.80825043f,0.02283105068f,0.2890625f,0.49609375f},{228.7610474f,37.26029205f,0.02283105068f,0.453125f,0.24609375f},{228.7610474f,77.80825043f,0.02283105068f,0.453125f,0.49609375f}}},
  {{{228.7610474f,37.26029205f,0.02283105068f,0.7890625f,0.f},{228.7610474f,77.80825043f,0.02283105068f,0.7890625f,0.24609375f},{259.1689758f,37.26029205f,0.02283105068f,0.953125f,0.f},{259.1689758f,77.80825043f,0.02283105068f,0.953125f,0.24609375f}}},
  {{{198.3531342f,37.26029205f,0.02283105068f,0.953125f,0.f},{167.9452209f,37.26031494f,0.02283105068f,0.7890625f,0.f},{198.3531342f,77.80825043f,0.02283105068f,0.953125f,0.24609375f},{167.9452209f,77.80825043f,0.02283105068f,0.7890625f,0.24609375f}}},
  {{{634.2466431f,77.80832672f,0.02283105068f,1.f,0.99609375f},{634.2466431f,37.26036072f,0.02283105068f,1.f,0.74609375f},{320.000061f,77.80832672f,0.02283105068f,0.f,0.99609375f},{320.000061f,37.26036072f,0.02283105068f,0.f,0.74609375f}}},
 }};
 std::array<std::array<Point,4>,6> hits_{};bool valid_{};

 struct Watched{uint32_t first,poly,triangles;};
 std::array<Watched,160> watched_{};unsigned watched_count_{};
 template<class C>static auto no_modifiers(const C& c,int)->decltype(c.modtrig.empty()){return c.modtrig.empty();}
 template<class C>static bool no_modifiers(const C&,long){return true;}
 static bool close(float a,float b,float tolerance){return std::isfinite(a)&&std::abs(a-b)<=tolerance;}
 template<class P>static bool state(const P& p,uint32_t tcw,uint32_t tsp,uint32_t pcw=0x828c002a){return p.isp.full==0xc8800000&&p.tsp.full==tsp&&p.tcw.full==tcw&&p.pcw.full==pcw&&p.tileclip==0x1c09c0;}
 template<class P>static bool same_state(const P& a,const P& b){return a.isp.full==b.isp.full&&a.tsp.full==b.tsp.full&&a.tcw.full==b.tcw.full&&a.pcw.full==b.pcw.full&&a.tileclip==b.tileclip;}
 template<class C>bool watch(const C& c,unsigned i){if(watched_count_==watched_.size()||i>=c.global_param_tr.size())return false;
  const auto& p=c.global_param_tr[i];if(p.count!=4||uint64_t(p.first)+4>c.verts.size())return false;
  watched_[watched_count_++]={p.first,i,0};return true;}
 template<class C>static bool shape(const C& c,unsigned i,const std::array<Point,4>& points){if(i>=c.global_param_tr.size())return false;
  const auto& p=c.global_param_tr[i];if(p.count!=4||uint64_t(p.first)+4>c.verts.size())return false;
  for(unsigned j=0;j<4;++j){const auto& v=c.verts[p.first+j];const auto& expected=points[j];
   if(!close(v.x,expected.x,.0002f)||!close(v.y,expected.y,.0002f)||!close(v.z,expected.q,.0000001f)||v.u!=expected.u||v.v!=expected.v)return false;
  }return true;}
 template<class C>static bool uv_domain(const C& c,unsigned i,const std::array<Point,4>& expected){const auto& p=c.global_param_tr[i];
  if(p.count!=4||uint64_t(p.first)+4>c.verts.size())return false;
  unsigned found=0;for(unsigned j=0;j<4;++j){const auto& v=c.verts[p.first+j];unsigned corner=0;
   for(;corner<4;++corner)if(v.u==expected[corner].u&&v.v==expected[corner].v)break;
   if(corner==4||(found&(1u<<corner)))return false;
   found|=1u<<corner;
  }return found==15;
 }
 template<class C>static bool color(const C& c,unsigned i,unsigned gray,unsigned alpha){const auto& p=c.global_param_tr[i];
  for(unsigned j=0;j<4;++j){const auto& v=c.verts[p.first+j];if(v.col[0]!=gray||v.col[1]!=gray||v.col[2]!=gray||v.col[3]!=alpha)return false;}return true;}
 template<class C>bool draws(const C& c){
  std::sort(watched_.begin(),watched_.begin()+watched_count_,[](const auto& a,const auto& b){return a.first<b.first;});
  for(unsigned i=1;i<watched_count_;++i)if(uint64_t(watched_[i-1].first)+4>watched_[i].first)return false;
  const auto find=[&](uint32_t index)->int{auto it=std::upper_bound(watched_.begin(),watched_.begin()+watched_count_,index,[](uint32_t v,const auto& p){return v<p.first;});
   if(it==watched_.begin())return -1;
   --it;return uint64_t(index)<uint64_t(it->first)+4?int(it-watched_.begin()):-1;};
  for(const auto& draw:c.sortedTriangles){if(draw.polyIndex>=c.global_param_tr.size()||draw.first>c.idx.size()||draw.count>c.idx.size()-draw.first||draw.count%3)return false;
   for(uint64_t i=draw.first;i<uint64_t(draw.first)+draw.count;i+=3){const auto a=c.idx[i],b=c.idx[i+1],d=c.idx[i+2];if(a>=c.verts.size()||b>=c.verts.size()||d>=c.verts.size())return false;
    const int group=find(a),gb=find(b),gd=find(d);if(group<0&&gb<0&&gd<0)continue;
    if(group<0||group!=gb||group!=gd)return false;
    auto& p=watched_[unsigned(group)];if(!same_state(c.global_param_tr[p.poly],c.global_param_tr[draw.polyIndex]))return false;
    const unsigned mask=(1u<<(a-p.first))|(1u<<(b-p.first))|(1u<<(d-p.first)),bit=mask==7?1:mask==14?2:0;
    if(!bit||(p.triangles&bit))return false;
    p.triangles|=bit;
   }
  }for(unsigned i=0;i<watched_count_;++i)if(watched_[i].triangles!=3)return false;
  return true;
 }
 template<class C>bool caption(const C& c,unsigned active){const auto& ps=c.global_param_tr;unsigned panel=unsigned(ps.size());
  for(unsigned i=0;i<ps.size();++i)if(ps[i].pcw.full==0x828c0002&&ps[i].tsp.full==0x9411a4e4){if(panel!=ps.size())return false;panel=i;}
  if(panel==ps.size())return false;
  const auto& p=ps[panel];if(p.isp.full!=0xc8800000||p.tileclip!=0x1c09c0||p.count!=4||uint64_t(p.first)+4>c.verts.size())return false;
  for(unsigned j=0;j<4;++j){const auto& v=c.verts[p.first+j];if(v.x!=80+(j&2?440:0)||v.y!=390+(j&1?90:0)||v.z!=.7246376872062683f||v.u||v.v||v.col[0]||v.col[1]||v.col[2]||v.col[3]!=112)return false;}
  if(!watch(c,panel))return false;
  constexpr std::string_view body[]{"The game will proceed according to\nthe story.","Shooting range.","The player competes to clear the stage\nfaster than other players within a\nlimited amount of time."};
  unsigned index=panel+1,column=0,row=0;
  for(char ch:body[active==0?0:active<3?1:2]){if(ch=='\n'){++row;column=0;continue;}if(index>=ps.size())return false;
   const auto& poly=ps[index];if(!state(poly,0x500c0000,0x9411a4ed,0x828c000a)||poly.count!=4||uint64_t(poly.first)+4>c.verts.size())return false;
   const unsigned code=unsigned(ch)-32;
   for(unsigned j=0;j<4;++j){const auto& v=c.verts[poly.first+j];if(v.x!=90+10.f*column+(j&2?10:0)||v.y!=390+20.f*row+(j&1?20:0)||v.z!=.7299270033836365f||v.u!=(code&15)/16.f+(j&2?1.f/16:0)||v.v!=(code>>4)/16.f+(j&1?1.f/16:0))return false;}
   if(!color(c,index,255,255)||!watch(c,index))return false;
   ++index;++column;
  }return index==ps.size();
 }
 public:
 // Only this captured US initial six-choice family. SourceModeSelect/type0 is
 // independently required by the native consumer; no gameplay or render gate.
 template<class C>bool configure(std::string_view title,const C& c){*this=ModeSelectProof{};
  if(title!="T9507N"||c.isRTT||c.framebufferWidth!=640||c.framebufferHeight!=480||!c.dcCamera.supported||!c.dcCamera.generation||!c.dcCamera.submit_sequence||
    c.verts.size()<44||c.verts.size()>1024||c.idx.size()>2048||c.global_param_tr.size()<40||c.global_param_tr.size()>200||!c.global_param_pt.empty()||c.render_passes.size()!=1||c.sortedTriangles.empty()||c.sortedTriangles.size()>200||!no_modifiers(c,0))return false;
  const auto& pass=c.render_passes[0];if(!pass.autosort||pass.op_count!=c.global_param_op.size()||pass.pt_count||pass.tr_count!=c.global_param_tr.size()||pass.sorted_tr_count!=c.sortedTriangles.size())return false;
  for(const auto& v:c.verts)if(!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.z)||!std::isfinite(v.u)||!std::isfinite(v.v))return false;
  for(const auto& p:c.global_param_op)for(const auto& cell:cells_)if(p.tcw.full==cell.tcw)return false;
  const auto& ps=c.global_param_tr;
  for(unsigned i=0;i<11;++i)if(!state(ps[i],0x50049400,0x9491a4e4)||!shape(c,i,heading_[i])||!color(c,i,176,255)||!watch(c,i))return false;
  std::array<unsigned,6> counts{},alpha_bits{};unsigned active=6;
  for(unsigned i=11;i<ps.size();++i){const auto& p=ps[i];if(p.count!=4||uint64_t(p.first)+4>c.verts.size())continue;
   for(unsigned cell=0;cell<6;++cell){const auto& spec=cells_[cell];if(p.tcw.full!=spec.tcw)continue;
    const bool dim=shape(c,i,spec.pose[0]),bright=shape(c,i,spec.pose[1]);if(!dim&&!bright){if(uv_domain(c,i,spec.pose[0]))return false;continue;}
    if(!state(p,spec.tcw,spec.tsp)||!watch(c,i))return false;
    const unsigned alpha=c.verts[p.first].col[3];if(!color(c,i,bright?176:48,bright?alpha:175))return false;
    if(bright){if((alpha!=255&&alpha!=127)||(active!=6&&active!=cell))return false;active=cell;
     const unsigned bit=alpha==255?1:2;if(alpha_bits[cell]&bit)return false;alpha_bits[cell]|=bit;}
    else if(alpha_bits[cell]||counts[cell])return false;
    if(++counts[cell]>2)return false;
    for(unsigned j=0;j<4;++j){const auto& v=c.verts[p.first+j];const Point actual{v.x,v.y,v.z,v.u,v.v};
     if(counts[cell]==2){const auto& prior=hits_[cell][j];if(actual.x!=prior.x||actual.y!=prior.y||actual.q!=prior.q||actual.u!=prior.u||actual.v!=prior.v)return false;}
     hits_[cell][j]=actual;
    }
   }
  }if(active==6)return false;
  for(unsigned i=0;i<6;++i)if(counts[i]!=(i==active?2u:1u)||alpha_bits[i]!=(i==active?3u:0u))return false;
  if(!caption(c,active)||!draws(c))return false;
  valid_=true;return true;
 }
 static bool triangle_contains(float x,float y,const Point& a,const Point& b,const Point& c){
  const auto edge=[&](const Point& p,const Point& q){return (x-p.x)*(q.y-p.y)-(y-p.y)*(q.x-p.x);};
  const float ab=edge(a,b),bc=edge(b,c),ca=edge(c,a);return (ab>=0&&bc>=0&&ca>=0)||(ab<=0&&bc<=0&&ca<=0);
 }
 dc_menu::MenuAction hit(float x,float y)const{if(!valid_||!std::isfinite(x)||!std::isfinite(y))return {};
  const auto action=[](unsigned cell){
   dc_menu::MenuAction a;
   a.kind=dc_menu::MenuKind::ModeSelect;a.verb=dc_menu::MenuVerb::Focus;
   a.row=cell==0?0:cell<=2?1:int(cell)-1;
   a.column=cell==1?0:cell==2?1:-1;
   return a;
  };
  for(unsigned i=0;i<6;++i){const auto& p=hits_[i];if(!triangle_contains(x,y,p[0],p[1],p[2])&&!triangle_contains(x,y,p[2],p[1],p[3]))continue;
   return action(i);
  }
  // The two group titles share the same framed cards as the smaller
  // difficulty/location labels. Extend each label's column up to its title.
  if(x>=148.f&&x<492.f){
   if(y>=87.f&&y<155.f)return action(x<320.f?1:2);
   if(y>=327.f&&y<402.f)return action(x<264.f?3:x<376.f?4:5);
  }
  return {};
 }
};
}
