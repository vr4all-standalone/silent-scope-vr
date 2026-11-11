// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "dc_menu_actions.h"
#include <array>
#include <vector>
namespace silent_scope::dc_menu_controls {
struct Rect {float x0{},y0{},x1{},y1{};bool hit(float x,float y)const{return std::isfinite(x)&&std::isfinite(y)&&x>=x0&&x<x1&&y>=y0&&y<y1;}};
struct Button {Rect bounds;const char* label{};dc_menu::MenuAction action;bool enabled{};};
// These are visible VR-owned controls in the native canvas gutters, not
// invented hit regions for the original game's unlabeled pixels.
class Controller {
 dc_menu::MenuKind kind_{dc_menu::MenuKind::None};std::uint64_t revision_{};
 int training_row_{},training_column_{};
 dc_menu::MenuFacts facts_;bool frame_{},selector_panel_{};std::vector<Button> buttons_;
 dc_menu::MenuAction action(dc_menu::MenuVerb verb)const{
  dc_menu::MenuAction a;a.kind=facts_.kind;a.verb=verb;a.context_revision=facts_.context_revision;
  if(a.kind==dc_menu::MenuKind::Options||a.kind==dc_menu::MenuKind::Ranking)a.page=a.from_page=facts_.page;
  return a;
 }
public:
 void reset(){*this=Controller{};}
 void begin(const dc_menu::MenuFacts& facts,bool current_native_frame,int rendered_page=-1,bool selector_panel=false){
  facts_=facts;frame_=current_native_frame;selector_panel_=selector_panel;buttons_.clear();
  if(kind_!=facts.kind||revision_!=facts.context_revision){
   kind_=facts.kind;revision_=facts.context_revision;
   training_row_=facts.row>=0&&facts.row<4?facts.row:0;training_column_=facts.column>=0&&facts.column<3?facts.column:0;
  }
  if(!frame_||!facts.verified||!facts.context_active||!facts.context_revision)return;
  using K=dc_menu::MenuKind;using V=dc_menu::MenuVerb;
  if(facts.back_button){
   const auto bounds=facts.source_context==dc_menu::OutdoorStageContext?Rect{8,418,72,454}:Rect{8,80,72,116};
   buttons_.push_back({bounds,facts.kind==K::Options?"Cancel":"Back",action(V::Back),facts.interactive});
  }
  if(facts.kind==K::Training&&facts.confirm_button){auto start=action(V::Confirm);start.row=training_row_;start.column=training_column_;
   buttons_.push_back({{568,80,632,116},"Start",start,facts.interactive});}
  if(facts.kind==K::Options&&facts.exit_button)buttons_.push_back({{568,80,632,116},"Exit",action(V::Exit),facts.interactive});
  if(facts.kind==K::Options&&facts.page==rendered_page&&facts.row>0&&facts.row<=facts.row_count&&facts.row<=6&&facts.value_verified){
   const float y=212+32.f*(facts.row-1);auto minus=action(V::Adjust);minus.row=facts.row;minus.direction=-1;
   auto plus=minus;plus.direction=1;
   buttons_.push_back({{24,y,72,y+32},"-",minus,facts.interactive});
   buttons_.push_back({{568,y,616,y+32},"+",plus,facts.interactive});
  }
  if(facts.kind==K::Dialog&&facts.source_context>=2&&facts.source_context<=5&&facts.confirm_button)
   buttons_.push_back({{512,80,632,116},"Continue",action(V::Confirm),facts.interactive});
 }
 const std::vector<Button>& buttons()const{return buttons_;}
 dc_menu::MenuAction context()const{auto a=action(dc_menu::MenuVerb::None);return a;}
 // Native geometry hits are independently proven by NativeMenusProof. Bind
 // them to the source family/message/page before creating a native request.
 dc_menu::MenuAction resolve(dc_menu::MenuAction native_hit,int rendered_page,unsigned rendered_context,float x,float y){
  using K=dc_menu::MenuKind;using V=dc_menu::MenuVerb;
  if(!frame_||!facts_.verified||!facts_.context_active)return context();
  for(const auto& button:buttons_)if(button.bounds.hit(x,y)){
   if(!button.enabled)return context();
   auto a=button.action;if(a.kind==K::Training&&a.verb==V::Confirm){a.row=training_row_;a.column=training_column_;}return a;
  }
  if(selector_panel_&&facts_.kind==K::ModeSelect&&facts_.mode_type==1&&
     facts_.source_context==dc_menu::StoryRouteContext&&facts_.interactive&&facts_.aim_verified){
   // Tower Building, Air Battle, and Stadium in the stock 640x480 route view.
   // The heading and lower scope previews are separate from these card faces.
   constexpr Rect cards[]{{17,169,219,260},{219,169,421,260},{421,169,623,260}};
   for(int route=0;route<3;++route){
    if(!cards[route].hit(x,y)||!(facts_.available_regions&(1u<<route)))continue;
    auto hit=action(V::Focus);hit.row=route;return hit;
   }
   return context();
  }
  if(selector_panel_&&facts_.kind==K::ModeSelect&&facts_.mode_type==4&&
     facts_.source_context==dc_menu::InvasionRouteContext&&facts_.interactive&&facts_.aim_verified){
   constexpr Rect cards[]{{80,168,280,260},{360,168,560,260}};
   for(int route=0;route<2;++route){
    if(!cards[route].hit(x,y)||!(facts_.available_regions&(1u<<route)))continue;
    auto hit=action(V::Focus);hit.row=route;return hit;
   }
   return context();
  }
  if(selector_panel_&&facts_.kind==K::ModeSelect&&facts_.mode_type==6&&
     facts_.source_context==dc_menu::OutdoorStageContext&&facts_.interactive&&facts_.aim_verified){
   // Authored Stage Select card faces, in the stock 640x480 raster.
   constexpr float left[]{54,243,433},right[]{230,420,611};
   for(int row=0;row<3;++row)for(int col=0;col<3;++col){
    const int stage=row*3+col;
    if(Rect{left[col],169.f+76*row,right[col],235.f+76*row}.hit(x,y)&&
       (facts_.available_regions&(1u<<stage))){
     auto hit=action(V::Focus);hit.row=stage;return hit;
    }
   }
   return context();
  }
  // Arcade's original panels have a fixed raster layout. The Quest can
  // reject the desktop art proof while the native selector remains active.
  if(selector_panel_&&facts_.kind==K::ModeSelect&&facts_.mode_type==0&&
     facts_.source_context==dc_menu::ArcadeModeContext&&facts_.interactive&&
     facts_.aim_verified&&native_hit.kind==K::None&&rendered_context==0){
   auto hit=action(V::Focus);
   if(Rect{148,87,492,155}.hit(x,y)){hit.row=1;hit.column=x<320?0:1;return hit;}
   if(Rect{141,172,499,309}.hit(x,y)){hit.row=0;return hit;}
   if(Rect{148,327,492,402}.hit(x,y)){hit.row=x<264?2:x<376?3:4;return hit;}
  }
  if(native_hit.kind!=facts_.kind||native_hit.verb==V::None)return context();
  if(rendered_context!=facts_.source_context)return context();
  if((facts_.kind==K::Options||facts_.kind==K::Ranking)&&rendered_page!=facts_.page)return context();
  native_hit.context_revision=facts_.context_revision;
  if(facts_.kind==K::Options||facts_.kind==K::Ranking){native_hit.from_page=rendered_page;if(native_hit.page<0)native_hit.page=rendered_page;}
  if(facts_.kind==K::Training&&native_hit.verb==V::Focus){
   if(native_hit.row>=0&&native_hit.row<4)training_row_=native_hit.row;
   if(native_hit.column>=0&&native_hit.column<3)training_column_=native_hit.column;
   native_hit.row=training_row_;native_hit.column=training_column_;
  }
  return native_hit;
 }
};
}
