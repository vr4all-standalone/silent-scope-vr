// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
// Explicit Windows native research: real Maple mouse packets only. No RAM
// stores and no production pointer claim. Inactive without the exact opt-in.
#include "dc_mouse_menu_source.h"
#include "dc_mouse_input_source.h"
#include "dc_options_modal_capture.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
namespace silent_scope::dc_menu_capture_probe {
inline bool poll(const char* title,double seconds,const std::uint8_t* ram,std::size_t bytes,
                 std::uint8_t& buttons,int& x,int& y,int& wheel){
 if(dc_options_modal_capture::poll(title,seconds,ram,bytes,buttons,x,y,wheel))return true;
 const char* mode=std::getenv("SILENT_SCOPE_DC_MENU_CAPTURE");
 if(!mode||!ram||bytes!=16u*1024*1024||!title||std::strcmp(title,"T9507N")||!std::isfinite(seconds))return false;
 const bool options=std::strcmp(mode,"options-pages")==0,training=std::strcmp(mode,"training-choices")==0,
            ranking=std::strcmp(mode,"ranking-pages")==0,pause=std::strcmp(mode,"pause-rows")==0,
            choices=std::strcmp(mode,"mode-choices")==0;
 if(!options&&!training&&!ranking&&!pause&&!choices)return false;
 const auto word=[&](unsigned address){const auto a=address&0xffffffu;
  return unsigned(ram[a])|(unsigned(ram[a+1])<<8)|(unsigned(ram[a+2])<<16)|(unsigned(ram[a+3])<<24);};
 const auto facts=dc_mouse::read_menu_source(ram,bytes,title);
 const auto game=dc_mouse::read_source(ram,bytes,title);
 const bool owned=facts.code_verified&&facts.mouse_present;
 const bool ready=owned&&(options?(facts.main_menu_state==5&&word(0x8c272010)==1&&word(0x8c272008)!=0&&
      word(0x8c272018)==0&&word(0x8c272028)==0&&word(0x8c272030)==0):
   training?(facts.main_menu_state==4&&word(0x8c272568)==1):
   ranking?(facts.main_menu_state==6&&word(0x8c272460)==1&&word(0x8c27244c)==1):
      (game.code_verified&&((word(0x8c26fda8)==0&&word(0x8c0ee860)==0&&game.context.scope_branch==1)||
       (word(0x8c26fda8)==1&&word(0x8c272544)==1))));
 struct State{bool active{},fault{},pending{},done{};unsigned sent{};int axis{},before{},expected{};
  double at{},next{42},last{-1};};
 static State s;
 // A fresh Arcade boot first shows MODE SELECT, not gameplay. Exercise one
 // native relative delta per axis, restore zero, then confirm the actual
 // highlighted region3. No selected-ID or angle memory is written.
 static bool mode_owned{},mode_confirmed{};static unsigned mode_steps{};static double mode_down{-1};
 const bool mode_select=owned&&(word(0x8c1b014c)&0xffu)==6&&ram[0x2214c0]==1&&ram[0x2214c1]==3&&word(0x8c2214c8)==0;
 if((pause||choices)&&!s.active&&seconds>=32&&(mode_select||mode_owned)){
  mode_owned=true;buttons=0xff;x=y=wheel=0;
  const auto signed16=[&](unsigned a){const unsigned v=unsigned(ram[a])|(unsigned(ram[a+1])<<8);return v<32768?int(v):int(v)-65536;};
  if(mode_select){
   constexpr double at[]{33,33.6,34.2,34.8};
   if(mode_steps<4&&seconds>=at[mode_steps]){if(mode_steps<2)x=mode_steps==0?16:-16;else y=mode_steps==2?16:-16;++mode_steps;
    std::fprintf(stderr,"SS_MODE_DELTA native_s=%.9f x=%d y=%d yaw=%d pitch=%d selector=%u\n",seconds,x,y,signed16(0x1b0862),signed16(0x1b0860),unsigned(ram[0x2214de]));}
   if(choices&&mode_steps==4&&seconds>=40){
    const int index=seconds>=54?5:seconds>=51?4:seconds>=48?3:seconds>=45?2:seconds>=42?1:0;
    constexpr int pitches[]{0,3500,3500,-3500,-3500,-3500},yaws[]{0,1000,-1000,1800,0,-1800};
    static double delta_at{-1};static int reported{-1};
    if(seconds-delta_at>=.15){
     x=std::clamp(int(std::lround((yaws[index]-signed16(0x1b0862))/-12.5)),-280,280);
     y=std::clamp(int(std::lround((pitches[index]-signed16(0x1b0860))/-12.5)),-280,280);delta_at=seconds;
    }
    if(reported!=index||(x==0&&y==0&&seconds-delta_at<.017)){
     std::fprintf(stderr,"SS_MODE_PICK native_s=%.9f requested=%d goalYaw=%d goalPitch=%d actualYaw=%d actualPitch=%d selector=%u\n",
      seconds,index,yaws[index],pitches[index],signed16(0x1b0862),signed16(0x1b0860),unsigned(ram[0x2214de]));reported=index;
    }
    return true;
   }
   if(!choices&&seconds>=36&&mode_steps==4&&!mode_confirmed&&signed16(0x1b0862)==0&&signed16(0x1b0860)==0&&ram[0x2214de]==0){mode_confirmed=true;mode_down=seconds;}
   if(mode_down>=0&&seconds-mode_down<.1)buttons=0xfb;
   if(seconds>=33&&seconds<36.3)std::fprintf(stderr,"SS_MODE_AIM native_s=%.9f yaw=%d pitch=%d copyYaw=%d copyPitch=%d selector=%u pressed=%u\n",
    seconds,signed16(0x1b0862),signed16(0x1b0860),signed16(0x2214d8),signed16(0x2214d6),unsigned(ram[0x2214de]),unsigned(0xffu&~buttons));
   return true;
  }
  if(!ready||seconds<40)return true;
 }
 if(!s.active){if(seconds<40||!ready)return false;s.active=true;
  std::fprintf(stderr,"SS_MENU_CAPTURE begin mode=%s native_s=%.9f direct_maple=true ram_writes=false\n",mode,seconds);}
 static bool metadata{};
 if(options&&!metadata){metadata=true;
  std::fprintf(stderr,"SS_OPTIONS_META device=%08x secondary=%08x",word(0x8c219ea0),word(0x8c271ff8));
  for(unsigned a=0x8c2722bc;a<=0x8c272304;a+=4)std::fprintf(stderr," raw%06x=%08x",a&0xffffffu,word(a));
  std::fputc('\n',stderr);
  for(unsigned page=0;page<6;++page)for(unsigned row=0;row<6;++row){const auto a=0x8c272058+page*72+row*12,ptr=word(a),limits=word(a+4);
   std::fprintf(stderr,"SS_OPTIONS_RECORD page=%u row=%u pointer=%08x limits=%08x default=%08x value=%08x\n",page,row,ptr,limits,word(a+8),ptr>=0x8c000000u&&ptr<=0x8cfffffcu?word(ptr):0);}
 }
 buttons=0xff;x=y=wheel=0;
 if(seconds>=42&&seconds<42.6){
  const auto pds=word(0x8c1d54ac);const bool pointer=pds>=0x8c000000u&&pds<=0x8cffffe4u;
  std::fprintf(stderr,"SS_MENU_AXIS native_s=%.9f accumX=%08x frameX=%08x frameY=%08x held=%08x press=%08x right=%d left=%d pds=%08x pds8=%08x pds10=%08x pds14=%08x\n",
   seconds,word(0x8c1e1d6c),word(0x8c1e1d78),word(0x8c1e1d7c),word(0x8c1e1d84),word(0x8c1e1d8c),
   int(static_cast<std::int8_t>(ram[0x27233a])),int(static_cast<std::int8_t>(ram[0x27233b])),pds,
   pointer?word(pds+8):0,pointer?word(pds+0x10):0,pointer?word(pds+0x14):0);
 }
 if(seconds<s.last)s.fault=true;s.last=seconds;
 if(!owned||s.fault||s.done||seconds>=79)return true;
 const auto current=[&](){return int(word(options?0x8c272004:ranking?0x8c272450:pause?(s.axis==3?0x8c26fda8:0x8c272548):s.axis==1?0x8c0f31d4:0x8c0f31d8));};
 if(s.pending){
  const int now=current();const bool ack=ranking?now!=s.before:now==s.expected;
  if(ack&&ready){s.pending=false;s.next=seconds+(ranking?1.1:2.7);
   std::fprintf(stderr,"SS_MENU_CAPTURE ack mode=%s native_s=%.9f ordinal=%u value=%d\n",mode,seconds,s.sent,now);
   if((options||training)&&s.sent>=5)s.done=true;
   if(pause&&s.sent>=3)s.done=true;
   if(ranking&&(now==0||s.sent>=14))s.done=true;
  }else if(seconds-s.at>1.2){s.fault=true;
   std::fprintf(stderr,"SS_MENU_CAPTURE fault mode=%s native_s=%.9f expected=%d actual=%d ready=%u\n",mode,seconds,s.expected,now,unsigned(ready));
  }else if(s.axis==3&&seconds-s.at<.1)buttons=0xf7;
  return true;
 }
 if(!ready||seconds<s.next)return true;
 s.axis=pause?(s.sent==0?3:1):training&&s.sent<3?1:2;s.before=current();s.expected=s.before+1;s.at=seconds;s.pending=true;++s.sent;
 if(s.axis==1)wheel=1;else if(s.axis==2)x=16;else buttons=0xf7;
 std::fprintf(stderr,"SS_MENU_CAPTURE sent mode=%s native_s=%.9f ordinal=%u x=%d wheel=%d before=%d expected=%d\n",
  mode,seconds,s.sent,x,wheel,s.before,s.expected);
 return true;
}
}
