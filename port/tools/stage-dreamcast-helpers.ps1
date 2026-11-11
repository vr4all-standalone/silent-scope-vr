param([switch]$RefreshTimestamps)
$ErrorActionPreference='Stop'
$scopeProject=Split-Path -Parent $PSScriptRoot
$scopeSource='D:/VR4ALL-SilentScope-Build/flycast-source'
$scopeRevision=git --no-optional-locks -C $scopeSource rev-parse HEAD
if($LASTEXITCODE -or $scopeRevision -ne '8398fead532ede9d52c37addef3353611b6c5ce7'){throw 'Unexpected isolated engine base'}
$scopeEntries=@()
foreach($scopeFile in Get-ChildItem -LiteralPath (Join-Path $scopeProject 'native/dreamcast') -File){
    if($scopeFile.Name -notmatch '^dc_.*\.(h|cpp)$'){continue}
    $scopeEntries+=@{from=$scopeFile.FullName;to="core/dreamcast/$($scopeFile.Name)"}
}
foreach($scopeFile in @('rifle_renderer.cpp','rifle_renderer.h','rifle_mesh.h','scope_optics.h','display_bridge.h','rifle_support.h','rifle_smoothing.h')){
    $scopeEntries+=@{from=(Join-Path $scopeProject "native/$scopeFile");to="core/dreamcast/$scopeFile"}
}
$scopeEntries+=@{from=(Join-Path $scopeProject 'native/atomic_file_io.h');to='core/atomic_file_io.h'}
# Quoted includes search these source-local directories before core/. Keep the
# diagnostic copy synchronized; otherwise a stale schema silently shadows it.
foreach($scopeFile in @('dc_input_diagnostic.h','native_input_probe.h')){
    $scopeEntries+=@{from=(Join-Path $scopeProject "native/dreamcast/$scopeFile");to="core/hw/maple/dreamcast/$scopeFile"}
}
$scopeEntries+=@{from=(Join-Path $scopeProject 'native/dreamcast/native_view_probe.h');to='core/rend/gles/dreamcast/native_view_probe.h'}
$scopeReceipt=@()
foreach($scopeEntry in $scopeEntries){
    $scopeDestination=Join-Path $scopeSource $scopeEntry.to
    $scopeHash=(Get-FileHash -LiteralPath $scopeEntry.from -Algorithm SHA256).Hash
    $scopeChanged=!(Test-Path -LiteralPath $scopeDestination) -or (Get-FileHash -LiteralPath $scopeDestination -Algorithm SHA256).Hash -ne $scopeHash
    if($scopeChanged -or $RefreshTimestamps){
        [IO.Directory]::CreateDirectory((Split-Path -Parent $scopeDestination))|Out-Null
        # Write bytes rather than preserving the old file's timestamp. Ninja
        # must see updates even if an earlier generated copy was built later.
        [IO.File]::WriteAllBytes($scopeDestination,[IO.File]::ReadAllBytes($scopeEntry.from))
    }
    $scopeReceipt+=@{source=$scopeEntry.from;destination=$scopeDestination;sha256=$scopeHash;written=($scopeChanged -or [bool]$RefreshTimestamps)}
}
$scopeReceipt|ConvertTo-Json -Depth 4|Set-Content -LiteralPath (Join-Path $scopeProject 'private/flycast-research/staged-helper-receipt.json')
Write-Output "Verified $($scopeEntries.Count) task-owned helper destinations"
