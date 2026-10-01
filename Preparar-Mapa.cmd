@echo off
title Ashen Hollow - Preparando o mapa...
rem ---------------------------------------------------------------------------
rem Roda os dois scripts de mapa com o Python que vem DENTRO da Unreal.
rem
rem   1. Fit-World.py   -- encaixa o mapa no Landscape: apaga o chao antigo,
rem                        cresce o volume de navegacao, poe o PlayerStart no
rem                        lugar, confere as seis alturas contra o heightmap e
rem                        avisa se o heightmap no mapa for o antigo.
rem   2. Dress-World.py -- constroi o material do terreno (grama, cascalho e
rem                        rocha decididos pela forma do chao) e liga a grama
rem                        procedural. E o que acaba com as areas pretas.
rem
rem CADA UM ESCREVE O SEU PROPRIO LOG, e ha uma espera entre os dois. A primeira
rem versao mandava os dois para o mesmo arquivo em sequencia e o segundo morria
rem com "The process cannot access the file because it is being used by another
rem process" -- o primeiro editor ainda nao tinha soltado o arquivo. O passo 2
rem nunca chegou a rodar, e o chao continuou preto.
rem
rem Nao e preciso ter Python instalado no Windows.
rem O Unreal precisa estar FECHADO.
rem ---------------------------------------------------------------------------
setlocal
set "ENGINE=C:\Program Files\Epic Games\UE_5.8"
set "PROJECT=%~dp0unreal\AshenHollow\AshenHollow.uproject"
set "SCRIPTS=%~dp0unreal\AshenHollow\Scripts"
set "FITLOG=%~dp0Fit-Log.txt"
set "ARTLOG=%~dp0Art-Log.txt"
set "UE=%ENGINE%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"

if not exist "%UE%" (
    echo Unreal 5.8 nao encontrado em "%ENGINE%".
    pause
    exit /b 2
)

echo.
echo  [1/2] Encaixando o mapa no terreno...
"%UE%" "%PROJECT%" -run=pythonscript -script="%SCRIPTS%\Fit-World.py" ^
    -unattended -nopause -nosplash -stdout -FullStdOutLogOutput > "%FITLOG%" 2>&1

rem Deixa o primeiro editor fechar de verdade antes de abrir o segundo: ele
rem segura o mutex do projeto por alguns segundos depois de terminar.
echo       (esperando o editor fechar)
ping -n 11 127.0.0.1 > nul

echo  [2/2] Vestindo o terreno e plantando a grama...
"%UE%" "%PROJECT%" -run=pythonscript -script="%SCRIPTS%\Dress-World.py" ^
    -unattended -nopause -nosplash -stdout -FullStdOutLogOutput > "%ARTLOG%" 2>&1

echo.
echo  ================ ENCAIXE DO MAPA ================
findstr /C:"AH_FIT" /C:"AH_SITE" "%FITLOG%" | findstr /V "LogInit"
echo.
echo  ================ MATERIAL E GRAMA ===============
if exist "%ARTLOG%" (
    findstr /C:"AH_ART" "%ARTLOG%" | findstr /V "LogInit"
) else (
    echo  O passo 2 nao chegou a rodar.
)
echo  =================================================
echo.
echo  As seis linhas AH_SITE tem que bater (erro menor que 4 m).
echo  Depois: abra o Unreal, menu Build ^> Build Paths, e salve o mapa.
echo.
echo  Logs completos: Fit-Log.txt e Art-Log.txt
pause
