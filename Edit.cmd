@echo off
cd /d "%~dp0"
set "GODOT_EXE=%~dp0.tools\godot\Godot_v4.6.1-stable_win64.exe"
if not exist "%GODOT_EXE%" (
  echo Godot 4.6.1 is missing. See README.md for setup instructions.
  pause
  exit /b 1
)
start "Ashen Hollow Editor" "%GODOT_EXE%" --editor --path "%~dp0."
