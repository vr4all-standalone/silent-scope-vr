// SPDX-License-Identifier: GPL-2.0-or-later
package com.flycast.emulator;
import android.content.Context;
import android.preference.PreferenceManager;
import java.io.*;
import java.nio.file.*;
import java.nio.charset.StandardCharsets;
final class SilentScopeConfig {
    static void prepare(Context context)throws IOException{
        File home=SilentScopeHome.prepare(context.getFilesDir());
        if(!PreferenceManager.getDefaultSharedPreferences(context).edit().putString("home_directory",home.getAbsolutePath()).commit())throw new IOException("Could not save console home preference");
    }
    private SilentScopeConfig(){}
}
