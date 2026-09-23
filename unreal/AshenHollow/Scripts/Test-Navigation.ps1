param([string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8', [switch]$Rendered)
$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
if (-not (Test-Path -LiteralPath $editor)) { throw "Unreal Editor nao encontrado: $editor" }
$renderArgs = @('-NullRHI')
$reportPrefix = 'Navigation-'
if ($Rendered) {
    $editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
    $renderArgs = @('-windowed','-ResX=1280','-ResY=720')
    $reportPrefix = 'Navigation-Rendered-'
}
$report = Join-Path $projectRoot ('Saved\Automation\' + $reportPrefix + (Get-Date -Format 'yyyyMMdd-HHmmss'))
& $editor (Join-Path $projectRoot 'AshenHollow.uproject') '/Game/AshenHollow/Maps/Courtyard' '-game' '-unattended' '-nop4' @renderArgs '-nosound' '-nosplash' '-ExecCmds=Automation RunTests AshenHollow.Navigation' '-TestExit=Automation Test Queue Empty' "-ReportExportPath=$report" '-stdout'
if ($LASTEXITCODE -ne 0) { throw "Unreal encerrou com codigo $LASTEXITCODE" }
$reportFile = Join-Path $report 'index.json'
if (-not (Test-Path -LiteralPath $reportFile)) { throw "Relatorio ausente: $reportFile" }
$result = Get-Content -LiteralPath $reportFile -Raw | ConvertFrom-Json
if ($result.failed -gt 0 -or $result.succeeded -ne 1 -or $result.notRun -gt 0) {
    throw "Navegacao reprovada ou incompleta: $reportFile"
}
Write-Host "Navegacao de todas as classes aprovada: $reportFile"
