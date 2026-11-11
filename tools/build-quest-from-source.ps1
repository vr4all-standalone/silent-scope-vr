param(
    [Parameter(Mandatory=$true)][string]$AndroidSdk,
    [Parameter(Mandatory=$true)][string]$JavaHome,
    [string]$BuildDirectory,
    [int]$Jobs=6,
    [switch]$EngineOnly,
    [switch]$Release,
    [switch]$Offline
)
$ErrorActionPreference='Stop'
$sourceRoot=Split-Path -Parent $PSScriptRoot
$engine=Join-Path $sourceRoot 'flycast'
$port=Join-Path $sourceRoot 'port'
if(!(Test-Path "$engine/CMakeLists.txt") -or !(Test-Path "$port/VERSION")){
    throw 'Extract the complete release source ZIP and run its root tools/build-quest-from-source.ps1'
}
if(!$BuildDirectory){$BuildDirectory=Join-Path $sourceRoot 'build'}
$build=[IO.Path]::GetFullPath($BuildDirectory)
$sdk=[IO.Path]::GetFullPath($AndroidSdk)
$env:JAVA_HOME=[IO.Path]::GetFullPath($JavaHome)
$env:ANDROID_HOME=$sdk
$ndk=Join-Path $sdk 'ndk/29.0.14206865'
$cmake=Join-Path $sdk 'cmake/3.22.1/bin/cmake.exe'
$ninja=Join-Path $sdk 'cmake/3.22.1/bin/ninja.exe'
foreach($tool in @($cmake,$ninja,"$ndk/build/cmake/android.toolchain.cmake","$env:JAVA_HOME/bin/java.exe")){
    if(!(Test-Path -LiteralPath $tool)){throw "Missing prerequisite $tool"}
}
$aar=Join-Path $sourceRoot 'dependencies/openxr_loader_for_android-1.1.43.aar'
$openxr=Join-Path $build 'openxr'
if(!(Test-Path "$openxr/prefab/modules/openxr_loader/libs/android.arm64-v8a/libopenxr_loader.so")){
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    [IO.Directory]::CreateDirectory($openxr)|Out-Null
    [IO.Compression.ZipFile]::ExtractToDirectory($aar,$openxr)
}
$module=Join-Path $openxr 'prefab/modules/openxr_loader'
$native=Join-Path $build 'flycast'
& $cmake -S $engine -B $native -G Ninja "-DCMAKE_MAKE_PROGRAM=$ninja" "-DCMAKE_TOOLCHAIN_FILE=$ndk/build/cmake/android.toolchain.cmake" "-DOpenXR_DIR=$port/tools/cmake" "-DSS_OPENXR_MODULE=$module" -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-29 -DANDROID_STL=c++_shared -DCMAKE_BUILD_TYPE=Release -DLIBRETRO=OFF -DBUILD_TESTING=OFF -DENABLE_CTEST=OFF -DUSE_OPENXR=ON -DUSE_OPENGL=ON -DUSE_GLES=ON -DUSE_GLES2=OFF -DUSE_VULKAN=OFF -DUSE_DX9=OFF -DUSE_DX11=OFF -DUSE_OPENMP=OFF -DUSE_HOST_SDL=OFF -DUSE_HOST_LIBZIP=OFF -DUSE_HOST_LIBCHDR=OFF -DUSE_LUA=OFF -DUSE_DISCORD=OFF -DUSE_BREAKPAD=OFF -DENABLE_LOG=OFF -DFETCHCONTENT_FULLY_DISCONNECTED=ON
if($LASTEXITCODE){throw 'Quest engine configuration failed'}
& $cmake --build $native --target flycast --parallel $Jobs
if($LASTEXITCODE){throw 'Quest engine build failed'}
if($EngineOnly){Write-Output "$native/libflycast.so";exit 0}
$libs=Join-Path $build 'jniLibs/arm64-v8a'
[IO.Directory]::CreateDirectory($libs)|Out-Null
Copy-Item "$native/libflycast.so","$module/libs/android.arm64-v8a/libopenxr_loader.so","$ndk/toolchains/llvm/prebuilt/windows-x86_64/sysroot/usr/lib/aarch64-linux-android/libc++_shared.so" -Destination $libs -Force
$task=if($Release){':app:assembleRelease'}else{':app:assembleDebug'}
$argsList=@('-p',"$port/android-dreamcast",$task,'--no-daemon','--console=plain',
    "-Pvr4allBuildRoot=$build/gradle","-Pvr4allJniLibs=$build/jniLibs",
    "-Pvr4allEngineSource=$engine","-Pvr4allJavaFramework=$sourceRoot/android-framework",
    '-Dorg.gradle.workers.max=2')
if($Offline){$argsList+='--offline'}
& "$port/android/gradlew.bat" @argsList
if($LASTEXITCODE){throw 'APK assembly failed'}
Write-Output "$build/gradle/app/outputs/apk"
