@echo off
setlocal
rem O mapa NAO e escrito aqui. Ele e lido de Config\DefaultEngine.ini, que e a
rem unica fonte da verdade: quando o mapa padrao do projeto muda, este atalho
rem acompanha sozinho. Antes o nome do mapa estava fixo nesta linha, e trocar o
rem mapa do projeto nao mudava nada aqui -- o atalho continuava abrindo o antigo.
set "AH_ROOT=%~dp0unreal\AshenHollow"
set "AH_EDITOR=C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
if not exist "%AH_EDITOR%" (
    echo Unreal 5.8 nao encontrado em "%AH_EDITOR%".
    pause
    exit /b 2
)

set "AH_MAP="
for /f "usebackq tokens=2 delims==" %%M in (`findstr /b /c:"GameDefaultMap=" "%AH_ROOT%\Config\DefaultEngine.ini"`) do set "AH_MAP=%%M"
if not defined AH_MAP (
    echo Nao achei GameDefaultMap em Config\DefaultEngine.ini.
    pause
    exit /b 3
)

rem /Game/AshenHollow/Maps/Nome  ->  Content\AshenHollow\Maps\Nome.umap
set "AH_FILE=%AH_MAP:/Game/=%"
set "AH_FILE=%AH_FILE:/=\%"
if not exist "%AH_ROOT%\Content\%AH_FILE%.umap" (
    echo Mapa "%AH_MAP%" nao existe em disco.
    echo Gere-o com Scripts\Build-Arena.ps1, ou aponte GameDefaultMap para um mapa que exista.
    pause
    exit /b 4
)

echo Abrindo %AH_MAP%
start "Ashen Hollow" "%AH_EDITOR%" "%AH_ROOT%\AshenHollow.uproject" "%AH_MAP%" -game -windowed -ResX=1280 -ResY=720
