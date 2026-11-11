// SPDX-License-Identifier: GPL-2.0-or-later
// Silent Scope host; OpenXR call patterns follow the existing VR4ALL Hornet host.
// Game state, camera offsets, menus and player logic are not inherited from Teraburst.
#include "shared_runtime.h"
#include "gl_context.h"
#include "rifle_renderer.h"
#include "rifle_support.h"
#include "settings.h"
#include "settings_io.h"
#include "menu_controller.h"
#include "menu_presentation.h"
#include "aim_visuals.h"
#include "operator_service.h"
#include "shared/ui_compat.h"
#include "xr_input.h"
#include "cabinet_controls.h"
#include "scene/gl_scene.h"
#include "scene/scene_geometry.h"
#include <glad/gl.h>
#ifdef _WIN32
#include <windows.h>
#include <unknwn.h>
#define XR_USE_PLATFORM_WIN32
#define XR_USE_GRAPHICS_API_OPENGL
#define SS_EXPORT extern "C" __declspec(dllexport)
#else
#include <jni.h>
#include <android/asset_manager_jni.h>
#include <EGL/egl.h>
#include <dlfcn.h>
#include <unistd.h>
#define XR_USE_PLATFORM_ANDROID
#define XR_USE_GRAPHICS_API_OPENGL_ES
#define SS_EXPORT extern "C" __attribute__((visibility("default")))
#endif
#define XR_NO_PROTOTYPES
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <thread>

