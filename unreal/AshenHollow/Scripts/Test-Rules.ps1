param([string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8')
$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
if (-not (Test-Path -LiteralPath $editor)) { throw "Unreal Editor nao encontrado: $editor" }
$report = Join-Path $projectRoot ('Saved\Automation\' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
& $editor (Join-Path $projectRoot 'AshenHollow.uproject') '-unattended' '-nop4' '-NullRHI' '-nosound' '-nosplash' '-ExecCmds=Automation RunTests AshenHollow.Rules' '-TestExit=Automation Test Queue Empty' "-ReportExportPath=$report" '-stdout' '-FullStdOutLogOutput'
if ($LASTEXITCODE -ne 0) { throw "Unreal encerrou com codigo $LASTEXITCODE" }
$reportFile = Join-Path $report 'index.json'
if (-not (Test-Path -LiteralPath $reportFile)) { throw "O Unreal nao produziu o relatorio esperado: $reportFile" }
$result = Get-Content -LiteralPath $reportFile -Raw | ConvertFrom-Json
if ($result.failed -gt 0 -or $result.succeeded -lt 1 -or $result.notRun -gt 0) {
    throw "Testes incompletos ou com falhas. Consulte $reportFile"
}
Write-Host "Regras aprovadas pelo Unreal. Relatorio: $reportFile"
