param([ValidatePattern('^arm64-camera185-[0-9]{3}$')][string]$Run='arm64-camera185-001')
$ErrorActionPreference='Stop'
$scopeAdb='E:/AndroidSdk/platform-tools/adb.exe'
$scopeSerial='340YC10GB00YWB'
$scopeProject=Split-Path -Parent $PSScriptRoot
$scopeRoot='D:/VR4ALL-SilentScope-Build/diagnostics'
$scopeHost=Join-Path $scopeRoot $Run
$scopeDevice="/data/local/tmp/vr4all-silent-scope-dc-profile/$Run"
$scopeCue='/sdcard/Android/data/com.vrlightgunhub.silentscope/files/games/dreamcast/Silent Scope (USA).cue'
if(Test-Path -LiteralPath $scopeHost){throw 'Diagnostic run already exists; preserve its receipts'}
New-Item -ItemType Directory -Path $scopeHost|Out-Null
$scopeCore=Join-Path $scopeHost 'flycast_libretro.so'
$scopeFrontend=Join-Path $scopeProject 'private/flycast-research/flycast_libretro_profile'
Copy-Item -LiteralPath 'D:/VR4ALL-SilentScope-Build/flycast-android-profile/flycast_libretro.so' -Destination $scopeCore
& 'E:/AndroidSdk/ndk/29.0.14206865/toolchains/llvm/prebuilt/windows-x86_64/bin/llvm-strip.exe' --strip-unneeded $scopeCore
if($LASTEXITCODE){throw 'Private profile core stripping failed'}
$scopeHashes=@{core=(Get-FileHash -LiteralPath $scopeCore -Algorithm SHA256).Hash.ToLowerInvariant();frontend=(Get-FileHash -LiteralPath $scopeFrontend -Algorithm SHA256).Hash.ToLowerInvariant()}
& $scopeAdb -s $scopeSerial shell "test ! -e '$scopeDevice' && test -r '$scopeCue' && mkdir -m 700 '$scopeDevice' '$scopeDevice/probes'"
if($LASTEXITCODE){throw 'Quest output/media preflight failed'}
& $scopeAdb -s $scopeSerial push $scopeCore "$scopeDevice/flycast_libretro.so"
if($LASTEXITCODE){throw 'Core transfer failed'}
& $scopeAdb -s $scopeSerial push $scopeFrontend "$scopeDevice/flycast_libretro_profile"
if($LASTEXITCODE){throw 'Frontend transfer failed'}
$scopeRemoteHashes=& $scopeAdb -s $scopeSerial shell "sha256sum '$scopeDevice/flycast_libretro.so' '$scopeDevice/flycast_libretro_profile'"
if($LASTEXITCODE -or !$scopeRemoteHashes[0].StartsWith($scopeHashes.core) -or !$scopeRemoteHashes[1].StartsWith($scopeHashes.frontend)){throw 'Quest binary hash verification failed'}
$scopeScript=@"
#!/system/bin/sh
set -eu
chmod 700 '$scopeDevice/flycast_libretro_profile'
export SILENT_SCOPE_DC_CAMERA_WIDEN_PROBE=3
export SILENT_SCOPE_DC_OWNERSHIP_PROBE_DIR='$scopeDevice/probes'
export SILENT_SCOPE_DC_CAMERA_PROBE_DIR='$scopeDevice/probes'
export SILENT_SCOPE_DC_CAMERA_GEOMETRY_DIR='$scopeDevice/probes'
unset SILENT_SCOPE_DC_PROJECTOR_PROBE_DIR
exec timeout 100 '$scopeDevice/flycast_libretro_profile' '$scopeDevice/flycast_libretro.so' '$scopeCue' '$scopeDevice' 42 16 90 100000 --snapshots
"@
$scopeScriptPath=Join-Path $scopeHost 'run.sh'
[IO.File]::WriteAllText($scopeScriptPath,($scopeScript -replace "`r`n","`n")+"`n",[Text.Encoding]::ASCII)
& $scopeAdb -s $scopeSerial push $scopeScriptPath "$scopeDevice/run.sh"
if($LASTEXITCODE){throw 'Probe script transfer failed'}
$scopeBegin=[DateTime]::UtcNow
& $scopeAdb -s $scopeSerial shell "sh '$scopeDevice/run.sh'" *> (Join-Path $scopeHost 'shell-output.txt')
$scopeExit=$LASTEXITCODE
# Pull completed and failed diagnostics alike. No device file is deleted here.
& $scopeAdb -s $scopeSerial pull "$scopeDevice/." $scopeHost
if($LASTEXITCODE){throw 'Diagnostic receipt transfer failed'}
@{run=$Run;device=$scopeSerial;beginUtc=$scopeBegin.ToString('o');endUtc=[DateTime]::UtcNow.ToString('o');exitCode=$scopeExit;hashes=$scopeHashes;xr=$false;physicalPlay=$false;warmupAudioSeconds=42;measurementAudioSeconds=16;note='Offscreen EGL native camera probe; not XR FPS. Actual SH4 timestamps govern camera windows.'}|ConvertTo-Json -Depth 4|Set-Content -LiteralPath (Join-Path $scopeHost 'host-receipt.json')
Get-Content -LiteralPath (Join-Path $scopeHost 'shell-output.txt') -Tail 8
if($scopeExit){throw "Quest native camera probe failed: $scopeExit"}
Write-Output $scopeHost
