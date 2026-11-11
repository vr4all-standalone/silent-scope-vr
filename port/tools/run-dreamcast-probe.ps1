param(
    [Parameter(Mandatory=$true)][string]$OutputDirectory,
    [int]$Seconds=45,
    [ValidateSet('Controller','Mouse')][string]$Device='Controller',
    [ValidateSet('None','PadStart','MouseStart','MouseDiagnostic')][string]$InputRecipe='None',
    [switch]$RuntimeView
)
$ErrorActionPreference='Stop'
if($Seconds -lt 5 -or $Seconds -gt 90){throw 'Probe duration must be5..90seconds'}
if($InputRecipe -eq 'MouseDiagnostic' -and $Device -ne 'Mouse'){throw 'Mouse diagnostic requires the native mouse device'}
$scopeOutput=[IO.Path]::GetFullPath($OutputDirectory)
$scopeRoot=[IO.Path]::GetFullPath('D:/VR4ALL-SilentScope-Build/diagnostics')
if(!$scopeOutput.StartsWith($scopeRoot+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Probe output leaves the dedicated diagnostic root'}
if(Test-Path -LiteralPath $scopeOutput){throw 'Preserve previous evidence; select a new probe directory'}
$scopeBuilt='D:/VR4ALL-SilentScope-Build/flycast-native/flycast.exe'
$scopeCue='D:/VR4ALL-SilentScope-Build/diagnostics/dreamcast-owned-media-001/Silent Scope (USA).cue'
$scopeImport='D:/VR4ALL-SilentScope-Build/diagnostics/dreamcast-owned-media-001/import-verification.json'
if(!(Test-Path -LiteralPath $scopeBuilt -PathType Leaf)){throw 'Build the isolated native prototype first'}
if(!(Test-Path -LiteralPath $scopeCue -PathType Leaf) -or !(Test-Path -LiteralPath $scopeImport -PathType Leaf)){throw 'Import and verify the owned disc first'}
if((Get-Content -LiteralPath $scopeImport -Raw | ConvertFrom-Json).status -ne 'verified'){throw 'Owned disc import is not verified'}
New-Item -ItemType Directory -Path $scopeOutput | Out-Null
$scopeExe=Join-Path $scopeOutput 'flycast.exe'
Copy-Item -LiteralPath $scopeBuilt -Destination $scopeExe
$scopeOptions='config:UseReios=yes,config:pvr.rend=0,config:vr.Xr=no,config:vr.Reproject=no,config:Dreamcast.AutoLoadState=no,config:Dreamcast.AutoSaveState=no,config:aica.Volume=0,log:LogToFile=yes,log:Verbosity=3'
if($RuntimeView){$scopeOptions=$scopeOptions.Replace('vr.Xr=no','vr.Xr=yes').Replace('vr.Reproject=no','vr.Reproject=yes')}
if($Device -eq 'Mouse'){$scopeOptions+=',input:device1=6'}
$scopeArgs=@('-config',$scopeOptions,('"'+$scopeCue+'"'))
if($scopeArgs.Count -ne 3){throw 'Unexpected native command argument shape'}
$scopeOldProbe=$env:SILENT_SCOPE_DC_PROBE_DIR
$scopeOldInput=$env:SILENT_SCOPE_DC_INPUT_PROBE;$scopeOldTrace=$env:SILENT_SCOPE_DC_INPUT_PROBE_TRACE
$scopeOldMouseDiag=$env:SILENT_SCOPE_DC_MOUSE_DIAG
try {
    $env:SILENT_SCOPE_DC_PROBE_DIR=$scopeOutput
    $env:SILENT_SCOPE_DC_INPUT_PROBE=switch($InputRecipe){'PadStart'{'pad-start'}'MouseStart'{'mouse-start'}default{''}}
    $env:SILENT_SCOPE_DC_INPUT_PROBE_TRACE='1'
    $env:SILENT_SCOPE_DC_MOUSE_DIAG=if($InputRecipe -eq 'MouseDiagnostic'){'middle-steps'}else{''}
    $scopeProcess=Start-Process -FilePath $scopeExe -ArgumentList $scopeArgs -WorkingDirectory $scopeOutput -WindowStyle Hidden -PassThru -RedirectStandardError (Join-Path $scopeOutput 'stderr.txt') -RedirectStandardOutput (Join-Path $scopeOutput 'stdout.txt')
    @{executable=$scopeExe;pid=$scopeProcess.Id;seconds=$Seconds;device=$Device;inputRecipe=$InputRecipe;arguments=$scopeArgs;source='8398fead532ede9d52c37addef3353611b6c5ce7';sha256=(Get-FileHash -LiteralPath $scopeExe -Algorithm SHA256).Hash} | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $scopeOutput 'command.json') -Encoding UTF8
    Write-Output "Native prototype started: PID $($scopeProcess.Id), private output $scopeOutput"
    # Keep waits bounded even for a longer diagnostic recipe. The exact owned
    # Process remains the only process eligible for the final bounded stop.
    $scopeTimer=[Diagnostics.Stopwatch]::StartNew()
    $scopeDeadline=$false
    while(!$scopeDeadline -and $scopeTimer.Elapsed.TotalSeconds -lt $Seconds){
        $scopeRemaining=[Math]::Max(1,[int](($Seconds-$scopeTimer.Elapsed.TotalSeconds)*1000))
        $scopeDeadline=$scopeProcess.WaitForExit([Math]::Min(30000,$scopeRemaining))
    }
    $scopeClose=$false;$scopeForced=$false
    if(!$scopeDeadline){
        $scopeClose=$scopeProcess.CloseMainWindow()
        if(!$scopeProcess.WaitForExit(3000)){
            # This exact owned Process object was started above; no process-name
            # enumeration, unrelated game shutdown or headset operation occurs.
            $scopeProcess.Kill();$scopeProcess.WaitForExit();$scopeForced=$true
        }
    }
    @{exitCode=$scopeProcess.ExitCode;durationDeadlineReached=(!$scopeDeadline);closeRequested=$scopeClose;forcedBoundedStop=$scopeForced;headsetSession=$false} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $scopeOutput 'exit.json') -Encoding UTF8
} finally {$env:SILENT_SCOPE_DC_PROBE_DIR=$scopeOldProbe;$env:SILENT_SCOPE_DC_INPUT_PROBE=$scopeOldInput;$env:SILENT_SCOPE_DC_INPUT_PROBE_TRACE=$scopeOldTrace;$env:SILENT_SCOPE_DC_MOUSE_DIAG=$scopeOldMouseDiag}
