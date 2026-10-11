@echo off
cd /d "%~dp0"
if not exist ".venv\Scripts\python.exe" goto nosetup

".venv\Scripts\python.exe" tools\train.py
echo.
pause
exit /b 0

:nosetup
echo Run setup.bat first.
pause
exit /b 1
