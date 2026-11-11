// SPDX-License-Identifier: GPL-2.0-or-later
package com.vrlightgunhub.silentscope;
import org.libsdl.app.SDLActivity;
import android.content.Intent;
import java.io.File;
public final class GameActivity extends SDLActivity {
    @Override protected void onNativeMainReturned(){
        Intent setup=new Intent(Intent.ACTION_MAIN).setClass(this,SetupActivity.class).putExtra("returned",true);
        setup.addCategory("com.oculus.intent.category.2D");setup.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK|Intent.FLAG_ACTIVITY_CLEAR_TASK);
        startActivity(setup);finishAndRemoveTask();
    }
    @Override protected String[] getLibraries(){return new String[]{"SDL2","main"};}
    @Override protected String[] getArguments(){
        String root=getFilesDir().getAbsolutePath();
        nativeSetenv("SILENT_SCOPE_VR","1");
        nativeSetenv("SILENT_SCOPE_CPU_PROFILE",getIntent().getBooleanExtra("profile",false)?"1":"0");
        // The main DSP stays interpreted; only the independently verified
        // scope DSP uses recompilation. Diagnostic launches can disable it.
        nativeSetenv("SILENT_SCOPE_SHARC_DRC",getIntent().getBooleanExtra("scopeDrc",true)?"scope":"0");
        nativeSetenv("SILENT_SCOPE_XR_LIBRARY",getApplicationInfo().nativeLibraryDir+"/libsilent_scope_xr.so");
        nativeSetenv("SILENT_SCOPE_SETTINGS",root+"/settings.ini");
        File logs=getExternalFilesDir(null);
        nativeSetenv("SILENT_SCOPE_XR_LOG",new File(logs!=null?logs:getFilesDir(),"silent-scope-xr.log").getAbsolutePath());
        return new String[]{"sscope","-rompath",root+"/roms",
            "-nvram_directory",root+"/nvram","-cfg_directory",root+"/cfg","-diff_directory",root+"/diff","-state_directory",root+"/state","-snapshot_directory",root+"/snap",
            "-video","none","-sound","sdl","-skip_gameinfo","-noreadconfig","-nowriteconfig","-nomouse","-numprocessors","4","-throttle"};
    }
}
