// SPDX-License-Identifier: GPL-2.0-or-later
package com.vrlightgunhub.silentscope;
import java.io.*;
import java.nio.file.*;
import java.security.*;
import java.util.*;
import java.util.zip.*;

/** Validate owned media and prepare private filenames/operator defaults without changing the ZIP. */
public final class GameFiles {
    private static final String[] NAMES={"830d01.27p","830a08.7s","830a14.32u","830a13.24u","830a09.16p","830a10.14p"};
    private static final int[] SIZES={2097152,524288,4194304,4194304,4194304,4194304};
    private static final long[] CRC={0xde9b3dfaL,0x2805ea1dL,0x335793e1L,0xd6e7877eL,0xe4b9f305L,0x8b8aaf7eL};
    private static final String[] SHA={"660652a5f745cb04687481c3626d8a43cd169193","2556a51ee98cb8f59bf081e916c69a24532196f1","d582b53c3853abd59bc728f619a30c27cfc9497c","b4d0e17ada7dd126ec564a20e7140775b4b3fdb7","ce2c6f63bdc9374dde48d8359102b57e48b4fdeb","49b694dc171c149056b87c15410a6bf37ff2987f"};
    private static final String EXACT="ae2f5477e3999ecce5199fc4a53c5ddf78c4406d",LEGACY="91b10802791b68a8360c0cd6c376c0c4bbbc6fa0";
    private static String hex(byte[] bytes){StringBuilder out=new StringBuilder();for(byte value:bytes){out.append(Character.forDigit((value>>4)&15,16));out.append(Character.forDigit(value&15,16));}return out.toString();}
    private static String digest(byte[] data)throws IOException{try{return hex(MessageDigest.getInstance("SHA-1").digest(data));}catch(NoSuchAlgorithmException error){throw new IOException(error);}}
    private static List<ZipEntry> entries(ZipFile zip)throws IOException{
        ArrayList<ZipEntry> result=new ArrayList<>();HashSet<String> names=new HashSet<>();long total=0;
        Enumeration<? extends ZipEntry> iterator=zip.entries();
        while(iterator.hasMoreElements()){
            ZipEntry entry=iterator.nextElement();if(entry.isDirectory())continue;
            if(!names.add(entry.getName().toLowerCase(Locale.ROOT)))throw new IOException("Duplicate ZIP member: "+entry.getName());
            total+=entry.getSize();if(entry.getSize()<0||total>256L*1024*1024||result.size()>=4096)throw new IOException("Archive exceeds supported game-set limits");
            result.add(entry);
        }
        return result;
    }
    private static byte[] read(ZipFile zip,ZipEntry entry,int size)throws IOException{
        byte[] data=new byte[size];try(InputStream input=zip.getInputStream(entry)){
            int offset=0,count;while(offset<size&&(count=input.read(data,offset,size-offset))!=-1)offset+=count;
            if(offset!=size||input.read()!=-1)throw new IOException("Truncated or oversized ZIP member: "+entry.getName());
        }return data;
    }
    private static byte[] find(ZipFile zip,List<ZipEntry> entries,int size,long crc,String sha)throws IOException{
        for(ZipEntry entry:entries)if(entry.getSize()==size&&entry.getCrc()==crc){byte[] data=read(zip,entry,size);if(digest(data).equals(sha))return data;}
        return null;
    }
    private static byte[] timekeeper(ZipFile zip,List<ZipEntry> entries)throws IOException{
        byte[] data=find(zip,entries,8192,0x458900fbL,EXACT);
        if(data==null)data=find(zip,entries,8192,0xee815325L,LEGACY);
        if(data==null)throw new IOException("Missing supported US timekeeper data. Use the complete sscope US UAD1.33 set.");
        return data;
    }
    public static void validate(File archive)throws IOException{
        try(ZipFile zip=new ZipFile(archive)){
            List<ZipEntry> entries=entries(zip);
            for(int i=0;i<NAMES.length;i++)if(find(zip,entries,SIZES[i],CRC[i],SHA[i])==null)throw new IOException("Missing or invalid game ROM: "+NAMES[i]);
            timekeeper(zip,entries);
        }
    }
    public static void prepare(File archive,File root)throws IOException{
        // Every byte is checked before writing private output. No archive member path is extracted.
        validate(archive);
        File roms=new File(root,"roms"),nvramFolder=new File(root,"nvram/sscope");
        if(!roms.isDirectory()&&!roms.mkdirs())throw new IOException("Cannot create private game folder");
        if(!nvramFolder.isDirectory()&&!nvramFolder.mkdirs())throw new IOException("Cannot create native saved-state folder");
        File savedState=new File(nvramFolder,"m48t58");
        if(savedState.exists()&&savedState.length()!=8192)throw new IOException("Existing native saved state is incomplete; it has been preserved for recovery");
        File temporary=File.createTempFile("sscope-",".zip",roms);byte[] seed;
        try{
            try(ZipFile zip=new ZipFile(archive);ZipOutputStream output=new ZipOutputStream(new FileOutputStream(temporary))){
                List<ZipEntry> entries=entries(zip);
                for(int i=0;i<NAMES.length;i++){output.putNextEntry(new ZipEntry(NAMES[i]));output.write(find(zip,entries,SIZES[i],CRC[i],SHA[i]));output.closeEntry();}
                seed=timekeeper(zip,entries);output.putNextEntry(new ZipEntry("m48t58y-70pc1_ua"));output.write(seed);output.closeEntry();
            }
            Files.move(temporary.toPath(),new File(roms,"sscope.zip").toPath(),StandardCopyOption.REPLACE_EXISTING,StandardCopyOption.ATOMIC_MOVE);
        }finally{if(temporary.exists())temporary.delete();}
        if(digest(seed).equals(LEGACY))InitialSettings.apply(seed);
        if(!savedState.exists()){
            File staged=File.createTempFile("scope-init-",".tmp",nvramFolder);
            try{
                try(FileOutputStream output=new FileOutputStream(staged)){output.write(seed);output.getFD().sync();}
                // Same-directory hard-link installation is atomic and cannot replace an existing file.
                try{Files.createLink(savedState.toPath(),staged.toPath());}
                catch(FileAlreadyExistsException preserved){/* Another preparation completed first. */}
            }finally{if(staged.exists())staged.delete();}
        }
    }
}
