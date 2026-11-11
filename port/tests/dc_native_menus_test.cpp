#include "dreamcast/dc_native_menus.h"
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <vector>
struct V{float x{},y{},z{},u{},v{};uint8_t col[4]{};};
struct P{uint32_t first{},count{};struct W{uint32_t full{};}isp,tsp,tcw,pcw;uint32_t tileclip{};};
struct Pass{bool autosort{};uint32_t op_count{},pt_count{},tr_count{},sorted_tr_count{};};struct Sorted{uint32_t polyIndex{},first{},count{};};
struct Ctx{bool isRTT{};unsigned framebufferWidth{640},framebufferHeight{480};
 struct{uint64_t generation{3},submit_sequence{1073};bool supported{true};}dcCamera;
 std::vector<V>verts;std::vector<uint32_t>idx;std::vector<P>global_param_op,global_param_pt,global_param_tr;std::vector<Pass>render_passes;std::vector<Sorted>sortedTriangles;std::vector<int>modtrig;
};
template<class T>T read(std::istream& f){T v{};f.read(reinterpret_cast<char*>(&v),sizeof v);assert(f);return v;}
Ctx load(std::istream& f){Ctx c;(void)read<double>(f);c.dcCamera.generation=read<uint64_t>(f);c.dcCamera.submit_sequence=read<uint64_t>(f);
 auto nv=read<uint32_t>(f),ni=read<uint32_t>(f),no=read<uint32_t>(f),np=read<uint32_t>(f),nt=read<uint32_t>(f),pass=read<uint32_t>(f),sort=read<uint32_t>(f);
 assert(nv<=65536&&ni<=262144&&no+np+nt<=16384&&pass<=64&&sort<=16384);c.verts.resize(nv);f.read(reinterpret_cast<char*>(c.verts.data()),nv*sizeof(V));c.idx.resize(ni);f.read(reinterpret_cast<char*>(c.idx.data()),ni*4);
 const auto polys=[&](auto& ps,unsigned count){for(unsigned i=0;i<count;++i){P p;p.first=read<uint32_t>(f);p.count=read<uint32_t>(f);p.isp.full=read<uint32_t>(f);p.tsp.full=read<uint32_t>(f);p.tcw.full=read<uint32_t>(f);p.pcw.full=read<uint32_t>(f);p.tileclip=read<uint32_t>(f);ps.push_back(p);}};
 polys(c.global_param_op,no);polys(c.global_param_pt,np);polys(c.global_param_tr,nt);
 for(unsigned i=0;i<pass;++i){Pass p;p.autosort=bool(read<uint32_t>(f));p.op_count=read<uint32_t>(f);p.pt_count=read<uint32_t>(f);p.tr_count=read<uint32_t>(f);p.sorted_tr_count=read<uint32_t>(f);c.render_passes.push_back(p);}
 for(unsigned i=0;i<sort;++i)c.sortedTriangles.push_back({read<uint32_t>(f),read<uint32_t>(f),read<uint32_t>(f)});
 return c;
}
Ctx canvas(){Ctx c;c.verts={{-256,0,8.999988494906574e-6f,0,0,{0,0,0,255}},{896,0,8.999988494906574e-6f,0,0,{0,0,0,255}},
 {-256,480,8.999988494906574e-6f,0,0,{0,0,0,255}},{896,480,8.999988494906574e-6f,0,0,{0,0,0,255}}};c.idx={0,1,2,3};
 c.global_param_op={{0,4,{0xe0800000},{0x208004c0},{0},{2},0}};return c;}
void add(Ctx& c,float x,float y,float w,float h,float q,float u,float v,float uw,float vh,uint32_t tcw,unsigned alpha=255,unsigned gray=255,bool texture=true,uint32_t tsp=0x9491a4ed){
 P p{uint32_t(c.verts.size()),4,{0xc8800000},{tsp},{tcw},{texture?0x828c000au:0x828c0002u},0x1c09c0};
 for(unsigned i=0;i<4;++i)c.verts.push_back({x+(i&2?w:0),y+(i&1?h:0),q,u+(i&2?uw:0),v+(i&1?vh:0),{uint8_t(gray),uint8_t(gray),uint8_t(gray),uint8_t(alpha)}});
 c.sortedTriangles.push_back({uint32_t(c.global_param_tr.size()),uint32_t(c.idx.size()),6});
 c.idx.insert(c.idx.end(),{p.first,p.first+1,p.first+2,p.first+2,p.first+1,p.first+3});c.global_param_tr.push_back(p);
}
void finish(Ctx& c){c.render_passes={{true,uint32_t(c.global_param_op.size()),0,uint32_t(c.global_param_tr.size()),uint32_t(c.sortedTriangles.size())}};}
void font(Ctx& c,std::string_view s,float x,float y,uint32_t atlas=0x400c0900,uint32_t tsp=0,unsigned gray=255){const float start=x;const bool hint=atlas==0x500c0000;const float w=hint?10.f:20.f,h=hint?20.f:30.f,step=hint?1.f/16:3.f/32;
 for(char ch:s){if(ch=='\n'){x=start;y+=h;continue;}const unsigned cell=unsigned(ch)-32;add(c,x,y,w,h,.7299270033836365f,(cell&15)/16.f,(cell>>4)*step,1.f/16,step,atlas,255,gray,true,tsp?tsp:hint?0x9491a4ed:0x9490a4ed);x+=w;}}
