// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
namespace silent_scope { namespace dreamcast {
constexpr uint32_t ExactPacketRamSize=0x1000000;
// All ordinary TA vertex A/payload structs start after their4-byte PCW.
// The decoder hands vert_cvt_base that payload, not the complete32-byte packet.
inline bool native_vertex_payload_packet_offset(uintptr_t base,uintptr_t payload,std::size_t size,uint32_t& packet){
    if(payload<base||payload-base<4)return false;
    const uintptr_t offset=payload-base-4;
    if((offset&31)||offset>UINT32_MAX||size<32||offset>size-32)return false;
    packet=uint32_t(offset);return true;
}
// Native-thread-only exact byte receipts. A sparse direct page directory avoids
// address-hash collisions, eviction and render-thread reads of emulated RAM.
// Uncaptured packets, changed bytes, another generation and overflow fail closed.
template<std::size_t CapacityPages> class ExactPacketSnapshots {
    static_assert(CapacityPages>0&&CapacityPages<=4096,"bounded physical RAM pages");
    struct Page {std::array<uint8_t,4096> bytes;std::array<uint64_t,2> valid;};
    std::array<uint16_t,4096> directory_{};
    std::array<Page,CapacityPages> pages_;
    std::size_t count_{};uint64_t generation_{};bool overflow_{};
public:
    void reset(uint64_t generation){directory_.fill(0);count_=0;generation_=generation;overflow_=false;}
    std::size_t pages()const{return count_;}
    bool overflowed()const{return overflow_;}
    bool capture(uint32_t begin,uint32_t length,const uint8_t* ram,std::size_t size,uint64_t generation){
        if(overflow_||!generation||generation!=generation_||!ram||!length||(begin&31)||(length&31)
            ||uint64_t(begin)+length>ExactPacketRamSize||uint64_t(begin)+length>size)return false;
        const uint32_t end=begin+length;
        for(uint32_t at=begin;at<end;at+=32){
            auto& slot=directory_[at>>12];
            if(!slot){
                if(count_==CapacityPages){directory_.fill(0);count_=0;overflow_=true;return false;}
                slot=uint16_t(++count_);pages_[slot-1].valid.fill(0);
            }
            auto& page=pages_[slot-1];const unsigned packet=(at&4095)>>5;
            std::memcpy(page.bytes.data()+(at&4095),ram+at,32);
            page.valid[packet>>6]|=uint64_t(1)<<(packet&63);
        }
        return true;
    }
    const uint8_t* captured_packet(uint32_t begin,uint64_t generation)const{
        if(overflow_||!generation||generation!=generation_||(begin&31)
            ||uint64_t(begin)+32>ExactPacketRamSize)return nullptr;
        const auto slot=directory_[begin>>12];if(!slot)return nullptr;
        const auto& page=pages_[slot-1];const unsigned packet=(begin&4095)>>5;
        return page.valid[packet>>6]&(uint64_t(1)<<(packet&63))?page.bytes.data()+(begin&4095):nullptr;
    }
    bool matches(uint32_t begin,const void* actual,std::size_t length,uint64_t generation)const{
        if(!actual||length!=32)return false;
        const auto expected=captured_packet(begin,generation);
        return expected&&std::memcmp(expected,actual,32)==0;
    }
};
}}
