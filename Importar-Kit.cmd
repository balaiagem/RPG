@echo off
title Ashen Hollow - Importando o kit de arte...
rem ---------------------------------------------------------------------------
rem Traz os FBX de Art\Kit para dentro do projeto, cria os materiais do kit e
rem liga um no outro. Nao precisa abrir o Blender nem ter Python instalado.
rem
rem O Unreal precisa estar FECHADO.
rem ---------------------------------------------------------------------------
set "ENGINE=C:\Program Files\Epic Games\UE_5.8"
set "PROJECT=%~dp0unreal\AshenHollow\AshenHollow.uproject"
set "SCRIPT=%~dp0unreal\AshenHollow\Scripts\Import-Kit.py"
set "LOG=%~dp0Kit-Log.txt"

if not exist "%ENGINE%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" (
    echo Unreal 5.8 nao encontrado em "%ENGINE%".
    pause
    exit /b 2
)

echo.
echo  Importando o kit... o resultado vai para Kit-Log.txt
echo.

"%ENGINE%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%PROJECT%" ^
    -run=pythonscript -script="%SCRIPT%" ^
    -unattended -nopause -nosplash -stdout -FullStdOutLogOutput > "%LOG%" 2>&1

echo  ---- o que o script disse ----
findstr /C:"AH_KIT" "%LOG%" | findstr /V "LogInit"
echo  ------------------------------
echo.
echo  As malhas ficam em Content/AshenHollow/Kit/Meshes.
pause
