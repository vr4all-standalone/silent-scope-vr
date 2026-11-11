#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#if __has_include("dreamcast/native_input_probe.h")
#include "dreamcast/native_input_probe.h"
#define HAS_DC_INPUT_PROBE 1
#endif
#define REQUIRE(x) do { if (!(x)) {std::cerr << "FAIL line " << __LINE__ << ": " #x "\n"; return 1;} } while (false)
int main() {
#ifndef HAS_DC_INPUT_PROBE
    REQUIRE(false && "Native input probe missing");
#else
    using namespace silent_scope::dc_input_probe;
    REQUIRE(parse_mode(nullptr)==Mode::Disabled && parse_mode("")==Mode::Disabled);
    REQUIRE(parse_mode("pad-start")==Mode::PadStart && parse_mode("mouse-start")==Mode::MouseStart);
    REQUIRE(parse_mode("PAD-START")==Mode::Disabled && parse_mode("pad-start ")==Mode::Disabled);
    Policy off; REQUIRE(!off.poll("T9507N",Device::Pad,0,100).owned);
    Policy pad(Mode::PadStart);
    REQUIRE(!pad.poll("OTHER",Device::Pad,0,1).owned);
    REQUIRE(!pad.poll("T9507N",Device::Mouse,0,2).owned);
    REQUIRE(!pad.poll("T9507N",Device::Pad,1,3).owned);
    REQUIRE(!pad.poll("T9507N",Device::Pad,0,std::numeric_limits<double>::quiet_NaN()).owned);
    REQUIRE(pad.poll("T9507N",Device::Pad,0,100).pad_pressed==PadA);
    REQUIRE(pad.poll("T9507N",Device::Pad,0,100.099).pad_pressed==PadA);
    REQUIRE(pad.poll("T9507N",Device::Pad,0,100.101).pad_pressed==0);
    REQUIRE(pad.poll("T9507N",Device::Pad,0,101).pad_pressed==PadA);
    REQUIRE(pad.poll("T9507N",Device::Pad,0,114).pad_pressed==PadA);
    REQUIRE(pad.poll("T9507N",Device::Pad,0,115).pad_pressed==0);
    REQUIRE(pad.poll("T9507N",Device::Pad,0,119.999).pad_pressed==0);
    REQUIRE(pad.poll("T9507N",Device::Pad,0,120).pad_pressed==PadStart);
    REQUIRE(pad.poll("T9507N",Device::Pad,0,120.199).pad_pressed==PadStart);
    REQUIRE(pad.poll("T9507N",Device::Pad,0,120.201).pad_pressed==0);
    REQUIRE(pad.poll("T9507N",Device::Pad,0,121).pad_pressed==PadA);
    REQUIRE(pad.poll("T9507N",Device::Pad,0,127).pad_pressed==PadA);
    REQUIRE(pad.poll("T9507N",Device::Pad,0,128).pad_pressed==0);
    REQUIRE(pad.poll("T9507N",Device::Pad,0,130).pad_pressed==0);
    REQUIRE(pad.poll("T9507N",Device::Pad,0,120).pad_pressed==0); // Never replay Start on clock reversal.
    REQUIRE(pad.poll("T9507N",Device::Pad,0,10000).pad_pressed==0);
    Policy late(Mode::PadStart);late.poll("T9507N",Device::Pad,0,0);
    REQUIRE(late.poll("T9507N",Device::Pad,0,21).pad_pressed==PadA); // Missed Start never queued.
    Policy changed(Mode::PadStart);changed.poll("T9507N",Device::Pad,0,1);
    REQUIRE(!changed.poll("OTHER",Device::Pad,0,2).owned);
    REQUIRE(!changed.poll("T9507N",Device::Pad,0,3).owned); // No schedule restart across content changes.
    Policy mouse(Mode::MouseStart);
    REQUIRE(!mouse.poll("T9507N",Device::Pad,0,50).owned);
    auto value=mouse.poll("T9507N",Device::Mouse,0,100);
    REQUIRE(value.owned && value.mouse_pressed==MouseLeft && !value.pad_pressed);
    std::uint8_t buttons=0xff;int x=123,y=-23,w=12;
    REQUIRE(apply_mouse(value,buttons,x,y,w));
    REQUIRE(buttons==std::uint8_t(0xff & ~MouseLeft) && x==0 && y==0 && w==0);
    value=mouse.poll("T9507N",Device::Mouse,0,120);
    REQUIRE(value.mouse_pressed==MouseLeft && !value.pad_pressed);
    REQUIRE(mouse.poll("T9507N",Device::Mouse,0,127).mouse_pressed==MouseLeft);
    REQUIRE(mouse.poll("T9507N",Device::Mouse,0,128).mouse_pressed==0);
    REQUIRE(mouse.poll("T9507N",Device::Mouse,0,130).mouse_pressed==0);
    std::uint32_t code=0xfffffffd;apply_pad(Sample{true,PadA,0},code);
    REQUIRE(code==(0xfffffffd & ~PadA)); // Preserve another native held button.
    buttons=0xa5;x=12;y=13;w=14;code=0x12345678;
    REQUIRE(!apply_mouse(Sample{},buttons,x,y,w));apply_pad(Sample{},code);
    REQUIRE(buttons==0xa5 && x==12 && y==13 && w==14 && code==0x12345678);
    Policy dense(Mode::PadStart);unsigned starts=0;bool previous=false;
    for(unsigned i=0;i<=4000;++i) {
        auto sample=dense.poll("T9507N",Device::Pad,0,i/100.0);
        bool pressed=sample.pad_pressed & PadStart;starts+=pressed&&!previous;previous=pressed;
        if(i>=3000)REQUIRE(!sample.pad_pressed);
        REQUIRE(!(sample.pad_pressed & ~(PadA|PadStart)) && !sample.mouse_pressed);
    }
    REQUIRE(starts==1);
#endif
    return 0;
}
