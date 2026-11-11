// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "dc_hud.h"
#include "dc_scope_composite.h"
namespace silent_scope::dc_hud_message {
class HurryUpGroup {
    const void* frame_{};uint64_t generation_{},submission_{};bool valid_{};
    static constexpr unsigned Columns=43, Rows=4, Slots=1+Rows*Columns;
    static constexpr char Missing='\0';
    static constexpr std::size_t NoPolygon=std::size_t(-1);
    std::array<std::size_t,Slots> polygons_{};
    std::array<char,Slots> glyphs_{};
    template<class Polygon> static bool material(const Polygon& p,bool plate){
        if(p.isp.full!=0xc8800000||p.tileclip!=0x00100200)return false;
        // The untextured plate can inherit the preceding draw's sampler bits.
        if(plate)return p.pcw.full==0x828c0002u&&
            (p.tsp.full==0x9491a4e4u||p.tsp.full==0x9491a4edu||p.tsp.full==0x9491a4dbu);
        return p.pcw.full==0x828c000au&&p.tsp.full==0x9491a4edu&&p.tcw.full==0x500c0000;
    }
    template<class Context,class Polygon> static bool quad(const Context& ctx,const Polygon& p,unsigned slot,char glyph){
        const bool plate=slot==0;if(!material(p,plate)||p.count!=4||uint64_t(p.first)+4>ctx.verts.size())return false;
        const float x=plate?80.f:90.f+10.f*((slot-1)%Columns),y=plate?390.f:390.f+20.f*((slot-1)/Columns);
        const float w=plate?440.f:10.f,h=plate?90.f:20.f;
        const unsigned cell=plate?0:unsigned(glyph)-32;
        const float u=(cell&15)/16.f,v=(cell>>4)/16.f,q=plate?.7246376872062683f:.7299270033836365f;
        for(unsigned i=0;i<4;++i){const auto& a=ctx.verts[p.first+i];
            if(a.x!=x+(i&2?w:0)||a.y!=y+(i&1?h:0)||a.z!=q||
               a.col[0]!=(plate?0:255)||a.col[1]!=(plate?0:255)||a.col[2]!=(plate?0:255)||a.col[3]!=(plate?112:255)||
               (!plate&&(a.u!=u+(i&2?1.f/16:0)||a.v!=v+(i&1?1.f/16:0))))return false;
        }
        return true;
    }
public:
    template<class Context> bool configure(std::string_view title,const Context& ctx){
        *this=HurryUpGroup{};
        if(title!="T9507N"||ctx.isRTT||ctx.framebufferWidth!=640||ctx.framebufferHeight!=480||!ctx.dcCamera.supported||
           !ctx.dcCamera.generation||!ctx.dcCamera.submit_sequence||ctx.verts.size()>65536||ctx.idx.size()>262144||
           ctx.global_param_tr.size()>16384||ctx.render_passes.size()>64||ctx.sortedTriangles.size()>16384)return false;
        uint64_t previous_tr=0,previous_sorted=0;
        for(const auto& pass:ctx.render_passes){
            if(pass.tr_count<previous_tr||pass.tr_count>ctx.global_param_tr.size()||pass.sorted_tr_count<previous_sorted||
               pass.sorted_tr_count>ctx.sortedTriangles.size()||(pass.sorted_tr_count>previous_sorted&&!pass.autosort))return false;
            previous_tr=pass.tr_count;previous_sorted=pass.sorted_tr_count;
        }
        if(previous_tr!=ctx.global_param_tr.size()||previous_sorted!=ctx.sortedTriangles.size())return false;
        polygons_.fill(NoPolygon);
        for(std::size_t n=0;n<ctx.global_param_tr.size();++n){
            const auto& p=ctx.global_param_tr[n];
            if(p.count!=4||uint64_t(p.first)+4>ctx.verts.size())continue;
            const bool plate=material(p,true);
            if(!plate&&!material(p,false))continue;
            if(dc_scope::translucent_storage(ctx,n)!=dc_scope::PolygonStorage::Vertices)continue;
            unsigned slot=0;
            char glyph=Missing;
            if(!plate){
                const auto& a=ctx.verts[p.first];
                const float row=(a.y-390.f)/20.f;
                if(!(row>=0&&row<Rows&&row==std::floor(row))||!std::isfinite(a.x)||a.x<90||a.x>510||
                   a.x!=90+10*unsigned((a.x-90)/10))continue;
                if(!(a.u>=0&&a.u<1&&a.v>=0&&a.v<1))continue;
                const unsigned u=unsigned(a.u*16),v=unsigned(a.v*16);
                if(a.u!=u/16.f||a.v!=v/16.f||u+16*v>94)continue;
                glyph=char(32+u+16*v);
                slot=1+unsigned(row)*Columns+unsigned((a.x-90)/10);
            }
            if(!quad(ctx,p,slot,glyph))continue;
            if(polygons_[slot]!=NoPolygon)return false;
            polygons_[slot]=n;
            glyphs_[slot]=glyph;
        }
        if(polygons_[0]==NoPolygon)return false;
        const auto row_matches=[&](unsigned row,std::string_view text){
            if(text.size()>Columns)return false;
            for(unsigned column=0;column<Columns;++column)
                if(glyphs_[1+row*Columns+column]!=(column<text.size()?text[column]:Missing))return false;
            return true;
        };
        // Native US radio pages are identified as a complete plate and exact
        // glyph rows. A font atlas match alone cannot attest a message group.
        static constexpr std::string_view pages[]{
            "Hurry up!", "Calm down!", "Bad luck!", "You've lost your touch.",
            "Watch out!\nThe enemy is going to shoot!",
            "They are demanding freedom for their\ndictator.",
            "Go to the location immediately.\nTake out the terrorists and save the\nPresident and his family.",
            "The police are returning fire.\nPlease provide backup.",
            "Take out all surviving sniper enemies\nfrom the roof!",
            "I received information of the enemies.\nWhat's your next move?",
            "Understood! Leave the remains to me!\nWipe them out from the helicopter.\nGood luck!",
            "Understood! Leave remains to me!\nShoot it from the helicopter.\nGood Luck!",
            "I've found the First Lady at the top of\nthe tower building.",
            "Go to the location immediately,\nwipe out the terrorists and save\nthe First Lady.",
            "It's him!\nIt's Scorpion!",
            "I've spotted an unidentified aircraft.",
            "Terrorists have kidnapped the President's\ndaughter and are hiding in the stadium.",
            "We can't identify them from here.\nGo and search for them! Over.",
            "Don't let the enemy get away.\nChase them and save her!",
            "Some terrorists have kidnapped the\nPresident's daughter and are escaping\nby the highway.\nChase them and save her from harm. Over.",
            "I've found the President's daughter\nin the black convertible with Cobra.",
            "Take him out and save the President's\ndaughter. Over.",
            "Oh my gosh!\nHe's alive!",
            "Watch out!\nHe's hijacked a trailer and is heading\nfor you.",
            "I've received information that the\nFirst Lady has been captured by the\nterrorists at the Hotel Imperial.",
            "Find their hideout and remove them all.\nOver.",
            "These guys are members of\nthe terrorist gang.\nRemove!",
            "I've spotted the target!",
            "I've found the First Lady.\nShe's being held by Hornet.",
            "He's the expert sniper.\nBe careful!",
            "I have received information on the\nlocation of the enemy's base.\nThe President must be imprisoned there.\nBreak into their base.",
            "You can get there either by going through\nthe woods or by parachuting in.\nWhat's your next move?",
            "Understood.\nBeware of the security systems!",
            "Understood.\nIf you are caught in the searchlight,\nyou will be killed quickly!",
            "There are intricate security systems.\nDestroy the infrared sensor!",
            "Destroy the main power supply box to\nconfuse the enemies.",
            "The enemies spotted you!\nShoot them!\nShoot them!",
            "The enemy has found us and\nwe're fighting back!!\nWe can't support you anymore.",
            "You must rescue the President for us!\nAny way you can...",
            "Let's break the bullet proof glass.",
            "But, do you know what this means?",
            "This is the live switch to the bomb\nwhich will destroy everything.",
            "It's a waste to grind you now.",
            "If you miss,\nI will send you to heaven to join your\nhero the President.",
            "Target the head and bring him down\nwith the last bullet!",
            "The President's rescue operation was a\nsuccess.\nYou done it! You are the greatest!",
            "Whenever I'm in danger,\nyou are the only one who can save me.",
            "Can you hear me.\nPlease report on the President's\nsituation.",
            "Please reply!\nPlease reply!",
            "NIGHT VISION DEVICE ON.",
            "We've secured the President's daughter,\nbut they caught the President and have\ngone! Standby for the next order. Over.",
            "You've done it!\nWe've secured the First Lady.\nStandby for your next order. Over.",
            "We've secured the President's daughter.",
        };
        const auto matches_page=[&](std::string_view page){
            for(unsigned row=0;row<Rows;++row){
                const auto newline=page.find('\n');
                if(!row_matches(row,page.substr(0,newline)))return false;
                page=newline==std::string_view::npos?std::string_view{}:page.substr(newline+1);
            }
            return page.empty();
        };
        if(!std::any_of(std::begin(pages),std::end(pages),matches_page))return false;
        frame_=&ctx;generation_=ctx.dcCamera.generation;submission_=ctx.dcCamera.submit_sequence;valid_=true;return true;
    }
    template<class Context> dc_hud::Decision classify(const Context& ctx,unsigned list,std::size_t polygon,uint32_t a,uint32_t b,uint32_t c)const{
        if(!valid_||frame_!=&ctx||generation_!=ctx.dcCamera.generation||submission_!=ctx.dcCamera.submit_sequence||ctx.isRTT||
           ctx.framebufferWidth!=640||ctx.framebufferHeight!=480||!ctx.dcCamera.supported||list!=2||polygon>=ctx.global_param_tr.size()||
           a>=ctx.verts.size()||b>=ctx.verts.size()||c>=ctx.verts.size()||a==b||a==c||b==c)return {};
        const auto& given=ctx.global_param_tr[polygon];
        const bool is_plate=material(given,true);if(!is_plate&&!material(given,false))return {};
        for(unsigned slot=is_plate?0:1;slot<(is_plate?1:Slots);++slot){if(polygons_[slot]>=ctx.global_param_tr.size())continue;
            const auto& p=ctx.global_param_tr[polygons_[slot]];const bool plate=slot==0;
            if(!material(ctx.global_param_tr[polygon],plate)||a<p.first||b<p.first||c<p.first||
               uint64_t(a)>=uint64_t(p.first)+4||uint64_t(b)>=uint64_t(p.first)+4||uint64_t(c)>=uint64_t(p.first)+4)continue;
            const unsigned corners=(1u<<(a-p.first))|(1u<<(b-p.first))|(1u<<(c-p.first));
            if((corners==7||corners==14)&&quad(ctx,p,slot,glyphs_[slot]))return {plate?dc_hud::Layer::MessagePlate:dc_hud::Layer::MessageFont,1};
        }
        return {};
    }
};
}
