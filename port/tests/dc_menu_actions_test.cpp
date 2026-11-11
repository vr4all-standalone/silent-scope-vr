// SPDX-License-Identifier: GPL-2.0-or-later
#include "dc_menu_actions.h"
#include <cassert>
#include <limits>
using namespace silent_scope::dc_menu;
static MenuFacts training(){MenuFacts f;f.kind=MenuKind::Training;f.verified=f.interactive=true;f.context_revision=7;f.row_count=4;f.column_count=3;f.row=f.column=0;f.confirm_button=NativeLeft;f.back_button=NativeRight;return f;}
static PolicyInput sample(MenuFacts f,double time=1){PolicyInput i;i.host_seconds=i.native_seconds=time;i.facts=f;return i;}
static MenuAction action(MenuKind k,MenuVerb v,int row=-1,int column=-1){MenuAction a;a.kind=k;a.verb=v;a.row=row;a.column=column;a.context_revision=7;return a;}
int main(){
 // The absent-VMU AutoSave error requires release, then an explicit fresh
 // Continue click. It must not inherit the click that opened the modal.
 {
  Policy dialog;MenuFacts f;f.kind=MenuKind::Dialog;f.verified=f.interactive=f.context_active=true;
  f.context_revision=42;f.source_context=5;f.confirm_button=NativeLeft;
  auto i=sample(f);i.trigger_down=true;i.fresh_click=true;
  i.clicked=action(MenuKind::Dialog,MenuVerb::Confirm);i.clicked.context_revision=42;i.click_host_seconds=1;
  assert(dialog.poll(i).buttons==0);
  i.trigger_down=i.fresh_click=false;dialog.poll(i);
  i.trigger_down=i.fresh_click=true;auto out=dialog.poll(i);assert(out.buttons==NativeLeft);
  i.fresh_click=false;i.facts.kind=MenuKind::Options;i.facts.source_context=0;i.facts.context_revision=43;
  assert(dialog.poll(i).buttons==0);
 }
 assert(!valid_action({}));
 auto a=action(MenuKind::Training,MenuVerb::Focus,2);assert(valid_action(a));a.direction=2;assert(!valid_action(a));
 // A card changes the actual column through hardware X and waits for native ACK.
 Policy p;auto i=sample(training());p.poll(i);i.hover=action(MenuKind::Training,MenuVerb::Focus,-1,2);
 auto o=p.poll(i);assert(o.mouse_x==16&&o.wheel==0&&o.buttons==0);
 i.host_seconds=i.native_seconds=1.02;o=p.poll(i);assert(o.mouse_x==0&&o.status==PolicyStatus::WaitingAck);
 i.facts.column=1;i.host_seconds=i.native_seconds=1.04;i.facts.navigation_neutral=false;o=p.poll(i);assert(o.mouse_x==0);
 i.facts.navigation_neutral=true;i.host_seconds=i.native_seconds=1.06;o=p.poll(i);assert(o.mouse_x==16);
 i.facts.column=2;i.host_seconds=i.native_seconds=1.08;o=p.poll(i);assert(o.buttons==0&&o.mouse_x==0);
 // Trigger on a mode only selects; starting requires an explicit Confirm action.
 i.hover=action(MenuKind::Training,MenuVerb::Focus,3);i.fresh_click=true;i.clicked=i.hover;i.click_host_seconds=1.08;i.trigger_down=true;
 i.hover=action(MenuKind::Training,MenuVerb::Focus,0); // later producer hover cannot replace captured mode3
 o=p.poll(i);assert(o.wheel==1&&o.buttons==0);
 i.fresh_click=false;i.facts.row=1;i.host_seconds=i.native_seconds=1.10;o=p.poll(i);assert(o.wheel==1);
 i.facts.row=2;i.host_seconds=i.native_seconds=1.12;o=p.poll(i);assert(o.wheel==1);
 i.facts.row=3;i.host_seconds=i.native_seconds=1.14;o=p.poll(i);assert(o.buttons==0);
 i.trigger_down=false;i.hover={};p.poll(i);i.trigger_down=true;i.fresh_click=true;i.clicked=action(MenuKind::Training,MenuVerb::Confirm,3,2);i.click_host_seconds=1.14;
 o=p.poll(i);assert(o.buttons==NativeLeft&&o.fresh_confirm);
 i.fresh_click=false;i.host_seconds=i.native_seconds=1.19;o=p.poll(i);assert(o.buttons==NativeLeft&&!o.fresh_confirm);
 // Source/phase loss cancels a held confirmation immediately.
 i.facts.verified=false;o=p.poll(i);assert(o.buttons==0);
 i.facts=training();i.trigger_down=true;i.fresh_click=true;i.clicked=action(MenuKind::Training,MenuVerb::Confirm,0,0);i.click_host_seconds=1.19;
 o=p.poll(i);assert(o.buttons==0); // restoration requires a genuine release
 i.trigger_down=false;i.fresh_click=false;p.poll(i);i.trigger_down=true;i.fresh_click=true;o=p.poll(i);assert(o.buttons==NativeLeft);
 // Stale Options geometry cannot modify the newly entered page.
 Policy q;MenuFacts f;f.kind=MenuKind::Options;f.context_revision=7;f.verified=f.interactive=f.navigation_neutral=f.value_verified=true;f.page_count=6;f.page=1;f.row=1;f.row_count=4;f.value=3;f.value_min=0;f.value_max=7;f.back_button=NativeRight;f.exit_button=NativeMiddle;
 f.value_neighbors_verified=true;f.previous_value=2;f.next_value=4;
 auto j=sample(f);q.poll(j);j.trigger_down=true;j.fresh_click=true;j.clicked=action(MenuKind::Options,MenuVerb::Adjust,1);j.clicked.page=j.clicked.from_page=0;j.clicked.direction=1;j.click_host_seconds=1;
 o=q.poll(j);assert(o.mouse_x==0&&o.buttons==0);
 j.trigger_down=false;j.fresh_click=false;q.poll(j);j.trigger_down=true;j.fresh_click=true;j.clicked.page=j.clicked.from_page=1;
 o=q.poll(j);assert(o.mouse_x==16);j.fresh_click=false;j.facts.value=4;j.host_seconds=j.native_seconds=1.04;o=q.poll(j);assert(o.mouse_x==0&&o.buttons==0);
 Policy skip;auto n=sample(f);n.facts.value=2;n.facts.next_value=5;n.facts.previous_value=1;n.facts.value_neighbors_verified=true;skip.poll(n);
 n.fresh_click=n.trigger_down=true;n.clicked=action(MenuKind::Options,MenuVerb::Adjust,1);n.clicked.page=n.clicked.from_page=1;n.clicked.direction=1;n.click_host_seconds=1;
 o=skip.poll(n);assert(o.mouse_x==16);n.fresh_click=false;n.facts.value=5;n.host_seconds=n.native_seconds=1.06;o=skip.poll(n);assert(o.status!=PolicyStatus::WaitingAck&&o.mouse_x==0);
 // A source-bound Back/Exit uses its verified button, never guessed Left.
 Policy r;auto k=sample(f);r.poll(k);k.fresh_click=k.trigger_down=true;k.clicked=action(MenuKind::Options,MenuVerb::Exit);k.clicked.page=k.clicked.from_page=1;k.click_host_seconds=1;
 o=r.poll(k);assert(o.buttons==NativeMiddle);
 k.fresh_click=false;k.host_seconds=1.26;k.native_seconds=1.01;o=r.poll(k);assert(o.buttons==0); // host deadline even if guest is stalled
 k.host_seconds=std::numeric_limits<double>::quiet_NaN();o=r.poll(k);assert(o.buttons==0&&o.mouse_x==0&&o.wheel==0);
 // Only an expected page ACK can advance an already captured page transaction.
 Policy pages;auto v=sample(f);v.facts.row=0;pages.poll(v);v.hover=action(MenuKind::Options,MenuVerb::Focus);v.hover.page=3;v.hover.from_page=1;
 o=pages.poll(v);assert(o.mouse_x==0); // page arrow hover never changes a page
 v.clicked=v.hover;v.fresh_click=v.trigger_down=true;v.click_host_seconds=1;o=pages.poll(v);assert(o.mouse_x==16);
 v.fresh_click=false;v.facts.page=2;v.host_seconds=v.native_seconds=1.04;o=pages.poll(v);assert(o.mouse_x==16);
 v.facts.page=3;v.host_seconds=v.native_seconds=1.08;o=pages.poll(v);assert(o.mouse_x==0&&o.status==PolicyStatus::Ready);
 v.hover.page=4;v.hover.from_page=3;v.host_seconds=v.native_seconds=1.10;o=pages.poll(v);assert(o.mouse_x==0); // held trigger does not click the next arrow
 Policy ranking;MenuFacts rf;rf.kind=MenuKind::Ranking;rf.verified=rf.interactive=true;rf.context_revision=7;rf.page_count=14;rf.page=0;rf.next_page=4;rf.previous_page=13;rf.back_button=NativeRight;
 auto z=sample(rf);ranking.poll(z);z.clicked=action(MenuKind::Ranking,MenuVerb::Adjust);z.clicked.page=z.clicked.from_page=0;z.clicked.direction=1;z.fresh_click=z.trigger_down=true;z.click_host_seconds=1;
 o=ranking.poll(z);assert(o.mouse_x==16);z.fresh_click=false;z.facts.page=4;z.host_seconds=z.native_seconds=1.05;o=ranking.poll(z);assert(o.mouse_x==0&&o.buttons==0);
 Policy busy;auto b=sample(training());busy.poll(b);b.trigger_down=b.fresh_click=true;b.clicked=action(MenuKind::Training,MenuVerb::Confirm,1,1);b.click_host_seconds=1;
 o=busy.poll(b);assert(o.wheel==1);b.fresh_click=false;b.facts.interactive=false;b.facts.row=1;b.host_seconds=b.native_seconds=1.03;o=busy.poll(b);assert(o.buttons==0);
 b.facts.interactive=true;b.host_seconds=b.native_seconds=1.06;o=busy.poll(b);assert(o.mouse_x==16);b.facts.column=1;b.host_seconds=b.native_seconds=1.09;o=busy.poll(b);assert(o.buttons==NativeLeft);
 // No generic policy output can replace the accepted Main Menu adapter.
 Policy main;auto m=sample(training());m.facts.kind=MenuKind::Main;m.fresh_click=true;m.clicked=action(MenuKind::Main,MenuVerb::Confirm,0);o=main.poll(m);assert(o.buttons==0&&o.wheel==0&&o.mouse_x==0);
}
