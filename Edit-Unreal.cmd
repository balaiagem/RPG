@echo off
setlocal
set "AH_EDITOR=C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
if not exist "%AH_EDITOR%" (
    echo Unreal Engine 5.8 nao encontrado no caminho esperado.
    pause
    exit /b 2
)
if not exist "%~dp0unreal\AshenHollow\Binaries\Win64\UnrealEditor-AshenHollow.dll" (
    echo Compile primeiro com unreal\AshenHollow\Scripts\Build-Editor.ps1.
    pause
    exit /b 3
)
start "" "%AH_EDITOR%" "%~dp0unreal\AshenHollow\AshenHollow.uproject"
