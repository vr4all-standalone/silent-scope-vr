// SPDX-License-Identifier: GPL-2.0-or-later
// Original VR4ALL procedural geometry. No extracted or bundled game model.
#pragma once
#include "scope_optics.h"
#include <array>
#include <vector>

namespace silent_scope {
struct RifleVertex {Vec3 position;std::array<float,4> color;};
struct RifleMesh {
    std::vector<RifleVertex> vertices, bolt, magazine;
    Vec3 ocular{0,.115,.09};
    double aperture_radius{.022};
    double image_inset{.006};
};
inline RifleMesh make_rifle_mesh() {
    RifleMesh mesh;
    using Color=std::array<float,4>;
    const Color metal{.12f,.14f,.16f,1},stock{.25f,.30f,.19f,1},rubber{.055f,.06f,.065f,1},rim{.25f,.27f,.29f,1};
    auto triangle=[&](Vec3 a,Vec3 b,Vec3 c,Color color){
        for(auto point:{a,b,c})mesh.vertices.push_back({point,color});
    };
    auto quad=[&](Vec3 a,Vec3 b,Vec3 c,Vec3 d,Color color){triangle(a,b,c,color);triangle(a,c,d,color);};
    auto box=[&](Vec3 minimum,Vec3 maximum,Color color){
        Vec3 p[8];for(int i=0;i<8;++i)p[i]={i&1?maximum.x:minimum.x,i&2?maximum.y:minimum.y,i&4?maximum.z:minimum.z};
        for(auto face:std::array<std::array<int,4>,6>{{{0,1,3,2},{4,6,7,5},{0,4,5,1},{2,3,7,6},{0,2,6,4},{1,5,7,3}}})
            quad(p[face[0]],p[face[1]],p[face[2]],p[face[3]],color);
    };
    auto tube=[&](double y,double front,double rear,double outer,double inner,Color color){
        constexpr int sides=24;
        constexpr double pi=3.14159265358979323846;
        auto point=[&](double angle,double radius,double z){return Vec3{std::cos(angle)*radius,y+std::sin(angle)*radius,z};};
        for(int i=0;i<sides;++i) {
            const double a=2*pi*i/sides,b=2*pi*(i+1)/sides;
            quad(point(a,outer,front),point(b,outer,front),point(b,outer,rear),point(a,outer,rear),color);
            quad(point(a,inner,front),point(a,inner,rear),point(b,inner,rear),point(b,inner,front),rubber);
            for(auto z:{front,rear})quad(point(a,inner,z),point(b,inner,z),point(b,outer,z),point(a,outer,z),rim);
        }
    };
    box({-.032,-.035,-.20},{.032,.035,.15},metal); // receiver
    box({-.034,-.045,.09},{.034,.025,.33},stock); // shoulder stock
    box({-.036,-.047,.32},{.036,.026,.35},rubber);
    box({-.024,-.14,-.025},{.024,-.025,.035},stock); // pistol grip
    box({-.035,-.025,-.49},{.035,.025,-.18},stock); // fore-end
    tube(.005,-.94,-.35,.012,.007,metal); // barrel and open muzzle
    box({-.008,-.10,-.075},{.008,-.035,-.065},metal); // trigger guard
    box({-.008,-.105,-.075},{.008,-.095,.02},metal);
    box({-.008,-.095,.01},{.008,-.035,.02},metal);
    box({-.007,-.07,-.04},{.007,-.03,-.03},rubber); // trigger
    for(double z:{-.22,-.05})box({-.014,.033,z-.012},{.014,.096,z+.012},metal);
    tube(mesh.ocular.y,-.32,mesh.ocular.z,.028,mesh.aperture_radius,metal);
    tube(mesh.ocular.y,-.36,-.28,.039,.028,metal); // objective bell
    box({.025,.008,.015},{.085,.018,.03},metal); // fixed bolt handle; manual cycle deferred
    box({.072,-.005,.008},{.095,.025,.038},rubber);
    return mesh;
}
}
