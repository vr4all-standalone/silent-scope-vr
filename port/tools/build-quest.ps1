param(
    [string]$WorkRoot='D:/VR4ALL-SilentScope-Build',
    [string]$AndroidSdk='E:/AndroidSdk',
    [string]$JavaHome='E:/.gradle/jdks/eclipse_adoptium-21-amd64-windows.2',
    [string]$CMake='E:/AndroidSdk/cmake/3.22.1/bin/cmake.exe',
    [string]$Ninja='C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe',
    [string]$OpenXRLoader='D:/StorageRelief/ege19/2026-10-25/vr4all-workspace-cache/carnevil-port/openxr-android/src/loader/libopenxr_loader.so',
    [switch]$EngineBuilt,
    [switch]$Offline,
    [int]$Jobs=4
)
$ErrorActionPreference='Stop'
$scopeProject=Split-Path -Parent $PSScriptRoot
$scopeNdk=Join-Path $AndroidSdk 'ndk/29.0.14206865'
$scopeHost=Join-Path $WorkRoot 'quest-host'
foreach($scopeTool in @($CMake,$Ninja,$OpenXRLoader,(Join-Path $JavaHome 'bin/java.exe'))){if(!(Test-Path -LiteralPath $scopeTool)){throw "Missing tool: $scopeTool"}}
& $CMake -S $scopeProject -B $scopeHost -G Ninja "-DCMAKE_MAKE_PROGRAM=$Ninja" "-DCMAKE_TOOLCHAIN_FILE=$scopeNdk/build/cmake/android.toolchain.cmake" -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-29 -DANDROID_STL=c++_shared -DCMAKE_BUILD_TYPE=Release -DSILENT_SCOPE_BUILD_GRAPHICS=ON
if($LASTEXITCODE){throw 'Quest host configure failed'}
& $CMake --build $scopeHost --target silent_scope_xr -j $Jobs
if($LASTEXITCODE){throw 'Quest host compile failed'}
if(!$EngineBuilt){& (Join-Path $PSScriptRoot 'build-engine.ps1') -WorkEngine (Join-Path $WorkRoot 'mame-source') -AndroidNdk $scopeNdk -Platform Android -Regenerate -Jobs $Jobs;if($LASTEXITCODE){throw 'Android engine build failed'}}
$scopeLibraries=Join-Path $WorkRoot 'jniLibs/arm64-v8a'
New-Item -ItemType Directory -Path $scopeLibraries -Force | Out-Null
foreach($scopeName in @('libmain.so','libSDL2.so','libc++_shared.so')){Copy-Item -LiteralPath (Join-Path $WorkRoot "mame-source/android-project/app/src/main/libs/arm64-v8a/$scopeName") -Destination (Join-Path $scopeLibraries $scopeName) -Force}
Copy-Item -LiteralPath (Join-Path $scopeHost 'libsilent_scope_xr.so'),$OpenXRLoader -Destination $scopeLibraries -Force
$scopeKey=Join-Path $scopeProject 'private/silent-scope.keystore'
if(!(Test-Path -LiteralPath $scopeKey)){
    & (Join-Path $JavaHome 'bin/keytool.exe') -genkeypair -keystore $scopeKey -storepass android -keypass android -alias silent-scope -keyalg RSA -keysize 3072 -validity 10000 -dname 'CN=Silent Scope VR4ALL Local Development'
    if($LASTEXITCODE){throw 'New Silent Scope signing identity creation failed'}
}
$scopeOldJava=$env:JAVA_HOME;$scopeOldSdk=$env:ANDROID_HOME
try{
    $env:JAVA_HOME=$JavaHome;$env:ANDROID_HOME=$AndroidSdk
    $scopeArgs=@('-p',(Join-Path $scopeProject 'android'),':app:assembleRelease','--no-daemon','--console=plain',"-Pvr4allBuildRoot=$WorkRoot/gradle","-Pvr4allJniLibs=$WorkRoot/jniLibs",'-Dorg.gradle.workers.max=2')
    if($Offline){$scopeArgs+='--offline'}
    & (Join-Path $scopeProject 'android/gradlew.bat') @scopeArgs
    if($LASTEXITCODE){throw 'Silent Scope APK assembly failed'}
    $scopeApk=Join-Path $WorkRoot 'gradle/app/outputs/apk/release/app-release.apk'
    & (Join-Path $AndroidSdk 'build-tools/36.0.0/apksigner.bat') verify --verbose --print-certs $scopeApk
    if($LASTEXITCODE){throw 'APK signature verification failed'}
    $scopeArtifacts=Join-Path $scopeProject 'artifacts';New-Item -ItemType Directory -Path $scopeArtifacts -Force | Out-Null
    Copy-Item -LiteralPath $scopeApk -Destination (Join-Path $scopeArtifacts 'silent-scope-vr-quest-test.apk') -Force
}finally{$env:JAVA_HOME=$scopeOldJava;$env:ANDROID_HOME=$scopeOldSdk}
