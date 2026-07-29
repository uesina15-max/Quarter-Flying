#!/bin/bash
echo "Restoring legacy GLFW main..."
cd "$(dirname "$0")/engine"
git checkout HEAD -- app/main.cpp app/DemoScene.cpp
echo "Legacy main restored."
echo "Please rebuild with: cmake -DBUILD_LEGACY_MAIN=ON .."
