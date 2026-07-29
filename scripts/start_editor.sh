#!/bin/bash
cd "$(dirname "$0")/engine"
python3 editor/main.py
if [ $? -ne 0 ]; then
    echo "Error running editor"
    exit 1
fi
