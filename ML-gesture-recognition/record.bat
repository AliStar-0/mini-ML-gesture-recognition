@echo off
cd /d "%~dp0"
if not exist ".venv\Scripts\python.exe" goto nosetup

set "NAME="
set /p "NAME=Gesture name, for example circle: "
if "%NAME%"=="" exit /b 1

".venv\Scripts\python.exe" tools\collect.py --label "%NAME%"
echo.
pause
exit /b 0

:nosetup
echo Run setup.bat first.
pause
exit /b 1
