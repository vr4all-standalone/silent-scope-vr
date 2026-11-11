#include "scope_optics.h"
#include <cmath>
#include <iostream>
using namespace silent_scope;
#define REQUIRE(x) do { if(!(x)){std::cerr << "FAIL line " << __LINE__ << ": " #x "\n";return 1;} } while(false)
int main() {
    ScopeGeometry geometry;
    auto center=scope_visibility({0,0,.1},geometry);
    REQUIRE(center.visible);
    REQUIRE(!scope_visibility({.064,0,.1},geometry).visible);
    REQUIRE(!scope_visibility({0,0,-.1},geometry).visible);
    REQUIRE(!scope_visibility({0,0,.5},geometry).visible);
    REQUIRE(!scope_visibility({NAN,0,.1},geometry).visible);
    REQUIRE(scope_visibility({geometry.eye_box_radius,0,.1},geometry).visible);
    auto invalid=geometry;invalid.eye_box_radius=-1;
    REQUIRE(!scope_visibility({0,0,.1},invalid).visible);
    auto top_left=scope_uv({-.5,.5},4.0/3.0);
    auto bottom_right=scope_uv({.5,-.5},4.0/3.0);
    REQUIRE(top_left.u<bottom_right.u);REQUIRE(top_left.v<bottom_right.v);
    REQUIRE(std::abs((bottom_right.u-top_left.u)/(bottom_right.v-top_left.v)-.75)<1e-9);
    REQUIRE(!scope_uv({0,0},NAN).valid);
    REQUIRE(!scope_uv({NAN,0},1).valid);
    std::cout << "Per-eye aperture, eye relief, native display aspect and UV orientation checks passed\n";
}
