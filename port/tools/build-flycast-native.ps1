param([int]$Jobs=8,[switch]$ConfigureOnly)
$ErrorActionPreference='Stop'
if($Jobs -lt 1 -or $Jobs -gt 32){throw 'Jobs must be1..32'}
$scopeSource='D:/VR4ALL-SilentScope-Build/flycast-source'
$scopeBuild='D:/VR4ALL-SilentScope-Build/flycast-native'
$scopeCMake='E:/AndroidSdk/cmake/3.22.1/bin/cmake.exe'
$scopeNinja='C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe'
$scopeVs='C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/Common7/Tools/VsDevCmd.bat'
foreach($scopeTool in @($scopeCMake,$scopeNinja,$scopeVs)){if(!(Test-Path -LiteralPath $scopeTool -PathType Leaf)){throw "Missing native compiler tool: $scopeTool"}}
$scopeRevision=git --no-optional-locks -C $scopeSource rev-parse HEAD
if($LASTEXITCODE -or $scopeRevision -ne '8398fead532ede9d52c37addef3353611b6c5ce7'){throw 'Prototype differs from audited released engine base'}
New-Item -ItemType Directory -Path $scopeBuild -Force | Out-Null
$scopeCommand='D:/VR4ALL-SilentScope-Build/build-flycast-native.cmd'
$scopeLines=@('@echo off',"call `"$scopeVs`" -no_logo -arch=x64 -host_arch=x64",'if errorlevel 1 exit /b %errorlevel%',
"`"$scopeCMake`" -S `"$scopeSource`" -B `"$scopeBuild`" -G Ninja `"-DCMAKE_MAKE_PROGRAM=$scopeNinja`" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF -DENABLE_CTEST=OFF -DLIBRETRO=OFF -DUSE_OPENXR=OFF -DUSE_OPENGL=ON -DUSE_GLES=OFF -DUSE_GLES2=OFF -DUSE_VULKAN=OFF -DUSE_DX9=OFF -DUSE_DX11=OFF -DUSE_HOST_SDL=OFF -DUSE_HOST_LIBZIP=OFF -DUSE_DISCORD=OFF -DUSE_LUA=OFF -DUSE_BREAKPAD=OFF -DUSE_OPENMP=OFF -DENABLE_LOG=ON -DFETCHCONTENT_FULLY_DISCONNECTED=ON `"-DCMAKE_INSTALL_PREFIX=$scopeBuild/private-install`"",'if errorlevel 1 exit /b %errorlevel%')
if(!$ConfigureOnly){$scopeLines+=@("`"$scopeCMake`" --build `"$scopeBuild`" --target flycast --parallel $Jobs",'if errorlevel 1 exit /b %errorlevel%')}
$scopeLines+='exit /b 0'
[IO.File]::WriteAllLines($scopeCommand,$scopeLines,[Text.Encoding]::ASCII)
& $env:COMSPEC /d /c $scopeCommand
if($LASTEXITCODE){throw "Native console prototype build failed: $LASTEXITCODE"}
if(!$ConfigureOnly){Write-Output (Join-Path $scopeBuild 'flycast.exe')}
