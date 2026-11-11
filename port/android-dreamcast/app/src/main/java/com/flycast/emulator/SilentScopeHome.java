// SPDX-License-Identifier: GPL-2.0-or-later
package com.flycast.emulator;
import java.io.*;
import java.nio.file.*;
import java.nio.charset.StandardCharsets;
/** File-only console home preparation, shared by setup and direct native entry. */
final class SilentScopeHome {
    static synchronized File prepare(File privateFiles)throws IOException{
        File home=new File(privateFiles,"dreamcast");
        if(Files.isSymbolicLink(home.toPath())||(!home.isDirectory()&&!home.mkdirs()))throw new IOException("Cannot prepare console saved-game folder");
        File config=new File(home,"emu.cfg");
        if(Files.exists(config.toPath(),LinkOption.NOFOLLOW_LINKS)){
            if(Files.isSymbolicLink(config.toPath())||!config.isFile())throw new IOException("Console preferences are not a regular file");
            return home;
        }
        String text="[config]\nUseReios=yes\naica.DSPEnabled=yes\nrend.RenderToTextureBuffer=yes\nrend.NativeDepthInterpolation=yes\nvr.Xr=yes\nvr.Reproject=yes\nvr.DualWield=no\nvr.HandsPrep=no\nvr.GameHands=no\nvr.SlideReload=no\nvr.ShowControllers=yes\nvr.WorldScale=0.1\nvr.XrRefreshRate=72\nvr.ShowGun=yes\nvr.Laser=yes\nvr.Haptics=medium\nvr.GunAngle=0\nvr.GunAngleZeroVersion=1\nvr.ReticleMode=1\nvr.ReticleMinSize=0.25\nvr.ReticleMaxSize=1\nvr.ReticleShrinkDistance=6\nvr.HudSize=1\nDreamcast.AutoLoadState=no\nDreamcast.AutoSaveState=no\n";
        File lockPath=new File(home,".vr4all-config.lock");
        if(Files.isSymbolicLink(lockPath.toPath()))throw new IOException("Invalid console preference lock");
        try(java.nio.channels.FileChannel channel=java.nio.channels.FileChannel.open(lockPath.toPath(),StandardOpenOption.CREATE,StandardOpenOption.WRITE);
            java.nio.channels.FileLock lock=channel.lock()){
            if(!lock.isValid())throw new IOException("Cannot lock console preferences");
            if(Files.exists(config.toPath(),LinkOption.NOFOLLOW_LINKS)){
                if(Files.isSymbolicLink(config.toPath())||!config.isFile())throw new IOException("Console preferences are not a regular file");
                return home;
            }
            File temporary=File.createTempFile(".config-",".tmp",home);
            try{
                try(FileOutputStream output=new FileOutputStream(temporary)){output.write(text.getBytes(StandardCharsets.UTF_8));output.getFD().sync();}
                Files.move(temporary.toPath(),config.toPath(),StandardCopyOption.ATOMIC_MOVE);
            }finally{Files.deleteIfExists(temporary.toPath());}
        }
        return home;
    }
    private SilentScopeHome(){}
}
