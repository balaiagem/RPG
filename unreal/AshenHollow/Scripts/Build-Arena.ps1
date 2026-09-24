param([string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8', [switch]$KeepDefaultMap)
$ErrorActionPreference = 'Stop'

# This script does NOT modify Courtyard. It writes a new map, ArenaVillage, so the
# grey-box arena stays as a known-good fallback: if the new one comes out wrong,
# open Courtyard and the game is exactly as it was.
Write-Host 'Constroi Content/AshenHollow/Maps/ArenaVillage. Courtyard nao e alterado.' -ForegroundColor Cyan

$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
if (-not (Test-Path -LiteralPath $editor)) { throw "Unreal Editor nao encontrado: $editor" }
$script = Join-Path $projectRoot 'Scripts\Build-Arena.py'
$log = Join-Path $projectRoot 'Saved\Arena-console.log'

& $editor (Join-Path $projectRoot 'AshenHollow.uproject') `
    '-run=pythonscript' "-script=$script" `
    '-unattended' '-nop4' '-nosplash' '-nosound' '-stdout' 2>&1 | Tee-Object -FilePath $log
if ($LASTEXITCODE -ne 0) { throw "Unreal encerrou com codigo $LASTEXITCODE - veja $log" }

# Unreal prints each warning twice (LogPython and the LogInit echo), so take
# one line: on an array, -match returns the matching items and never fills $Matches.
$built = Select-String -LiteralPath $log -Pattern 'AH_ARENA_BUILT' | Select-Object -First 1
if (-not $built) { throw "A arena nao foi construida - veja $log" }
Write-Host $built.Line.Trim() -ForegroundColor Green
Select-String -LiteralPath $log -Pattern 'AH_ARENA_NAV|AH_ARENA_HOUSE' | ForEach-Object { Write-Host $_.Line.Trim() -ForegroundColor Green }

$problems = Select-String -LiteralPath $log -Pattern 'AH_ARENA_PROBLEMS'
if ($problems) {
    Write-Warning 'Alguns assets ou propriedades foram ignorados:'
    Select-String -LiteralPath $log -Pattern '^\s+\w+' | Select-Object -Last 30 |
        ForEach-Object { Write-Warning $_.Line.Trim() }
} else {
    Write-Host 'Nenhum asset faltando e nenhuma propriedade recusada.' -ForegroundColor Green
}

# A map can "build" and still be a void: if spawning fails the script happily
# saves a level with nothing but lights in it, and walking into that drops the
# character through the floor. The actor count is the cheap check that the level
# has real content, and it gates the default-map switch.
$count = 0
if ($built.Line -match 'with (\d+) actors') { $count = [int]$Matches[1] }
if ($count -lt 100) {
    throw "A arena saiu praticamente vazia ($count atores). O mapa padrao NAO foi trocado; o jogo continua no Courtyard. Veja $log"
}

# Only now, with a map that actually built, point the project at it. Doing this
# before the build could leave the game aimed at a map that does not exist.
if (-not $KeepDefaultMap) {
    $ini = Join-Path $projectRoot 'Config\DefaultEngine.ini'
    $text = [IO.File]::ReadAllText($ini)
    $text = $text -replace 'EditorStartupMap=/Game/AshenHollow/Maps/\w+', 'EditorStartupMap=/Game/AshenHollow/Maps/ArenaVillage'
    $text = $text -replace 'GameDefaultMap=/Game/AshenHollow/Maps/\w+',   'GameDefaultMap=/Game/AshenHollow/Maps/ArenaVillage'
    [IO.File]::WriteAllText($ini, $text)
    Write-Host 'Mapa padrao agora e ArenaVillage.' -ForegroundColor Green
    Write-Host 'Para voltar ao antigo, troque as duas linhas Map= em Config\DefaultEngine.ini de volta para Courtyard.' -ForegroundColor Cyan
}
