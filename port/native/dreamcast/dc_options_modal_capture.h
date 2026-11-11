// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
// Private Windows diagnostic only: absent-VMU informational errors, no RAM stores.
#include "dc_mouse_menu_source.h"
#include "dc_menu_source.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
namespace silent_scope::dc_options_modal_capture {
inline bool poll(const char* title,double now,const std::uint8_t* ram,std::size_t bytes,
                 std::uint8_t& buttons,int& x,int& y,int& wheel) {
 const auto* mode=std::getenv("SILENT_SCOPE_DC_MENU_CAPTURE");
 if(!mode||std::strcmp(mode,"options-vmu")||!title||std::strcmp(title,"T9507N")||!ram||
    bytes!=16u*1024*1024||!std::isfinite(now))return false;
 const auto word=[&](unsigned address){const auto a=address&0xffffffu;return unsigned(ram[a])|
  (unsigned(ram[a+1])<<8)|(unsigned(ram[a+2])<<16)|(unsigned(ram[a+3])<<24);};
 const auto facts=dc_mouse::read_menu_source(ram,bytes,title);
 const bool owned=facts.code_verified&&facts.mouse_present&&facts.main_menu_state==5;
 const bool ready=owned&&word(0x8c272010)==1&&word(0x8c272008)!=0&&word(0x8c272028)==0&&word(0x8c272030)==0;
 struct State {bool owns{},fault{},done{};int stage{},row{1},wait{-1};double at{},next{40},last{-1};};
 static State s;
 if(!s.owns){if(now<32||!ready||word(0x8c1e3c10)!=0||word(0x8c26fe14)!=1)return false;s.owns=true;}
 buttons=0xff;x=y=wheel=0;
 if(now<s.last)s.fault=true;s.last=now;
 if(!owned||word(0x8c1e3c10)!=0||word(0x8c26fe14)!=1||now>=79)s.fault=true;
 const auto page=word(0x8c272004),row=word(0x8c272018),phase=word(0x8c272430),modal=word(0x8c272030);
 static unsigned records{};static unsigned prior[9]{};
 const unsigned fields[]{page,row,phase,modal,word(0x8c272298),word(0x8c27243c),word(0x8c272440),word(0x8c272448),unsigned(s.stage)};
 if(records<160&&!std::equal(std::begin(fields),std::end(fields),std::begin(prior))){
  std::copy(std::begin(fields),std::end(fields),std::begin(prior));
  const auto typed=dc_menu::read_source(ram,bytes,title);
  std::fprintf(stderr,"SS_MODAL_SOURCE native_s=%.9f kind=%d verified=%u context=%u interactive=%u\n",
   now,int(typed.kind),unsigned(typed.verified),typed.source_context,unsigned(typed.interactive));
  std::fprintf(stderr,"SS_OPTIONS_MODAL native_s=%.9f stage=%d goal=%d page=%u row=%u phase=%u modal=%u caller=%u saveop=%u loadop=%u autoop=%u vmu=%u done=%u fault=%u direct_maple=true ram_writes=false\n",
   now,s.stage,s.row,page,row,phase,modal,fields[4],fields[5],fields[6],fields[7],word(0x8c1e3c10),unsigned(s.done),unsigned(s.fault));++records;
 }
 if(s.fault||s.done)return true;
 if(s.stage==0){
  if(!ready||now<s.next)return true;
  if(page!=4){
   if(s.wait>=0){if(page==unsigned(s.wait))s.wait=-1;else if(now-s.at>1.2)s.fault=true;return true;}
   if(row!=0){s.fault=true;return true;}
   s.wait=int((page+1)%6);x=16;s.at=now;s.next=now+.35;return true;
  }
  s.stage=1;s.wait=-1;s.next=now+.4;
 }
 if(s.stage==1){
  if(page!=4||!ready||now<s.next)return true;
  if(row!=unsigned(s.row)){
   if(s.wait>=0){if(row==unsigned(s.wait))s.wait=-1;else if(now-s.at>1.2)s.fault=true;return true;}
   if(row>=unsigned(s.row)){s.fault=true;return true;}
   s.wait=int(row+1);wheel=1;s.at=now;s.next=now+.35;return true;
  }
  if(s.row==3){
   const auto ptr=word(0x8c272058+4*72+2*12);
   if(ptr<0x8c000000||ptr>0x8cfffffc||word(ptr)!=0){s.fault=true;return true;}
   x=16;
  }else buttons=0xfb;
  s.stage=2;s.at=now;return true;
 }
 if(s.stage==2){
  if(s.row<3&&now-s.at<.1)buttons=0xfb;
  const unsigned expectedPhase=s.row==3?2:3;
  const unsigned operation=word(s.row==1?0x8c27243c:s.row==2?0x8c272440:0x8c272448);
  if(page==4&&row==unsigned(s.row)&&modal==1&&word(0x8c272298)==2&&phase==expectedPhase&&operation==7){
   s.stage=3;s.at=now;buttons=0xff;
  }else if(now-s.at>3)s.fault=true;
  return true;
 }
 if(s.stage==3){if(now-s.at<2.7)return true;s.stage=4;s.at=now;buttons=0xfb;return true;}
 if(s.stage==4){
  if(now-s.at<.1)buttons=0xfb;
  if(ready&&page==4&&row==unsigned(s.row)){
   buttons=0xff;std::fprintf(stderr,"SS_OPTIONS_MODAL returned native_s=%.9f row=%d\n",now,s.row);
   if(s.row==3){s.done=true;s.stage=5;const auto ptr=word(0x8c272058+4*72+2*12);
    if(ptr<0x8c000000||ptr>0x8cfffffc||word(ptr)!=0)s.fault=true;
    std::fprintf(stderr,"SS_OPTIONS_MODAL complete native_s=%.9f autosaveStillOff=%u fault=%u\n",now,unsigned(!s.fault),unsigned(s.fault));
   }else{++s.row;s.stage=1;s.wait=-1;s.next=now+.5;}
  }else if(now-s.at>3)s.fault=true;
 }
 return true;
}
}