Ctx training(unsigned location=0,unsigned mode=0){Ctx c=canvas();
 for(unsigned i=0;i<3;++i)add(c,80+176.f*i,64,128,128,1.f/3,i==1?.5f:0,i==2?.5f:0,.5f,.5f,0x40054e00,i==location?255:95);
 add(c,80+176.f*location,64,128,128,.5f,.5f,.5f,.5f,.5f,0x40054e00,143);
 add(c,48,209,256,32,.5f,0,0,1,.125f,0x400d4b00);add(c,304,209,256,32,.5f,0,.125f,1,.125f,0x400d4b00);add(c,560,209,32,32,.5f,0,.25f,.125f,.125f,0x400d4b00);
 constexpr float widths[]{176,200,216,168};
 for(unsigned i=0;i<4;++i)add(c,64,240+32.f*i,widths[i],32,.5f,i==0?.125f:0,.25f+.125f*i,widths[i]/256,.125f,0x400d4b00,i==mode?255:95);
 for(unsigned i=0;i<4;++i)add(c,i<2?336+16.f*i:384+16.f*(i-2),240,16,24,1.f/3,9.f/16,.75f,1.f/16,3.f/32,0x400d4b00);
 add(c,368,240,16,24,.5f,.625f,.75f,1.f/16,3.f/32,0x400d4b00);
 for(unsigned i=1;i<4;++i)add(c,400,240+32.f*i,16,24,1.f/3,0,.75f,1.f/16,3.f/32,0x400d4b00);
 for(unsigned i=0;i<4;++i)add(c,536,240+32.f*i,24,24,1.f/3,15.f/32,27.f/32,3.f/32,3.f/32,0x400d4b00);
 add(c,12,374,616,72,1.f/3,0,0,0,0,0x3eaaaaab,119,119,false);
 constexpr std::string_view hints[]{"Try for the fastest clear time.","Shoot as many targets in a row as you can, trying for the\nlongest streak. Miss, and it's game over.","Try to shoot as accurately as possible to earn points.","Try to hit as many targets as possible within the time\nlimit."};
 font(c,hints[mode],20,380,0x500c0000);finish(c);return c;
}
Ctx ranking(){Ctx c=canvas();c.verts.insert(c.verts.end(),{{616,462,1.f/48,2,.99609375f,{176,176,176,255}},{616,18.00001335144043f,1.f/48,2,0,{176,176,176,255}},
 {24.000011444091797f,462,1.f/48,0,.99609375f,{176,176,176,255}},{24.000011444091797f,18.00001335144043f,1.f/48,0,0,{176,176,176,255}}});
 c.idx.insert(c.idx.end(),{4,5,6,7});c.global_param_op.push_back({4,4,{0xc8800000},{0x2088a4ed},{0x480c9a80},{0x808c002a},0x1c09c0});
 for(unsigned i=0;i<2;++i)add(c,i?448:160,20,48,48,2.f/3,.625f+i*3.f/32,0,3.f/32,3.f/32,0x40054858,247,255,true,0x9490a4ed);
 font(c,"RANKING",250,30);font(c,"STORY MODE",220,70);font(c,"Rank",100,110);font(c,"Score",220,110);font(c,"Hit",380,110);font(c,"Name",460,110);
 constexpr std::string_view suffix[]{"st","nd","rd","th","th","th","th","th"};
 for(unsigned row=0;row<8;++row)font(c,std::string(1,char('1'+row))+std::string(suffix[row])+" 99999999 100% Z9.",110,140+30.f*row);
 finish(c);return c;
}
void verify(Ctx& c,int family,int page=0){using namespace silent_scope;dc_native_menus::NativeMenusProof proof;assert(proof.configure("T9507N",c));
 const auto kind=family==1?dc_menu::MenuKind::Training:dc_menu::MenuKind::Ranking;assert(proof.kind()==kind&&proof.page()==(family==1?-1:page));
 if(family==1){for(int col=0;col<3;++col){auto a=proof.hit(c,80+176.f*col,64);assert(a.kind==kind&&a.verb==dc_menu::MenuVerb::Focus&&a.column==col&&a.row==-1);
   assert(proof.hit(c,208+176.f*col,64).kind==dc_menu::MenuKind::None);}
  constexpr float widths[]{176,200,216,168};for(int row=0;row<4;++row){auto a=proof.hit(c,64,240+32.f*row);assert(a.kind==kind&&a.row==row&&a.column==-1);assert(proof.hit(c,64+widths[row],240+32.f*row).kind==dc_menu::MenuKind::None);}
  assert(proof.hit(c,400,350).kind==dc_menu::MenuKind::None); // Record data is not a mode control.
 }else{auto left=proof.hit(c,160,44),right=proof.hit(c,495.99f,44);assert(left.kind==kind&&left.verb==dc_menu::MenuVerb::Adjust&&left.direction==-1&&left.from_page==page);
  assert(right.kind==kind&&right.direction==1);assert(proof.hit(c,208,20).kind==dc_menu::MenuKind::None);assert(proof.hit(c,448,68).kind==dc_menu::MenuKind::None);
  assert(proof.hit(c,250,140).kind==dc_menu::MenuKind::None); // A score entry is not a native item.
 }
 assert(proof.hit(c,0,0).kind==dc_menu::MenuKind::None&&proof.hit(c,std::numeric_limits<float>::quiet_NaN(),64).kind==dc_menu::MenuKind::None);
 auto clone=c;assert(!proof.valid_for(clone));++c.dcCamera.submit_sequence;assert(!proof.valid_for(c));--c.dcCamera.submit_sequence;
 for(unsigned change=0;change<21;++change){auto bad=c;
  if(change==0)bad.isRTT=true;
  if(change==1)bad.dcCamera.supported=false;
  if(change==2)bad.dcCamera.generation=0;
  if(change==3)bad.framebufferWidth=641;
  if(change==4)bad.verts[0].x=-255;
  if(change==5)bad.verts.back().u+=.01f;
  if(change==6)bad.verts[bad.global_param_tr[0].first].col[0]=254;
  if(change==7)bad.global_param_tr[0].tcw.full^=1;
  if(change==8)bad.global_param_tr[0].pcw.full^=8;
  if(change==9)bad.idx.back()=bad.idx[bad.idx.size()-2];
  if(change==10)bad.sortedTriangles[0].count=3;
  if(change==11)bad.sortedTriangles.back().first=bad.sortedTriangles[0].first;
  if(change==12)bad.render_passes[0].autosort=false;
  if(change==13)bad.modtrig.push_back(1);
  if(change==14)bad.global_param_tr.push_back(bad.global_param_tr[0]);
  if(change==15)bad.verts[bad.global_param_tr[0].first].x+=.5f; // Real transition-like partial movement.
  if(change==16)bad.verts[bad.global_param_tr[0].first].z=std::numeric_limits<float>::quiet_NaN();
  if(change==17)bad.verts[bad.global_param_tr[0].first].v=1e30f;
  if(change==18)bad.global_param_tr[0].first=UINT32_MAX;
  if(change==19)bad.global_param_pt.push_back(bad.global_param_tr[0]);
  if(change==20)bad.global_param_tr.back().tsp.full^=1; // Merged command cannot cover mismatching original material.
  assert(!proof.configure("T9507N",bad)&&proof.hit(bad,160,64).kind==dc_menu::MenuKind::None&&proof.page()==-1);
 }
 assert(!proof.configure("T9505D",c));assert(proof.configure("T9507N",c));++c.dcCamera.generation;assert(!proof.valid_for(c));--c.dcCamera.generation;
}
Ctx no_vmu(unsigned selected=0){Ctx c=canvas();constexpr float panels[4][6]{{440,314,50,52,.7246376872062683f,0},{438,312,54,56,.7194244861602783f,255},{40,114,548,180,.5f,0},{38,112,552,184,.49751242995262146f,255}};
 for(unsigned i=0;i<4;++i){const auto& p=panels[i];add(c,p[0],p[1],p[2],p[3],p[4],0,0,0,0,0,i&1?255:224,unsigned(p[5]),false,0x949004ed);}
 font(c,"YES",450,320,0x500c0000,0x949004ed,selected==0?255:128);font(c,"NO",450,340,0x500c0000,0x949004ed,selected==1?255:128);
 font(c,"Memory card not inserted in Controller Port A\nExpansion Socket 1. Without a memory card, you will\nbe unable to save data even if you start the game. To\nsave, please insert a memory card into Controller\nPort A Expansion Socket 1.\nNote: You will need 20 free blocks available.\n\nDo you wish to start the game now?",50,120,0x500c0000,0x949004ed);finish(c);return c;}
