param(
    [string]$Compiler='D:/msys64/mingw64/bin/g++.exe',
    [string]$CMake='E:/AndroidSdk/cmake/3.22.1/bin/cmake.exe',
    [string]$Ninja='C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe',
    [int]$Jobs=4,
    [string]$Python='D:/Python314/python.exe',
    [string]$FlycastSource='D:/VR4ALL-SilentScope-Build/flycast-source'
)
$ErrorActionPreference='Stop'
$scopeProject=Split-Path -Parent $PSScriptRoot
$scopeBuild=Join-Path $scopeProject 'build/host-tests'
foreach($scopeTool in @($Compiler,$CMake,$Ninja,$Python)){if(!(Test-Path -LiteralPath $scopeTool -PathType Leaf)){throw "Missing build tool: $scopeTool"}}
if($Jobs -lt 1 -or $Jobs -gt 32){throw 'Jobs must be 1..32'}
$scopeOldPath=$env:PATH
try {
    $env:PATH=(Split-Path -Parent $Compiler)+';'+$scopeOldPath
    & $CMake -S $scopeProject -B $scopeBuild -G Ninja "-DCMAKE_CXX_COMPILER=$Compiler" "-DCMAKE_MAKE_PROGRAM=$Ninja" "-DPython3_EXECUTABLE=$Python" -DCMAKE_BUILD_TYPE=Debug -DSILENT_SCOPE_BUILD_GRAPHICS=ON "-DSILENT_SCOPE_FLYCAST_SOURCE=$FlycastSource"
    if($LASTEXITCODE){throw 'Host configuration failed'}
    & $CMake --build $scopeBuild -j $Jobs
    if($LASTEXITCODE){throw 'Host compilation failed'}
    & (Join-Path (Split-Path -Parent $CMake) 'ctest.exe') --test-dir $scopeBuild --output-on-failure
    if($LASTEXITCODE){throw 'Host production-code checks failed'}
} finally {$env:PATH=$scopeOldPath}
