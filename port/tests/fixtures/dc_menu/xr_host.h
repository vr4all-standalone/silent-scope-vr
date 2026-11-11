#pragma once
#include "cfg/option.h"
namespace vr::xr {
inline bool paused{},help{},zoom{};inline unsigned quit_calls{},recenter_calls{},service_calls{},knocks{};
inline bool menuPaused(){return paused;}inline void requestPause(bool on){paused=on;}
inline void recenter(){++recenter_calls;}inline void quitToStart(){++quit_calls;}
inline bool howToPlayOn(){return help;}inline void setHowToPlay(bool on,const char*){help=on;}
inline bool gameZoomOn(){return zoom;}inline void setGameZoom(bool on){zoom=on;}
inline bool serviceMenuAvailable(){return false;}inline bool serviceMenuExitAvailable(){return false;}
inline bool openServiceMenu(){++service_calls;return false;}
inline int twoPlayerP1Hand(){return config::VrTwoPlayerP1Hand.get()=="right"?1:0;}
inline int hapticsLevel(){auto v=config::VrHaptics.get();return v=="off"?0:v=="light"?1:v=="strong"?3:2;}
inline void sampleKnock(int){++knocks;}
}
