@echo off
cd /d "%~dp0"
set "GODOT_EXE=%~dp0.tools\godot\Godot_v4.6.1-stable_win64_console.exe"
if not exist "%GODOT_EXE%" (
  echo Godot 4.6.1 is missing. See README.md for setup instructions.
  pause
  exit /b 1
)
"%GODOT_EXE%" --headless --path . --editor --import --quit
if errorlevel 1 exit /b 1
"%GODOT_EXE%" --headless --path . --script tests/test_runner.gd
if errorlevel 1 exit /b 1
"%GODOT_EXE%" --headless --path . --script tests/test_fifth_edition.gd
set "TEST_RESULT=%ERRORLEVEL%"
pause
exit /b %TEST_RESULT%
