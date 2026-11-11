param([string]$Serial='340YC10GB00YWB',[switch]$RemoveVerifiedBenchmarkCopy)
$ErrorActionPreference='Stop'
$scopeAdb='E:/AndroidSdk/platform-tools/adb.exe'
$scopeLocal='D:/VR4ALL-SilentScope-Build/diagnostics/dreamcast-owned-media-001'
$scopeDestination='/sdcard/Android/data/com.vrlightgunhub.silentscope/files/games/dreamcast'
$scopeBenchmark='/data/local/tmp/vr4all-silent-scope-dc-profile/media'
$scopeFiles=@(
    @('Silent Scope (USA).cue',348,'6e3b5cf89c4cf244900d59fc270de6578794aab1d0b570d11e0f2faa54d9e68b'),
    @('Silent Scope (USA) (Track 1).bin',1058400,'411b78355cdd9be410b7452df0dd4bc45da2e282393f32f94529d4f2312f3823'),
    @('Silent Scope (USA) (Track 2).bin',1589952,'c021c659506fd32bd6736f5dcc3fba27450e23a18e64d38aae314ad9c3e9cfdc'),
    @('Silent Scope (USA) (Track 3).bin',1185760800,'171bb5b26365ded8f668b45ece7e1e75ff66571e489c94b3825605fafa507134')
)
function Device-Hash([string]$scopePath){
    $scopeOutput=& $scopeAdb -s $Serial shell "sha256sum '$scopePath'" 2>&1
    if($LASTEXITCODE -or ($scopeOutput -join '') -notmatch '^([a-fA-F0-9]{64})\s'){throw "Cannot hash device file: $scopePath"}
    return $Matches[1].ToLowerInvariant()
}
foreach($scopeFile in $scopeFiles){
    $scopePath=Join-Path $scopeLocal $scopeFile[0]
    if((Get-Item -LiteralPath $scopePath).Length -ne $scopeFile[1] -or (Get-FileHash -LiteralPath $scopePath).Hash.ToLowerInvariant() -ne $scopeFile[2]){throw 'Owned source manifest mismatch'}
}
# Android's app-private external directories use owner-only app access. Creating
# dreamcast with ADB makes it shell-owned (0770), so shell hashes can pass while
# the app cannot even stat the CUE. Let the app create this directory first.
$scopeUidText=(& $scopeAdb -s $Serial shell pm list packages -U com.vrlightgunhub.silentscope | Out-String).Trim()
if($LASTEXITCODE -or $scopeUidText -notmatch '^package:com\.vrlightgunhub\.silentscope uid:(\d+)$'){throw 'Install the Silent Scope APK before preparing its game files'}
$scopeAppUid=$Matches[1]
$scopeDirectoryUid=(& $scopeAdb -s $Serial shell "stat -c %u '$scopeDestination'" 2>&1 | Out-String).Trim()
if($LASTEXITCODE){throw 'Open the Silent Scope setup screen once so the app creates its own game folder, then retry'}
if($scopeDirectoryUid -ne $scopeAppUid){throw "Game folder is owned by UID$scopeDirectoryUid instead of the app UID$scopeAppUid. Preserve it and let the app create a replacement before transferring media"}
$scopeReceipt=@()
foreach($scopeFile in $scopeFiles){
    $scopePath="$scopeDestination/$($scopeFile[0])"
    & $scopeAdb -s $Serial shell "test -e '$scopePath'"
    if($LASTEXITCODE -ne 0){
        & $scopeAdb -s $Serial push (Join-Path $scopeLocal $scopeFile[0]) $scopePath
        if($LASTEXITCODE){throw "Disc file push failed: $($scopeFile[0])"}
    }
    if((Device-Hash $scopePath) -ne $scopeFile[2]){throw "Existing/copied disc file is invalid; preserved for inspection: $scopePath"}
    $scopeReceipt+=@{name=$scopeFile[0];bytes=$scopeFile[1];sha256=$scopeFile[2];destination=$scopePath}
}
# Only this task's temporary benchmark duplicates may be removed, after every
# app working-copy file and the D: source have passed the complete manifest.
if($RemoveVerifiedBenchmarkCopy){
    foreach($scopeFile in $scopeFiles){
        $scopePath="$scopeBenchmark/$($scopeFile[0])"
        & $scopeAdb -s $Serial shell "test -e '$scopePath'"
        if($LASTEXITCODE -eq 0){
            if((Device-Hash $scopePath) -ne $scopeFile[2]){throw "Unexpected benchmark copy; no deletion: $scopePath"}
            & $scopeAdb -s $Serial shell "rm -- '$scopePath'"
            if($LASTEXITCODE){throw "Cannot remove verified task-owned duplicate: $scopePath"}
        }
    }
}
@{serial=$Serial;appUid=$scopeAppUid;gameDirectoryUid=$scopeDirectoryUid;directoryOwnedByApp=$true;files=$scopeReceipt;benchmarkDuplicatesRemoved=[bool]$RemoveVerifiedBenchmarkCopy;apkInstalled=$false;appLaunched=$false}|ConvertTo-Json -Depth 5|Set-Content -LiteralPath "$PSScriptRoot/../private/flycast-research/quest-app-disc-receipt.json"
Write-Output 'Owned disc working copy verified in the existing Silent Scope app folder; no APK installation or launch.'
