param(
    [string]$WorkEngine='D:/VR4ALL-SilentScope-Build/mame-source',
    [string]$MsysRoot='D:/msys64',
    [string]$PythonExe='D:/Python314/python.exe',
    [int]$Jobs=8,
    [ValidateSet('Windows','Android')][string]$Platform='Windows',
    [string]$AndroidNdk='E:/AndroidSdk/ndk/29.0.14206865',
    [string]$SdlRoot='D:/StorageRelief/ege19/2026-10-25/vr4all-workspace-cache/carnevil-port/sdl-android',
    [switch]$Regenerate
)
$ErrorActionPreference='Stop'
if($Jobs -lt 1 -or $Jobs -gt 32){throw 'Jobs must be 1..32'}
$scopeWork=[IO.Path]::GetFullPath($WorkEngine)
if($scopeWork -match '\s'){throw 'MAME compiler source path must not contain spaces'}
& $PythonExe (Join-Path $PSScriptRoot 'prepare-engine.py') $scopeWork
if($LASTEXITCODE){throw 'Pinned source preparation failed'}
$scopeOldPath=$env:PATH;$scopeOldMingw=$env:MINGW64;$scopeOldNdk=$env:ANDROID_NDK_HOME
try {
    $env:PATH=(Join-Path $MsysRoot 'mingw64/bin')+';'+(Split-Path -Parent $PythonExe)+';'+(Join-Path $MsysRoot 'usr/bin')+';'+$scopeOldPath
    $env:MINGW64=(Join-Path $MsysRoot 'mingw64').Replace('\','/')
    Push-Location -LiteralPath $scopeWork
    try {
        $scopeArgs=@("-j$Jobs",'SUBTARGET=silentscope','SOURCES=src/mame/konami/hornet.cpp','USE_QTDEBUG=0','USE_TAPTUN=0','USE_PCAP=0','NO_USE_PORTAUDIO=1','USE_BGFX=0',"PYTHON_EXECUTABLE=$([IO.Path]::GetFileName($PythonExe))",'NOWERROR=1','SYMBOLS=0','IGNORE_GIT=1')
        if($Regenerate){$scopeArgs+='REGENIE=1'}
        if($Platform -eq 'Android'){
            foreach($scopeDependency in @((Join-Path $AndroidNdk 'build/cmake/android.toolchain.cmake'),(Join-Path $SdlRoot 'lib/libSDL2.so'))){if(!(Test-Path -LiteralPath $scopeDependency)){throw "Missing Android dependency: $scopeDependency"}}
            $env:ANDROID_NDK_HOME=$AndroidNdk.Replace('\','/')
            $scopeArgs+=@('android-arm64','NO_X11=1','NO_USE_XINPUT=1','NO_OPENGL=1','NO_USE_PULSEAUDIO=1','NO_USE_PIPEWIRE=1',"SDL_INSTALL_ROOT=$($SdlRoot.Replace('\','/'))")
        }
        & (Join-Path $MsysRoot 'usr/bin/make.exe') @scopeArgs
        if($LASTEXITCODE){throw "Engine compilation failed: $LASTEXITCODE"}
    } finally {Pop-Location}
} finally {$env:PATH=$scopeOldPath;$env:MINGW64=$scopeOldMingw;$env:ANDROID_NDK_HOME=$scopeOldNdk}
$scopeOutput=Join-Path $scopeWork $(if($Platform -eq 'Android'){'android-project/app/src/main/libs/arm64-v8a/libmain.so'}else{'silentscope.exe'})
if(!(Test-Path -LiteralPath $scopeOutput)){throw 'Compiler produced no expected engine binary'}
Write-Output $scopeOutput
