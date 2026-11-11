param([Parameter(Mandatory=$true)][string]$Run,[ValidatePattern('^[a-zA-Z0-9]+$')][string]$Serial='340YC10GB00YWB')
$ErrorActionPreference='Stop'
if($Run -notmatch '^[a-z0-9-]+$'){throw 'Invalid diagnostic run name'}
$scopeProject=Split-Path -Parent $PSScriptRoot
$scopeSource='D:/VR4ALL-SilentScope-Build/flycast-source'
$scopeOutput="D:/VR4ALL-SilentScope-Build/diagnostics/$Run"
if(Test-Path -LiteralPath $scopeOutput){throw 'Refusing to overwrite an existing install receipt'}
$scopeAdb='E:/AndroidSdk/platform-tools/adb.exe'
$scopeSerial=$Serial
$scopeApk="$scopeProject/artifacts/silent-scope-vr-quest-dreamcast-candidate.apk"
$scopeVersion=[IO.File]::ReadAllText("$scopeProject/VERSION").Trim()
$scopeHash=(Get-FileHash -LiteralPath $scopeApk -Algorithm SHA256).Hash
$scopeDevice=(& $scopeAdb -s $scopeSerial get-state 2>&1 | Out-String).Trim()
if($LASTEXITCODE -or $scopeDevice -ne 'device'){throw 'The authorized Quest is not connected'}
New-Item -ItemType Directory -Path $scopeOutput | Out-Null
$scopeReceipt=[ordered]@{utc=[DateTime]::UtcNow.ToString('o');serial=$scopeSerial;package='com.vrlightgunhub.silentscope';backend='Dreamcast';version=$scopeVersion;candidateSha256=$scopeHash;apkBytes=(Get-Item -LiteralPath $scopeApk).Length;replacePreserveData=$true;installed=$false;installedHashVerified=$false;physicalPlayTested=$false;xrPerformanceTested=$false}
try {
    & $scopeAdb -s $scopeSerial shell dumpsys package com.vrlightgunhub.silentscope | Set-Content -LiteralPath "$scopeOutput/before.txt"
    & $scopeAdb -s $scopeSerial install -r $scopeApk *> "$scopeOutput/install.txt"
    $scopeReceipt.installExitCode=$LASTEXITCODE
    if($LASTEXITCODE){throw 'Quest package replacement failed'}
    $scopeReceipt.installed=$true
    $scopeAfter=& $scopeAdb -s $scopeSerial shell dumpsys package com.vrlightgunhub.silentscope
    $scopeAfter | Set-Content -LiteralPath "$scopeOutput/after.txt"
    if(($scopeAfter|Out-String) -notmatch ('versionName='+[regex]::Escape($scopeVersion)+'\s')){throw 'Installed version mismatch'}
    $scopePath=(& $scopeAdb -s $scopeSerial shell pm path com.vrlightgunhub.silentscope | Select-Object -First 1).Trim()
    if($scopePath -notmatch '^package:(/data/app/.+/base\.apk)$'){throw 'Unexpected installed APK path'}
    & $scopeAdb -s $scopeSerial pull $Matches[1] "$scopeOutput/installed-base.apk" *> "$scopeOutput/pull.txt"
    if($LASTEXITCODE){throw 'Could not pull installed APK for verification'}
    $scopeInstalledHash=(Get-FileHash -LiteralPath "$scopeOutput/installed-base.apk" -Algorithm SHA256).Hash
    $scopeReceipt.installedSha256=$scopeInstalledHash
    if($scopeInstalledHash -ne $scopeHash){throw 'Installed APK differs from the verified candidate'}
    $scopeReceipt.installedHashVerified=$true
    Copy-Item -LiteralPath $scopeApk -Destination "$scopeProject/artifacts/silent-scope-vr-quest-v$scopeVersion.apk"
    $scopeFiles=git -C $scopeSource ls-files --modified --others --exclude-standard -- core shell CMakeLists.txt
    if($LASTEXITCODE){throw 'Could not enumerate current isolated source'}
    $scopeManifest=@($scopeFiles|Sort-Object -Unique|ForEach-Object{
        $scopeFile=Join-Path $scopeSource $_
        if(Test-Path -LiteralPath $scopeFile -PathType Leaf){@{path=$_;sha256=(Get-FileHash -LiteralPath $scopeFile -Algorithm SHA256).Hash;bytes=(Get-Item -LiteralPath $scopeFile).Length}}
    })
    git -C $scopeSource diff --binary | Set-Content -LiteralPath "$scopeOutput/engine-working-tree.patch" -Encoding utf8
    @{base=(git -C $scopeSource rev-parse HEAD);branch=(git -C $scopeSource branch --show-current);files=$scopeManifest;apkSha256=$scopeHash;engineSha256=(Get-FileHash -LiteralPath 'D:/VR4ALL-SilentScope-Build/flycast-quest/libflycast.so' -Algorithm SHA256).Hash}|ConvertTo-Json -Depth 6|Set-Content -LiteralPath "$scopeOutput/source-manifest.json"
    & $scopeAdb -s $scopeSerial shell dumpsys power | Set-Content -LiteralPath "$scopeOutput/power.txt"
    Write-Output "Installed and hash-verified Silent Scope $scopeVersion ($scopeHash)"
} catch {
    $scopeReceipt.error=$_.Exception.Message
    throw
} finally {
    $scopeReceipt|ConvertTo-Json -Depth 6|Set-Content -LiteralPath "$scopeOutput/receipt.json"
}