namespace silent_scope {
namespace {
std::thread worker;
std::atomic<bool> stopping{};
std::mutex lifecycle;
std::ofstream logfile;
#ifdef __ANDROID__
JavaVM* java_vm{};
jobject android_activity{};
uint32_t emulation_thread{};
#endif
void log(const std::string& message){if(logfile){logfile<<message<<'\n';logfile.flush();}}
void check(XrResult result,const char* operation){if(XR_FAILED(result))throw std::runtime_error(std::string(operation)+": "+std::to_string(result));}
#define XR_FUNCTIONS(F) \
 F(CreateInstance) F(DestroyInstance) F(GetSystem) F(CreateSession) F(DestroySession) \
 F(EnumerateViewConfigurationViews) F(EnumerateSwapchainFormats) F(CreateSwapchain) F(DestroySwapchain) \
 F(EnumerateSwapchainImages) F(AcquireSwapchainImage) F(WaitSwapchainImage) F(ReleaseSwapchainImage) \
 F(CreateReferenceSpace) F(DestroySpace) F(PollEvent) F(BeginSession) F(EndSession) \
 F(WaitFrame) F(BeginFrame) F(EndFrame) F(LocateViews) F(LocateSpace) F(StringToPath) \
 F(CreateActionSet) F(DestroyActionSet) F(CreateAction) F(SuggestInteractionProfileBindings) \
 F(AttachSessionActionSets) F(CreateActionSpace) F(SyncActions) F(GetActionStateBoolean) F(GetActionStateFloat) \
 F(GetActionStateVector2f) F(ApplyHapticFeedback) F(StopHapticFeedback)
#define DECLARE(name) PFN_xr##name xr##name{};
XR_FUNCTIONS(DECLARE)
#undef DECLARE
PFN_xrGetInstanceProcAddr get_proc{};
void load(XrInstance instance){
#define LOAD(name) check(get_proc(instance,"xr" #name,reinterpret_cast<PFN_xrVoidFunction*>(&xr##name)),"load xr" #name);
    XR_FUNCTIONS(LOAD)
#undef LOAD
}
glm::mat4 pose(const XrPosef& value){return glm::translate(glm::mat4(1),{value.position.x,value.position.y,value.position.z})*
    glm::mat4_cast(glm::quat(value.orientation.w,value.orientation.x,value.orientation.y,value.orientation.z));}
glm::mat4 projection(const XrFovf& fov){
    float left=std::tan(fov.angleLeft),right=std::tan(fov.angleRight),up=std::tan(fov.angleUp),down=std::tan(fov.angleDown);
    glm::mat4 result(0);result[0][0]=2/(right-left);result[1][1]=2/(up-down);
    result[2][0]=(right+left)/(right-left);result[2][1]=(up+down)/(up-down);
    result[2][2]=-1;result[2][3]=-1;result[3][2]=-.01f;return result;
}
void screenshot(const std::string& path,int width,int height){
    std::vector<uint8_t> pixels(size_t(width)*height*3);glPixelStorei(GL_PACK_ALIGNMENT,1);
    glReadPixels(0,0,width,height,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());
    std::ofstream out(path,std::ios::binary);out<<"P6\n"<<width<<' '<<height<<"\n255\n";
    for(int y=height-1;y>=0;--y)out.write(reinterpret_cast<const char*>(pixels.data()+size_t(y)*width*3),width*3);
}
void simulation(SharedRuntime& runtime){
    GLContext context;RifleRenderer renderer;
    GLuint target{},color{},depth{};glGenFramebuffers(1,&target);glBindFramebuffer(GL_FRAMEBUFFER,target);
    glGenTextures(1,&color);glBindTexture(GL_TEXTURE_2D,color);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,640,640,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,color,0);
    glGenRenderbuffers(1,&depth);glBindRenderbuffer(GL_RENDERBUFFER,depth);glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,640,640);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,depth);
    if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)throw std::runtime_error("Diagnostic framebuffer incomplete");
    runtime.ready=true;log("Diagnostic GPU host ready; no headset session");uint64_t saved{};
    while(!stopping&&!runtime.quit){
        auto main=runtime.displays.acquire(Display::Main),scope=runtime.displays.acquire(Display::Scope);
        if(scope&&scope->sequence>=saved+120){
            saved=scope->sequence;glBindFramebuffer(GL_FRAMEBUFFER,target);glViewport(0,0,640,640);
            glClearColor(.025f,.018f,.04f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
            auto ocular=make_rifle_mesh().ocular;glm::vec3 eye(ocular.x,ocular.y,ocular.z+.12);
            auto vp=glm::perspective(glm::radians(32.f),1.f,.005f,5.f)*glm::lookAt(eye,glm::vec3(ocular.x,ocular.y,ocular.z),glm::vec3(0,1,0));
            renderer.draw_rifle(vp,glm::mat4(1),eye,scope);
            if(const char* directory=std::getenv("SILENT_SCOPE_SIM_DIR"))screenshot(std::string(directory)+"/scope-"+std::to_string(saved)+".ppm",640,640);
            log("GPU scope "+std::to_string(saved)+", main "+std::to_string(main?main->sequence:0)+", source "+std::to_string(scope->width)+"x"+std::to_string(scope->height));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    glDeleteRenderbuffers(1,&depth);glDeleteTextures(1,&color);glDeleteFramebuffers(1,&target);runtime.ready=false;
}
struct Host {
    SharedRuntime& runtime;GLContext context;RifleRenderer renderer;scene::GLScene world;Settings settings;RifleInput rifle_input;
#ifdef _WIN32
    HMODULE loader{};
#else
    void* loader{};
#endif
    XrInstance instance{};XrSystemId system{};XrSession session{};XrSpace local{};
    XrActionSet actions{};XrAction aim{},trigger{},start{},menu{},secondary{},recenter{},stick{},grip{},haptic{};
    XrPath hands[2]{};XrSpace aim_space[2]{};
    struct Eye {XrSwapchain swapchain{};uint32_t width{},height{};GLuint target{},depth{};std::vector<GLuint> colors;} eyes[2];
    XrSessionState session_state{};bool running{},origin_set{},menu_open{};
    PFN_xrPerfSettingsSetPerformanceLevelEXT performance{};
#ifdef __ANDROID__
    PFN_xrSetAndroidApplicationThreadKHR set_thread{};
#endif
    glm::mat4 origin{1};CabinetControls cabinet_controls;
    MenuController menu_controller;MenuPresentation menu_presentation;RifleSupport rifle_support;AimVisuals aim_visuals;
    glm::mat4 menu_model{1};bool panel_placed{},previous_click[2]{},page_consumed[2]{},previous_tracked[2]{},previous_laser{},previous_recenter{},previous_shot{};
    double fps_began{};uint64_t fps_frames{};int previous_operator{};
    explicit Host(SharedRuntime& value):runtime(value){if(const char* file=std::getenv("SILENT_SCOPE_SETTINGS")){std::ifstream in(file);settings=Settings::load(in);}}
    ~Host(){
        vr::xr::termPanel();runtime.pause_requested=false;
        runtime.ready=false;runtime.release_inputs();
        for(auto& eye:eyes){if(eye.depth)glDeleteRenderbuffers(1,&eye.depth);if(eye.target)glDeleteFramebuffers(1,&eye.target);if(eye.swapchain)xrDestroySwapchain(eye.swapchain);}
        for(auto space:aim_space)if(space)xrDestroySpace(space);
        if(local)xrDestroySpace(local);if(session)xrDestroySession(session);if(actions)xrDestroyActionSet(actions);if(instance)xrDestroyInstance(instance);
#ifdef _WIN32
        if(loader)FreeLibrary(loader);
#else
        if(loader)dlclose(loader);
#endif
    }
    XrPath path(const char* name){XrPath value{};check(xrStringToPath(instance,name,&value),name);return value;}
    XrAction action(const char* name,XrActionType type){
        XrActionCreateInfo info{XR_TYPE_ACTION_CREATE_INFO};std::strncpy(info.actionName,name,sizeof(info.actionName)-1);std::strncpy(info.localizedActionName,name,sizeof(info.localizedActionName)-1);
        info.actionType=type;info.countSubactionPaths=2;info.subactionPaths=hands;XrAction value{};check(xrCreateAction(actions,&info,&value),name);return value;
    }
    void init(){
#ifdef _WIN32
        loader=LoadLibraryW(L"openxr_loader.dll");if(!loader)loader=LoadLibraryW(L"libopenxr_loader.dll");
        if(!loader)throw std::runtime_error("OpenXR loader missing");
        get_proc=reinterpret_cast<PFN_xrGetInstanceProcAddr>(GetProcAddress(loader,"xrGetInstanceProcAddr"));if(!get_proc)throw std::runtime_error("Invalid OpenXR loader");
#else
        loader=dlopen("libopenxr_loader.so",RTLD_NOW|RTLD_LOCAL);if(!loader)throw std::runtime_error("OpenXR loader missing");
        get_proc=reinterpret_cast<PFN_xrGetInstanceProcAddr>(dlsym(loader,"xrGetInstanceProcAddr"));if(!get_proc)throw std::runtime_error("Invalid OpenXR loader");
        if(!java_vm||!android_activity)throw std::runtime_error("Android activity unavailable");
        PFN_xrInitializeLoaderKHR initialize{};check(get_proc(XR_NULL_HANDLE,"xrInitializeLoaderKHR",reinterpret_cast<PFN_xrVoidFunction*>(&initialize)),"Loader init function");
        XrLoaderInitInfoAndroidKHR init{XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR};init.applicationVM=java_vm;init.applicationContext=android_activity;
        check(initialize(reinterpret_cast<const XrLoaderInitInfoBaseHeaderKHR*>(&init)),"Initialize Android loader");
#endif
        check(get_proc(XR_NULL_HANDLE,"xrCreateInstance",reinterpret_cast<PFN_xrVoidFunction*>(&xrCreateInstance)),"CreateInstance function");
#ifdef _WIN32
        std::vector<const char*> extensions{XR_KHR_OPENGL_ENABLE_EXTENSION_NAME};
#else
        std::vector<const char*> extensions{XR_KHR_OPENGL_ES_ENABLE_EXTENSION_NAME,XR_KHR_ANDROID_CREATE_INSTANCE_EXTENSION_NAME};
#endif
        PFN_xrEnumerateInstanceExtensionProperties enumerate{};
        check(get_proc(XR_NULL_HANDLE,"xrEnumerateInstanceExtensionProperties",reinterpret_cast<PFN_xrVoidFunction*>(&enumerate)),"Extension function");
        uint32_t extension_count{};check(enumerate(nullptr,0,&extension_count,nullptr),"Extension count");
        std::vector<XrExtensionProperties> available(extension_count,{XR_TYPE_EXTENSION_PROPERTIES});
        check(enumerate(nullptr,extension_count,&extension_count,available.data()),"Extensions");
        bool sustained=std::any_of(available.begin(),available.end(),[](const auto& e){return std::strcmp(e.extensionName,XR_EXT_PERFORMANCE_SETTINGS_EXTENSION_NAME)==0;});
        if(sustained)extensions.push_back(XR_EXT_PERFORMANCE_SETTINGS_EXTENSION_NAME);
#ifdef __ANDROID__
        bool thread_settings=std::any_of(available.begin(),available.end(),[](const auto& e){return std::strcmp(e.extensionName,XR_KHR_ANDROID_THREAD_SETTINGS_EXTENSION_NAME)==0;});
        if(thread_settings)extensions.push_back(XR_KHR_ANDROID_THREAD_SETTINGS_EXTENSION_NAME);
#endif
        XrInstanceCreateInfo create{XR_TYPE_INSTANCE_CREATE_INFO};std::strcpy(create.applicationInfo.applicationName,"Silent Scope - VR4ALL");
        std::strcpy(create.applicationInfo.engineName,"MAME VR4ALL");create.applicationInfo.apiVersion=XR_MAKE_VERSION(1,0,34);
        create.enabledExtensionCount=extensions.size();create.enabledExtensionNames=extensions.data();
#ifdef __ANDROID__
        XrInstanceCreateInfoAndroidKHR android{XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR};android.applicationVM=java_vm;android.applicationActivity=android_activity;create.next=&android;
#endif
        check(xrCreateInstance(&create,&instance),"CreateInstance");load(instance);
        if(sustained)check(get_proc(instance,"xrPerfSettingsSetPerformanceLevelEXT",reinterpret_cast<PFN_xrVoidFunction*>(&performance)),"Performance function");
#ifdef __ANDROID__
        if(thread_settings)check(get_proc(instance,"xrSetAndroidApplicationThreadKHR",reinterpret_cast<PFN_xrVoidFunction*>(&set_thread)),"Thread settings function");
#endif
        XrSystemGetInfo sys{XR_TYPE_SYSTEM_GET_INFO};sys.formFactor=XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;check(xrGetSystem(instance,&sys,&system),"GetSystem");
#ifdef _WIN32
        PFN_xrGetOpenGLGraphicsRequirementsKHR requirements{};check(get_proc(instance,"xrGetOpenGLGraphicsRequirementsKHR",reinterpret_cast<PFN_xrVoidFunction*>(&requirements)),"Requirements function");
        XrGraphicsRequirementsOpenGLKHR req{XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR};check(requirements(instance,system,&req),"Graphics requirements");
#else
        PFN_xrGetOpenGLESGraphicsRequirementsKHR requirements{};check(get_proc(instance,"xrGetOpenGLESGraphicsRequirementsKHR",reinterpret_cast<PFN_xrVoidFunction*>(&requirements)),"Requirements function");
        XrGraphicsRequirementsOpenGLESKHR req{XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_ES_KHR};check(requirements(instance,system,&req),"Graphics requirements");
#endif
        GLint major{},minor{};glGetIntegerv(GL_MAJOR_VERSION,&major);glGetIntegerv(GL_MINOR_VERSION,&minor);
        auto version=XR_MAKE_VERSION(major,minor,0);if(version<req.minApiVersionSupported||version>req.maxApiVersionSupported)throw std::runtime_error("Incompatible OpenGL context version");
#ifdef _WIN32
        XrGraphicsBindingOpenGLWin32KHR binding{XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR};binding.hDC=static_cast<HDC>(context.display());binding.hGLRC=static_cast<HGLRC>(context.context());
#else
        XrGraphicsBindingOpenGLESAndroidKHR binding{XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR};binding.display=context.display();binding.config=context.config();binding.context=context.context();
#endif
        XrSessionCreateInfo si{XR_TYPE_SESSION_CREATE_INFO};si.next=&binding;si.systemId=system;check(xrCreateSession(instance,&si,&session),"CreateSession");
        XrReferenceSpaceCreateInfo reference{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};reference.referenceSpaceType=XR_REFERENCE_SPACE_TYPE_LOCAL;reference.poseInReferenceSpace.orientation.w=1;
        check(xrCreateReferenceSpace(session,&reference,&local),"Reference space");
        uint32_t count{};check(xrEnumerateViewConfigurationViews(instance,system,XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,0,&count,nullptr),"View count");
        if(count!=2)throw std::runtime_error("Stereo views required");std::vector<XrViewConfigurationView> views(count,{XR_TYPE_VIEW_CONFIGURATION_VIEW});
        check(xrEnumerateViewConfigurationViews(instance,system,XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,count,&count,views.data()),"Views");
        check(xrEnumerateSwapchainFormats(session,0,&count,nullptr),"Format count");std::vector<int64_t> formats(count);check(xrEnumerateSwapchainFormats(session,count,&count,formats.data()),"Formats");
        int64_t format=0;for(auto candidate:{GL_RGBA8})if(std::find(formats.begin(),formats.end(),candidate)!=formats.end()){format=candidate;break;}
        if(!format)throw std::runtime_error("RGBA8 eye format required");
        for(int i=0;i<2;++i){auto& eye=eyes[i];eye.width=views[i].recommendedImageRectWidth;eye.height=views[i].recommendedImageRectHeight;
            XrSwapchainCreateInfo chain{XR_TYPE_SWAPCHAIN_CREATE_INFO};chain.usageFlags=XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;chain.format=format;chain.sampleCount=1;chain.width=eye.width;chain.height=eye.height;chain.faceCount=chain.arraySize=chain.mipCount=1;
            check(xrCreateSwapchain(session,&chain,&eye.swapchain),"Swapchain");check(xrEnumerateSwapchainImages(eye.swapchain,0,&count,nullptr),"Image count");
#ifdef _WIN32
            std::vector<XrSwapchainImageOpenGLKHR> images(count,{XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR});
#else
            std::vector<XrSwapchainImageOpenGLESKHR> images(count,{XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR});
#endif
            check(xrEnumerateSwapchainImages(eye.swapchain,count,&count,reinterpret_cast<XrSwapchainImageBaseHeader*>(images.data())),"Images");
            for(auto image:images)eye.colors.push_back(image.image);glGenFramebuffers(1,&eye.target);glGenRenderbuffers(1,&eye.depth);glBindRenderbuffer(GL_RENDERBUFFER,eye.depth);glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,eye.width,eye.height);
        }
        hands[0]=path("/user/hand/left");hands[1]=path("/user/hand/right");
        XrActionSetCreateInfo set{XR_TYPE_ACTION_SET_CREATE_INFO};std::strcpy(set.actionSetName,"rifle");std::strcpy(set.localizedActionSetName,"Rifle");check(xrCreateActionSet(instance,&set,&actions),"Action set");
        aim=action("aim",XR_ACTION_TYPE_POSE_INPUT);trigger=action("trigger",XR_ACTION_TYPE_FLOAT_INPUT);start=action("start",XR_ACTION_TYPE_BOOLEAN_INPUT);
        menu=action("menu",XR_ACTION_TYPE_BOOLEAN_INPUT);secondary=action("secondary",XR_ACTION_TYPE_BOOLEAN_INPUT);recenter=action("recenter",XR_ACTION_TYPE_BOOLEAN_INPUT);
        stick=action("stick",XR_ACTION_TYPE_VECTOR2F_INPUT);grip=action("grip",XR_ACTION_TYPE_FLOAT_INPUT);haptic=action("feedback",XR_ACTION_TYPE_VIBRATION_OUTPUT);
        std::vector<XrActionSuggestedBinding> bindings;
        for(int i=0;i<2;++i){std::string root=i?"/user/hand/right":"/user/hand/left";
            bindings.push_back({aim,path((root+"/input/aim/pose").c_str())});bindings.push_back({trigger,path((root+"/input/trigger/value").c_str())});
            bindings.push_back({start,path((root+(i?"/input/a/click":"/input/x/click")).c_str())});
            bindings.push_back({secondary,path((root+(i?"/input/b/click":"/input/y/click")).c_str())});
            bindings.push_back({recenter,path((root+"/input/thumbstick/click").c_str())});
            bindings.push_back({stick,path((root+"/input/thumbstick").c_str())});
            bindings.push_back({grip,path((root+"/input/squeeze/value").c_str())});
            bindings.push_back({haptic,path((root+"/output/haptic").c_str())});
        }
        bindings.push_back({menu,path("/user/hand/left/input/menu/click")});
        XrInteractionProfileSuggestedBinding suggest{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};suggest.interactionProfile=path("/interaction_profiles/oculus/touch_controller");suggest.countSuggestedBindings=bindings.size();suggest.suggestedBindings=bindings.data();
        check(xrSuggestInteractionProfileBindings(instance,&suggest),"Touch bindings");XrSessionActionSetsAttachInfo attach{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};attach.countActionSets=1;attach.actionSets=&actions;check(xrAttachSessionActionSets(session,&attach),"Attach actions");
        for(int i=0;i<2;++i){XrActionSpaceCreateInfo space{XR_TYPE_ACTION_SPACE_CREATE_INFO};space.action=aim;space.subactionPath=hands[i];space.poseInActionSpace.orientation.w=1;check(xrCreateActionSpace(session,&space,&aim_space[i]),"Aim space");}
        runtime.ready=true;log("OpenXR host ready; native main-board geometry stereo; independent native scope raster");
    }
    bool button(XrAction action,int hand){XrActionStateGetInfo get{XR_TYPE_ACTION_STATE_GET_INFO};get.action=action;get.subactionPath=hands[hand];XrActionStateBoolean value{XR_TYPE_ACTION_STATE_BOOLEAN};check(xrGetActionStateBoolean(session,&get,&value),"Button");return value.isActive&&value.currentState;}
    void draw_controller_help(const glm::mat4& vp,const glm::mat4& controller,int hand){
        const char* labels[]{"Trigger: Fire / select","Grip: Support rifle",hand?"Stick click: Laser":"Stick click: Recenter",hand?"A: Start | Hold B: Menu":"X: Start | Hold Y: Menu"};
        for(int line=0;line<4;++line){
            int width{},height{};auto texture=vr::xr::textTexture(labels[line],30,vr::xr::theme::Accent,width,height);
            if(!texture||height<=0)continue;
            auto model=menu_model;
            model[3]=glm::vec4(glm::vec3(controller[3])+glm::vec3(menu_model*glm::vec4(hand?.18f:-.18f,.16f-line*.045f,.025f,0)),1);
            const float h=.022f,w=std::min(.37f,h*float(width)/height);
            vr::xr::drawTexture(vp,model,texture,w*.5f,h*.5f);
        }
    }
    void run(){
        while(!stopping&&!runtime.quit){
            XrEventDataBuffer event{XR_TYPE_EVENT_DATA_BUFFER};
            while(xrPollEvent(instance,&event)==XR_SUCCESS){
                if(event.type==XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED){auto state=reinterpret_cast<XrEventDataSessionStateChanged*>(&event)->state;session_state=state;
                    runtime.release_inputs();rifle_input.reset();rifle_support.reset();cabinet_controls.suspend();
                    MenuFrame interrupted;interrupted.focused=false;menu_controller.update(interrupted,settings);
                    runtime.publish_host_command(state!=XR_SESSION_STATE_FOCUSED||menu_controller.open());
                    for(auto hand:hands){XrHapticActionInfo info{XR_TYPE_HAPTIC_ACTION_INFO};info.action=haptic;info.subactionPath=hand;xrStopHapticFeedback(session,&info);}
                    if(state==XR_SESSION_STATE_READY){XrSessionBeginInfo begin{XR_TYPE_SESSION_BEGIN_INFO};begin.primaryViewConfigurationType=XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;check(xrBeginSession(session,&begin),"BeginSession");running=true;
#ifdef __ANDROID__
                        if(set_thread){auto app=set_thread(session,XR_ANDROID_THREAD_TYPE_APPLICATION_MAIN_KHR,emulation_thread);auto render=set_thread(session,XR_ANDROID_THREAD_TYPE_RENDERER_MAIN_KHR,uint32_t(gettid()));log("XR thread roles emulation="+std::to_string(emulation_thread)+" result="+std::to_string(app)+" renderer="+std::to_string(gettid())+" result="+std::to_string(render));}
#endif
                        if(performance){auto cpu=performance(session,XR_PERF_SETTINGS_DOMAIN_CPU_EXT,XR_PERF_SETTINGS_LEVEL_SUSTAINED_HIGH_EXT);auto gpu=performance(session,XR_PERF_SETTINGS_DOMAIN_GPU_EXT,XR_PERF_SETTINGS_LEVEL_SUSTAINED_HIGH_EXT);log("Sustained performance CPU/GPU="+std::to_string(cpu)+"/"+std::to_string(gpu));}}
                    if(state==XR_SESSION_STATE_STOPPING){check(xrEndSession(session),"EndSession");running=false;}
                    if(state==XR_SESSION_STATE_EXITING||state==XR_SESSION_STATE_LOSS_PENDING){runtime.quit=true;return;}}
                event={XR_TYPE_EVENT_DATA_BUFFER};
            }
            if(!running){std::this_thread::sleep_for(std::chrono::milliseconds(20));continue;}
            XrFrameWaitInfo wait{XR_TYPE_FRAME_WAIT_INFO};XrFrameState frame{XR_TYPE_FRAME_STATE};check(xrWaitFrame(session,&wait,&frame),"WaitFrame");
            XrFrameBeginInfo begin{XR_TYPE_FRAME_BEGIN_INFO};check(xrBeginFrame(session,&begin),"BeginFrame");
            XrViewLocateInfo locate{XR_TYPE_VIEW_LOCATE_INFO};locate.viewConfigurationType=XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;locate.displayTime=frame.predictedDisplayTime;locate.space=local;
            XrViewState view_state{XR_TYPE_VIEW_STATE};XrView views[2]={{XR_TYPE_VIEW},{XR_TYPE_VIEW}};uint32_t count{};check(xrLocateViews(session,&locate,&view_state,2,&count,views),"LocateViews");
            bool valid=count==2&&(view_state.viewStateFlags&3)==3,focused=session_state==XR_SESSION_STATE_FOCUSED;
            if(valid&&!origin_set){auto head=pose(views[0].pose);auto forward=glm::vec3(head*glm::vec4(0,0,-1,0));auto center=(glm::vec3(pose(views[0].pose)[3])+glm::vec3(pose(views[1].pose)[3]))*.5f;
                origin=glm::translate(glm::mat4(1),center)*glm::rotate(glm::mat4(1),std::atan2(-forward.x,-forward.z),glm::vec3(0,1,0));origin_set=true;panel_placed=false;}
            XrActiveActionSet set{actions,XR_NULL_PATH};XrActionsSyncInfo sync{XR_TYPE_ACTIONS_SYNC_INFO};sync.countActiveActionSets=1;sync.activeActionSets=&set;check(xrSyncActions(session,&sync),"SyncActions");
            double now=double(frame.predictedDisplayTime)*1e-9;
            bool input_tracking=valid&&view_tracking_good(view_state.viewStateFlags);
            const int operator_state=runtime.native_service_state.load();
            if(operator_state!=previous_operator){
                previous_operator=operator_state;runtime.release_inputs();rifle_input.reset();rifle_support.reset();cabinet_controls.suspend();
                MenuFrame interrupted;interrupted.focused=false;menu_controller.update(interrupted,settings);
            }
            MenuCapabilities capabilities{runtime.wider_available.load(),runtime.wider_applied.load()&&!runtime.service_active.load(),operator_state==int(OperatorState::Normal),runtime.service_active.load()};
            menu_presentation.update(settings,capabilities,menu_controller);
            MenuFrame menu_frame;menu_frame.time=now;menu_frame.focused=focused&&input_tracking;menu_frame.menu_action=button(menu,0);
            HandSample samples[2]{};glm::mat4 models[2]={glm::mat4(1),glm::mat4(1)};
            std::array<RifleSupportHand,2> support_hands;
            bool page_turned=false;
            for(int i=0;i<2;++i){XrSpaceLocation location{XR_TYPE_SPACE_LOCATION};check(xrLocateSpace(aim_space[i],local,frame.predictedDisplayTime,&location),"Locate rifle");
                models[i]=glm::inverse(origin)*pose(location.pose)*glm::rotate(glm::mat4(1),glm::radians(float(settings.gun_angle-10)),glm::vec3(1,0,0));auto forward=glm::vec3(models[i]*glm::vec4(0,0,-1,0));
                XrActionStateGetInfo get{XR_TYPE_ACTION_STATE_GET_INFO};get.action=trigger;get.subactionPath=hands[i];XrActionStateFloat pulled{XR_TYPE_ACTION_STATE_FLOAT};check(xrGetActionStateFloat(session,&get,&pulled),"Trigger");
                samples[i]=tracked_hand(location.locationFlags,{forward.x,forward.y,forward.z},pulled.isActive&&pulled.currentState>.55f);
                get.action=grip;XrActionStateFloat squeezed{XR_TYPE_ACTION_STATE_FLOAT};check(xrGetActionStateFloat(session,&get,&squeezed),"Grip");
                support_hands[i]={models[i],samples[i].tracked,squeezed.isActive&&squeezed.currentState>.55f};
                auto& hand=menu_frame.hands[i];hand.tracked=samples[i].tracked;
                hand.back=button(secondary,i);hand.select=button(start,i);hand.trigger=samples[i].trigger;
                get.action=stick;XrActionStateVector2f axes{XR_TYPE_ACTION_STATE_VECTOR2F};check(xrGetActionStateVector2f(session,&get,&axes),"Thumbstick");
                if(axes.isActive){hand.stick_x=axes.currentState.x;hand.stick_y=axes.currentState.y;}
                if(menu_controller.open()&&hand.tracked){
                    auto hit=menu_presentation.pointer(menu_model,glm::vec3(models[i][3]),forward);
                    hand.pointer_row=hit.row;hand.pointer_u=hit.u;
                    bool click=hand.trigger||hand.select;
                    if(hit.page_delta&&click&&!previous_click[i]&&!page_turned){menu_presentation.change_page(hit.page_delta);page_consumed[i]=true;page_turned=true;}
                    if(!click)page_consumed[i]=false;
                    if(page_consumed[i])hand.trigger=hand.select=false;
                    previous_click[i]=click;
                }else previous_click[i]=hand.trigger||hand.select;
            }
            for(int hand=0;hand<2;++hand){
                if(page_turned){page_consumed[hand]=page_consumed[hand]||samples[hand].trigger||button(start,hand);menu_frame.hands[hand].trigger=menu_frame.hands[hand].select=false;}
                if(previous_tracked[hand]&&!samples[hand].tracked)cabinet_controls.suspend();
                previous_tracked[hand]=samples[hand].tracked;
            }
            bool left_click=button(recenter,0),right_click=button(recenter,1);
            menu_frame.recentered=focused&&input_tracking&&left_click&&!previous_recenter;
            previous_recenter=left_click;
            const bool was_menu_open=menu_open;
            auto effects=menu_controller.update(menu_frame,settings,capabilities);menu_open=menu_controller.open();
            if(menu_open&&!was_menu_open)panel_placed=false;
            if(effects.release_inputs){runtime.release_inputs();rifle_input.reset();rifle_support.reset();cabinet_controls.suspend();previous_shot=false;
                for(auto hand:hands){XrHapticActionInfo info{XR_TYPE_HAPTIC_ACTION_INFO};info.action=haptic;info.subactionPath=hand;xrStopHapticFeedback(session,&info);}}
            if(effects.recenter||menu_frame.recentered){origin_set=false;panel_placed=false;rifle_input.reset();}
            if(effects.quit)runtime.quit=true;
            if(effects.gameplay_allowed&&right_click&&!previous_laser){settings.laser_on=!settings.laser_on;effects.save=true;}
            previous_laser=right_click;
            auto support=rifle_support.update(support_hands,{effects.gameplay_allowed,false,settings.active_hand});
            if(support.valid){
                models[settings.active_hand]=support.model;
                const auto forward=glm::vec3(support.model*glm::vec4(0,0,-1,0));
                samples[settings.active_hand]=tracked_hand(15,{forward.x,forward.y,forward.z},samples[settings.active_hand].trigger);
            }
            if(effects.save){
                std::string error;const char* file=std::getenv("SILENT_SCOPE_SETTINGS");
                if(!file||!save_settings_atomic(file,settings,&error))menu_presentation.save_error(error.empty()?"Settings path unavailable":error);
                else menu_presentation.save_error("");
            }
            runtime.wider_requested=settings.wider_view;
            runtime.publish_host_command(menu_open||!focused||!input_tracking,effects.service);
            if((menu_open||settings.fps_counter)&&!panel_placed&&origin_set){
                auto head=glm::inverse(origin)*pose(views[0].pose);auto direction=glm::vec3(head*glm::vec4(0,0,-1,0));
                auto facing=glm::rotate(glm::mat4(1),std::atan2(-direction.x,-direction.z),glm::vec3(0,1,0));
                menu_model=glm::translate(glm::mat4(1),glm::vec3(head[3]))*facing*glm::translate(glm::mat4(1),glm::vec3(0,0,-1.2f));panel_placed=true;
            }else if(!menu_open&&!settings.fps_counter)panel_placed=false;
            bool start_pressed=(menu_frame.hands[0].tracked&&menu_frame.hands[0].select)||(menu_frame.hands[1].tracked&&menu_frame.hands[1].select);
            auto controls=cabinet_controls.update(now,effects.gameplay_allowed&&!runtime.service_active,{false,start_pressed,false,false});
            auto cabinet=rifle_input.update(samples[0],samples[1],{effects.gameplay_allowed,false,settings.active_hand},settings.calibration);
            if(cabinet.trigger&&!previous_shot&&!runtime.service_active&&settings.haptics!=Haptics::Off){
                constexpr float levels[]{0,.35f,.65f,1};XrHapticVibration vibration{XR_TYPE_HAPTIC_VIBRATION};vibration.amplitude=levels[int(settings.haptics)];vibration.duration=35'000'000;vibration.frequency=XR_FREQUENCY_UNSPECIFIED;
                XrHapticActionInfo action_info{XR_TYPE_HAPTIC_ACTION_INFO};action_info.action=haptic;action_info.subactionPath=hands[settings.active_hand];
                auto result=xrApplyHapticFeedback(session,&action_info,reinterpret_cast<const XrHapticBaseHeader*>(&vibration));
                if(XR_FAILED(result))log("Rifle feedback unavailable: "+std::to_string(result));
            }
            previous_shot=cabinet.trigger;
            {std::lock_guard lock(runtime.input_mutex);runtime.input=cabinet;runtime.coin=controls.coin;runtime.start=runtime.service_active?effects.gameplay_allowed&&start_pressed:controls.start;runtime.service=false;}
            if(!fps_began)fps_began=now;
            ++fps_frames;if(now-fps_began>=1){menu_presentation.status_fps(fps_frames/(now-fps_began),runtime.native_scene_hz.load());fps_began=now;fps_frames=0;}
            menu_presentation.update(settings,capabilities,menu_controller);
            auto main=runtime.displays.acquire(Display::Main),scope=runtime.displays.acquire(Display::Scope);
            auto spatial=runtime.main_scene.load();world.prepare(spatial,false);
            aim_visuals.prepare(spatial,settings,models[settings.active_hand],effects.gameplay_allowed&&!runtime.service_active);
            XrCompositionLayerProjectionView submitted[2]={{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW},{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW}};
            if(frame.shouldRender&&valid)for(int i=0;i<2;++i){auto& eye=eyes[i];uint32_t index{};XrSwapchainImageAcquireInfo acquire{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};check(xrAcquireSwapchainImage(eye.swapchain,&acquire,&index),"Acquire eye");
                XrSwapchainImageWaitInfo wait_image{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};wait_image.timeout=XR_INFINITE_DURATION;check(xrWaitSwapchainImage(eye.swapchain,&wait_image),"Wait eye");
                glBindFramebuffer(GL_FRAMEBUFFER,eye.target);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,eye.colors[index],0);glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,eye.depth);glViewport(0,0,eye.width,eye.height);
                glClearColor(menu_open?.12f:.015f,.015f,menu_open?.22f:.02f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
                auto camera=glm::inverse(origin)*pose(views[i].pose);auto vp=projection(views[i].fov)*glm::inverse(camera);
                if(spatial&&spatial->lens.valid()&&!runtime.service_active)world.draw(vp,scene::SceneMetresPerUnit*float(settings.world_size/.1),float(settings.panel_distance),1,{},spatial->wider_applied?float(settings.hud_size):1.f);
                else renderer.draw_panel(vp,main,float(settings.panel_distance));
                if(samples[settings.active_hand].tracked)renderer.draw_rifle(vp,models[settings.active_hand],glm::vec3(camera[3]),scope);
                aim_visuals.draw(vp,glm::vec3(camera[3]));
                menu_presentation.draw(vp,menu_model);
                if(menu_open&&focused&&input_tracking)for(int hand=0;hand<2;++hand)if(samples[hand].tracked){
                    auto forward=glm::vec3(models[hand]*glm::vec4(0,0,-1,0));
                    auto hit=menu_presentation.pointer(menu_model,glm::vec3(models[hand][3]),forward);
                    if(hit.row>=0||hit.page_delta)AimVisuals::draw_menu_pointer(vp,glm::vec3(models[hand][3]),hit.point,true,glm::vec3(camera[3]));
                    if(menu_controller.how_to_play())draw_controller_help(vp,models[hand],hand);
                }
                glFlush();XrSwapchainImageReleaseInfo release{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};check(xrReleaseSwapchainImage(eye.swapchain,&release),"Release eye");
                submitted[i].pose=views[i].pose;submitted[i].fov=views[i].fov;submitted[i].subImage.swapchain=eye.swapchain;submitted[i].subImage.imageRect={{0,0},{int32_t(eye.width),int32_t(eye.height)}};
            }
            XrCompositionLayerProjection layer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};layer.space=local;layer.viewCount=2;layer.views=submitted;const XrCompositionLayerBaseHeader* layers[]={reinterpret_cast<const XrCompositionLayerBaseHeader*>(&layer)};
            XrFrameEndInfo end{XR_TYPE_FRAME_END_INFO};end.displayTime=frame.predictedDisplayTime;end.environmentBlendMode=XR_ENVIRONMENT_BLEND_MODE_OPAQUE;end.layerCount=frame.shouldRender&&valid?1:0;end.layers=layers;check(xrEndFrame(session,&end),"EndFrame");
        }
    }
};
}
}
SS_EXPORT bool silent_scope_xr_start(silent_scope::SharedRuntime* runtime){
    using namespace silent_scope;std::lock_guard lock(lifecycle);
    if(!runtime||runtime->abi!=SharedRuntime::Abi||worker.joinable())return false;
    stopping=false;runtime->quit=false;
#ifdef __ANDROID__
    // The caller runs native emulation; the worker below owns XR rendering.
    emulation_thread=uint32_t(gettid());
#endif
    if(const char* file=std::getenv("SILENT_SCOPE_XR_LOG"))logfile.open(file,std::ios::app);
    worker=std::thread([runtime]{
#ifdef __ANDROID__
        JNIEnv* env{};bool attached=java_vm&&java_vm->AttachCurrentThread(&env,nullptr)==JNI_OK;
#endif
        try{
#ifdef __ANDROID__
            if(!attached)throw std::runtime_error("Cannot attach XR thread to Java VM");
#endif
            if(std::getenv("SILENT_SCOPE_SIMULATION"))simulation(*runtime);else{Host host(*runtime);host.init();host.run();}}
        catch(const std::exception& error){log(error.what());runtime->quit=true;}runtime->ready=false;runtime->release_inputs();
#ifdef __ANDROID__
        if(attached)java_vm->DetachCurrentThread();
#endif
    });return true;
}
SS_EXPORT void silent_scope_xr_stop(){
    using namespace silent_scope;std::lock_guard lock(lifecycle);stopping=true;if(worker.joinable())worker.join();logfile.close();
#ifdef __ANDROID__
    JNIEnv* env{};if(java_vm&&android_activity&&java_vm->GetEnv(reinterpret_cast<void**>(&env),JNI_VERSION_1_6)==JNI_OK)env->DeleteGlobalRef(android_activity);android_activity=nullptr;
#endif
}
#ifdef __ANDROID__
SS_EXPORT void silent_scope_xr_android_init(void* vm,void* activity){
    using namespace silent_scope;java_vm=static_cast<JavaVM*>(vm);JNIEnv* env{};
    if(java_vm&&java_vm->GetEnv(reinterpret_cast<void**>(&env),JNI_VERSION_1_6)==JNI_OK){
        if(android_activity)env->DeleteGlobalRef(android_activity);android_activity=env->NewGlobalRef(static_cast<jobject>(activity));
        auto activity_class=env->GetObjectClass(android_activity);
        auto get_assets=env->GetMethodID(activity_class,"getAssets","()Landroid/content/res/AssetManager;");
        if(get_assets){auto assets=env->CallObjectMethod(android_activity,get_assets);if(assets){resource::set_android_assets(AAssetManager_fromJava(env,assets));env->DeleteLocalRef(assets);}}
        env->DeleteLocalRef(activity_class);
    }
}
#endif
