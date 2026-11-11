param([switch]$Offline,[switch]$EngineBuilt,[int]$Jobs=8)
$ErrorActionPreference='Stop'
$scopeProject=Split-Path -Parent $PSScriptRoot
$scopeRoot='D:/VR4ALL-SilentScope-Build'
$scopeSource="$scopeRoot/flycast-source"
$scopeFramework="$scopeRoot/android-framework-001"
$scopeSdk='E:/AndroidSdk'
$scopeNdk="$scopeSdk/ndk/29.0.14206865"
$scopeJava='E:/.gradle/jdks/eclipse_adoptium-21-amd64-windows.2'
$scopeLoader='E:/.gradle/caches/9.4.1/transforms/b10ea37c0e7b9edc8561496d16c70ccd/transformed/openxr_loader_for_android-1.1.43/prefab/modules/openxr_loader/libs/android.arm64-v8a/libopenxr_loader.so'
if(!(Test-Path -LiteralPath "$scopeProject/private/silent-scope.keystore")){throw 'Preserved Silent Scope signing identity is required'}
if(!$EngineBuilt){& "$PSScriptRoot/build-flycast-quest-engine.ps1" -Jobs $Jobs}
if(!(Test-Path -LiteralPath "$scopeFramework/framework-manifest.json")){throw 'Prepare the audited Java framework first'}
$scopeLibs="$scopeRoot/jniLibs-dreamcast/arm64-v8a"
New-Item -ItemType Directory -Path $scopeLibs -Force | Out-Null
Copy-Item -LiteralPath "$scopeRoot/flycast-quest/libflycast.so",$scopeLoader,"$scopeNdk/toolchains/llvm/prebuilt/windows-x86_64/sysroot/usr/lib/aarch64-linux-android/libc++_shared.so" -Destination $scopeLibs -Force
$scopeOldJava=$env:JAVA_HOME;$scopeOldSdk=$env:ANDROID_HOME
try {
    $env:JAVA_HOME=$scopeJava;$env:ANDROID_HOME=$scopeSdk
    $scopeArgs=@('-p',"$scopeProject/android-dreamcast",':app:assembleRelease','--no-daemon','--console=plain',"-Pvr4allBuildRoot=$scopeRoot/gradle-dreamcast","-Pvr4allJniLibs=$scopeRoot/jniLibs-dreamcast","-Pvr4allEngineSource=$scopeSource","-Pvr4allJavaFramework=$scopeFramework",'-Dorg.gradle.workers.max=2')
    if($Offline){$scopeArgs+='--offline'}
    & "$scopeProject/android/gradlew.bat" @scopeArgs
    if($LASTEXITCODE){throw 'Dreamcast APK assembly failed'}
    $scopeApk="$scopeRoot/gradle-dreamcast/app/outputs/apk/release/app-release.apk"
    & "$scopeSdk/build-tools/36.0.0/apksigner.bat" verify --verbose --print-certs $scopeApk
    if($LASTEXITCODE){throw 'APK signing verification failed'}
    Copy-Item -LiteralPath $scopeApk -Destination "$scopeProject/artifacts/silent-scope-vr-quest-dreamcast-candidate.apk" -Force
} finally {$env:JAVA_HOME=$scopeOldJava;$env:ANDROID_HOME=$scopeOldSdk}
