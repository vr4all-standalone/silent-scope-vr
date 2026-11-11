import com.vrlightgunhub.silentscope.GameFiles;
import java.nio.file.*;
import java.security.*;
import java.util.*;
import java.util.zip.*;
public class GameFilesHostTest {
    private static void require(boolean value,String message){if(!value)throw new AssertionError(message);}
    public static void main(String[] args)throws Exception {
        Path original=Path.of(args[0]),root=Path.of(args[1]);Files.createDirectories(root);
        byte[] before=MessageDigest.getInstance("SHA-256").digest(Files.readAllBytes(original));
        GameFiles.validate(original.toFile());GameFiles.prepare(original.toFile(),root.toFile());
        Path nvram=root.resolve("nvram/sscope/m48t58"),canonical=root.resolve("roms/sscope.zip");
        require(Files.size(nvram)==8192,"Private initial operator configuration missing");
        StringBuilder prefixHash=new StringBuilder();for(byte value:MessageDigest.getInstance("SHA-1").digest(Arrays.copyOf(Files.readAllBytes(nvram),8184)))prefixHash.append(String.format("%02x",value&255));
        require(prefixHash.toString().equals("404150cba502012005cd18f312ae3df41a9966d6"),"First-run settings differ from the native-calibrated configuration");
        try(ZipFile zip=new ZipFile(canonical.toFile())){
            require(zip.size()==7,"Canonical archive must contain exactly the required seven entries");
            require(zip.getEntry("830a13.24u")!=null,"Owned source CRC alias was not normalized");
            require(zip.getEntry("m48t58y-70pc1_ua")!=null,"Missing private legacy timekeeper alias");
        }
        byte[] custom=Files.readAllBytes(nvram);custom[500]=(byte)0x71;Files.write(nvram,custom);
        GameFiles.prepare(original.toFile(),root.toFile());
        require(Arrays.equals(custom,Files.readAllBytes(nvram)),"Existing native preferences were overwritten");
        require(Arrays.equals(before,MessageDigest.getInstance("SHA-256").digest(Files.readAllBytes(original))),"Original archive changed");
        Path bad=root.resolve("bad.zip");try(ZipOutputStream zip=new ZipOutputStream(Files.newOutputStream(bad))){zip.putNextEntry(new ZipEntry("830d01.27p"));zip.write(new byte[20]);zip.closeEntry();}
        boolean rejected=false;try{GameFiles.validate(bad.toFile());}catch(java.io.IOException expected){rejected=true;}
        require(rejected,"Corrupt game program accepted");
        Path partial=root.resolve("partial/nvram/sscope/m48t58");Files.createDirectories(partial.getParent());Files.write(partial,new byte[]{1,2,3});
        rejected=false;try{GameFiles.prepare(original.toFile(),root.resolve("partial").toFile());}catch(java.io.IOException expected){rejected=true;}
        require(rejected&&Files.size(partial)==3,"Incomplete existing NVRAM must be preserved and rejected rather than declared ready");
        System.out.println("Owned media normalization, original preservation, first-run config and existing-state preservation passed");
    }
}
