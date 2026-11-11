// SPDX-License-Identifier: GPL-2.0-or-later
package com.flycast.emulator;
import java.io.*;
import java.nio.file.*;
import java.security.*;
import java.util.*;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.locks.ReentrantLock;

/** Exact owned US disc layout. No media data or native save is supplied here. */
public final class DreamcastFiles {
    public static final String[] NAMES={"Silent Scope (USA).cue","Silent Scope (USA) (Track 1).bin","Silent Scope (USA) (Track 2).bin","Silent Scope (USA) (Track 3).bin"};
    private static final long[] SIZES={348,1058400,1589952,1185760800};
    private static final String[] HASH={"6e3b5cf89c4cf244900d59fc270de6578794aab1d0b570d11e0f2faa54d9e68b","411b78355cdd9be410b7452df0dd4bc45da2e282393f32f94529d4f2312f3823","c021c659506fd32bd6736f5dcc3fba27450e23a18e64d38aae314ad9c3e9cfdc","171bb5b26365ded8f668b45ece7e1e75ff66571e489c94b3825605fafa507134"};
    private static final String RECEIPT=".vr4all-t9507n-verified";
    private static final ReentrantLock OPERATIONS=new ReentrantLock();
    private static final AtomicBoolean IMPORT_ACTIVE=new AtomicBoolean();
    public static int index(String name){for(int i=0;i<NAMES.length;i++)if(NAMES[i].equals(name))return i;return -1;}
    public static long expectedSize(String name)throws IOException{int i=index(name);if(i<0)throw new IOException("Choose only the four Silent Scope US CUE/BIN files");return SIZES[i];}
    private static String hex(byte[] bytes){StringBuilder b=new StringBuilder();for(byte v:bytes)b.append(String.format(Locale.ROOT,"%02x",v&255));return b.toString();}
    private static String fingerprint(File root)throws IOException{
        if(Files.isSymbolicLink(root.toPath())||!root.isDirectory())throw new IOException("Invalid private game folder");
        StringBuilder b=new StringBuilder("VR4ALL T9507N US V1.000\n");
        for(int i=0;i<NAMES.length;i++){
            File f=new File(root,NAMES[i]);
            if(Files.isSymbolicLink(f.toPath())||!f.isFile()||f.length()!=SIZES[i]||!f.getCanonicalFile().getParentFile().equals(root.getCanonicalFile()))throw new IOException("Missing or incomplete: "+NAMES[i]);
            b.append(NAMES[i]).append(':').append(f.length()).append(':').append(f.lastModified()).append('\n');
        }
        return b.toString();
    }
    public static File readyCue(File root)throws IOException{
        if(!OPERATIONS.tryLock())throw new IOException("Disc validation or import is still in progress");
        try{
            String now=fingerprint(root);File marker=new File(root,RECEIPT);
            if(Files.isSymbolicLink(marker.toPath())||!marker.isFile()||marker.length()>4096||!new String(Files.readAllBytes(marker.toPath()),java.nio.charset.StandardCharsets.UTF_8).equals(now))throw new IOException("Validate the owned disc from the start screen first");
            return new File(root,NAMES[0]);
        }finally{OPERATIONS.unlock();}
    }
    public static File validate(File root)throws IOException{
        OPERATIONS.lock();try{return validateLocked(root);}finally{OPERATIONS.unlock();}
    }
    private static File validateLocked(File root)throws IOException{
        String before=fingerprint(root);
        // A failed full recheck must not leave an older launch receipt usable.
        Files.deleteIfExists(new File(root,RECEIPT).toPath());
        for(int i=0;i<NAMES.length;i++){
            MessageDigest d;try{d=MessageDigest.getInstance("SHA-256");}catch(NoSuchAlgorithmException e){throw new IOException(e);}
            try(InputStream in=new FileInputStream(new File(root,NAMES[i]))){byte[] buffer=new byte[262144];int n;while((n=in.read(buffer))!=-1)d.update(buffer,0,n);}
            if(!hex(d.digest()).equals(HASH[i]))throw new IOException("Owned disc contents do not match the supported US layout: "+NAMES[i]);
        }
        if(!before.equals(fingerprint(root)))throw new IOException("Game files changed during validation; retry");
        File marker=new File(root,RECEIPT);File temp=File.createTempFile(".verify-",".tmp",root);
        try{
            try(FileOutputStream out=new FileOutputStream(temp)){out.write(before.getBytes(java.nio.charset.StandardCharsets.UTF_8));out.getFD().sync();}
            Files.move(temp.toPath(),marker.toPath(),StandardCopyOption.REPLACE_EXISTING,StandardCopyOption.ATOMIC_MOVE);
        }finally{temp.delete();}
        return readyCue(root);
    }
    public static void copyTrack(InputStream in,File root,String name)throws IOException{
        long size=expectedSize(name);File target=new File(root,name);
        if(!root.isDirectory()||!target.getCanonicalFile().getParentFile().equals(root.getCanonicalFile()))throw new IOException("Invalid private import folder");
        if(Files.isSymbolicLink(root.toPath()))throw new IOException("Invalid private import folder");
        java.nio.channels.FileChannel file=java.nio.channels.FileChannel.open(target.toPath(),StandardOpenOption.CREATE_NEW,StandardOpenOption.WRITE);
        boolean complete=false;
        try(java.nio.channels.FileChannel owned=file;OutputStream out=java.nio.channels.Channels.newOutputStream(owned)){
            byte[] buffer=new byte[262144];long total=0;int n;
            while((n=in.read(buffer))!=-1){total+=n;if(total>size)throw new IOException("Oversized disc track: "+name);out.write(buffer,0,n);}
            if(total!=size)throw new IOException("Incomplete disc track: "+name);
            owned.force(true);complete=true;
        }finally{if(!complete)Files.deleteIfExists(target.toPath());}
    }
    public static boolean importInProgress(){return IMPORT_ACTIVE.get();}
    public static ImportSession beginImport(File appGames)throws IOException{return new ImportSession(appGames);}
    public static final class ImportSession implements AutoCloseable {
        private final File root;private File staging;private boolean closed;
        private ImportSession(File parent)throws IOException{
            if(!IMPORT_ACTIVE.compareAndSet(false,true))throw new IOException("Another disc import is already in progress");
            OPERATIONS.lock();
            try{
                root=parent.getCanonicalFile();
                if(Files.isSymbolicLink(parent.toPath())||!root.isDirectory())throw new IOException("Invalid private games folder");
                staging=Files.createTempDirectory(root.toPath(),"dc-import-").toFile();
            }catch(IOException|RuntimeException failure){OPERATIONS.unlock();IMPORT_ACTIVE.set(false);throw failure;}
        }
        public File stagingFolder(){return staging;}
        public void commit()throws IOException{
            if(closed||staging==null)throw new IOException("Import is no longer active");
            validateLocked(staging);promoteValidatedStage(root,staging);staging=null;
        }
        @Override public void close()throws IOException{
            if(closed)return;closed=true;
            try{if(staging!=null)discardStaging(root,staging);}
            finally{OPERATIONS.unlock();IMPORT_ACTIVE.set(false);}
        }
    }
    // Caller validates the private stage before promotion. Existing owned files
    // are preserved in a unique sibling; only a newly created stage is cleaned.
    static File promoteValidatedStage(File appGames,File stage)throws IOException{
        OPERATIONS.lock();try{
            File root=appGames.getCanonicalFile();verifyStaging(root,stage);File destination=gameFolder(root);
            File preserved=null;
            if(Files.exists(destination.toPath(),LinkOption.NOFOLLOW_LINKS)){
                if(Files.isSymbolicLink(destination.toPath())||!destination.isDirectory())throw new IOException("Existing game folder is not a private directory");
                preserved=new File(root,"dreamcast-preserved-"+UUID.randomUUID());
                Files.move(destination.toPath(),preserved.toPath(),StandardCopyOption.ATOMIC_MOVE);
            }
            try{Files.move(stage.toPath(),destination.toPath(),StandardCopyOption.ATOMIC_MOVE);}
            catch(IOException failure){
                if(preserved!=null)try{Files.move(preserved.toPath(),destination.toPath(),StandardCopyOption.ATOMIC_MOVE);}
                catch(IOException restore){failure.addSuppressed(restore);throw new IOException("Import failed; previous disc remains in "+preserved.getName(),failure);}
                throw failure;
            }
            return preserved;
        }finally{OPERATIONS.unlock();}
    }
    private static void verifyStaging(File root,File stage)throws IOException{
        if(Files.isSymbolicLink(stage.toPath())||!stage.isDirectory()||!stage.getName().startsWith("dc-import-")||!stage.getCanonicalFile().getParentFile().equals(root.getCanonicalFile()))throw new IOException("Invalid private staging directory");
    }
    private static void discardStaging(File root,File stage)throws IOException{
        verifyStaging(root,stage);
        Files.walkFileTree(stage.toPath(),new SimpleFileVisitor<Path>(){
            @Override public FileVisitResult visitFile(Path file,java.nio.file.attribute.BasicFileAttributes attrs)throws IOException{Files.delete(file);return FileVisitResult.CONTINUE;}
            @Override public FileVisitResult postVisitDirectory(Path directory,IOException error)throws IOException{if(error!=null)throw error;Files.delete(directory);return FileVisitResult.CONTINUE;}
        });
    }
    public static File gameFolder(File appGames){return new File(appGames,"dreamcast");}
    private DreamcastFiles(){}
}
