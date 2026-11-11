// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "background_depth.h"
#include "voodoo_capture.h"
#include <algorithm>
#include <limits>
namespace silent_scope::scene {
struct BackgroundVertex {float x{},y{},q{};};
class BackgroundSurface {
    uint32_t width_{},height_{},step_{2},columns_{},rows_{};
    std::vector<uint8_t> cells_;
    double near_{},far_{};
public:
    std::vector<BackgroundVertex> vertices;
    std::vector<uint32_t> indices;
    bool valid() const{return !indices.empty();}
    double far_units() const{return far_;}
    static BackgroundSurface build(const RawFrame& frame) {
        BackgroundSurface out;
        const size_t count=size_t(frame.width)*frame.height;
        if(!frame.complete||!frame.lens.valid()||frame.width>2048||frame.height>2048||!count
                ||frame.background.size()!=count||frame.background_depth.size()!=count)return out;
        uint16_t low=65535,high=0;size_t usable=0;
        for(auto d:frame.background_depth)if(d&&d!=65535){low=std::min(low,d);high=std::max(high,d);++usable;}
        if(low==high||usable<count/2)return out;
        out.width_=frame.width;out.height_=frame.height;
        while(((frame.width+out.step_-1)/out.step_)*((frame.height+out.step_-1)/out.step_)>32768)out.step_*=2;
        out.columns_=(frame.width+out.step_-1)/out.step_+1;out.rows_=(frame.height+out.step_-1)/out.step_+1;
        out.vertices.reserve(size_t(out.columns_)*out.rows_);out.near_=std::numeric_limits<double>::infinity();
        for(uint32_t row=0;row<out.rows_;row++)for(uint32_t col=0;col<out.columns_;col++) {
            const uint32_t x=std::min(col*out.step_,frame.width),y=std::min(row*out.step_,frame.height);
            const auto d=frame.background_depth[size_t(std::min(y,frame.height-1))*frame.width+std::min(x,frame.width-1)];
            const double q=decode_background_w(d);
            out.vertices.push_back({float(x),float(y),float(q)});
            const float stored=out.vertices.back().q;
            if(stored>0){out.near_=std::min(out.near_,1./stored);out.far_=std::max(out.far_,1./stored);}
        }
        out.cells_.resize(size_t(out.columns_-1)*(out.rows_-1));out.indices.reserve(out.cells_.size()*6);
        for(uint32_t row=0;row+1<out.rows_;row++)for(uint32_t col=0;col+1<out.columns_;col++) {
            const uint32_t a=row*out.columns_+col,b=a+1,c=a+out.columns_,d=c+1;
            const std::array<std::array<uint32_t,3>,2> triangles{{{a,b,c},{c,b,d}}};
            for(int t=0;t<2;t++) {
                float qmin=std::numeric_limits<float>::infinity(),qmax=0;
                for(auto i:triangles[t]){qmin=std::min(qmin,out.vertices[i].q);qmax=std::max(qmax,out.vertices[i].q);}
                if(qmin<=0||qmax/qmin>1.25f)continue;
                out.cells_[size_t(row)*(out.columns_-1)+col]|=1<<t;
                for(auto i:triangles[t])out.indices.push_back(i);
            }
        }
        return out;
    }
    std::optional<Vec3> intersect(const Ray& ray,const NativeLens& lens,double scale) const {
        if(!valid()||!lens.valid()||!finite(ray.origin)||!finite(ray.direction)
                ||dot(ray.direction,ray.direction)<1e-18||!std::isfinite(scale)||scale<=0)return {};
        double enter=0,leave=std::numeric_limits<double>::infinity();
        auto clip=[&](Vec3 n,double offset) {
            const double at=dot(n,ray.origin)+offset,along=dot(n,ray.direction);
            if(std::abs(along)<1e-14)return at>=-1e-9;
            if(along>0)enter=std::max(enter,-at/along);else leave=std::min(leave,-at/along);
            return enter<=leave+1e-9;
        };
        if(!clip({0,0,-1},-near_*scale)||!clip({0,0,1},far_*scale)
                ||!clip({lens.fx,0,-lens.cx},0)||!clip({-lens.fx,0,-(width_-lens.cx)},0)
                ||!clip({0,-lens.fy,-lens.cy},0)||!clip({0,lens.fy,-(height_-lens.cy)},0)
                ||!std::isfinite(leave)||leave<enter)return {};
        auto pixel=[&](double t){auto p=ray.origin+ray.direction*t;return std::array<double,2>{lens.fx*p.x/-p.z+lens.cx,-lens.fy*p.y/-p.z+lens.cy};};
        const auto start=pixel(enter),end=pixel(leave);
        const double x=std::clamp(start[0],0.,double(width_)-1e-8),y=std::clamp(start[1],0.,double(height_)-1e-8);
        const double dx=end[0]-start[0],dy=end[1]-start[1],inf=std::numeric_limits<double>::infinity();
        int col=int(x/step_),row=int(y/step_),sx=dx>1e-12?1:dx< -1e-12?-1:0,sy=dy>1e-12?1:dy< -1e-12?-1:0;
        double next_x=sx?((col+(sx>0))*step_-x)/dx:inf,next_y=sy?((row+(sy>0))*step_-y)/dy:inf;
        const double delta_x=sx?step_/std::abs(dx):inf,delta_y=sy?step_/std::abs(dy):inf;
        for(uint32_t visited=0;visited<columns_+rows_+4;visited++) {
            if(col<0||row<0||col>=int(columns_-1)||row>=int(rows_-1))break;
            if(cells_[size_t(row)*(columns_-1)+col]) {
                const uint32_t a=row*columns_+col,b=a+1,c=a+columns_,d=c+1;
                std::optional<Vec3> nearest;double distance=inf;
                const std::array<std::array<uint32_t,3>,2> triangles{{{a,b,c},{c,b,d}}};
                for(int t=0;t<2;t++) {
                    if(!(cells_[size_t(row)*(columns_-1)+col]&(1<<t)))continue;
                    const auto& ids=triangles[t];
                    std::array<Vec3,3> triangle;
                    for(int v=0;v<3;v++){auto p=vertices[ids[v]];triangle[v]=lens.unproject(p.x,p.y,scale/p.q);}
                    if(auto point=intersect_triangle(ray,triangle)){auto delta=*point-ray.origin;const double metric=dot(delta,delta);if(metric<distance){distance=metric;nearest=point;}}
                }
                if(nearest)return nearest;
            }
            const double next=std::min(next_x,next_y);if(!std::isfinite(next)||next>1+1e-9)break;
            const bool cross_x=next_x<=next+1e-12,cross_y=next_y<=next+1e-12;
            if(cross_x){col+=sx;next_x+=delta_x;}if(cross_y){row+=sy;next_y+=delta_y;}
        }
        return {};
    }
};
}

