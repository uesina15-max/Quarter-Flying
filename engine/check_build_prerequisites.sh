#!/bin/bash

# Quarter Flying Build Prerequisites Check Script
# This script checks if your system meets the requirements to build the project

echo "========================================"
echo "Quarter Flying Build Prerequisites Check"
echo "========================================"
echo ""

errors=0
warnings=0

# Function to check command availability
check_command() {
    if command -v "$1" &> /dev/null; then
        return 0
    else
        return 1
    fi
}

# 1. Check CMake
echo "Checking CMake..."
if check_command cmake; then
    cmake_version=$(cmake --version | head -n1)
    echo "  ✓ CMake found: $cmake_version"
    
    # Check CMake version
    cmake_version_num=$(cmake --version | grep -oP '(?<=cmake version )\d+\.\d+' | head -n1)
    if [ "$(printf '%s\n' "3.15" "$cmake_version_num" | sort -V | head -n1)" != "3.15" ]; then
        echo "    ⚠ CMake version $cmake_version_num is older than recommended 3.15"
        warnings=$((warnings + 1))
    fi
else
    echo "  ✗ CMake not found. Please install CMake 3.15 or later."
    echo "    Ubuntu/Debian: sudo apt-get install cmake"
    echo "    Fedora/RHEL: sudo dnf install cmake"
    echo "    macOS: brew install cmake"
    errors=$((errors + 1))
fi
echo ""

# 2. Check C++ Compiler
echo "Checking C++ Compiler..."
if check_command g++; then
    gcc_version=$(g++ --version | head -n1)
    echo "  ✓ GCC found: $gcc_version"
    
    # Check GCC version for C++23 support
    gcc_version_num=$(g++ --version | grep -oP '(?<=gcc )\d+\.\d+' | head -n1)
    if [ "$(printf '%s\n' "11.0" "$gcc_version_num" | sort -V | head -n1)" != "11.0" ]; then
        echo "    ⚠ GCC version $gcc_version_num may not fully support C++23. Recommended: GCC 11+"
        warnings=$((warnings + 1))
    fi
elif check_command clang++; then
    clang_version=$(clang++ --version | head -n1)
    echo "  ✓ Clang found: $clang_version"
    
    # Check Clang version for C++23 support
    clang_version_num=$(clang++ --version | grep -oP '(?<=version )\d+\.\d+' | head -n1)
    if [ "$(printf '%s\n' "14.0" "$clang_version_num" | sort -V | head -n1)" != "14.0" ]; then
        echo "    ⚠ Clang version $clang_version_num may not fully support C++23. Recommended: Clang 14+"
        warnings=$((warnings + 1))
    fi
else
    echo "  ✗ C++ compiler not found. Please install GCC or Clang."
    echo "    Ubuntu/Debian: sudo apt-get install build-essential"
    echo "    Fedora/RHEL: sudo dnf groupinstall 'Development Tools'"
    echo "    macOS: xcode-select --install"
    errors=$((errors + 1))
fi
echo ""

# 3. Check Git
echo "Checking Git..."
if check_command git; then
    git_version=$(git --version)
    echo "  ✓ Git found: $git_version"
else
    echo "  ⚠ Git not found. Git is recommended for dependency management."
    echo "    Ubuntu/Debian: sudo apt-get install git"
    echo "    Fedora/RHEL: sudo dnf install git"
    echo "    macOS: brew install git"
    warnings=$((warnings + 1))
fi
echo ""

# 4. Check Python
echo "Checking Python..."
if check_command python3; then
    python_version=$(python3 --version)
    echo "  ✓ Python found: $python_version"
elif check_command python; then
    python_version=$(python --version)
    echo "  ✓ Python found: $python_version"
else
    echo "  ⚠ Python not found. Python is required for Python bindings."
    echo "    Ubuntu/Debian: sudo apt-get install python3"
    echo "    Fedora/RHEL: sudo dnf install python3"
    echo "    macOS: brew install python"
    warnings=$((warnings + 1))
fi
echo ""

# 5. Check Disk Space
echo "Checking Disk Space..."
disk_info=$(df -k . | tail -n1)
available_kb=$(echo $disk_info | awk '{print $4}')
available_mb=$((available_kb / 1024))
available_gb=$((available_mb / 1024))

echo "  Available disk space: ${available_gb}GB (${available_mb}MB)"

if [ $available_mb -lt 2048 ]; then
    echo "  ⚠ Low disk space. Recommended: 2GB+ free space"
    warnings=$((warnings + 1))
else
    echo "  ✓ Sufficient disk space"
fi
echo ""

# 6. Check Memory
echo "Checking System Memory..."
if check_command free; then
    mem_info=$(free -m | grep Mem)
    total_mb=$(echo $mem_info | awk '{print $2}')
    total_gb=$((total_mb / 1024))
    
    echo "  Total memory: ${total_gb}GB (${total_mb}MB)"
    
    if [ $total_mb -lt 4096 ]; then
        echo "  ⚠ Low memory. Recommended: 4GB+ RAM"
        warnings=$((warnings + 1))
    else
        echo "  ✓ Sufficient memory"
    fi
else
    echo "  ⚠ Cannot check memory on this system"
fi
echo ""

# 7. Check Internet Connection
echo "Checking Internet Connection..."
if check_command curl; then
    if curl -s --head https://github.com | head -n 1 | grep -q "HTTP"; then
        echo "  ✓ Internet connection available"
    else
        echo "  ⚠ Cannot reach GitHub. Dependency downloads may fail."
        echo "    Check your internet connection or firewall settings."
        warnings=$((warnings + 1))
    fi
elif check_command wget; then
    if wget -q --spider https://github.com; then
        echo "  ✓ Internet connection available"
    else
        echo "  ⚠ Cannot reach GitHub. Dependency downloads may fail."
        echo "    Check your internet connection or firewall settings."
        warnings=$((warnings + 1))
    fi
else
    echo "  ⚠ Neither curl nor wget found. Cannot check internet connection."
    warnings=$((warnings + 1))
fi
echo ""

# 8. Platform-specific checks
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    echo "Platform: Linux"
    
    # Check for OpenGL development libraries
    echo "Checking OpenGL development libraries..."
    if [ -f /usr/include/GL/gl.h ] || [ -f /usr/include/GL/glew.h ]; then
        echo "  ✓ OpenGL headers found"
    else
        echo "  ⚠ OpenGL development libraries may be missing."
        echo "    Ubuntu/Debian: sudo apt-get install libgl1-mesa-dev libglew-dev"
        warnings=$((warnings + 1))
    fi
    
elif [[ "$OSTYPE" == "darwin"* ]]; then
    echo "Platform: macOS"
    
    # Check for Xcode Command Line Tools
    if check_command xcodebuild; then
        echo "  ✓ Xcode Command Line Tools found"
    else
        echo "  ⚠ Xcode Command Line Tools not found."
        echo "    Run: xcode-select --install"
        errors=$((errors + 1))
    fi
    
    # Check architecture
    arch=$(uname -m)
    echo "  Architecture: $arch"
    
fi
echo ""

# Summary
echo "========================================"
echo "Summary"
echo "========================================"

if [ $errors -eq 0 ] && [ $warnings -eq 0 ]; then
    echo "✓ All prerequisites met! You can proceed with the build."
    exit 0
elif [ $errors -eq 0 ]; then
    echo "⚠ $warnings warning(s) found. Build may work but with limitations."
    exit 0
else
    echo "✗ $errors error(s) found. Please fix the issues before building."
    exit 1
fi
