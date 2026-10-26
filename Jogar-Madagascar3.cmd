@echo off
rem DreamWorks Madagascar 3: The Video Game — Launcher PC Port
setlocal
cd /d "%~dp0"
set "BIN_DIR=%~dp0project\out\build\win-amd64-release"
if exist "%~dp0build-location.txt" set /p BIN_DIR=<"%~dp0build-location.txt"
if not exist "%BIN_DIR%\madagascar3.exe" (
  echo Executavel nao encontrado em %BIN_DIR%.
  echo Compile o projeto primeiro com build-madagascar.cmd.
  pause
  exit /b 1
)
cd /d "%BIN_DIR%"
start "" madagascar3.exe "--game_data_root=%~dp0game_files" %*
