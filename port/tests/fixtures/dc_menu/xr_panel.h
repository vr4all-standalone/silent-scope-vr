#pragma once
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <cmath>
namespace vr::xr {
struct PanelRow{std::string label,value;bool selected{},dim{},stepper{};};
struct RowsPanel{std::string title;std::vector<PanelRow> rows;std::string footer;};
struct RowsLayout{float width,height,rowPitch,topRow;};
inline RowsPanel drawn;
inline RowsLayout rowsLayout(const RowsPanel& p){float h=100+72*float(p.rows.size())+76;return {.9f,h*.9f/1024,72*.9f/1024,(h*.5f-136)*.9f/1024};}
// Deterministic panel-intersection boundary for testing the real menu's row/page
// translation. Ray origin.y is the requested local row, x is across the panel.
inline bool rowsPanelHit(const glm::mat4&,const RowsPanel& p,const glm::vec3& o,const glm::vec3& d,glm::vec3* point=nullptr,glm::vec2* uv=nullptr){
 if(d.z>=0||o.x<0||o.x>1||o.y<-1||o.y>float(p.rows.size())+1)return false;
 if(point)*point=o;
 if(uv)*uv={o.x,(100+72*(o.y+.5f))/(100+72*float(p.rows.size())+76)};
 return true;
}
inline int rowAt(const glm::mat4& m,const RowsPanel& p,const glm::vec3& o,const glm::vec3& d,float* u){if(!rowsPanelHit(m,p,o,d))return -1;if(u)*u=o.x;int row=int(std::floor(o.y));return row>=0&&row<int(p.rows.size())?row:-1;}
inline void drawRowsPanel(const glm::mat4&,const glm::mat4&,const RowsPanel& p){drawn=p;}
}

namespace vr::xr {
struct SettingsRow {
 std::string label,value,note;
 std::vector<std::string> choices;
 int choice=-1;
 bool selected{},dim{},stepper{},action{};
};
struct SettingsPanel {
 std::string game,title,subtitle,footer;
 int activeTab{},selectedTab=-1;
 bool back{},backSelected{},closeSelected{},dialog{};
 std::vector<SettingsRow> rows;
};
struct SettingsHit {
 enum class Kind {None,Close,Back,Tab,Row};
 Kind kind=Kind::None;
 int index=-1,choice=-1,direction=0;
};
inline SettingsPanel drawnSettings;
inline bool settingsPanelHit(const glm::mat4&,const SettingsPanel&,const glm::vec3& o,const glm::vec3& d,glm::vec3* point,glm::vec2* uv){
 if(d.z>=0)return false;
 *point=o;*uv={o.x,o.y};return true;
}
inline SettingsHit settingsAt(const SettingsPanel& p,glm::vec2 uv){
 using K=SettingsHit::Kind;
 if(uv.y==-1)return {K::Tab,int(uv.x)};
 if(uv.y==-2)return {K::Back};
 const int row=int(uv.y);
 if(row<0||row>=int(p.rows.size()))return {};
 const auto& r=p.rows[row];
 if(!r.choices.empty()&&uv.x>=0&&uv.x<int(r.choices.size()))return {K::Row,row,int(uv.x)};
 return {K::Row,row,-1,r.stepper?(uv.x<0?-1:1):0};
}
inline void drawSettingsPanel(const glm::mat4&,const glm::mat4&,const SettingsPanel& p){drawnSettings=p;}
inline void drawPanelContact(const glm::mat4&,const glm::mat4&){}
}