void verify_dialog(Ctx& c){using namespace silent_scope;dc_native_menus::NativeMenusProof proof;assert(proof.configure("T9507N",c)&&proof.kind()==dc_menu::MenuKind::Dialog&&proof.page()==-1&&proof.source_context()==1);
 const auto yes=proof.hit(c,450,320),no=proof.hit(c,469.99f,359.99f);assert(yes.kind==dc_menu::MenuKind::Dialog&&yes.verb==dc_menu::MenuVerb::Confirm&&yes.row==0&&no.row==1);
 assert(proof.hit(c,470,350).kind==dc_menu::MenuKind::None&&proof.hit(c,480,330).kind==dc_menu::MenuKind::None&&proof.hit(c,450,360).kind==dc_menu::MenuKind::None);
 assert(proof.hit(c,80,80).kind==dc_menu::MenuKind::None&&proof.hit(c,50,120).kind==dc_menu::MenuKind::None);
 auto clone=c;assert(!proof.valid_for(clone));++c.dcCamera.submit_sequence;assert(!proof.valid_for(c));--c.dcCamera.submit_sequence;
 for(unsigned n=0;n<8;++n){auto bad=c;if(n==0)bad.verts.back().u+=1.f/16; // Different message cannot grant these actions.
  if(n==1)bad.verts[4].x+=1;
  if(n==2)bad.global_param_tr[0].tsp.full^=1;
  if(n==3)bad.global_param_tr[4].tcw.full^=1;
  if(n==4)bad.sortedTriangles[0].first=bad.sortedTriangles.back().first;
  if(n==5)bad.verts[20].col[0]=100;
  if(n==6)bad.isRTT=true;
  if(n==7)bad.global_param_tr.push_back(bad.global_param_tr[0]);
  assert(!proof.configure("T9507N",bad));}
}
Ctx options_game(){Ctx c=canvas();const auto sprite=[&](float x,float y,float w,float h,float q,unsigned u,unsigned v,unsigned uw,unsigned vh,uint32_t atlas,unsigned alpha=255){add(c,x,y,w,h,q,u/256.f,v/256.f,uw/256.f,vh/256.f,atlas,alpha);};
 constexpr uint32_t tiles[]{0x08055a58,0x080d91ac,0x08059a58,0x080dd1ac,0x0805da58,0x080e11ac};for(unsigned i=0;i<6;++i)sprite(256.f*(i%3),256.f*(i/3),256,256,.025f,0,0,256,256,tiles[i]);
 sprite(224,416,192,32,1.f/30,0,216,192,32,0x40055158);sprite(144,120,256,72,.05f,0,96,256,72,0x40053f58);sprite(400,120,96,72,.05f,0,168,96,72,0x40053f58);sprite(240,160,160,24,.1f,0,0,160,24,0x40054858);
 constexpr unsigned top[6][5]{{152,120,104,104,0},{256,120,120,0,16},{376,120,112,0,32},{152,136,80,144,64},{232,136,112,0,48},{344,136,144,0,64}};
 for(const auto& t:top)sprite(float(t[0]),float(t[1]),float(t[2]),16,.1f,t[3],t[4],t[2],16,0x40053f58);
 constexpr unsigned labels[4][3]{{136,0,0},{160,0,24},{184,0,72},{120,136,0}};
 for(unsigned i=0;i<4;++i)sprite(144,216+32.f*i,float(labels[i][0]),24,.1f,labels[i][1],labels[i][2],labels[i][0],24,0x40061a58,127);
 for(unsigned i=0;i<2;++i){sprite(96,376-32.f*i,256,24,.1f,0,168,256,24,0x40062358);sprite(352,376-32.f*i,192,24,.1f,0,192,192,24,0x40062358);}
 for(unsigned i=0;i<4;++i){add(c,i==2?440:448,216+32.f*i,i==2?32:16,24,.1f,i==0?.25f:i==1?.1875f:i==2?.625f:.125f,0,i==2?.125f:.0625f,.09375f,0x40062358,127);auto& p=c.global_param_tr.back();for(unsigned v=0;v<4;++v)c.verts[p.first+v].col[0]=c.verts[p.first+v].col[2]=0;}
 sprite(192,160,24,24,.1f,160,0,24,24,0x40054858,164);sprite(424,160,24,24,.1f,184,0,24,24,0x40054858,164);finish(c);return c;}
