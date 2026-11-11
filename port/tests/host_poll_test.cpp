#include "host_poll.h"
#include <cassert>
#include <map>
#include <string>
using namespace silent_scope;
struct Field {uint32_t value{};void set_value(uint32_t v){value=v;}};
struct Port {std::map<uint32_t,Field> fields;Field* field(uint32_t mask){return &fields[mask];}};
struct Device {std::map<std::string,Port> ports;Port* ioport(const char* tag){return &ports[tag];}};
struct Machine {
    struct System {const char* name="sscope";} definition;
    Device device;bool is_paused{},exit{};int pauses{},resumes{};
    struct Time {double value{};double as_double() const{return value;}} clock;
    Time time() const{return clock;}
    const System& system(){return definition;}
    Device& root_device(){return device;}
    bool paused(){return is_paused;}
    void pause(){is_paused=true;++pauses;}
    void resume(){is_paused=false;++resumes;}
    void schedule_exit(){exit=true;}
};
int main(){
#ifdef _WIN32
    _putenv_s("SILENT_SCOPE_VR","1");_putenv_s("SILENT_SCOPE_SIMULATION","");
#else
    setenv("SILENT_SCOPE_VR","1",1);unsetenv("SILENT_SCOPE_SIMULATION");
#endif
    auto& runtime=shared_runtime();runtime.reset();runtime.ready=true;
    int camera_polls=0;scene::set_native_camera_provider({},[&]{++camera_polls;});
    Machine machine;runtime.input={777,1555,true};runtime.coin=runtime.start=true;
    poll_host(machine);assert(!machine.is_paused&&machine.device.ports[":IN0"].fields[4].value==1);
    runtime.pause_requested=true;poll_host(machine);
    assert(machine.is_paused&&machine.pauses==1&&machine.device.ports[":IN0"].fields[4].value==0);
    assert(machine.device.ports[":IN2"].fields[128].value==0);
    // No screen callback occurs while paused. OSD polling must still resume.
    runtime.release_inputs();runtime.pause_requested=false;poll_host(machine);
    assert(!machine.is_paused&&machine.resumes==1&&camera_polls==3);
    assert(machine.device.ports[":IN0"].fields[4].value==0);
    runtime.native_service_state=int(OperatorState::Normal);++runtime.service_request;machine.clock.value=20;
    poll_host(machine);assert(machine.device.ports[":IN2"].fields[16].value==1);
    // Pulse follows native time, independent of host/XR frame rate.
    poll_host(machine);assert(machine.device.ports[":IN2"].fields[16].value==1);
    machine.clock.value=20.3;poll_host(machine);assert(machine.device.ports[":IN2"].fields[16].value==0);
    ++runtime.service_request;runtime.pause_requested=true;poll_host(machine);
    runtime.pause_requested=false;poll_host(machine);assert(machine.device.ports[":IN2"].fields[16].value==0);
    runtime.native_service_state=int(OperatorState::Service);++runtime.service_request;poll_host(machine);
    assert(machine.device.ports[":IN2"].fields[16].value==0);
    // Closing the paused menu and requesting entry are one coherent command.
    runtime.native_service_state=int(OperatorState::Normal);runtime.publish_host_command(true);poll_host(machine);
    runtime.publish_host_command(false,true);poll_host(machine);
    assert(!machine.is_paused&&machine.device.ports[":IN2"].fields[16].value==1);
    runtime.quit=true;poll_host(machine);assert(machine.exit);
    runtime.reset();runtime.ready=false;Machine not_ready;poll_host(not_ready);assert(not_ready.device.ports.empty());
    scene::clear_native_camera_provider();
}
