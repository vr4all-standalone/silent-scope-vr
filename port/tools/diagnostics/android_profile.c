// SPDX-License-Identifier: GPL-2.0-or-later
// Standalone diagnostic only. Calls the unmodified SDL_main entry with a dummy
// video driver; overrides the Activity-only working-directory lookup and sends
// MAME's Android log messages to stderr. Does not load the XR host or touch app data.
#include <dlfcn.h>
#include <stdio.h>
const char* SDL_AndroidGetExternalStoragePath(void){return ".";}
int __android_log_write(int priority,const char* tag,const char* message){fprintf(stderr,"%s: %s\n",tag,message);return 0;}
int main(int argc,char** argv){void* module=dlopen("libmain.so",RTLD_NOW|RTLD_GLOBAL);if(!module){fprintf(stderr,"dlopen: %s\n",dlerror());return 1;}int(*entry)(int,char**)=(int(*)(int,char**))dlsym(module,"SDL_main");if(!entry){fprintf(stderr,"SDL_main: %s\n",dlerror());return 1;}void(*ready)(void)=(void(*)(void))dlsym(module,"SDL_SetMainReady");if(ready)ready();return entry(argc,argv);}
