@echo off
title Ashen Hollow - Ajustando o mapa ao terreno...
rem ---------------------------------------------------------------------------
rem Roda Scripts\Fit-World.py com o Python que vem DENTRO da Unreal.
rem
rem Nao e preciso ter Python instalado no Windows: a engine traz o dela, e o
rem PythonScriptPlugin ja esta habilitado no AshenHollow.uproject.
rem
rem O Unreal precisa estar FECHADO -- o commandlet abre o projeto sozinho, e
rem com o editor aberto ele fica esperando o mutex sem dizer nada.
rem ---------------------------------------------------------------------------
set ENGINE=C:\Program Files\Epic Games\UE_5.8
set PROJECT=%~dp0unreal\AshenHollow\AshenHollow.uproject
set SCRIPT=%~dp0unreal\AshenHollow\Scripts\Fit-World.py
set LOG=%~dp0Fit-Log.txt

echo Ajustando o mapa ao Landscape... o resultado vai para Fit-Log.txt
echo.

"%ENGINE%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%PROJECT%" ^
    -run=pythonscript -script="%SCRIPT%" ^
    -unattended -nopause -nosplash -stdout -FullStdOutLogOutput > "%LOG%" 2>&1

echo.
echo ---- o que o script disse ----
findstr /C:"AH_FIT" /C:"AH_SITE" "%LOG%"
echo ------------------------------
echo.
echo Se as seis linhas AH_SITE baterem (erro menor que 4 m), esta certo.
echo Depois: abra o Unreal, menu Build ^> Build Paths, e salve o mapa.
pause
