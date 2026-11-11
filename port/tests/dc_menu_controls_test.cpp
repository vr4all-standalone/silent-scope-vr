#include "dreamcast/dc_menu_controls.h"
#include <iostream>
#define REQUIRE(x) do{if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": " #x "\n";return 1;}}while(false)
int main(){using namespace silent_scope;using K=dc_menu::MenuKind;using V=dc_menu::MenuVerb;
 dc_menu_controls::Controller ui;dc_menu::MenuFacts f;f.kind=K::Training;f.verified=f.context_active=f.interactive=true;
 f.context_revision=7;f.row=f.column=0;f.confirm_button=4;f.back_button=2;ui.begin(f,true);
 REQUIRE(ui.buttons().size()==2);dc_menu::MenuAction card;card.kind=K::Training;card.verb=V::Focus;card.column=2;
 auto a=ui.resolve(card,-1,0,450,100);REQUIRE(a.row==0&&a.column==2&&a.context_revision==7);
 dc_menu::MenuAction mode=card;mode.column=-1;mode.row=3;a=ui.resolve(mode,-1,0,100,350);REQUIRE(a.row==3&&a.column==2);
 // Source ACK has not arrived yet. Start must capture desired, not old pair0/0.
 ui.begin(f,true);a=ui.resolve({},-1,0,600,100);REQUIRE(a.verb==V::Confirm&&a.row==3&&a.column==2);
 a=ui.resolve({},-1,0,30,100);REQUIRE(a.verb==V::Back);
 a=ui.resolve({},-1,0,400,450);REQUIRE(a.kind==K::Training&&a.verb==V::None);
 f.context_revision=8;ui.begin(f,true);a=ui.resolve({},-1,0,600,100);REQUIRE(a.row==0&&a.column==0);
 f.kind=K::Ranking;f.page=1;f.source_context=0;ui.begin(f,true);dc_menu::MenuAction arrow;arrow.kind=K::Ranking;arrow.verb=V::Adjust;arrow.direction=1;
 a=ui.resolve(arrow,0,0,470,40);REQUIRE(a.verb==V::None);a=ui.resolve(arrow,1,0,470,40);REQUIRE(a.verb==V::Adjust&&a.page==1&&a.from_page==1);
 f.kind=K::Dialog;f.source_context=2;f.back_button=0;ui.begin(f,true);REQUIRE(ui.buttons().size()==1);
 a=ui.resolve({},-1,0,550,100);REQUIRE(a.verb==V::Confirm);dc_menu::MenuAction yes;yes.kind=K::Dialog;yes.verb=V::Confirm;yes.row=0;
 a=ui.resolve(yes,-1,1,455,330);REQUIRE(a.verb==V::None); // Old YES/NO raster cannot authorize new message.
 for(unsigned context=3;context<=5;++context){f.source_context=context;ui.begin(f,true);
  REQUIRE(ui.buttons().size()==1);a=ui.resolve({},-1,0,550,100);REQUIRE(a.verb==V::Confirm&&a.row==-1);}
 f.source_context=6;ui.begin(f,true);REQUIRE(ui.buttons().empty());
 f.source_context=2;
 f.interactive=false;ui.begin(f,true);a=ui.resolve({},-1,0,550,100);REQUIRE(a.verb==V::None);
 f.kind=K::Options;f.page=0;f.row=1;f.row_count=4;f.value_verified=true;f.interactive=true;f.back_button=2;f.exit_button=8;
 ui.begin(f,true,0);a=ui.resolve({},0,0,590,228);REQUIRE(a.verb==V::Adjust&&a.row==1&&a.direction==1&&a.from_page==0);
 ui.begin(f,true,1);a=ui.resolve({},1,0,590,228);REQUIRE(a.verb==V::None); // Other page's retained row cannot supply +/-.
 ui.begin(f,false);REQUIRE(ui.buttons().empty());
 f.kind=K::ModeSelect;f.source_context=dc_menu::StoryRouteContext;f.mode_type=1;f.aim_verified=true;f.back_button=0;
 f.available_regions=7;
 ui.begin(f,true,-1,true);
 // Quest clicks on the actual Tower Building, Air Battle and Stadium cards.
 struct Click {float x,y;int row;};
 for(const Click click: {Click{110.8f,208.2f,0},{167.1f,225.5f,0},{333.6f,204.5f,1},
                         {253.f,204.2f,1},{485.2f,186.5f,2},{543.7f,198.4f,2}}){
  a=ui.resolve({},-1,0,click.x,click.y);
  REQUIRE(a.verb==V::Focus&&a.row==click.row&&a.context_revision==f.context_revision);
 }
 REQUIRE(ui.buttons().empty());
 REQUIRE(ui.resolve({},-1,0,130,435).verb==V::None);
 REQUIRE(ui.resolve({},-1,0,320,120).verb==V::None);
 REQUIRE(ui.resolve({},-1,0,320,340).verb==V::None);
 REQUIRE(ui.resolve({},-1,0,218.9f,169).row==0);
 REQUIRE(ui.resolve({},-1,0,219,169).row==1);
 REQUIRE(ui.resolve({},-1,0,421,259.9f).row==2);
 REQUIRE(ui.resolve({},-1,0,421,260).verb==V::None);
 ui.begin(f,true,-1,false);REQUIRE(ui.resolve({},-1,0,320,205).verb==V::None);
 f.aim_verified=false;ui.begin(f,true,-1,true);
 REQUIRE(ui.resolve({},-1,0,320,205).verb==V::None);
 f.source_context=0x100;ui.begin(f,true);REQUIRE(ui.buttons().empty());
 // The native game confirms Arcade ownership, but its Quest draw stream
 // can fail the desktop art matcher. The original card layout stays usable.
 f.mode_type=0;f.aim_verified=true;f.source_context=0x100;
 ui.begin(f,true,-1,true);
 a=ui.resolve({},-1,0,405,105);REQUIRE(a.verb==V::Focus&&a.row==1&&a.column==1);
 a=ui.resolve({},-1,0,200,347);REQUIRE(a.verb==V::Focus&&a.row==2&&a.column==-1);
 a=ui.resolve({},-1,0,432,347);REQUIRE(a.verb==V::Focus&&a.row==4);
 a=ui.resolve({},-1,0,320,240);REQUIRE(a.verb==V::Focus&&a.row==0);
 a=ui.resolve({},-1,0,570,347);REQUIRE(a.verb==V::None); // Side-stage preview.
 ui.begin(f,true,-1,false);REQUIRE(ui.resolve({},-1,0,405,105).verb==V::None);
 f.mode_type=4;f.source_context=dc_menu::InvasionRouteContext;f.available_regions=3;
 ui.begin(f,true,-1,true);
 a=ui.resolve({},-1,0,180,214);REQUIRE(a.verb==V::Focus&&a.row==0);
 a=ui.resolve({},-1,0,460,214);REQUIRE(a.verb==V::Focus&&a.row==1);
 REQUIRE(ui.resolve({},-1,0,320,214).verb==V::None);
 REQUIRE(ui.resolve({},-1,0,180,340).verb==V::None);
 ui.begin(f,true,-1,false);REQUIRE(ui.resolve({},-1,0,180,214).verb==V::None);
 f.mode_type=6;f.source_context=0x106;f.available_regions=1;
 ui.begin(f,true,-1,true);
 a=ui.resolve({},-1,0,141,201);REQUIRE(a.verb==V::Focus&&a.row==0);
 REQUIRE(ui.resolve({},-1,0,330,201).verb==V::None); // Locked Training 2.
 f.available_regions=0x1ff;ui.begin(f,true,-1,true);
 a=ui.resolve({},-1,0,525,352);REQUIRE(a.verb==V::Focus&&a.row==8);
 REQUIRE(ui.resolve({},-1,0,236,201).verb==V::None); // Gap between cards.
 std::cout<<"Visible native-menu controls: desired training pair, gutters, epochs, page/message identity and inactive guards pass\n";
}
