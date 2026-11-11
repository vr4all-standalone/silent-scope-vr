param([int]$Jobs=8,[switch]$ConfigureOnly,[switch]$VerboseLogging)
$ErrorActionPreference='Stop'
if($Jobs -lt 1 -or $Jobs -gt 32){throw 'Jobs must be1..32'}
$scopeSource='D:/VR4ALL-SilentScope-Build/flycast-source'
$scopeBuild='D:/VR4ALL-SilentScope-Build/flycast-quest'
$scopeNdk='E:/AndroidSdk/ndk/29.0.14206865'
$scopeCMake='E:/AndroidSdk/cmake/3.22.1/bin/cmake.exe'
$scopeNinja='C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe'
$scopeOpenXR='E:/.gradle/caches/9.4.1/transforms/b10ea37c0e7b9edc8561496d16c70ccd/transformed/openxr_loader_for_android-1.1.43/prefab/modules/openxr_loader'
$scopeRevision=git --no-optional-locks -C $scopeSource rev-parse HEAD
if($LASTEXITCODE -or $scopeRevision -ne '8398fead532ede9d52c37addef3353611b6c5ce7'){throw 'Engine differs from audited base'}
if(!(Test-Path -LiteralPath "$scopeOpenXR/../headers/include/openxr/openxr.h")){throw 'Missing cached audited OpenXR loader package'}
# ENABLE_LOG adds DEBUGFAST even in Release and enables register-level traces.
# Keep production Notice/Error/Warning (including bounded SS_PERF/SS_SCOPE),
# with full diagnostic logging only through an explicit developer switch.
$scopeLogMode=if($VerboseLogging){'ON'}else{'OFF'}
& $scopeCMake -S $scopeSource -B $scopeBuild -G Ninja "-DCMAKE_MAKE_PROGRAM=$scopeNinja" "-DCMAKE_TOOLCHAIN_FILE=$scopeNdk/build/cmake/android.toolchain.cmake" "-DOpenXR_DIR=$PSScriptRoot/cmake" "-DSS_OPENXR_MODULE=$scopeOpenXR" -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-29 -DANDROID_STL=c++_shared -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DLIBRETRO=OFF -DBUILD_TESTING=OFF -DENABLE_CTEST=OFF -DUSE_OPENXR=ON -DUSE_OPENGL=ON -DUSE_GLES=ON -DUSE_GLES2=OFF -DUSE_VULKAN=OFF -DUSE_DX9=OFF -DUSE_DX11=OFF -DUSE_OPENMP=OFF -DUSE_HOST_SDL=OFF -DUSE_HOST_LIBZIP=OFF -DUSE_HOST_LIBCHDR=OFF -DUSE_LUA=OFF -DUSE_DISCORD=OFF -DUSE_BREAKPAD=OFF "-DENABLE_LOG=$scopeLogMode" -DFETCHCONTENT_FULLY_DISCONNECTED=ON
if($LASTEXITCODE){throw 'Quest engine configure failed'}
if($ConfigureOnly){return}
& $scopeCMake --build $scopeBuild --target flycast --parallel $Jobs
if($LASTEXITCODE){throw 'Quest engine build failed'}
Get-ChildItem -LiteralPath $scopeBuild -Filter '*.so' | Select-Object FullName,Length
