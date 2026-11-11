param(
    [Parameter(Mandatory=$true)][string]$OutputDirectory,
    [ValidateSet('Seed','Load','Control','Wide')][string]$Mode='Load',
    [string]$Checkpoint,
    [ValidateRange(5,90)][int]$Seconds=65,
    [ValidateSet(16,32,48)][int]$StepCounts=32,
    [switch]$LensBoundary,
    [switch]$Projectors,
    [switch]$MouseConsumer,
    [switch]$Ownership,
    [switch]$ConsumerFire,
    [switch]$AllDecoded,
    [switch]$PersistentRuntime,
    [switch]$ScopeVisibility,
    [ValidateSet('.2','.5')][string]$ScopeScale='.2',
    [ValidateRange(-5500,5500)][int]$ScopeYaw=3300,
    [switch]$ScopeVisibilityControl
)
$ErrorActionPreference='Stop'
$scopeOutput=[IO.Path]::GetFullPath($OutputDirectory)
$scopeRoot=[IO.Path]::GetFullPath('D:/VR4ALL-SilentScope-Build/diagnostics')
if(!$scopeOutput.StartsWith($scopeRoot+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Controlled output leaves the private diagnostic root'}
if($Mode -eq 'Seed'){$Checkpoint=Join-Path $scopeOutput 'native40.dcstate'}
elseif(!$Checkpoint -or !(Test-Path -LiteralPath $Checkpoint -PathType Leaf)){throw 'Supply the completed private native checkpoint'}
$scopeValues=@{
    SILENT_SCOPE_DC_CHECKPOINT_MODE=$(if($Mode -eq 'Seed'){'save40'}else{'load30'})
    SILENT_SCOPE_DC_CHECKPOINT_FILE=$Checkpoint
    SILENT_SCOPE_DC_CAMERA_PROBE_DIR=$scopeOutput
    SILENT_SCOPE_DC_CAMERA_GEOMETRY_DIR=$scopeOutput
    SILENT_SCOPE_DC_SCOPE_PROBE_DIR=$scopeOutput
    SILENT_SCOPE_DC_CAMERA_WIDEN_PROBE=$(switch($Mode){Control{'1'}Wide{'3'}default{''}})
    SILENT_SCOPE_DC_PROBE_BEGIN_SECONDS='35'
    SILENT_SCOPE_DC_MOUSE_DIAG_COUNTS=[string]$StepCounts
    SILENT_SCOPE_DC_LENS_BOUNDARY_DIR=$(if($LensBoundary -or $Projectors){$scopeOutput}else{''})
    SILENT_SCOPE_DC_PROJECTOR_PROBE_DIR=$(if($Projectors){$scopeOutput}else{''})
    SILENT_SCOPE_DC_MOUSE_CONSUMER_PROBE=$(if($MouseConsumer -or $ConsumerFire){'1'}else{''})
    SILENT_SCOPE_DC_MOUSE_CONSUMER_FIRE_PROBE=$(if($ConsumerFire){'1'}else{''})
    SILENT_SCOPE_DC_OWNERSHIP_PROBE_DIR=$(if($Ownership){$scopeOutput}else{''})
    SILENT_SCOPE_DC_OWNERSHIP_ALL_DECODED=$(if($AllDecoded){'1'}else{''})
}
if($PersistentRuntime){
    if($MouseConsumer -or $ConsumerFire){throw 'Persistent runtime owns its private input producer'}
    $scopeValues.SILENT_SCOPE_DC_VIEW_RUNTIME_PROBE='1'
    $scopeValues.SILENT_SCOPE_DC_CAMERA_WIDEN_PROBE=''
    $scopeValues.SILENT_SCOPE_DC_OWNERSHIP_END_SECONDS='80'
    $scopeValues.SILENT_SCOPE_DC_CAMERA_PROBE_END_SECONDS='80'
}
if($ScopeVisibility){
    if(!$PersistentRuntime){throw 'Scope visibility replay requires the no-fire native runtime producer'}
    $scopeValues.SILENT_SCOPE_DC_CAMERA_WIDEN_PROBE='1'
    $scopeValues.SILENT_SCOPE_DC_NATIVE_SCOPE_PROBE='1'
    $scopeValues.SILENT_SCOPE_DC_SCOPE_VISIBILITY_PROBE='1'
    $scopeValues.SILENT_SCOPE_DC_SCOPE_VISIBILITY_SCALE=$ScopeScale
    $scopeValues.SILENT_SCOPE_DC_SCOPE_VISIBILITY_AIM=[string]$ScopeYaw
    $scopeValues.SILENT_SCOPE_DC_SCOPE_VISIBILITY_CONTROL=$(if($ScopeVisibilityControl){'1'}else{''})
}
$scopePrevious=@{}
try {
    foreach($scopeName in $scopeValues.Keys){$scopePrevious[$scopeName]=[Environment]::GetEnvironmentVariable($scopeName,'Process');[Environment]::SetEnvironmentVariable($scopeName,$scopeValues[$scopeName],'Process')}
    & "$PSScriptRoot/run-dreamcast-probe.ps1" -OutputDirectory $scopeOutput -Device Mouse -InputRecipe MouseDiagnostic -Seconds $Seconds -RuntimeView:$PersistentRuntime
    $scopeExit=Get-Content -LiteralPath (Join-Path $scopeOutput 'exit.json') -Raw | ConvertFrom-Json
    $scopeRecord=@{mode=$Mode;checkpoint=$Checkpoint;environment=$scopeValues;seconds=$Seconds;nativeExit=$scopeExit;xrTested=$false;physicalPlayTested=$false}
    if($ScopeVisibility){
        $scopeLog=Get-Content -LiteralPath (Join-Path $scopeOutput 'stderr.txt') -Raw
        $scopeExpected=if($ScopeVisibilityControl){'SS_SCOPE_VISIBILITY[^\r\n]*guard=0[^\r\n]*applied=0'}else{
            'SS_SCOPE_VISIBILITY[^\r\n]*guard=1[^\r\n]*scale='+$(if($ScopeScale -eq '.2'){'0\.20'}else{'0\.50'})+' applied=1'
        }
        $scopeRecord.scopeVisibilityVerified=$scopeLog -match $scopeExpected
    }
    if(Test-Path -LiteralPath $Checkpoint -PathType Leaf){$scopeRecord.checkpointSha256=(Get-FileHash -LiteralPath $Checkpoint -Algorithm SHA256).Hash;$scopeRecord.checkpointBytes=(Get-Item -LiteralPath $Checkpoint).Length}
    $scopeRecord | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $scopeOutput 'controlled.json')
    if($ScopeVisibility -and !$scopeRecord.scopeVisibilityVerified){throw 'Native scope control/correction was not observed; do not count this replay as verification'}
    if($scopeExit.exitCode -ne 0 -and !$scopeExit.forcedBoundedStop){throw "Native controlled diagnostic failed (exit $($scopeExit.exitCode)); inspect preserved stderr/exit receipts"}
} finally {
    foreach($scopeName in $scopePrevious.Keys){[Environment]::SetEnvironmentVariable($scopeName,$scopePrevious[$scopeName],'Process')}
}
