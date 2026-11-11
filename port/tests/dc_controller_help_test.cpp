#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <iterator>
#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include "imgui/imstb_truetype.h"
stbtt_fontinfo font;
namespace vr::xr {
namespace controllers {enum Button {A,B,X,Y,Menu,Stick,Trigger,Grip,Buttons};}
unsigned textTexture(const std::string& text,float px,const glm::vec3&,int& w,int& h){
 float width=0,scale=stbtt_ScaleForPixelHeight(&font,px);
 for(size_t i=0;i<text.size();++i){
  int advance,bearing;stbtt_GetCodepointHMetrics(&font,(unsigned char)text[i],&advance,&bearing);
  width+=advance*scale;
  if(i+1<text.size())width+=stbtt_GetCodepointKernAdvance(&font,(unsigned char)text[i],(unsigned char)text[i+1])*scale;
 }
 w=int(std::ceil(width))+int(std::lround(px*.4f))*2;h=int(std::lround(px*1.45f));return 1;
}
namespace labels {
#include "layout.inc"
}}
int main(int argc,char** argv){
 if(argc!=3)return 2;
 std::ifstream file(argv[1],std::ios::binary);
 std::vector<unsigned char> bytes{std::istreambuf_iterator<char>(file),{}};
 if(bytes.empty()||!stbtt_InitFont(&font,bytes.data(),0))return 2;
 using namespace vr::xr::labels;
 int checks=0,failures=0;
 auto check=[&](bool good,const char* why){++checks;if(!good){++failures;if(failures<20)std::cerr<<"FAIL "<<why<<'\n';}};
 const char* profiles[]{"/interaction_profiles/oculus/touch_controller","/interaction_profiles/valve/index_controller",
  "/interaction_profiles/htc/vive_controller","/interaction_profiles/microsoft/motion_controller"};
 std::ofstream json(argv[2]);json<<"[";
 bool first=true;
 for(int model=0;model<2;++model)for(int kind=0;kind<4;++kind)for(int firing=0;firing<2;++firing)for(int sim=0;sim<2;++sim){
  const auto& words=silentScopeWords(profiles[kind],firing,sim);
  bool menuSettings=false,padSettings=false,scopeZoom=false;
  for(const auto& label:words.hand[0]){
   menuSettings|=std::strstr(label.text,"Menu - Settings")!=nullptr;
   padSettings|=std::strstr(label.text,"Pad squeeze - Settings")!=nullptr;
  }
  for(const auto& label:words.hand[firing])scopeZoom|=std::strstr(label.text,"Scope zoom")!=nullptr||std::strstr(label.text,"Zoom")!=nullptr;
  check(menuSettings==(kind==0),"Settings help mismatches Menu binding");
  check(padSettings==(kind==1),"Settings help mismatches Index pad binding");
  check(scopeZoom,"Scope zoom instruction missing");

  for(int hand=0;hand<2;++hand)for(int pose=0;pose<9;++pose){
   const float side=hand?1.f:-1.f;
   const auto grip=glm::translate(glm::mat4(1),glm::vec3(side*.22f,-.25f,-.5f))*
    glm::rotate(glm::mat4(1),float(pose-4)*.07f,glm::vec3(0,0,1));
   Layout layout;
   for(const auto& label:words.hand[hand]){
    layout.at.push_back(glm::vec3(grip*glm::vec4(model?BundledAnchors[hand][label.button]:Anchors[hand][label.button],1)));
    check(!std::strstr(label.text,"P1")&&!std::strstr(label.text,"P2"),"Generic hand roles");
   }
   layOut(layout,hand,grip,words,glm::vec3(0));
   glm::vec3 middle(0),x,y,z;
   for(auto anchor:layout.at)middle+=anchor;
   middle/=float(layout.at.size());
   facing(middle,glm::vec3(0),glm::vec3(grip[0]),x,y,z);
   const Placed* trigger=nullptr;
   for(const auto& placed:layout.placed)if(placed.text.find("Trigger")!=std::string::npos)trigger=&placed;
   check(trigger!=nullptr,"Trigger label missing");
   if(!trigger)continue;
   check(side*glm::dot(trigger->centre-trigger->from,x)<-.02f,"Trigger does not extend inward");
   for(const auto& other:layout.placed)if(&other!=trigger){
    const auto delta=trigger->centre-other.centre;
    check(std::abs(glm::dot(delta,x))>=trigger->hx+other.hx+.001f||
     std::abs(glm::dot(delta,y))>=trigger->hy+other.hy+.001f,"Trigger label overlap");
   }
   if(pose==4&&kind==0){
    if(!first)json<<',';first=false;
    json<<"{\"model\":"<<model<<",\"firing\":"<<firing<<",\"simulation\":"<<sim<<",\"hand\":"<<hand<<",\"labels\":[";
    bool firstLabel=true;
    for(const auto& placed:layout.placed){
     if(!firstLabel)json<<',';firstLabel=false;
     json<<"{\"text\":\""<<placed.text<<"\",\"x\":"<<glm::dot(placed.centre-middle,x)
      <<",\"y\":"<<glm::dot(placed.centre-middle,y)<<",\"hx\":"<<placed.hx<<",\"hy\":"<<placed.hy
      <<",\"ax\":"<<glm::dot(placed.from-middle,x)<<",\"ay\":"<<glm::dot(placed.from-middle,y)<<"}";
    }
    json<<"]}";
   }
  }
 }
 json<<"]";
 std::cout<<checks<<" production-font controller-help checks, "<<failures<<" failures\n";
 return failures?1:0;
}
