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

rem ---------------------------------------------------------------------------
rem A DLL e mais velha que o codigo? Entao o jogo que abrir NAO e o codigo atual.
rem Isso ja custou um dia inteiro: um build falhou, a Unreal continuou usando a
rem DLL antiga sem dizer nada, e passamos a noite discutindo bugs de um jogo que
rem ninguem estava rodando. Um atalho que abre silenciosamente a versao errada e
rem pior do que um atalho que nao abre.
rem ---------------------------------------------------------------------------
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0Check-Fresh.ps1" -Root "%AH_ROOT%"
if errorlevel 2 (
    echo  Rode Build-Now.bat primeiro.
    pause
    exit /b 5
)
if errorlevel 1 (
    choice /c SN /n /m "  Abrir mesmo assim a versao antiga? [S/N] "
    if errorlevel 2 exit /b 6
)

echo Abrindo %AH_MAP%
start "Ashen Hollow" "%AH_EDITOR%" "%AH_ROOT%\AshenHollow.uproject" "%AH_MAP%" -game -windowed -ResX=1280 -ResY=720
