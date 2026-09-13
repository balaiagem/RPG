# Build-Step3.ps1 - compila Step 3 do Ashen Hollow (UE5)
# Execute na pasta unreal\AshenHollow:
#   .\Scripts\Build-Step3.ps1

param(
    [string]$UERoot  = "C:\Program Files\Epic Games\UE_5.8",
    [int]   $MaxJobs = 2
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$Root = Split-Path $PSScriptRoot -Parent

Write-Host ""
Write-Host "=== Ashen Hollow - Step 3 ===" -ForegroundColor Cyan
Write-Host ""

# -- Verifica arquivos novos --------------------------------------------------
$Files = @(
    "Source\AshenHollow\Public\AHAnimInstance.h",
    "Source\AshenHollow\Private\AHAnimInstance.cpp",
    "Source\AshenHollow\Public\AHNotify_MeleeImpact.h",
    "Source\AshenHollow\Private\AHNotify_MeleeImpact.cpp"
)
foreach ($F in $Files) {
    $Path = Join-Path $Root $F
    if (-not (Test-Path $Path)) {
        Write-Error "Arquivo nao encontrado: $F"
    }
    Write-Host "  OK  $F" -ForegroundColor Green
}

# -- Usa Build.bat (inclui bootstrapping do .NET) ----------------------------
# Build.bat é o jeito oficial do UE5 para builds de linha de comando.
# Ele detecta e inicializa o .NET automaticamente.
$BuildBat = Join-Path $UERoot "Engine\Build\BatchFiles\Build.bat"
if (-not (Test-Path $BuildBat)) {
    Write-Error "Build.bat nao encontrado em: $BuildBat`nVerifique se UERoot esta correto: $UERoot"
}

Write-Host ""
Write-Host "Compilando com Build.bat... (pode demorar 2-5 min)" -ForegroundColor Cyan

$Project = Join-Path $Root "AshenHollow.uproject"
$Proc = Start-Process -FilePath $BuildBat `
    -ArgumentList "AshenHollowEditor", "Win64", "Development", "`"$Project`"", "-MaxParallelActions=$MaxJobs", "-NoHotReload" `
    -Wait -PassThru -NoNewWindow

if ($Proc.ExitCode -ne 0) {
    Write-Error "Falha na compilacao (codigo $($Proc.ExitCode))"
}

Write-Host ""
Write-Host "=== Compilacao concluida! ===" -ForegroundColor Green
Write-Host ""
Write-Host "Proximo passo: rode Play-Unreal.cmd na raiz do projeto." -ForegroundColor Yellow
Write-Host ""
