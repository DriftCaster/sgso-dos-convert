@echo off
cd /d "%~dp0"
py -3 -c "import numpy" 2>nul || py -3 -m pip install --quiet numpy
py -3 sgso_convert.py %*
if errorlevel 1 pause
