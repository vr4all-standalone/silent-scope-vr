param(
    [string]$MameExe='D:/VR4ALL-Tera-Build/build/mame-source/teraburst.exe',
    [string]$RomDirectory='E:/___flattovrdev/confidential mission/Roms',
    [Parameter(Mandatory=$true)][string]$OutputDirectory,
    [string]$LegacyTimekeeper,
    [string]$SeedNvram,
    [string]$PythonExe='D:/Python314/python.exe',
    [int]$Seconds=45,
    [switch]$DrcUseC,
    [double]$TestTime=16
    ,[string]$ProbeScript=(Join-Path $PSScriptRoot 'boot-probe.lua')
)
$ErrorActionPreference='Stop'
$scopeRoot=Split-Path -Parent $PSScriptRoot
$scopeOutput=[IO.Path]::GetFullPath($OutputDirectory)
if(Test-Path -LiteralPath $scopeOutput){throw 'Use a new probe output directory to preserve previous evidence.'}
if($Seconds -lt 1 -or $Seconds -gt 600){throw 'Probe duration must be 1..600 native seconds.'}
foreach($scopeTool in @($MameExe,$PythonExe,$ProbeScript)){if(!(Test-Path -LiteralPath $scopeTool -PathType Leaf)){throw "Missing tool: $scopeTool"}}
$scopeRom=(Resolve-Path -LiteralPath $RomDirectory).ProviderPath
New-Item -ItemType Directory -Path $scopeOutput | Out-Null
$scopeSearch=$scopeRom
if($LegacyTimekeeper){
    $scopeOverrides=Join-Path $scopeOutput 'media'
    & $PythonExe (Join-Path $PSScriptRoot 'media.py') (Join-Path $scopeRom 'sscope.zip') --candidate $LegacyTimekeeper --diagnostic-output (Join-Path $scopeOverrides 'sscope')
    if($LASTEXITCODE){throw 'Recognized legacy timekeeper preparation failed.'}
    $scopeSearch="$scopeOverrides;$scopeRom"
}
foreach($scopeDir in @('nvram','cfg','state','snap','diff')){New-Item -ItemType Directory -Path (Join-Path $scopeOutput $scopeDir) | Out-Null}
if($SeedNvram){
    $scopeSeed=(Resolve-Path -LiteralPath $SeedNvram).ProviderPath
    Get-ChildItem -LiteralPath $scopeSeed | ForEach-Object {Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $scopeOutput 'nvram') -Recurse}
}
$scopeOldDir=$env:SILENT_SCOPE_PROBE_DIR;$scopeOldTime=$env:SILENT_SCOPE_TEST_TIME
try {
    $env:SILENT_SCOPE_PROBE_DIR=$scopeOutput.Replace('\','/')
    $env:SILENT_SCOPE_TEST_TIME=$TestTime.ToString([Globalization.CultureInfo]::InvariantCulture)
    $scopeArgs=@('sscope','-rompath',$scopeSearch,'-noreadconfig','-nowriteconfig','-video','none','-sound','none','-snapview','native','-skip_gameinfo','-nothrottle','-seconds_to_run',"$Seconds",'-autoboot_script',$ProbeScript)
    if($DrcUseC){$scopeArgs+='-drc_use_c'}
    foreach($scopeDir in @('nvram','cfg','state','diff')){$scopeArgs+=@("-$($scopeDir)_directory",(Join-Path $scopeOutput $scopeDir))}
    $scopeArgs+=@('-snapshot_directory',(Join-Path $scopeOutput 'snap'))
    @{executable=$MameExe;arguments=$scopeArgs;legacyDiagnostic=[bool]$LegacyTimekeeper;testTime=$TestTime} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $scopeOutput 'command.json') -Encoding UTF8
    & $MameExe @scopeArgs 2>&1 | Tee-Object -FilePath (Join-Path $scopeOutput 'engine.txt')
    $scopeCode=$LASTEXITCODE
    @{exitCode=$scopeCode} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $scopeOutput 'exit.json') -Encoding UTF8
    if($scopeCode){throw "Native diagnostic exited $scopeCode; see engine.txt"}
} finally {$env:SILENT_SCOPE_PROBE_DIR=$scopeOldDir;$env:SILENT_SCOPE_TEST_TIME=$scopeOldTime}
