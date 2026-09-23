param([string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8')
$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
if (-not (Test-Path -LiteralPath $editor)) { throw "Unreal Editor nao encontrado: $editor" }
$script = Join-Path $projectRoot 'Scripts\Light-Courtyard.py'
$log = Join-Path $projectRoot 'Saved\Lighting-console.log'
& $editor (Join-Path $projectRoot 'AshenHollow.uproject') `
    '-run=pythonscript' "-script=$script" `
    '-unattended' '-nop4' '-nosplash' '-nosound' '-stdout' 2>&1 | Tee-Object -FilePath $log
if ($LASTEXITCODE -ne 0) { throw "Unreal encerrou com codigo $LASTEXITCODE - veja $log" }
if (-not (Select-String -LiteralPath $log -Pattern 'AH_LIGHTING_APPLIED' -Quiet)) {
    throw "Iluminacao nao foi aplicada - veja $log"
}
$skipped = Select-String -LiteralPath $log -Pattern 'AH_LIGHTING_SKIPPED'
if ($skipped) {
    Write-Warning 'Algumas propriedades foram ignoradas (nomes mudam entre versoes do motor):'
    Select-String -LiteralPath $log -Pattern '^\s+\w+\.\w+ \(' | ForEach-Object { Write-Warning $_.Line.Trim() }
} else {
    Write-Host 'Iluminacao aplicada; todas as propriedades aceitas.' -ForegroundColor Green
}
