#include "rifle_mesh.h"
#include <cmath>
#include <iostream>
using namespace silent_scope;
#define REQUIRE(x) do { if(!(x)){std::cerr << "FAIL line " << __LINE__ << ": " #x "\n";return 1;} } while(false)
int main() {
    auto mesh=make_rifle_mesh();
    REQUIRE(mesh.vertices.size()>300 && mesh.vertices.size()<10000);
    REQUIRE(mesh.vertices.size()%3==0);
    double min_z=0,max_z=0,min_y=0;
    for(const auto& vertex:mesh.vertices) {
        REQUIRE(std::isfinite(vertex.position.x)&&std::isfinite(vertex.position.y)&&std::isfinite(vertex.position.z));
        REQUIRE(vertex.color[3]==1);
        min_z=std::min(min_z,vertex.position.z);max_z=std::max(max_z,vertex.position.z);min_y=std::min(min_y,vertex.position.y);
        if(std::abs(vertex.position.z-mesh.ocular.z)<1e-6)
            REQUIRE(std::hypot(vertex.position.x-mesh.ocular.x,vertex.position.y-mesh.ocular.y)>=mesh.aperture_radius-.0001);
    }
    REQUIRE(min_z<-.8 && max_z>.2 && min_y<-.10);
    REQUIRE(mesh.ocular.y>.08 && mesh.aperture_radius>.015 && mesh.aperture_radius<.04);
    std::cout << "Original rifle geometry: "<<mesh.vertices.size()/3<<" triangles, stock, grip, barrel and open scope aperture\n";
}
