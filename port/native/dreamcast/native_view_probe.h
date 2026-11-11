// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
// Private desktop OpenGL diagnostic. No Flycast/GL headers or runtime mutation.
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <limits>
#include <locale>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

namespace silent_scope { namespace dc_probe {
enum class PassKind { Rtt, Main };
struct Geometry {
    uint64_t vertices{},indices{},opaque_params{},punch_params{},translucent_params{},modifier_triangles{};
    // Nondegenerate indexed triangle candidates in original TA strips, not
    // visibility/coverage or an invented vertex-count/3 estimate.
    uint64_t context_indexed_triangles{},q_valid{};double q_min{},q_max{};
};
struct ViewMetadata {
    PassKind kind{PassKind::Rtt};unsigned width{},height{},native_width{},native_height{};
    uint32_t destination{},pack_mode{},stride{};int clip_x{},clip_y{},clip_width{},clip_height{};
    bool native_copy_enabled{};
    Geometry geometry;
};
template<class Context> ViewMetadata describe_context(const Context& context,PassKind kind) {
    ViewMetadata result;result.kind=kind;result.width=result.native_width=context.framebufferWidth;
    result.height=result.native_height=context.framebufferHeight;result.destination=context.fb_W_SOF1&0x7fffff;
    result.pack_mode=context.fb_W_CTRL.fb_packmode;result.stride=context.fb_W_LINESTRIDE*8;
    result.clip_x=context.fbClip.origin.x;result.clip_y=context.fbClip.origin.y;
    result.clip_width=context.fbClip.size.x;result.clip_height=context.fbClip.size.y;
    auto& geometry=result.geometry;geometry.vertices=context.verts.size();geometry.indices=context.idx.size();
    geometry.opaque_params=context.global_param_op.size();geometry.punch_params=context.global_param_pt.size();
    geometry.translucent_params=context.global_param_tr.size();geometry.modifier_triangles=context.modtrig.size();
    for(const auto& vertex:context.verts)if(std::isfinite(vertex.z)&&vertex.z>0) {
        if(!geometry.q_valid)geometry.q_min=geometry.q_max=vertex.z;
        else {geometry.q_min=std::min(geometry.q_min,double(vertex.z));geometry.q_max=std::max(geometry.q_max,double(vertex.z));}
        ++geometry.q_valid;
    }
    const auto count=[&](const auto& polygons) {
        for(const auto& polygon:polygons) {
            const auto first=std::size_t(polygon.first);if(first>=context.idx.size())continue;
            const auto count=std::min(std::size_t(polygon.count),context.idx.size()-first);
            for(std::size_t i=2;i<count;++i) {
                const auto a=context.idx[first+i-2],b=context.idx[first+i-1],c=context.idx[first+i];
                if(a==b||b==c||a==c||a>=context.verts.size()||b>=context.verts.size()||c>=context.verts.size())continue;
                const auto& x=context.verts[a];const auto& y=context.verts[b];const auto& z=context.verts[c];
                const double area=(double(y.x)-x.x)*(double(z.y)-x.y)-(double(y.y)-x.y)*(double(z.x)-x.x);
                if(std::isfinite(area)&&std::abs(area)>1e-12)++geometry.context_indexed_triangles;
            }
        }
    };
    count(context.global_param_op);count(context.global_param_pt);count(context.global_param_tr);return result;
}
// The renderer supplies its actual entry points. No GL loader is owned here.
// read_rgba must read from the current read FBO and must not drain GL errors.
struct GlApi {
    std::function<int(unsigned)> integer;
    std::function<void(unsigned,int)> pixel_store;
    std::function<void(unsigned,unsigned)> bind_buffer;
    std::function<bool()> read_framebuffer_complete;
    std::function<void(unsigned,unsigned,void*)> read_rgba;
};
struct ProbeLimits {
    unsigned max_images{32},max_records{256},max_dimension{2048};uint64_t max_bytes{256ull*1024*1024};
    double max_seconds{120},metadata_interval_seconds{1},image_interval_seconds{5},min_image_elapsed{};
};
struct ProbeStats {unsigned metadata_records{},images{},readback_attempts{};uint64_t bytes{};};
struct ProbeResult {bool recorded{},captured{};std::filesystem::path image_path;};
inline double monotonic_seconds(){return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();}
namespace detail {
constexpr unsigned PackBuffer=0x88eb,PackBufferBinding=0x88ed,ReadFboBinding=0x8caa,DrawFboBinding=0x8ca6,ReadBuffer=0x0c02;
// All desktop pixel-pack settings that affect uncompressed RGBA reads. Compressed
// block pack descriptors do not participate in glReadPixels(GL_RGBA, UBYTE).
constexpr std::array<unsigned,8> PackFields{{0x0d05,0x0d02,0x806c,0x0d03,0x0d04,0x806b,0x0d00,0x0d01}};
struct PackGuard {
    const GlApi& api;std::array<int,8> values{};int buffer{};bool ready{};
    explicit PackGuard(const GlApi& value):api(value) {
        for(std::size_t i=0;i<values.size();++i)values[i]=api.integer(PackFields[i]);
        buffer=api.integer(PackBufferBinding);ready=true;
    }
    void tight(){api.bind_buffer(PackBuffer,0);for(std::size_t i=0;i<values.size();++i)api.pixel_store(PackFields[i],i==0?1:0);}
    ~PackGuard(){if(ready){try{for(std::size_t i=0;i<values.size();++i)api.pixel_store(PackFields[i],values[i]);api.bind_buffer(PackBuffer,unsigned(buffer));}catch(...){}}}
};
inline const char* kind(PassKind value){return value==PassKind::Rtt?"rtt":"main";}
}
class NativeViewProbe {
public:
    explicit NativeViewProbe(std::filesystem::path directory,ProbeLimits limits={}):directory_(std::move(directory)),limits_(limits) {
        std::error_code error;enabled_=directory_.is_absolute()&&std::filesystem::is_directory(directory_,error)&&!error;
        limits_.max_images=std::min(limits_.max_images,32u);limits_.max_records=std::min(limits_.max_records,256u);
        limits_.max_dimension=std::min(limits_.max_dimension,2048u);limits_.max_bytes=std::min(limits_.max_bytes,256ull*1024*1024);
        if(!std::isfinite(limits_.max_seconds)||limits_.max_seconds<=0)limits_.max_seconds=120;
        limits_.max_seconds=std::min(limits_.max_seconds,120.);
        if(!std::isfinite(limits_.metadata_interval_seconds)||limits_.metadata_interval_seconds<0)limits_.metadata_interval_seconds=1;
        if(!std::isfinite(limits_.image_interval_seconds)||limits_.image_interval_seconds<0)limits_.image_interval_seconds=5;
        if(!std::isfinite(limits_.min_image_elapsed)||limits_.min_image_elapsed<0)limits_.min_image_elapsed=0;
        limits_.min_image_elapsed=std::min(limits_.min_image_elapsed,120.);
        static std::atomic<unsigned> nonce{};
        const auto stamp=std::chrono::system_clock::now().time_since_epoch().count();
        for(unsigned attempt=0;attempt<32;++attempt){prefix_="dc-view-"+std::to_string(stamp)+"-"+std::to_string(nonce.fetch_add(1));metadata_path_=directory_/(prefix_+".jsonl");if(!std::filesystem::exists(metadata_path_,error)&&!error)return;}
        enabled_=false;
    }
    bool enabled_for(const std::string& product)const{return enabled_&&product=="T9507N";}
    const ProbeStats& stats()const{return stats_;}
    const std::filesystem::path& metadata_path()const{return metadata_path_;}
    ProbeResult record(const std::string& product,const ViewMetadata& metadata,const GlApi& api,double now=monotonic_seconds()) noexcept {
        try{return record_impl(product,metadata,api,now);}catch(...){enabled_=false;return {};}
    }
private:
    using Key=std::tuple<int,uint32_t,unsigned,unsigned,uint32_t>;
    struct Sample {double metadata_time{-std::numeric_limits<double>::infinity()},image_time{-std::numeric_limits<double>::infinity()};};
    std::filesystem::path directory_,metadata_path_;std::string prefix_;ProbeLimits limits_;ProbeStats stats_;
    bool enabled_{},began_{};double start_{},last_time_{};uint64_t reserved_bytes_{},rtt_sequence_{},main_sequence_{};
    std::array<unsigned,2> kind_attempts_{};std::map<Key,Sample> samples_;std::mutex mutex_;
    ProbeResult record_impl(const std::string& product,const ViewMetadata& m,const GlApi& api,double now) {
        std::lock_guard<std::mutex> lock(mutex_);ProbeResult result;
        if(!enabled_for(product)||!std::isfinite(now))return result;
        if(stats_.metadata_records>=limits_.max_records){enabled_=false;return result;}
        if(!began_){began_=true;start_=last_time_=now;}
        if(now<last_time_||now-start_>=limits_.max_seconds){enabled_=false;return result;}last_time_=now;
        const unsigned kind=m.kind==PassKind::Rtt?0:1;
        if(kind==0)++rtt_sequence_;else ++main_sequence_;
        auto& sample=samples_[Key{int(kind),m.destination,m.width,m.height,m.pack_mode}];
        if(now-sample.metadata_time<limits_.metadata_interval_seconds)return result;
        sample.metadata_time=now;
        if(!api.integer||!api.pixel_store||!api.bind_buffer||!api.read_framebuffer_complete||!api.read_rgba){enabled_=false;return result;}
        const int read_fbo=api.integer(detail::ReadFboBinding),draw_fbo=api.integer(detail::DrawFboBinding),read_buffer=api.integer(detail::ReadBuffer);
        const uint64_t bytes=uint64_t(m.width)*m.height*4;const char* status="image_interval";
        const bool dimension=m.width>0&&m.height>0&&m.width<=limits_.max_dimension&&m.height<=limits_.max_dimension;
        if(now-start_<limits_.min_image_elapsed)status="before_image_window";
        else if(!dimension)status="dimension_limit";
        else if(stats_.readback_attempts>=limits_.max_images||kind_attempts_[kind]>=16)status="image_budget";
        else if(bytes>limits_.max_bytes-reserved_bytes_)status="byte_budget";
        else if(now-sample.image_time>=limits_.image_interval_seconds) {
            sample.image_time=now;reserved_bytes_+=bytes;++stats_.readback_attempts;++kind_attempts_[kind];
            if(!api.read_framebuffer_complete())status="incomplete_read_fbo";
            else {
                std::vector<unsigned char> pixels(std::size_t(bytes),0);
                try {
                    {detail::PackGuard state(api);state.tight();api.read_rgba(m.width,m.height,pixels.data());}
                    result.image_path=directory_/(prefix_+"-"+std::to_string(stats_.readback_attempts)+"-"+detail::kind(m.kind)+".rgba");
                    std::error_code error;
                    if(std::filesystem::exists(result.image_path,error)||error){status="image_path_exists";result.image_path.clear();}
                    else {
                        std::ofstream image(result.image_path,std::ios::binary|std::ios::out);image.write(reinterpret_cast<const char*>(pixels.data()),std::streamsize(pixels.size()));image.close();
                        if(image){result.captured=true;++stats_.images;stats_.bytes+=bytes;status="readback_attempted_gl_errors_not_consumed";}
                        else {status="image_write_failed";std::filesystem::remove(result.image_path,error);result.image_path.clear();}
                    }
                }catch(...){status="readback_exception";result.image_path.clear();}
            }
        }
        std::ofstream output(metadata_path_,std::ios::app);if(!output){enabled_=false;return result;}
        output.imbue(std::locale::classic());
        const auto& g=m.geometry;
        output<<std::setprecision(17)<<"{\"sequence\":"<<(stats_.metadata_records+1)<<",\"kind\":\""<<detail::kind(m.kind)
            <<"\",\"identity\":\"unclassified_native_view\",\"rtt_sequence\":"<<rtt_sequence_<<",\"main_sequence\":"<<main_sequence_
            <<",\"host_monotonic_seconds\":"<<now<<",\"elapsed_seconds\":"<<(now-start_)<<",\"destination\":"<<m.destination
            <<",\"width\":"<<m.width<<",\"height\":"<<m.height<<",\"native_width\":"<<m.native_width<<",\"native_height\":"<<m.native_height
            <<",\"pack_mode\":"<<m.pack_mode<<",\"stride\":"<<m.stride<<",\"clip\":["<<m.clip_x<<','<<m.clip_y<<','<<m.clip_width<<','<<m.clip_height
            <<"],\"native_copy_enabled\":"<<(m.native_copy_enabled?"true":"false")<<",\"read_fbo\":"<<read_fbo<<",\"draw_fbo\":"<<draw_fbo<<",\"read_buffer\":"<<read_buffer
            <<",\"vertices\":"<<g.vertices<<",\"indices\":"<<g.indices<<",\"opaque_params\":"<<g.opaque_params<<",\"punch_params\":"<<g.punch_params
            <<",\"translucent_params\":"<<g.translucent_params<<",\"modifier_triangles\":"<<g.modifier_triangles
            <<",\"context_indexed_triangles\":"<<g.context_indexed_triangles<<",\"q_valid\":"<<g.q_valid
            <<",\"q_min\":"<<(std::isfinite(g.q_min)?g.q_min:0)<<",\"q_max\":"<<(std::isfinite(g.q_max)?g.q_max:0)
            <<",\"origin\":\"lower-left\",\"format\":\"RGBA8\",\"status\":\""<<status<<"\",\"image\":\""<<result.image_path.filename().string()<<"\"}\n";
        output.close();if(output){++stats_.metadata_records;result.recorded=true;if(stats_.metadata_records>=limits_.max_records)enabled_=false;}else enabled_=false;
        return result;
    }
};
inline NativeViewProbe& environment_probe() {
    static NativeViewProbe value([]{const char* directory=std::getenv("SILENT_SCOPE_DC_PROBE_DIR");return directory?std::filesystem::path(directory):std::filesystem::path{};}(),
        []{ProbeLimits limits;const char* begin=std::getenv("SILENT_SCOPE_DC_PROBE_BEGIN_SECONDS");if(begin&&*begin){char* end{};const double parsed=std::strtod(begin,&end);if(end&&!*end&&std::isfinite(parsed)&&parsed>=0&&parsed<=120)limits.min_image_elapsed=parsed;}
        const char* interval=std::getenv("SILENT_SCOPE_DC_PROBE_IMAGE_INTERVAL_SECONDS");if(interval&&*interval){char* end{};const double parsed=std::strtod(interval,&end);if(end&&!*end&&std::isfinite(parsed)&&parsed>=1&&parsed<=5)limits.image_interval_seconds=parsed;}
        return limits;}());
    return value;
}
}}
