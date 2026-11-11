#include "dreamcast/dc_native_menu.h"
#include "dreamcast/dc_ui_domain.h"
#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <vector>
struct V{float x{},y{},z{},u{},v{};uint8_t col[4]{};};
struct P{uint32_t first{},count{};struct W{uint32_t full{};}isp,tsp,tcw,pcw;uint32_t tileclip{};};
struct Pass{bool autosort{};uint32_t op_count{},pt_count{},tr_count{},sorted_tr_count{};};struct Sorted{uint32_t polyIndex{},first{},count{};};
struct Ctx{bool isRTT{};unsigned framebufferWidth{640},framebufferHeight{480};
 struct{uint64_t generation{3},submit_sequence{1073};bool supported{true};}dcCamera;
 struct{bool supported{},valid{};uint64_t generation{},submit_sequence{};}dcLens;
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
Ctx measured_layout(){Ctx c;c.verts={{-256,0,8.999988494906574e-6f,0,0,{0,0,0,255}},{896,0,8.999988494906574e-6f,0,0,{0,0,0,255}},
  {-256,480,8.999988494906574e-6f,0,0,{0,0,0,255}},{896,480,8.999988494906574e-6f,0,0,{0,0,0,255}}};c.idx={0,1,2,3};
 c.global_param_op={{0,4,{0xe0800000},{0x208004c0},{0},{2},0}};
 const auto add=[&](float x,float y,float w,float h,float q,float u,float v,float uw,float vh,unsigned color,uint32_t tcw){
  P p{uint32_t(c.verts.size()),4,{0xc8800000},{0x9491a4ed},{tcw},{0x828c000a},0x1c09c0};
  for(unsigned i=0;i<4;++i)c.verts.push_back({x+(i&2?w:0),y+(i&1?h:0),q,u+(i&2?uw:0),v+(i&1?vh:0),{uint8_t(color),uint8_t(color),uint8_t(color),255}});
  c.idx.insert(c.idx.end(),{p.first,p.first+1,p.first+2,p.first+2,p.first+1,p.first+3});c.global_param_tr.push_back(p);
 };
 const uint32_t tcw[]{0x08063558,0x080e51ac,0x08067558,0x080e91ac,0x0806b558,0x080ed1ac};
 for(unsigned tile=0;tile<6;++tile)add(256.f*(tile%3),256.f*(tile/3),256,256,.1f,0,0,1,1,255,tcw[tile]);
 const std::string labels[]{"ARCADE MODE","TRAINING MODE","OPTIONS","RANKING"};
 for(unsigned row=0;row<4;++row)for(unsigned i=0;i<labels[row].size();++i){const unsigned cell=unsigned(labels[row][i])-32;
  add(60+20.f*i,70+30.f*row,20,30,.7299270033836365f,(cell%16)/16.f,(cell/16)*.09375f,.0625f,.09375f,row==0?255:128,0x400c0900);}
 for(unsigned tile=0;tile<6;++tile)c.sortedTriangles.push_back({tile,4+6*tile,6});
 c.sortedTriangles.push_back({6,40,228});c.render_passes={{true,1,0,44,7}};return c;
}
int main(int argc,char**argv){assert(argc<=3);std::ifstream f;unsigned count=1;
 if(argc>1){f.open(argv[1],std::ios::binary);char magic[8]{};f.read(magic,8);assert(std::memcmp(magic,"SSSCST01",8)==0);count=read<uint32_t>(f);assert(count&&count<=100);}
 for(unsigned n=0;n<count;++n){auto c=argc>1?load(f):measured_layout();silent_scope::dc_native_menu::MainMenuProof menu;
  assert(menu.configure("T9507N",c));
  assert(silent_scope::dc_ui::classify("T9507N",&c,false)==silent_scope::dc_ui::Domain::NativeUi);
  for(unsigned row=0;row<4;++row){const float width=row==0?220:row==1?260:140;const float y=70+30.f*row;
   assert(menu.hit(c,60,y)==int(row)&&menu.hit(c,60+width-.01f,y+29.99f)==int(row));
   assert(menu.hit(c,59.99f,y)==-1&&menu.hit(c,60+width,y)==-1);}
  assert(menu.hit(c,60,190)==-1&&menu.hit(c,60,69.99f)==-1&&menu.hit(c,400,100)==-1);
  assert(menu.hit(c,std::numeric_limits<float>::quiet_NaN(),100)==-1);
  for(unsigned change=0;change<16;++change){auto bad=c;
   if(change==0)bad.verts[14].x=767;
   if(change==1)bad.global_param_tr[0].tcw.full^=1;
   if(change==2)bad.verts[4].z=.2f;
   if(change==3)bad.verts[0].x=-255;
   if(change==4)bad.verts[28].u+=1.f/16;
   if(change==5)bad.verts[28].col[0]=254;
   if(change==6)bad.verts[28].x=61;
   if(change==7)bad.sortedTriangles[0].count=3;
   if(change==8)bad.idx[7]=bad.idx[4];
   if(change==9)bad.sortedTriangles.back().first=4;
   if(change==10)bad.global_param_tr.push_back(bad.global_param_tr[0]);
   if(change==11)bad.modtrig.push_back(1);
   if(change==12)bad.render_passes[0].autosort=false;
   if(change==13)bad.isRTT=true;
   if(change==14)bad.dcCamera.supported=false;
   if(change==15)bad.verts[4].x=std::numeric_limits<float>::quiet_NaN();
   assert(!menu.configure("T9507N",bad)&&menu.hit(bad,100,80)==-1);
   assert(silent_scope::dc_ui::classify("T9507N",&bad,false)==silent_scope::dc_ui::Domain::Unknown);
  }
  // The native selected-string prefix changes colour, never the item geometry.
  for(unsigned selected=0;selected<4;++selected){auto highlight=c;unsigned glyph=0;
   for(unsigned row=0;row<4;++row)for(unsigned ch=0;ch<(row==0?11u:row==1?13u:7u);++ch,++glyph)
    for(unsigned v=0;v<4;++v)for(unsigned color=0;color<3;++color)highlight.verts[28+4*glyph+v].col[color]=row==selected?255:128;
   assert(menu.configure("T9507N",highlight)&&menu.highlighted_item()==int(selected));
   auto stale=highlight;assert(menu.hit(stale,100,80)==-1); // copied geometry is not the stamped configured frame
   ++highlight.dcCamera.submit_sequence;assert(menu.hit(highlight,100,80)==-1);
  }
  assert(menu.configure("T9507N",c));++c.dcCamera.generation;assert(menu.hit(c,100,80)==-1);
  assert(!menu.configure("OTHER",c));
 }
 if(argc>2){std::ifstream world(argv[2],std::ios::binary);char magic[8]{};world.read(magic,8);assert(std::memcmp(magic,"SSSCST01",8)==0);const auto worlds=read<uint32_t>(world);assert(worlds&&worlds<=100);
  for(unsigned i=0;i<worlds;++i){auto c=load(world);silent_scope::dc_native_menu::MainMenuProof menu;assert(!menu.configure("T9507N",c));
   assert(silent_scope::dc_ui::classify("T9507N",&c,false)==silent_scope::dc_ui::Domain::Unknown);}
  std::cout<<"Preserved Unknown for actual gameplay frames="<<worlds<<'\n';}
 std::cout<<"Verified complete native Main Menu frames="<<count<<" and actual glyph-bounded item hits\n";
 return 0;
}
