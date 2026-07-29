@echo off
echo Restoring legacy GLFW main...
cd /d "%~dp0\engine"
git checkout HEAD -- app/main.cpp app/DemoScene.cpp
echo Legacy main restored.
echo Please rebuild with: cmake -DBUILD_LEGACY_MAIN=ON ..
pause