void verify_options(Ctx& c,int page){using namespace silent_scope;dc_native_menus::NativeMenusProof proof;assert(proof.configure("T9507N",c)&&proof.kind()==dc_menu::MenuKind::Options&&proof.page()==page);
 constexpr unsigned rows[]{4,2,3,3,3,0};const auto arrow=proof.hit(c,192,160);assert(arrow.kind==dc_menu::MenuKind::Options&&arrow.verb==dc_menu::MenuVerb::Focus&&arrow.page==(page+5)%6&&arrow.from_page==page);
 assert(proof.hit(c,216,170).kind==dc_menu::MenuKind::None&&proof.hit(c,448,170).kind==dc_menu::MenuKind::None);
 for(unsigned row=0;row<rows[page];++row){const auto a=proof.hit(c,144,216+32.f*row);assert(a.kind==dc_menu::MenuKind::Options&&a.verb==dc_menu::MenuVerb::Focus&&a.row==int(row)+1&&a.from_page==page);}
 assert(proof.hit(c,500,390).kind==dc_menu::MenuKind::None); // Blank art cannot become an item.
 auto copy=c;assert(!proof.valid_for(copy));++c.dcCamera.submit_sequence;assert(!proof.valid_for(c));--c.dcCamera.submit_sequence;
 for(unsigned change=0;change<9;++change){auto bad=c;
  if(change==0)bad.global_param_tr[0].tcw.full^=1;
  if(change==1)bad.idx.back()=bad.idx[bad.idx.size()-2];
  if(change==2)bad.verts[14].x-=1;
  if(change==3)bad.verts.back().u+=.01f;
  if(change==4)bad.global_param_tr.push_back(bad.global_param_tr[0]);
  if(change==5)bad.modtrig.push_back(1);
  if(change==6)bad.isRTT=true;
  if(change==7)bad.dcCamera.generation=0;
  if(change==8)bad.sortedTriangles[0].count=3;
  assert(!proof.configure("T9507N",bad));}
 // Exercise authored value alternatives while preserving their row-specific
 // atlas, dimensions and anchoring. These are fixture edits, never guest writes.
 for(const auto& p:c.global_param_tr){const auto& a=c.verts[p.first];if(a.col[0]!=0||a.col[1]!=255||a.col[2]!=0)continue;
  const unsigned row=unsigned((a.y-216)/32);if(row>=rows[page])continue;
  const auto replace=[&](uint32_t atlas,unsigned u,unsigned v,unsigned w){auto changed=c;auto& poly=changed.global_param_tr[&p-c.global_param_tr.data()];poly.tcw.full=atlas;
   for(unsigned i=0;i<4;++i){auto& z=changed.verts[poly.first+i];z.x=456-w*.5f+(i&2?float(w):0);z.u=(u+(i&2?w:0))/256.f;z.v=(v+(i&1?24:0))/256.f;}
   dc_native_menus::NativeMenusProof value;assert(value.configure("T9507N",changed)&&value.page()==page);
  };
  if((page==0&&row==2)||(page==1&&row==1)||(page==3&&row==1)||(page==4&&row==2)){replace(0x40062358,192,0,64);replace(0x40062358,160,0,32);}
  if(page==0&&row!=2){unsigned low=row<2?1:0,high=row==0?8:row==1?4:5;for(unsigned digit=low;digit<=high;++digit)replace(0x40062358,16*digit,0,16);}
  if(page==1&&row==0){for(const auto uv:std::array<std::array<unsigned,2>,5>{{{0,48},{0,0},{0,24},{96,24},{96,0}}})replace(0x40062c58,uv[0],uv[1],96);}
  if(page==2&&row==0){replace(0x40062358,0,72,112);replace(0x40062358,112,48,96);}
  if(page==2&&row>0)for(unsigned digit=0;digit<4;++digit)replace(0x40062358,16*digit,0,16);
  if(page==3&&row==0){replace(0x40061a58,160,216,16);replace(0x40061a58,176,216,16);}
  if(page==3&&row==2){replace(0x40062358,0,216,96);replace(0x40062c58,0,72,96);replace(0x40062358,96,216,96);}
 }
}
Ctx pause_overlay(unsigned row=0){auto c=canvas();
 for(unsigned i=0;i<4;++i){const unsigned color=i==0?192:i==row+1?255:64;
  add(c,i?200:160,i?240+30.f*i:150,i?240:320,i?30:80,.7692307829856873f,0,i?.375f+.125f*i:.125f,1,i?.125f:.375f,0x500c3600,255,color,true,0x9491a4e4);
  c.global_param_tr.back().tileclip=0x100200;
  if(i==row+1)for(unsigned j=0;j<4;++j)c.verts[c.verts.size()-4+j].col[2]=160;
 }finish(c);return c;
}
void verify_pause(Ctx& c){using namespace silent_scope;dc_native_menus::NativeMenusProof p;assert(p.configure("T9507N",c));assert(p.kind()==dc_menu::MenuKind::Pause&&p.page()==-1);
 for(int row=0;row<3;++row){auto a=p.hit(c,200,270+30.f*row);assert(a.kind==dc_menu::MenuKind::Pause&&a.verb==dc_menu::MenuVerb::Confirm&&a.row==row);}
 assert(p.hit(c,440,270).kind==dc_menu::MenuKind::None&&p.hit(c,320,230).kind==dc_menu::MenuKind::None&&p.hit(c,200,360).kind==dc_menu::MenuKind::None);
 auto clone=c;assert(!p.valid_for(clone));++c.dcCamera.submit_sequence;assert(!p.valid_for(c));--c.dcCamera.submit_sequence;
 unsigned first=0;for(;first<c.global_param_tr.size();++first)if(c.global_param_tr[first].tcw.full==0x500c3600)break;assert(first+4<=c.global_param_tr.size());
 for(unsigned change=0;change<23;++change){auto bad=c;auto& poly=bad.global_param_tr[first];auto& v=bad.verts[poly.first];
  if(change==0)bad.isRTT=true;
  if(change==1)bad.dcCamera.supported=false;
  if(change==2)bad.dcCamera.generation=0;
  if(change==3)bad.framebufferWidth=641;
  if(change==4)v.x+=1;
  if(change==5)v.u+=.01f;
  if(change==6)v.col[0]=191;
  if(change==7)poly.pcw.full^=8;
  if(change==8)poly.tcw.full^=1;
  if(change==9)poly.tileclip^=1;
  if(change==10)poly.count=3;
  if(change==11)poly.first=uint32_t(bad.verts.size());
  if(change==12)v.z=std::numeric_limits<float>::quiet_NaN();
  if(change==13)bad.verts[poly.first+3].col[3]=254;
  if(change==14)bad.sortedTriangles.clear();
  if(change==15)bad.render_passes[0].autosort=false;
  if(change==16)bad.idx.back()=uint32_t(bad.verts.size());
  if(change==17)bad.global_param_tr.erase(bad.global_param_tr.begin()+first+3);
  if(change==18)bad.global_param_tr.push_back(poly);
  if(change==19)for(unsigned row=1;row<=3;++row)for(unsigned j=0;j<4;++j){auto& v2=bad.verts[bad.global_param_tr[first+row].first+j];v2.col[0]=v2.col[1]=v2.col[2]=64;}
  if(change==20)for(unsigned row=1;row<=3;++row)for(unsigned j=0;j<4;++j){auto& v2=bad.verts[bad.global_param_tr[first+row].first+j];v2.col[0]=v2.col[1]=255;v2.col[2]=160;}
  if(change==21)bad.sortedTriangles.back().count-=3;
  if(change==22)++bad.dcCamera.submit_sequence; // A newly configured identity is valid; stale proof is not.
  if(change==22){assert(!p.valid_for(bad));continue;}
  assert(!p.configure("T9507N",bad));
 }
}
int main(int argc,char** argv){assert(argc==1||argc==3||argc==4||argc==5||argc==6||argc==7||argc==8||argc==9);unsigned total=0;
 for(unsigned row=0;row<3;++row){auto c=pause_overlay(row);verify_pause(c);}
 auto options=options_game();verify_options(options,0);
 for(unsigned selected=0;selected<2;++selected){auto c=no_vmu(selected);verify_dialog(c);}
 for(unsigned column=0;column<3;++column)for(unsigned row=0;row<4;++row){auto c=training(column,row);verify(c,1);}
 auto rank=ranking();verify(rank,2);
 if(argc>1)for(int family=1;family<=2;++family){std::ifstream f(argv[family],std::ios::binary);char magic[8]{};f.read(magic,8);assert(!std::memcmp(magic,"SSSCST01",8));auto count=read<uint32_t>(f);assert(count&&count<=100);
  for(unsigned n=0;n<count;++n){auto c=load(f);verify(c,family);++total;}
 }
 if(argc>3){std::ifstream f(argv[3],std::ios::binary);char magic[8]{};f.read(magic,8);assert(!std::memcmp(magic,"SSSCST01",8));auto count=read<uint32_t>(f);assert(count&&count<=100);
  for(unsigned n=0;n<count;++n){auto c=load(f);silent_scope::dc_native_menus::NativeMenusProof proof;assert(!proof.configure("T9507N",c));}
  std::cout<<"Preserved unproven actual gameplay frames="<<count<<'\n';
 }
 if(argc>4){std::ifstream f(argv[4],std::ios::binary);char magic[8]{};f.read(magic,8);assert(!std::memcmp(magic,"SSSCST01",8));const auto count=read<uint32_t>(f);unsigned positive=0;
  for(unsigned n=0;n<count;++n){auto c=load(f);if(c.verts.size()>1000){verify_dialog(c);++positive;}else{silent_scope::dc_native_menus::NativeMenusProof proof;assert(!proof.configure("T9507N",c));}}
  assert(positive==10);std::cout<<"Verified actualNoVMUquery10 and retained background-only Unknown6\n";
 }
 if(argc>5){using namespace silent_scope;std::ifstream f(argv[5],std::ios::binary);char magic[8]{};f.read(magic,8);assert(!std::memcmp(magic,"SSSCST01",8));const auto count=read<uint32_t>(f);assert(count==20);unsigned positive=0;
  for(unsigned n=0;n<count;++n){auto c=load(f);dc_native_menus::NativeMenusProof proof;
   if(c.verts.size()==292){assert(!proof.configure("T9507N",c));continue;} // Actual gray outlined page-change frame.
   const int page=n<3?0:n<5?1:n<8?2:n<11?3:n<14?4:5;
   verify_options(c,page);++positive;
  }assert(positive==19);std::cout<<"Verified actualOptions6categories19stable and1nativeoutline-transitionnegative\n";
 }
 if(argc>6){using namespace silent_scope;std::ifstream f(argv[6],std::ios::binary);char magic[8]{};f.read(magic,8);assert(!std::memcmp(magic,"SSSCST01",8));const auto count=read<uint32_t>(f);assert(count==20);unsigned positive=0;
  constexpr int pages[]{0,0,0,1,-1,2,11,-1,12,-1,13,0,0,0,0,0,0,0,0,0};
  for(unsigned n=0;n<count;++n){auto c=load(f);dc_native_menus::NativeMenusProof proof;
   if(pages[n]<0){assert(!proof.configure("T9507N",c));continue;}

   verify(c,2,pages[n]);++positive;
   // A malformed offscreen neighbor must reject the complete frame too.
   if(c.verts.size()>1000){auto bad=c;bad.verts[bad.global_param_tr[10].first].u+=.01f;assert(!proof.configure("T9507N",bad));}
  }assert(positive==17);std::cout<<"Verified retainedRanking17stable/3nativeanimations withrenderedcenterpage\n";
 }
 if(argc>7){using namespace silent_scope;std::ifstream f(argv[7],std::ios::binary);char magic[8]{};f.read(magic,8);assert(!std::memcmp(magic,"SSSCST01",8));const auto count=read<uint32_t>(f);unsigned positive=0;
  for(unsigned n=0;n<count;++n){auto c=load(f);const bool has_pause=std::any_of(c.global_param_tr.begin(),c.global_param_tr.end(),[](const auto& p){return p.tcw.full==0x500c3600;});
   if(has_pause){verify_pause(c);++positive;}else{dc_native_menus::NativeMenusProof p;assert(!p.configure("T9507N",c));}}
  assert(positive==21);std::cout<<"Verified21actualPauseoverlays/all3rows and19prePauseworldnegatives\n";
 }
 if(argc>8){using namespace silent_scope;std::ifstream f(argv[8],std::ios::binary);char magic[8]{};f.read(magic,8);assert(!std::memcmp(magic,"SSSCST01",8));const auto count=read<uint32_t>(f);assert(count==20);
  for(unsigned n=0;n<count;++n){auto c=load(f);dc_native_menus::NativeMenusProof p;assert(p.configure("T9507N",c)&&p.kind()==dc_menu::MenuKind::ModeSelect&&p.source_context()==0x100);
   constexpr float xy[][2]{{320,240},{230,140},{400,140},{200,390},{320,390},{430,390}};
   for(unsigned i=0;i<6;++i){auto a=p.hit(c,xy[i][0],xy[i][1]);assert(a.kind==dc_menu::MenuKind::ModeSelect&&a.verb==dc_menu::MenuVerb::Focus&&a.row==(i?i<3?1:int(i)-1:0)&&a.column==(i==1?0:i==2?1:-1));}
   // The large titles are part of the visible Shooting Range and Time Attack cards.
   for(int column=0;column<2;++column){auto a=p.hit(c,column==0?230:400,100);
    assert(a.kind==dc_menu::MenuKind::ModeSelect&&a.row==1&&a.column==column);}
   for(int column=0;column<3;++column){auto a=p.hit(c,200+115*column,345);
    assert(a.kind==dc_menu::MenuKind::ModeSelect&&a.row==2+column&&a.column==-1);}
   assert(p.hit(c,80,150).kind==dc_menu::MenuKind::None); // Decorative sidephotos are never actions.
   assert(p.hit(c,320,57).kind==dc_menu::MenuKind::None); // Choose-header is not a button.
   assert(p.hit(c,100,420).kind==dc_menu::MenuKind::None); // Caption is not a button.
   auto clone=c;assert(!p.valid_for(clone));++c.dcCamera.submit_sequence;assert(!p.valid_for(c));--c.dcCamera.submit_sequence;
   for(unsigned change=0;change<15;++change){auto bad=c;auto& poly=bad.global_param_tr[0];auto& v=bad.verts[poly.first];
    if(change==0)bad.isRTT=true;
    if(change==1)bad.dcCamera.supported=false;
    if(change==2)bad.dcCamera.generation=0;
    if(change==3)bad.framebufferWidth=641;
    if(change==4)v.x+=.01f;
    if(change==5)v.u+=.001f;
    if(change==6)v.col[0]=175;
    if(change==7)poly.tcw.full^=1;
    if(change==8)poly.tileclip^=1;
    if(change==9)bad.verts.back().u+=.001f;
    if(change==10)bad.sortedTriangles.back().count-=3;
    if(change==11)bad.modtrig.push_back(1);
    if(change==12)bad.global_param_tr.back().first=uint32_t(bad.verts.size());
    if(change==13)bad.idx.back()=uint32_t(bad.verts.size());
    if(change==14)v.x=std::numeric_limits<float>::quiet_NaN();
    assert(!p.configure("T9507N",bad));
   }
   // Every native action art cell must remain intact, including precise UV,
   // topology, dim/active color, material and projected source geometry.
   constexpr uint32_t atlas[]{0x50049700,0x100c9280,0x100c9280,0x50049700,0x50049700,0x50049400};
   constexpr float domain[][4]{{0,1,.50390625f,.74609375f},{.00390625f,.50390625f,.50390625f,.98828125f},{.00390625f,.62890625f,.00390625f,.48828125f},{0,.5f,.74609375f,.99609375f},{.5f,1,.74609375f,.99609375f},{0,.5f,.49609375f,.7421875f}};
   for(unsigned cell=0;cell<6;++cell){unsigned index=0;for(;index<c.global_param_tr.size();++index){const auto& poly2=c.global_param_tr[index];if(poly2.tcw.full!=atlas[cell]||poly2.count!=4)continue;
     float u0=10,u1=-10,v0=10,v1=-10;for(unsigned j=0;j<4;++j){const auto& v2=c.verts[poly2.first+j];u0=std::min(u0,v2.u);u1=std::max(u1,v2.u);v0=std::min(v0,v2.v);v1=std::max(v1,v2.v);}
     if(u0==domain[cell][0]&&u1==domain[cell][1]&&v0==domain[cell][2]&&v1==domain[cell][3])break;
    }assert(index<c.global_param_tr.size());
    for(unsigned change=0;change<6;++change){auto bad=c;auto& poly2=bad.global_param_tr[index];auto& v2=bad.verts[poly2.first];
     if(change==0)v2.x+=.01f;
     if(change==1)v2.z+=.001f;
     if(change==2)v2.u+=.001f;
     if(change==3)v2.col[3]=126;
     if(change==4)poly2.pcw.full^=8;
     if(change==5)bad.global_param_tr.erase(bad.global_param_tr.begin()+index);
     assert(!p.configure("T9507N",bad));
    }
   }
  }std::cout<<"Verified20actualModeSelectframes/all6nativeartcells\n";
 }
 std::cout<<"Verified12 source-derived Training pairs, Ranking dynamic data and actual native frames="<<total<<'\n';
}
