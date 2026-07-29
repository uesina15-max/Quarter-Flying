@echo off
cd /d "%~dp0\engine"
python editor/main.py
if errorlevel 1 (
    echo Error running editor
    pause
)
