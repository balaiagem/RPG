@echo off
setlocal
set "AH_EDITOR=C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
if not exist "%AH_EDITOR%" exit /b 2
if not exist "%~dp0unreal\AshenHollow\Content\AshenHollow\Maps\Courtyard.umap" (
    echo Mapa Unreal ainda nao gerado. Consulte unreal\AshenHollow\README.md.
    pause
    exit /b 3
)
start "Ashen Hollow" "%AH_EDITOR%" "%~dp0unreal\AshenHollow\AshenHollow.uproject" /Game/AshenHollow/Maps/Courtyard -game -windowed -ResX=1280 -ResY=720
