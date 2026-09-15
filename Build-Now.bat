@echo off
title Ashen Hollow - Compilando...
set ENGINE=C:\Program Files\Epic Games\UE_5.8
set PROJECT=%~dp0unreal\AshenHollow\AshenHollow.uproject
set BUILD=%ENGINE%\Engine\Build\BatchFiles\Build.bat
set LOG=%~dp0Build-Log.txt

echo Compilando... resultado sera salvo em Build-Log.txt
echo.

"%BUILD%" AshenHollowEditor Win64 Development "-Project=%PROJECT%" -WaitMutex -MaxParallelActions=4 > "%LOG%" 2>&1

if %ERRORLEVEL% == 0 (
    echo BUILD OK! Pode abrir o Play-Unreal.cmd
) else (
    echo BUILD FALHOU - veja o arquivo Build-Log.txt na pasta RPG
    echo Abrindo o log...
    notepad "%LOG%"
)
pause
