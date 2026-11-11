// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
// Private Windows diagnostic: native serializer at a stopped render boundary.
#if !defined(_WIN32) || defined(LIBRETRO)
#error Checkpoint probe is for the standalone Windows diagnostic only
#endif
#include "dreamcast/dc_checkpoint_policy.h"
#include "emulator.h"
#include "serialize.h"
#include "cfg/option.h"
#include "hw/sh4/sh4_mem.h"
#include "hw/sh4/sh4_sched.h"
#include "hw/sh4/sh4_mmr.h"
#include "hw/sh4/modules/modules.h"
#include "rend/vr/xr_host.h"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <vector>

// Populated at the actual native serializer/deserializer call sites by0012.
extern std::size_t dc_state_read_sh4_start,dc_state_write_sh4_start;
extern std::size_t dc_state_read_context_start,dc_state_write_context_start;

namespace silent_scope { namespace dc_checkpoint {
inline std::array<std::uint8_t,16> digest(const void* data,std::size_t bytes) {
    std::array<std::uint8_t,16> out{};MD5Sum().add(data,static_cast<unsigned long>(bytes)).getDigest(out.data());return out;
}
inline std::array<std::uint8_t,16> ram_digest() {
    const void* ram=GetMemPtr(0x0c000000u,RAM_SIZE);
    if(!ram||RAM_SIZE!=16u*1024*1024)throw std::runtime_error("RAM layout mismatch");
    return digest(ram,RAM_SIZE);
}
inline bool stock_code() {
    return ReadMem16_nommu(0x8c112854)==0xd201&&ReadMem16_nommu(0x8c112856)==0x000b
        &&ReadMem16_nommu(0x8c112858)==0xf24a&&ReadMem16_nommu(0x8c019002)==0xf338
        &&ReadMem32_nommu(0x8c1b0108)==0x440ac000;
}
inline std::vector<std::uint8_t> serialize_native() {
    Serializer count;dc_serialize(count);
    if(count.size()<8||count.size()>MaxStateBytes)throw std::runtime_error("Native state size outside diagnostic bound");
    std::vector<std::uint8_t> bytes(count.size());Serializer writer(bytes.data(),bytes.size());dc_serialize(writer);
    std::fprintf(stderr,"DC_CHECKPOINT serialize capacity=%zu written=%zu slack=%zu\n",count.size(),writer.size(),
        writer.size()<=count.size()?count.size()-writer.size():0);
    // serializeContext's dryrun reserves TA_DATA_SIZE per context; the native
    // frontend writes ser.size() from the second pass, not the maximum reserve.
    if(!trim_payload(bytes,writer.size()))throw std::runtime_error("Native writer size exceeds reserved bounds");
    return bytes;
}
inline std::filesystem::path private_path(Mode mode) {
    const char* text=std::getenv("SILENT_SCOPE_DC_CHECKPOINT_FILE");
    if(!text||!*text)throw std::runtime_error("Missing private checkpoint filename");
    const std::filesystem::path supplied(text);
    if(!supplied.is_absolute()||supplied.filename().empty())throw std::runtime_error("Checkpoint path must be absolute");
    const auto root=std::filesystem::weakly_canonical("D:/VR4ALL-SilentScope-Build/diagnostics");
    const auto path=std::filesystem::weakly_canonical(supplied);
    bool inside=false;
    for(auto parent=path.parent_path();!parent.empty()&&parent!=parent.parent_path();parent=parent.parent_path())
        if(parent==root){inside=true;break;}
    if(!inside||!std::filesystem::is_directory(path.parent_path()))throw std::runtime_error("Checkpoint must stay below the existing private diagnostics root");
    if(mode==Mode::Save&&std::filesystem::exists(path))throw std::runtime_error("Checkpoint output already exists");
    if(mode==Mode::Load&&(!std::filesystem::is_regular_file(path)||std::filesystem::is_symlink(supplied)))
        throw std::runtime_error("Checkpoint input must be an existing private regular file");
    return path;
}
inline void save(const std::filesystem::path& path) {
    if(!stock_code())throw std::runtime_error("Checkpoint guest camera code is not stock");
    Header header;header.version=SerializeBase::Current;header.ram_bytes=RAM_SIZE;header.cycles=sh4_sched_now64();
    if(header.cycles<40ull*SH4_MAIN_CLOCK||header.cycles>=41ull*SH4_MAIN_CLOCK)
        throw std::runtime_error("Checkpoint save missed native 40-second window");
    header.ram_md5=ram_digest();const auto payload=serialize_native();header.bytes=payload.size();header.payload_md5=digest(payload.data(),payload.size());
    const auto encoded=encode(header);
    std::unique_ptr<std::FILE,decltype(&std::fclose)> file(std::fopen(path.string().c_str(),"wbx"),&std::fclose);
    if(!file)throw std::runtime_error("Exclusive checkpoint create failed");
    if(std::fwrite(encoded.data(),1,encoded.size(),file.get())!=encoded.size()
        ||std::fwrite(payload.data(),1,payload.size(),file.get())!=payload.size()||std::fflush(file.get())!=0)
        throw std::runtime_error("Checkpoint write failed; partial file retained");
    if(std::fclose(file.release())!=0)throw std::runtime_error("Checkpoint close failed; file retained");
    std::fprintf(stderr,"DC_CHECKPOINT saved native_seconds=%.9f cycles=%llu payload_bytes=%llu version=%u stock_code=true\n",
        header.cycles/double(SH4_MAIN_CLOCK),static_cast<unsigned long long>(header.cycles),static_cast<unsigned long long>(header.bytes),header.version);
}
inline void load(const std::filesystem::path& path) {
    std::unique_ptr<std::FILE,decltype(&std::fclose)> file(std::fopen(path.string().c_str(),"rb"),&std::fclose);
    if(!file)throw std::runtime_error("Checkpoint read open failed");
    std::array<std::uint8_t,HeaderBytes> encoded{};Header header;
    if(std::fread(encoded.data(),1,encoded.size(),file.get())!=encoded.size()||!decode(encoded,header)
       ||header.version!=SerializeBase::Current||header.ram_bytes!=RAM_SIZE
       ||header.cycles<40ull*SH4_MAIN_CLOCK||header.cycles>=41ull*SH4_MAIN_CLOCK)
        throw std::runtime_error("Checkpoint header/title/version/clock mismatch");
    std::vector<std::uint8_t> payload(static_cast<std::size_t>(header.bytes));
    if(std::fread(payload.data(),1,payload.size(),file.get())!=payload.size()||std::fgetc(file.get())!=EOF||std::ferror(file.get()))
        throw std::runtime_error("Checkpoint payload is partial or has trailing data");
    if(digest(payload.data(),payload.size())!=header.payload_md5)throw std::runtime_error("Checkpoint payload digest mismatch");
    if(header.version!=854||header.bytes!=Seed002::PayloadBytes||header.cycles!=Seed002::Cycles
       ||header.payload_md5!=Seed002::PayloadMd5||header.ram_md5!=Seed002::RamMd5)
        throw std::runtime_error("Checkpoint is not the independently attributed seed002");
    Deserializer reader(payload.data(),payload.size());emu.loadstate(reader);
    if(reader.size()!=payload.size()||sh4_sched_now64()!=header.cycles||ram_digest()!=header.ram_md5||!stock_code())
        throw std::runtime_error("Loaded guest bytes/clock/RAM/stock-code verification failed");
    const auto live_handler=p_sh4rcb->cntx.doSqWrite;
    setSqwHandler(); // Verify the binding against the real native selector.
    const bool handler_verified=live_handler!=nullptr&&live_handler==p_sh4rcb->cntx.doSqWrite;
    auto roundtrip=serialize_native();const bool exact=roundtrip.size()==payload.size()&&digest(roundtrip.data(),roundtrip.size())==header.payload_md5;
    if(!exact) {
        const auto diff=compare_payloads(payload,roundtrip);
        std::fprintf(stderr,"DC_CHECKPOINT diff saved_bytes=%zu loaded_bytes=%zu different_bytes=%zu ranges_total=%zu bytes_logged=%zu ranges_logged=%zu\n",
            payload.size(),roundtrip.size(),diff.different,diff.range_count,diff.bytes.size(),diff.ranges.size());
        for(const auto& range:diff.ranges)std::fprintf(stderr,"DC_CHECKPOINT diff_range offset=%zu length=%zu\n",range.offset,range.length);
        for(const auto& byte:diff.bytes)std::fprintf(stderr,"DC_CHECKPOINT diff_byte offset=%zu saved=%02x loaded=%02x\n",byte.offset,unsigned(byte.saved),unsigned(byte.loaded));
        std::fflush(stderr);
    }
    std::fprintf(stderr,"DC_CHECKPOINT loaded native_seconds=%.9f cycles=%llu payload_bytes=%llu version=%u deserialize_complete=true ram_digest_match=true stock_code=true roundtrip_payload_match=%s\n",
        header.cycles/double(SH4_MAIN_CLOCK),static_cast<unsigned long long>(header.cycles),static_cast<unsigned long long>(header.bytes),header.version,exact?"true":"false");
    PostloadFacts facts;facts.cycles=header.cycles;facts.version=header.version;facts.clock=SH4_MAIN_CLOCK;
    facts.read_sh4=dc_state_read_sh4_start;facts.write_sh4=dc_state_write_sh4_start;
    facts.read_context=dc_state_read_context_start;facts.write_context=dc_state_write_context_start;
    facts.context_bytes=sizeof(Sh4Context);facts.pointer_relative=offsetof(Sh4Context,doSqWrite);
    facts.pointer_bytes=sizeof(p_sh4rcb->cntx.doSqWrite);
    facts.live_smr=SCIF_SCSMR2.full;facts.live_brr=SCIF_SCBRR2;
    facts.live_pointer=std::uint64_t(reinterpret_cast<std::uintptr_t>(live_handler));facts.handler_verified=handler_verified;
    auto expected=payload;const auto canonical=canonical_postload(expected,roundtrip,facts);
    if(!canonical.equal)throw std::runtime_error(canonical.reason);
    const auto sum=digest(roundtrip.data(),roundtrip.size());char hex[33]{};
    for(unsigned i=0;i<16;++i)std::snprintf(hex+2*i,3,"%02x",unsigned(sum[i]));
    std::fprintf(stderr,"DC_CHECKPOINT canonical rule=seed002_exact_scif_transform_host_sqwrite_only guest_payload_exact=true canonical_md5=%s bytes=%zu host_bytes_excluded=8 scif_guest_bytes_retained=true scif_start=%u scif_end=%u live_handler_verified=true\n",
        hex,roundtrip.size(),get32(roundtrip,Seed002::Scif+4),get32(roundtrip,Seed002::Scif+8));
}
struct Runtime {Mode mode{parse_mode(std::getenv("SILENT_SCOPE_DC_CHECKPOINT_MODE"))};Policy policy{mode};};
inline Runtime& runtime(){static Runtime value;return value;}
inline void observe(Event,void*) {
    runtime().policy.observe(settings.content.gameId,sh4_sched_now64()/double(SH4_MAIN_CLOCK));
}
struct Registration {Registration(){if(runtime().mode!=Mode::None)EventManager::listen(Event::VBlank,observe);}};
inline Registration registration;
inline void service() {
    const Mode action=runtime().policy.take();if(action==Mode::None)return;
    try {
        if(settings.content.gameId!="T9507N"||!settings.platform.isConsole()||!config::ThreadedRendering
           ||!dc_savestateAllowed()||settings.raHardcoreMode||config::GGPOEnable||vr::xr::enabled()||!emu.running())
            throw std::runtime_error("Unsupported checkpoint state/platform/thread/network mode");
        const auto path=private_path(action);
        const double before=sh4_sched_now64()/double(SH4_MAIN_CLOCK);
        emu.stop(); // Render/UI thread only; joins emulation before all state I/O.
        const double stopped=sh4_sched_now64()/double(SH4_MAIN_CLOCK);
        if(action==Mode::Load&&(stopped<30||stopped>=31))throw std::runtime_error("Checkpoint load missed native 30-second window");
        std::fprintf(stderr,"DC_CHECKPOINT stopped action=%s before_native=%.9f stopped_native=%.9f\n",action==Mode::Save?"save":"load",before,stopped);
        if(action==Mode::Save)save(path);else load(path);
        emu.start();
        std::fprintf(stderr,"DC_CHECKPOINT resumed action=%s running=%s\n",action==Mode::Save?"save":"load",emu.running()?"true":"false");
        std::fflush(stderr);
    } catch(const std::exception& error) {
        std::fprintf(stderr,"DC_CHECKPOINT failure=%s exit=3 private_partial_files_preserved=true\n",error.what());std::fflush(stderr);
        std::_Exit(3); // Fail this private render-boundary probe without unsafe atexit cleanup.
    }
}
}} // namespace silent_scope::dc_checkpoint
