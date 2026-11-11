// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cmath>
namespace silent_scope { namespace dc_mouse {
// Native mouse wheel: -1 is Up, +1 Down. A fresh neutral sample is required
// after every context reset. The mailbox supplies physical direction edges;
// this native-poll policy owns repeat and never accumulates missed notches.
class MenuNavigation {
 bool armed_{};int held_{};double repeat_at_{-1},last_{-1};
public:
 void reset()noexcept{armed_=false;held_=0;repeat_at_=last_=-1;}
 int poll(double now,int latest,bool fresh,int edge)noexcept{
  if(!std::isfinite(now)||now<0||(last_>=0&&now<last_)||latest< -1||latest>1||edge< -1||edge>1){reset();return 0;}
  last_=now;
  if(!latest){armed_=true;held_=0;repeat_at_=-1;}
  if(fresh&&edge&&armed_){held_=latest==edge?edge:0;repeat_at_=held_?now+.35:-1;return edge;}
  if(!armed_||!held_||latest!=held_){held_=0;repeat_at_=-1;return 0;}
  if(now>=repeat_at_){repeat_at_=now+.15;return held_;}
  return 0;
 }
};
}}
