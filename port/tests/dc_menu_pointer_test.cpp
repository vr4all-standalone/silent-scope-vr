#include <iostream>
#if __has_include("dreamcast/dc_menu_pointer.h")
#include "dreamcast/dc_menu_pointer.h"
#define HAS_MENU_POINTER 1
#endif
#define REQUIRE(x) do{if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": " #x "\n";return 1;}}while(false)
#ifdef HAS_MENU_POINTER
using namespace silent_scope::dc_mouse;
MenuPointerInput sample(double time,int selected,int wheel,int hover=-1){
 MenuPointerInput s;s.host_seconds=s.native_seconds=time;s.source_verified=true;s.selected=selected;
 s.count=4;s.native_wheel=wheel;s.hovered=hover;return s;
}
#endif
int main(){
#ifndef HAS_MENU_POINTER
 REQUIRE(false&&"Native pointer policy missing");
#else
 MenuPointer pointer;auto s=sample(1,0,0,3);auto p=pointer.poll(s);REQUIRE(p.wheel==1&&!p.confirm);
 s=sample(1.02,0,0,3);p=pointer.poll(s);REQUIRE(!p.wheel&&!p.confirm);
 s=sample(1.05,1,1,3);p=pointer.poll(s);REQUIRE(!p.wheel&&!p.confirm); // ACK still carries wheel: do not accumulate.
 s=sample(1.08,1,0,3);p=pointer.poll(s);REQUIRE(p.wheel==1&&!p.confirm);
 s=sample(1.13,2,1,3);REQUIRE(!pointer.poll(s).wheel);
 s=sample(1.16,2,0,3);REQUIRE(pointer.poll(s).wheel==1);
 s=sample(1.21,3,1,3);REQUIRE(!pointer.poll(s).confirm);
 s=sample(1.24,3,0,3);p=pointer.poll(s);REQUIRE(!p.wheel&&!p.confirm);
 // Capture the clicked item, not subsequent hover. A released short click is
 // still one gesture, confirmed only after native ACK and drained wheel.
 pointer.reset();pointer.poll(sample(2,0,0));
 s=sample(2.01,0,0,0);s.fresh_click=true;s.clicked=3;s.click_host_seconds=2.005;
 p=pointer.poll(s);REQUIRE(p.wheel==1&&!p.confirm&&p.requested_item==3);
 for(int n=1;n<=3;++n){
  s=sample(2.01+n*.08-.03,n,1,0);p=pointer.poll(s);REQUIRE(!p.wheel&&!p.confirm);
  s=sample(2.01+n*.08,n,0,0);p=pointer.poll(s);
  REQUIRE((n==3?p.confirm:p.wheel==1));
 }
 REQUIRE(p.requested_item==3);
 p=pointer.poll(sample(2.3,3,0,0));REQUIRE(!p.wheel&&!p.confirm); // No post-confirm hover race.
 s=sample(2.31,3,0,0);s.fresh_click=s.trigger_down=s.native_left_down=true;s.clicked=0;s.click_host_seconds=2.31;
 p=pointer.poll(s);REQUIRE(!p.wheel&&!p.confirm); // New click cannot move highlight under an older unacknowledged Left.
 s=sample(2.32,3,0,3);s.fresh_click=s.trigger_down=s.native_left_down=true;s.clicked=3;s.click_host_seconds=2.32;
 p=pointer.poll(s);REQUIRE(!p.wheel&&!p.confirm); // Same-item retry also waits for native Up.
 s=sample(2.33,3,0,0);s.trigger_down=true;p=pointer.poll(s);REQUIRE(!p.wheel&&p.left_down&&!p.confirm); // Finish the first bounded hold.
 p=pointer.poll(sample(2.36,3,0,0));REQUIRE(!p.left_down&&!p.wheel); // Actual native Up before retry.
 p=pointer.poll(sample(2.38,3,0,0));REQUIRE(p.confirm&&p.left_down&&!p.wheel);
 // Blank pointer and an invalid captured hit never choose or confirm an item.
 pointer.reset();s=sample(3,0,0);s.fresh_click=true;s.clicked=-1;s.click_host_seconds=3;
 p=pointer.poll(s);REQUIRE(!p.wheel&&!p.confirm);
 s=sample(3.01,0,0);s.manual_wheel=1;p=pointer.poll(s);REQUIRE(p.wheel==1&&!p.confirm);
 // Source/context failure discards click and requires a physical release.
 pointer.reset();pointer.poll(sample(4,0,0));s=sample(4.01,0,0,3);s.fresh_click=s.trigger_down=true;s.clicked=3;s.click_host_seconds=4.01;pointer.poll(s);
 s=sample(4.02,0,0,3);s.source_verified=false;s.trigger_down=true;p=pointer.poll(s);REQUIRE(!p.wheel&&!p.confirm);
 s=sample(4.03,3,0,3);s.trigger_down=true;s.clicked=3;s.click_host_seconds=4.03; // Caller has no new rearmed edge while held.
 REQUIRE(!pointer.poll(s).confirm);
 pointer.poll(sample(4.04,3,0,3));s.fresh_click=true;s.host_seconds=s.native_seconds=s.click_host_seconds=4.05;
 REQUIRE(pointer.poll(s).confirm);
 // No guessed delivery: missing or unexpected selection acknowledgement faults.
 pointer.reset();pointer.poll(sample(5,0,0,3));p=pointer.poll(sample(5.3,0,0,3));REQUIRE(p.status==MenuPointerStatus::Fault&&!p.confirm&&!p.wheel);
 pointer.reset();pointer.poll(sample(6,0,0,3));p=pointer.poll(sample(6.05,2,0,3));REQUIRE(p.status==MenuPointerStatus::Fault&&!p.confirm&&!p.wheel);
 pointer.reset();s=sample(7,0,0,3);s.count=5;p=pointer.poll(s);REQUIRE(!p.wheel&&!p.confirm);
 MenuPointer hold;hold.poll(sample(8,3,0,3));s=sample(8.01,3,0,3);s.fresh_click=true;s.clicked=3;s.click_host_seconds=8.01;
 p=hold.poll(s);REQUIRE(p.confirm&&p.left_down&&!p.wheel);
 s=sample(8.1,3,0,0);s.native_left_down=true;p=hold.poll(s);REQUIRE(p.left_down&&!p.confirm&&!p.wheel);
 s=sample(8.12,3,0,0);s.native_left_down=true;p=hold.poll(s);REQUIRE(!p.left_down&&!p.wheel);
 // Host bound wins even if the guest clock barely moves; guard loss cancels immediately.
 hold.reset();hold.poll(sample(9,3,0));s=sample(9.01,3,0,3);s.fresh_click=true;s.clicked=3;s.click_host_seconds=9.01;hold.poll(s);
 s=sample(9.3,3,0,3);s.native_seconds=9.02;REQUIRE(!hold.poll(s).left_down);
 hold.reset();hold.poll(sample(10,3,0));s=sample(10.01,3,0,3);s.fresh_click=true;s.clicked=3;s.click_host_seconds=10.01;hold.poll(s);
 s=sample(10.02,3,0);s.source_verified=false;p=hold.poll(s);REQUIRE(!p.left_down&&!p.wheel);
 hold.reset();hold.poll(sample(11,3,0));s=sample(11.01,3,0,3);s.fresh_click=true;s.clicked=3;s.click_host_seconds=11.01;hold.poll(s);
 p=hold.poll(sample(11.02,3,1,3));REQUIRE(!p.left_down&&!p.wheel); // Wheel activity invalidates the held confirmation.
 p=hold.poll(sample(11.03,3,0,3));REQUIRE(!p.left_down&&!p.wheel); // No automatic restart when it drains.
#endif
}
