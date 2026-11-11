param([int]$Jobs=8)
$ErrorActionPreference='Stop'
if($Jobs -lt 1 -or $Jobs -gt 32){throw 'Jobs must be1..32'}
$scopeSource='D:/VR4ALL-SilentScope-Build/flycast-source'
$scopeBuild='D:/VR4ALL-SilentScope-Build/flycast-android-profile'
$scopeNdk='E:/AndroidSdk/ndk/29.0.14206865'
$scopeCMake='E:/AndroidSdk/cmake/3.22.1/bin/cmake.exe'
$scopeNinja='C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe'
$scopeRevision=git --no-optional-locks -C $scopeSource rev-parse HEAD
if($LASTEXITCODE -or $scopeRevision -ne '8398fead532ede9d52c37addef3353611b6c5ce7'){throw 'Prototype differs from audited engine base'}
& $scopeCMake -S $scopeSource -B $scopeBuild -G Ninja "-DCMAKE_MAKE_PROGRAM=$scopeNinja" "-DCMAKE_TOOLCHAIN_FILE=$scopeNdk/build/cmake/android.toolchain.cmake" -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-29 -DANDROID_STL=c++_static -DCMAKE_BUILD_TYPE=Release -DLIBRETRO=ON -DSILENT_SCOPE_LIBRETRO_NONXR_PROFILE=ON -DBUILD_TESTING=OFF -DENABLE_CTEST=OFF -DUSE_OPENXR=OFF -DUSE_OPENGL=ON -DUSE_GLES=ON -DUSE_GLES2=OFF -DUSE_VULKAN=OFF -DUSE_DX9=OFF -DUSE_DX11=OFF -DUSE_HOST_SDL=OFF -DUSE_HOST_LIBZIP=OFF -DUSE_HOST_LIBCHDR=OFF -DUSE_LUA=OFF -DUSE_DISCORD=OFF -DUSE_BREAKPAD=OFF -DENABLE_LOG=OFF -DFETCHCONTENT_FULLY_DISCONNECTED=ON
if($LASTEXITCODE){throw 'Native ARM64 profile configure failed'}
& $scopeCMake --build $scopeBuild --target flycast_libretro --parallel $Jobs
if($LASTEXITCODE){throw 'Native ARM64 profile build failed'}
Get-ChildItem -LiteralPath $scopeBuild -Filter '*libretro*.so' | Select-Object FullName,Length
