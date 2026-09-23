param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
    [ValidateRange(1, 64)][int]$MaxParallelActions = 2,
    [switch]$CleanRebuild
)
$ErrorActionPreference = 'Stop'
$projectFile = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\AshenHollow.uproject'))
$buildFile = Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat'
$versionFile = Join-Path $EngineRoot 'Engine\Build\Build.version'
if (-not (Test-Path -LiteralPath $buildFile) -or -not (Test-Path -LiteralPath $versionFile)) {
    Write-Error 'Unreal 5.8 nao encontrado. Instale o motor ou informe -EngineRoot com a pasta correta.'
    exit 2
}
$engineVersion = Get-Content -LiteralPath $versionFile -Raw | ConvertFrom-Json
if ($engineVersion.MajorVersion -ne 5 -or $engineVersion.MinorVersion -ne 8) {
    Write-Error 'Este projeto esta fixado no Unreal 5.8. Selecione essa instalacao.'
    exit 3
}
$extraBuildArgs = @('-gather')
if ($CleanRebuild) { $extraBuildArgs += @('-Rebuild', '-NoUBA') }
& $buildFile 'AshenHollowEditor' 'Win64' 'Development' "-Project=$projectFile" '-WaitMutex' "-MaxParallelActions=$MaxParallelActions" @extraBuildArgs
exit $LASTEXITCODE
