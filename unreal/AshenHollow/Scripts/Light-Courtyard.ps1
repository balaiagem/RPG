param([string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8')
$ErrorActionPreference = 'Stop'

# This script edits and saves Courtyard.umap. Commit or stash first, so undoing is
# one command:  git checkout -- unreal/AshenHollow/Content/AshenHollow/Maps/Courtyard.umap
Write-Host 'Este script modifica e salva Courtyard.umap. Garanta um commit limpo antes.' -ForegroundColor Yellow
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$map = Join-Path $projectRoot 'Content\AshenHollow\Maps\Courtyard.umap'
$mapBackup = "$map.bak"
# Copia de seguranca automatica. Se a iluminacao sair errada, restaure com:
#   Copy-Item -LiteralPath '<mapBackup>' -Destination '<map>' -Force
if (Test-Path -LiteralPath $map) {
    Copy-Item -LiteralPath $map -Destination $mapBackup -Force
    Write-Host "Backup do mapa: $mapBackup" -ForegroundColor Cyan
}
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

Write-Host "Para desfazer:  Copy-Item -LiteralPath '$mapBackup' -Destination '$map' -Force" -ForegroundColor Cyan
