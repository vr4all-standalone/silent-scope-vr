// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string_view>
#include "dc_menu_actions.h"
#include "dc_mode_select.h"
namespace silent_scope::dc_native_menus {
using ProvenMenu=dc_menu::MenuKind;
struct Rect{float x0{},y0{},x1{},y1{};bool contains(float x,float y)const{return x>=x0&&x<x1&&y>=y0&&y<y1;}};
class NativeMenusProof {
 const void* frame_{};uint64_t generation_{},submission_{};ProvenMenu kind_{ProvenMenu::None};int location_{-1},mode_{-1},page_{-1};
 std::array<Rect,6> option_labels_{},option_values_{};unsigned option_rows_{};
 std::array<Rect,2> ranking_arrows_{};ModeSelectProof mode_select_{};
 template<class C>static auto no_modifiers(const C& c,int)->decltype(c.modtrig.empty()){return c.modtrig.empty();}
 template<class C>static bool no_modifiers(const C&,long){return true;}
 template<class P>static bool material(const P& p,uint32_t tcw,bool texture=true,uint32_t tsp=0x9491a4ed){return p.isp.full==0xc8800000&&p.tsp.full==tsp&&
  (!texture||p.tcw.full==tcw)&&p.pcw.full==(texture?0x828c000a:0x828c0002)&&p.tileclip==0x1c09c0;}
 template<class P>static bool same_state(const P& a,const P& b){return a.isp.full==b.isp.full&&a.tsp.full==b.tsp.full&&a.tcw.full==b.tcw.full&&a.pcw.full==b.pcw.full&&a.tileclip==b.tileclip;}
 template<class C,class P>static bool quad(const C& c,const P& p,float x,float y,float w,float h,float q,float u,float v,float uw,float vh,
  unsigned r=255,unsigned g=255,unsigned b=255,int alpha=255){
  if(p.count!=4||uint64_t(p.first)+4>c.verts.size())return false;
  const auto a=c.verts[p.first].col[3];
  for(unsigned i=0;i<4;++i){const auto& z=c.verts[p.first+i];
   if(z.x!=x+(i&2?w:0)||z.y!=y+(i&1?h:0)||z.z!=q||z.u!=u+(i&2?uw:0)||z.v!=v+(i&1?vh:0)||
     z.col[0]!=r||z.col[1]!=g||z.col[2]!=b||z.col[3]!=(alpha<0?a:unsigned(alpha)))return false;
  }return true;
 }
 template<class C>static bool background(const C& c){const auto& p=c.global_param_op[0];
  if(p.first||p.count!=4||p.isp.full!=0xe0800000||p.tsp.full!=0x208004c0||p.tcw.full||p.pcw.full!=2||p.tileclip)return false;
  for(unsigned i=0;i<4;++i){const auto& v=c.verts[i];if(c.idx[i]!=i||v.x!=(i&1?896.f:-256.f)||v.y!=(i&2?480.f:0.f)||v.z!=8.999988494906574e-6f||v.u||v.v||v.col[0]||v.col[1]||v.col[2]||v.col[3]!=255)return false;}
  return true;
 }
 // The actual sorted commands may merge equal materials. Recover the original
 // four-vertex polygon from each triangle; prove every native quad exactly once.
 template<class C>static bool complete_geometry(const C& c,unsigned opaque,bool ranking_neighbors=false){
  const auto n=c.global_param_tr.size();const unsigned base=opaque*4;
  if(n>(ranking_neighbors?704u:512u)||c.verts.size()!=base+4*n||c.idx.size()!=base+6*n||c.global_param_op.size()!=opaque||!c.global_param_pt.empty()||
    c.render_passes.size()!=1||c.sortedTriangles.empty()||c.sortedTriangles.size()>n||!no_modifiers(c,0))return false;
  const auto& pass=c.render_passes[0];if(!pass.autosort||pass.op_count!=opaque||pass.pt_count||pass.tr_count!=n||pass.sorted_tr_count!=c.sortedTriangles.size())return false;
  const float xmin=ranking_neighbors?-640.f:-256.f,xmax=ranking_neighbors?1280.f:896.f;
  for(const auto& v:c.verts)if(!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.z)||!std::isfinite(v.u)||!std::isfinite(v.v)||
    v.x<xmin||v.x>xmax||v.y<0||v.y>512||v.z<=0||v.z>1||std::abs(v.u)>8||std::abs(v.v)>8)return false;
  if(!background(c))return false;
  for(unsigned i=0;i<opaque;++i){const auto& p=c.global_param_op[i];if(p.first!=4*i||p.count!=4)return false;
   for(unsigned j=0;j<4;++j)if(c.idx[4*i+j]!=4*i+j)return false;}
  for(unsigned i=0;i<n;++i)if(c.global_param_tr[i].first!=base+4*i||c.global_param_tr[i].count!=4)return false;
  std::array<unsigned,704> triangles{};std::array<bool,4232> used{};for(unsigned i=0;i<base;++i)used[i]=true;
  for(const auto& draw:c.sortedTriangles){if(draw.polyIndex>=n||draw.first<base||draw.first>c.idx.size()||draw.count>c.idx.size()-draw.first||!draw.count||draw.count%3)return false;
   for(uint64_t j=draw.first;j<uint64_t(draw.first)+draw.count;j+=3){const auto a=c.idx[j],b=c.idx[j+1],d=c.idx[j+2];
    if(a<base||b<base||d<base||a>=c.verts.size()||b>=c.verts.size()||d>=c.verts.size()||a==b||a==d||b==d)return false;
    const unsigned polygon=(std::min({a,b,d})-base)/4,first=base+4*polygon;
    if(a>=first+4||b>=first+4||d>=first+4||!same_state(c.global_param_tr[polygon],c.global_param_tr[draw.polyIndex]))return false;
    const unsigned mask=(1u<<(a-first))|(1u<<(b-first))|(1u<<(d-first)),bit=mask==7?1:mask==14?2:0;
    if(!bit||(triangles[polygon]&bit))return false;
    triangles[polygon]|=bit;
    for(unsigned k=0;k<3;++k){if(used[j+k])return false;used[j+k]=true;}
   }
  }
  for(unsigned i=0;i<n;++i)if(triangles[i]!=3)return false;
  for(unsigned i=0;i<c.idx.size();++i)if(!used[i])return false;
  return true;
 }
 template<class C,class P>static int glyph(const C& c,const P& p,uint32_t atlas,float width,float height,float vstep,uint32_t tsp=0x9491a4ed,unsigned gray=255){
  if(!material(p,atlas,true,tsp)||p.count!=4||uint64_t(p.first)+4>c.verts.size())return -1;
  const auto& v=c.verts[p.first];if(!std::isfinite(v.u)||!std::isfinite(v.v))return -1;
  const int col=int(std::round(v.u*16)),row=int(std::round(v.v/vstep)),code=32+16*row+col;
  if(col<0||col>=16||row<0||row>=8||code<32||code>126||!quad(c,p,v.x,v.y,width,height,.7299270033836365f,col/16.f,row*vstep,1.f/16,vstep,gray,gray,gray))return -1;
  return code;
 }
 template<class C>bool training(const C& c){
  if(c.global_param_tr.size()<24||!complete_geometry(c,1))return false;
  const auto& ps=c.global_param_tr;
  int selected=-1;
  for(unsigned i=0;i<3;++i){const auto& p=ps[i];const auto alpha=c.verts[p.first].col[3];if(alpha!=95&&alpha!=255)return false;
   if(alpha==255){if(selected>=0)return false;selected=int(i);}
   if(!material(p,0x40054e00)||!quad(c,p,80+176.f*i,64,128,128,1.f/3,(i==1?.5f:0),(i==2?.5f:0),.5f,.5f,255,255,255,alpha))return false;}
  if(selected<0||!material(ps[3],0x40054e00)||!quad(c,ps[3],80+176.f*selected,64,128,128,.5f,.5f,.5f,.5f,.5f,255,255,255,-1))return false;
  constexpr float headers[3][8]{{48,209,256,32,0,0,1,.125f},{304,209,256,32,0,.125f,1,.125f},{560,209,32,32,0,.25f,.125f,.125f}};
  for(unsigned i=0;i<3;++i){const auto& t=headers[i];if(!material(ps[4+i],0x400d4b00)||!quad(c,ps[4+i],t[0],t[1],t[2],t[3],.5f,t[4],t[5],t[6],t[7]))return false;}
  constexpr float widths[]{176,200,216,168};constexpr float starts[]{.125f,0,0,0};constexpr float vs[]{.25f,.375f,.5f,.625f};
  int active=-1;
  for(unsigned i=0;i<4;++i){const auto& p=ps[7+i];const auto alpha=c.verts[p.first].col[3];if(alpha!=95&&alpha!=255)return false;
   if(alpha==255){if(active>=0)return false;active=int(i);}
   if(!material(p,0x400d4b00)||!quad(c,p,64,240+32.f*i,widths[i],32,.5f,starts[i],vs[i],widths[i]/256,.125f,255,255,255,alpha))return false;}
  if(active<0)return false;
  std::array<unsigned,4> digits{},ranks{};unsigned time_digits=0,separator=0,panel=0;std::array<std::array<bool,6>,4> digit_slots{};
  unsigned p=11;
  for(;p<ps.size()&&ps[p].tcw.full!=0x500c0000;++p){const auto& poly=ps[p];const auto& v=c.verts[poly.first];
   if(material(poly,0,false)&&quad(c,poly,12,374,616,72,1.f/3,0,0,0,0,119,119,119,119)){if(++panel!=1)return false;continue;}
   if(!material(poly,0x400d4b00))return false;
   const float r=(v.y-240)/32;const int row=int(r);if(r!=row||row<0||row>3)return false;
   if(v.x==536){const float cell=v.u/(3.f/32);const int rank=int(std::round(cell));
    if(rank<0||rank>6||cell!=rank||!quad(c,poly,536,v.y,24,24,1.f/3,rank*3.f/32,27.f/32,3.f/32,3.f/32)||++ranks[row]!=1)return false;
    continue;}
   if(row==0&&v.x==368){if(!quad(c,poly,368,240,16,24,.5f,.625f,.75f,1.f/16,3.f/32)||++separator!=1)return false;continue;}
   const float slot=(v.x-320)/16;const int s=int(slot),number=int(std::round(v.u*16));
   if(s<0||s>=6||slot!=s||number<0||number>9||!quad(c,poly,v.x,v.y,16,24,1.f/3,number/16.f,.75f,1.f/16,3.f/32)||digit_slots[row][s])return false;
   digit_slots[row][s]=true;++digits[row];if(row==0)++time_digits;
  }
  if(panel!=1||separator!=1||time_digits!=4||!digit_slots[0][1]||!digit_slots[0][2]||!digit_slots[0][4]||!digit_slots[0][5])return false;
  for(unsigned r=0;r<4;++r){if(ranks[r]!=1||!digits[r])return false;if(r)for(unsigned s=6-digits[r];s<6;++s)if(!digit_slots[r][s])return false;}
  // The four English hint literals are authored at8c0a4df8..4ef3. Match exact
  // native glyph sequence and layout; unknown/localized text remains unproven.
  constexpr std::string_view hints[]{"Try for the fastest clear time.","Shoot as many targets in a row as you can, trying for the\nlongest streak. Miss, and it's game over.",
   "Try to shoot as accurately as possible to earn points.","Try to hit as many targets as possible within the time\nlimit."};
  unsigned line=0,column=0;
  for(char ch:hints[active]){if(ch=='\n'){++line;column=0;continue;}if(p>=ps.size()||glyph(c,ps[p],0x500c0000,10,20,1.f/16)!=ch||
    c.verts[ps[p].first].x!=20+10.f*column||c.verts[ps[p].first].y!=380+20.f*line)return false;
   ++p;++column;}
  if(p!=ps.size())return false;
  location_=selected;mode_=active;return true;
 }
 template<class C>static bool rank_group(const C& c,unsigned& p,unsigned page,float offset){
  const auto& ps=c.global_param_tr;const bool outdoor=page>=2&&page<=10,time=page>=11;
  const auto text=[&](std::string_view s,float x,float y){for(char ch:s){if(p>=ps.size()||glyph(c,ps[p],0x400c0900,20,30,3.f/32,0x9490a4ed)!=ch||
    c.verts[ps[p].first].x!=x+offset||c.verts[ps[p].first].y!=y)return false;
   ++p;x+=20;}return true;};
  const std::string_view title=page==0?"STORY MODE":page==1?"SHOOTING RANGE INDOOR":outdoor?"SHOOTING RANGE OUTDOOR":
   page==11?"TIME ATTACK MODE [EASY]":page==12?"TIME ATTACK MODE [MEDIUM]":"TIME ATTACK MODE [HARD]";
  if(!text(title,320-10.f*title.size(),70))return false;
  if(outdoor){constexpr std::string_view names[]{"TRAINING 1","TRAINING 2","TRAINING 3","TRAINING 4","TRAINING 5","TRAINING 6","TRAINING 7","TRAINING 8","TRAINING 9"};
   if(!text(names[page-2],220,100))return false;}
  const float y=outdoor?140.f:110.f,x=page==0?100.f:10.f;
  if(!text("Rank",x,y)||!text(time?"Time":"Score",x+(time?130:120),y)||!text("Hit",x+280,y)||!text("Name",x+360,y))return false;
  for(unsigned row=0;row<8;++row){std::array<char,21> chars{};
   for(unsigned col=0;col<21;++col){if(p>=ps.size())return false;const auto& poly=ps[p++];const int code=glyph(c,poly,0x400c0900,20,30,3.f/32,0x9490a4ed);
    if(code<0||c.verts[poly.first].x!=x+10+20.f*col+offset||c.verts[poly.first].y!=y+30+30.f*row)return false;
    chars[col]=char(code);}
   constexpr std::string_view suffix[]{"st","nd","rd","th","th","th","th","th"};
   if(chars[0]!=char('1'+row)||chars[1]!=suffix[row][0]||chars[2]!=suffix[row][1]||chars[3]!=' '||chars[12]!=' '||chars[16]!='%'||chars[17]!=' ')return false;
   const auto digit=[](char ch){return ch>='0'&&ch<='9';};bool seen=false;
   if(time){if((chars[4]!=' '&&!digit(chars[4]))||!digit(chars[5])||chars[6]!='\''||!digit(chars[7])||!digit(chars[8])||chars[9]!='"'||!digit(chars[10])||!digit(chars[11])||chars[7]>'5')return false;}
   else{for(unsigned col=4;col<=11;++col){const auto ch=chars[col];if(digit(ch))seen=true;else if(ch!=' '||seen)return false;}if(!seen)return false;}
   seen=false;unsigned percent=0;for(unsigned col=13;col<=15;++col){const auto ch=chars[col];if(digit(ch)){seen=true;percent=percent*10+unsigned(ch-'0');}else if(ch!=' '||seen)return false;}if(!seen||percent>100)return false;
   for(unsigned col=18;col<21;++col)if(!((chars[col]>='A'&&chars[col]<='Z')||digit(chars[col])||chars[col]=='.'||chars[col]==' '))return false;
  }
  if(page){if(!text("Class",480,y))return false;
   for(unsigned row=0;row<8;++row){if(p>=ps.size())return false;const auto& poly=ps[p++];const auto& v=c.verts[poly.first];const int grade=int(std::round(v.v*8));
    if(grade<0||grade>7||!material(poly,0x100c3900,true,0x9490a4e4)||!quad(c,poly,460+offset,y+40+30.f*row,160,25,.7299270033836365f,0,grade/8.f,1,1.f/8))return false;}
  }return true;
 }
 template<class C>bool ranking(const C& c){
  if(c.global_param_tr.size()<203||!complete_geometry(c,2,true))return false;
  const auto& bg=c.global_param_op[1];if(bg.isp.full!=0xc8800000||bg.tsp.full!=0x2088a4ed||bg.tcw.full!=0x480c9a80||bg.pcw.full!=0x808c002a||bg.tileclip!=0x1c09c0)return false;
  for(unsigned i=0;i<4;++i){const auto& v=c.verts[4+i];if(v.x!=(i&2?24.000011444091797f:616.f)||v.y!=(i&1?18.00001335144043f:462.f)||
    v.z!=1.f/48||v.u!=(i&2?0.f:2.f)||v.v!=(i&1?0.f:.99609375f)||v.col[0]!=176||v.col[1]!=176||v.col[2]!=176||v.col[3]!=255)return false;}
  const auto& ps=c.global_param_tr;
  //090220 scales arrow Y by the signed native cosine after a press, around44.
  // Reversed/collapsed art is legitimate; its actual current bounds are used.
  for(unsigned i=0;i<2;++i){const auto& poly=ps[i];const auto& a=c.verts[poly.first];const auto& b=c.verts[poly.first+1];
   if(!material(poly,0x40054858,true,0x9490a4ed)||std::abs(a.y+b.y-88)>1e-4f||std::abs(b.y-a.y)>48.00001f)return false;
   for(unsigned j=0;j<4;++j){const auto& v=c.verts[poly.first+j];if(v.x!=(i?448.f:160.f)+(j&2?48:0)||v.y!=(j&1?b.y:a.y)||v.z!=2.f/3||
     v.u!=.625f+i*3.f/32+(j&2?3.f/32:0)||v.v!=(j&1?3.f/32:0)||v.col[0]!=255||v.col[1]!=255||v.col[2]!=255||v.col[3]!=a.col[3])return false;}
   ranking_arrows_[i]={i?448.f:160.f,std::min(a.y,b.y),i?496.f:208.f,std::max(a.y,b.y)};
  }
  unsigned p=2;bool heading=false;
  //090500 retains neighboring page groups; the fixed heading is emitted at a
  // page boundary, not necessarily before the retained previous page.
  const auto read_heading=[&](){if(p>=ps.size()||c.verts[ps[p].first].y!=30)return true;
   if(heading)return false;
   heading=true;unsigned column=0;
   for(char ch:std::string_view("RANKING")){if(p>=ps.size()||glyph(c,ps[p],0x400c0900,20,30,3.f/32,0x9490a4ed)!=ch||c.verts[ps[p].first].x!=250+20.f*column||c.verts[ps[p].first].y!=30)return false;++p;++column;}return true;};
  //090500 authors either one page or a retained previous/current/next triplet.
  // Every neighbor is proven as a full native UI page at exactly±640 pixels.
  // A partially translated animation has no centered, clickable page here.
  const bool triplet=ps.size()>240;std::array<int,3> pages{{-1,-1,-1}};
  for(unsigned group=0;group<(triplet?3u:1u);++group){const float offset=triplet?640.f*(int(group)-1):0;
   if(!read_heading())return false;
   for(unsigned candidate=0;candidate<14;++candidate){unsigned next=p;if(rank_group(c,next,candidate,offset)){pages[group]=int(candidate);p=next;break;}}
   if(pages[group]<0)return false;
  }
  if(!read_heading()||!heading||p!=ps.size())return false;
  if(triplet){const int a=(pages[1]-pages[0]+14)%14,b=(pages[2]-pages[1]+14)%14;if(!a||!b||a+b>=14)return false;}
  page_=pages[triplet?1:0];return true;
 }
 template<class C>bool no_vmu_dialog(const C& c){
  if(c.global_param_tr.size()!=312||!complete_geometry(c,1))return false;
  const auto& ps=c.global_param_tr;
  constexpr float panels[4][6]{{440,314,50,52,.7246376872062683f,0},{438,312,54,56,.7194244861602783f,255},
   {40,114,548,180,.5f,0},{38,112,552,184,.49751242995262146f,255}};
  for(unsigned i=0;i<4;++i){const auto& q=panels[i];if(!material(ps[i],0,false,0x949004ed)||
    !quad(c,ps[i],q[0],q[1],q[2],q[3],q[4],0,0,0,0,unsigned(q[5]),unsigned(q[5]),unsigned(q[5]),i&1?255:224))return false;}
  unsigned p=4;int selected=-1;
  const auto text=[&](std::string_view s,float x,float y,unsigned color){const float begin=x;for(char ch:s){if(ch=='\n'){x=begin;y+=20;continue;}
    if(p>=ps.size()||glyph(c,ps[p],0x500c0000,10,20,1.f/16,0x949004ed,color)!=ch||c.verts[ps[p].first].x!=x||c.verts[ps[p].first].y!=y)return false;
    ++p;x+=10;}return true;};
  constexpr std::string_view labels[]{"YES","NO"};
  for(unsigned row=0;row<2;++row){if(p>=ps.size())return false;const auto gray=c.verts[ps[p].first].col[0];if(gray!=128&&gray!=255)return false;
   if(gray==255){if(selected>=0)return false;selected=int(row);}
   if(!text(labels[row],450,320+20.f*row,gray))return false;}
  if(selected<0)return false;
  // Captured English No-VMU query only. Native source separately guards
  //08e4e0phase2/operation7/noVMU; overwrite08d4fe is a different dialog.
  constexpr std::string_view body="Memory card not inserted in Controller Port A\nExpansion Socket 1. Without a memory card, you will\nbe unable to save data even if you start the game. To\nsave, please insert a memory card into Controller\nPort A Expansion Socket 1.\nNote: You will need 20 free blocks available.\n\nDo you wish to start the game now?";
  return text(body,50,120,255)&&p==ps.size();
 }
 // Native Options authoring uses exact sprite rectangles, not inferred row
 // bands. Values below enumerate the primary tables0a406c/4198/41c0/41fc and
 //0ee824/834 plus the actual014 record limits, so a legitimate adjustment does
 // not destroy the displayed-page proof by changing its value glyph.
 template<class C,class P>static bool option_value(const C& c,const P& p,unsigned page,unsigned row){
  const float y=216+32.f*row;
  const auto sprite=[&](uint32_t atlas,unsigned u,unsigned v,unsigned w){return material(p,atlas)&&quad(c,p,456-w*.5f,y,float(w),24,.1f,u/256.f,v/256.f,w/256.f,3.f/32,0,255,0,-1);};
  const auto number=[&](unsigned low,unsigned high){for(unsigned n=low;n<=high;++n)if(sprite(0x40062358,n*16,0,16))return true;return false;};
  const auto boolean=[&]{return sprite(0x40062358,192,0,64)||sprite(0x40062358,160,0,32);};
  if(page==0){if(row==0)return number(1,8);if(row==1)return number(1,4);if(row==2)return boolean();return number(0,5);}
  if(page==1){if(row==1)return boolean();constexpr unsigned uv[5][2]{{0,48},{0,0},{0,24},{96,24},{96,0}};
   for(const auto& r:uv)if(sprite(0x40062c58,r[0],r[1],96))return true;
   return false;}
  if(page==2){if(row)return number(0,3);return sprite(0x40062358,0,72,112)||sprite(0x40062358,112,48,96);}
  if(page==3){if(row==0)return sprite(0x40061a58,160,216,16)||sprite(0x40061a58,176,216,16);if(row==1)return boolean();
   return sprite(0x40062358,0,216,96)||sprite(0x40062c58,0,72,96)||sprite(0x40062358,96,216,96);}
  return page==4&&row==2&&boolean();
 }
 template<class C>bool option_page(const C& c,unsigned page){
  const auto& ps=c.global_param_tr;const auto count=ps.size();unsigned n=0;
  const auto sprite=[&](float x,float y,float w,float h,float q,unsigned u,unsigned v,unsigned uw,unsigned vh,uint32_t atlas,int alpha=255){
   if(n>=count||!material(ps[n],atlas)||!quad(c,ps[n],x,y,w,h,q,u/256.f,v/256.f,uw/256.f,vh/256.f,255,255,255,alpha))return false;
   ++n;return true;};
  constexpr uint32_t tiles[]{0x08055a58,0x080d91ac,0x08059a58,0x080dd1ac,0x0805da58,0x080e11ac};
  for(unsigned i=0;i<6;++i)if(!sprite(256.f*(i%3),256.f*(i/3),256,256,.025f,0,0,256,256,tiles[i]))return false;
  if(page!=5&&!sprite(224,416,192,32,1.f/30,0,216,192,32,0x40055158))return false;
  if(!sprite(144,120,256,72,.05f,0,96,256,72,0x40053f58)||!sprite(400,120,96,72,.05f,0,168,96,72,0x40053f58))return false;
  constexpr unsigned title_x[]{240,232,240,256,240,208},title_w[]{160,176,160,144,160,224},title_v[]{0,24,48,120,72,96};
  if(!sprite(float(title_x[page]),160,float(title_w[page]),24,.1f,0,title_v[page],title_w[page],24,0x40054858))return false;
  constexpr unsigned top[6][8]{{152,120,104,0,0,104,0,0},{256,120,120,0,16,120,0,0},{376,120,112,0,32,112,0,0},
   {152,136,80,144,64,0,16,0},{232,136,112,0,48,112,0,0},{344,136,144,0,64,0,16,0}};
  for(unsigned i=0;i<6;++i){const auto& t=top[i];if(!sprite(float(t[0]),float(t[1]),float(t[2]),16,.1f,t[3]+(page==i?t[5]:0),t[4]+(page==i?t[6]:0),t[2],16,0x40053f58))return false;}
  constexpr unsigned rows[]{4,2,3,3,3,0};const auto row_count=rows[page];
  // Each record is{width,textureU,textureV,atlas}. Position arrays0a39e4/39fc
  // independently author x144 and y216+32*row for the current Mouse profile.
  constexpr uint32_t labels[5][4][4]{
   {{136,0,0,0x40061a58},{160,0,24,0x40061a58},{184,0,72,0x40061a58},{120,136,0,0x40061a58}},
   {{128,0,96,0x40061a58},{128,120,144,0x40061a58},{0,0,0,0},{0,0,0,0}},
   {{80,0,168,0x40061a58},{152,80,168,0x40061a58},{128,128,48,0x40061a58},{0,0,0,0}},
   {{152,0,216,0x40061a58},{112,0,168,0x40054858},{152,0,192,0x40054858},{0,0,0,0}},
   {{64,0,192,0x40061a58},{64,64,192,0x40061a58},{112,128,192,0x40061a58},{0,0,0,0}}};
  for(unsigned r=0;r<row_count;++r){const auto& q=labels[page][r];if(!sprite(144,216+32.f*r,float(q[0]),24,.1f,q[1],q[2],q[0],24,q[3],-1))return false;
   option_labels_[r]={144,216+32.f*r,144+float(q[0]),240+32.f*r};}
  if(page==5){
   if(n>=count||!material(ps[n],0,false,0x9491a4f5)||!quad(c,ps[n],32,216,576,192,.2f,0,0,0,0,0,0,0))return false;
   ++n;
   constexpr float samples[3][5]{{200,225,72,138,64},{310,257,54,104,32},{396,290,36,69,11}};
   for(const auto& q:samples){if(n>=count||!material(ps[n],0x40053658)||!quad(c,ps[n],q[0],q[1],q[2],q[3],2.f/3,1.f/32,.125f,.375f,23.f/32,unsigned(q[4]),unsigned(q[4]),unsigned(q[4])))return false;++n;}
  }
  for(unsigned empty=6;empty>row_count;--empty){const float y=216+32.f*(empty-1);
   if(!sprite(96,y,256,24,.1f,0,168,256,24,0x40062358)||!sprite(352,y,192,24,.1f,0,192,192,24,0x40062358))return false;}
  for(unsigned r=0;r<row_count;++r){if(page==4&&r<2)continue;if(n>=count||!option_value(c,ps[n],page,r))return false;
   const auto& a=c.verts[ps[n].first];const auto& b=c.verts[ps[n].first+3];option_values_[r]={a.x,a.y,b.x,b.y};++n;}
  if(page==5){
   if(n+2>count||!material(ps[n],0,false)||!quad(c,ps[n],110,374,428,84,.5f,0,0,0,0,0,0,0,224))return false;
   ++n;
   if(!material(ps[n],0,false)||!quad(c,ps[n],108,372,432,88,.49751242995262146f,0,0,0,0))return false;
   ++n;
  }
  if(!sprite(192,160,24,24,.1f,160,0,24,24,0x40054858,-1)||!sprite(424,160,24,24,.1f,184,0,24,24,0x40054858,-1))return false;
  if(page==5){unsigned row=0,column=0;constexpr std::string_view hint="Adjust the brightness on your TV\nso that you will not see the\npicture on the far right.";
   for(char ch:hint){if(ch=='\n'){++row;column=0;continue;}if(n>=count||glyph(c,ps[n],0x500c0000,12,24,1.f/16)!=ch||c.verts[ps[n].first].x!=120+12.f*column||c.verts[ps[n].first].y!=380+24.f*row)return false;++n;++column;}
  }
  if(n!=count)return false;
  option_rows_=row_count;return true;
 }
 template<class C>bool options(const C& c){if(c.global_param_tr.size()<28||!complete_geometry(c,1))return false;
  for(unsigned page=0;page<6;++page){option_labels_={};option_values_={};if(option_page(c,page)){page_=int(page);return true;}}return false;
 }
 // Native090e60/naga_tex index5. This is an exact four-quad overlay proof,
 // not a whole-frame 2D exemption. The native consumer must separately own Pause.
 template<class C>static bool pause_overlay(const C& c){
  const auto& ps=c.global_param_tr;
  if(c.verts.size()>65536||c.idx.size()>262144||ps.size()>16384||c.render_passes.size()!=1||c.sortedTriangles.empty()||c.sortedTriangles.size()>16384||!no_modifiers(c,0))return false;
  const auto& pass=c.render_passes[0];if(!pass.autosort||pass.tr_count!=ps.size()||pass.sorted_tr_count!=c.sortedTriangles.size()||pass.op_count!=c.global_param_op.size()||pass.pt_count!=c.global_param_pt.size())return false;
  unsigned begin=unsigned(ps.size()),found=0;
  for(unsigned i=0;i<ps.size();++i)if(ps[i].tcw.full==0x500c3600){if(!found)begin=i;if(i!=begin+found||++found>4)return false;}
  if(found!=4)return false;
  for(const auto& p:c.global_param_op)if(p.tcw.full==0x500c3600)return false;
  for(const auto& p:c.global_param_pt)if(p.tcw.full==0x500c3600)return false;
  unsigned selected=0;const auto base=ps[begin].first;
  for(unsigned i=0;i<4;++i){const auto& p=ps[begin+i];if(p.isp.full!=0xc8800000||p.tsp.full!=0x9491a4e4||p.tcw.full!=0x500c3600||p.pcw.full!=0x828c000a||p.tileclip!=0x100200||p.first!=uint64_t(base)+4*i||p.count!=4||uint64_t(p.first)+4>c.verts.size())return false;
   const auto& color=c.verts[p.first].col;const bool active=i&&color[0]==255&&color[1]==255&&color[2]==160;if(active)++selected;
   const unsigned r=i?(active?255:64):192,g=r,b=active?160:r;
   if(!quad(c,p,i?200:160,i?240+30.f*i:150,i?240:320,i?30:80,.7692307829856873f,0,i?.375f+.125f*i:.125f,1,i?.125f:.375f,r,g,b))return false;
  }if(selected!=1)return false;
  // Merged sorted commands retain original vertex indices. Require both native
  // triangles of each quad exactly once, including their material and range.
  std::array<unsigned,4> triangles{};
  for(const auto& draw:c.sortedTriangles){if(draw.polyIndex>=ps.size()||draw.first>c.idx.size()||draw.count>c.idx.size()-draw.first||draw.count%3)return false;
   for(uint64_t j=draw.first;j<uint64_t(draw.first)+draw.count;j+=3){const auto a=c.idx[j],b=c.idx[j+1],d=c.idx[j+2];if(a>=c.verts.size()||b>=c.verts.size()||d>=c.verts.size())return false;
    const auto in=[&](unsigned v){return v>=base&&uint64_t(v)<uint64_t(base)+16;};if(!in(a)&&!in(b)&&!in(d))continue;
    if(!in(a)||!in(b)||!in(d))return false;
    const unsigned polygon=(std::min({a,b,d})-base)/4,first=base+4*polygon;
    if(a>=first+4||b>=first+4||d>=first+4||!same_state(ps[begin+polygon],ps[draw.polyIndex]))return false;
    const unsigned mask=(1u<<(a-first))|(1u<<(b-first))|(1u<<(d-first)),bit=mask==7?1:mask==14?2:0;
    if(!bit||(triangles[polygon]&bit))return false;
    triangles[polygon]|=bit;
   }
  }for(auto bits:triangles)if(bits!=3)return false;
  return true;
 }
 public:
 // Strict whole-frame proofs or the exact Pause overlay. A native consumer verifies stable
 // screen/selection authority; these methods never read or write live guest RAM.
 template<class C>bool configure(std::string_view title,const C& c){*this=NativeMenusProof{};
  if(title!="T9507N"||c.isRTT||c.framebufferWidth!=640||c.framebufferHeight!=480||!c.dcCamera.supported||!c.dcCamera.generation||!c.dcCamera.submit_sequence||
    c.verts.size()<4||c.idx.size()<4||c.global_param_op.empty())return false;
  if(mode_select_.configure(title,c))kind_=ProvenMenu::ModeSelect;
  else if(pause_overlay(c))kind_=ProvenMenu::Pause;
  else if(c.global_param_op.size()==1&&no_vmu_dialog(c))kind_=ProvenMenu::Dialog;
  else if(c.global_param_op.size()==1&&training(c))kind_=ProvenMenu::Training;
  else if(c.global_param_op.size()==1&&options(c))kind_=ProvenMenu::Options;
  else if(c.global_param_op.size()==2&&ranking(c))kind_=ProvenMenu::Ranking;
  else return false;
  frame_=&c;generation_=c.dcCamera.generation;submission_=c.dcCamera.submit_sequence;return true;
 }
 template<class C>bool valid_for(const C& c)const{return kind_!=ProvenMenu::None&&frame_==&c&&c.dcCamera.supported&&!c.isRTT&&c.framebufferWidth==640&&c.framebufferHeight==480&&generation_==c.dcCamera.generation&&submission_==c.dcCamera.submit_sequence;}
 ProvenMenu kind()const{return kind_;}
 int page()const{return page_;} // Rendered geometry's page, never live native RAM.
 int source_context()const{return kind_==ProvenMenu::Dialog?1:kind_==ProvenMenu::ModeSelect?0x100:0;} // Exact native semantic family.
 int training_location()const{return location_;}int training_mode()const{return mode_;}
 template<class C>dc_menu::MenuAction hit(const C& c,float x,float y)const{
  if(!valid_for(c)||!std::isfinite(x)||!std::isfinite(y))return {};
  if(kind_==ProvenMenu::ModeSelect)return mode_select_.hit(x,y);
  dc_menu::MenuAction action;action.kind=kind_;
  if(kind_==ProvenMenu::Training){action.verb=dc_menu::MenuVerb::Focus;
   for(int i=0;i<3;++i)if(Rect{80+176.f*i,64,208+176.f*i,192}.contains(x,y)){action.column=i;return action;}
   constexpr float widths[]{176,200,216,168};for(int i=0;i<4;++i)if(Rect{64,240+32.f*i,64+widths[i],272+32.f*i}.contains(x,y)){action.row=i;return action;}}
  if(kind_==ProvenMenu::Ranking){action.verb=dc_menu::MenuVerb::Adjust;action.from_page=page_;
   if(ranking_arrows_[0].contains(x,y)){action.direction=-1;return action;}
   if(ranking_arrows_[1].contains(x,y)){action.direction=1;return action;}}
  if(kind_==ProvenMenu::Pause){action.verb=dc_menu::MenuVerb::Confirm;
   for(int row=0;row<3;++row)if(Rect{200,270+30.f*row,440,300+30.f*row}.contains(x,y)){action.row=row;return action;}}
  if(kind_==ProvenMenu::Dialog){action.verb=dc_menu::MenuVerb::Confirm;
   if(Rect{450,320,480,340}.contains(x,y)){action.row=0;return action;}
   if(Rect{450,340,470,360}.contains(x,y)){action.row=1;return action;}}
  if(kind_==ProvenMenu::Options){action.verb=dc_menu::MenuVerb::Focus;action.from_page=page_;
   if(Rect{192,160,216,184}.contains(x,y)){action.page=(page_+5)%6;return action;}
   if(Rect{424,160,448,184}.contains(x,y)){action.page=(page_+1)%6;return action;}
   constexpr Rect top[]{{152,120,256,136},{256,120,376,136},{376,120,488,136},{152,136,232,152},{232,136,344,152},{344,136,488,152}};
   for(unsigned i=0;i<6;++i)if(top[i].contains(x,y)){action.page=int(i);return action;}
   for(unsigned r=0;r<option_rows_;++r)if(option_labels_[r].contains(x,y)||option_values_[r].contains(x,y)){action.row=int(r)+1;return action;}}
  return {};
 }
};
}
