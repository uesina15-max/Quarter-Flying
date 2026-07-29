# Quarter Flying Build Prerequisites Check Script
# This script checks if your system meets the requirements to build the project

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Quarter Flying Build Prerequisites Check" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

$errors = 0
$warnings = 0

# Function to check command availability
function Test-Command {
    param($Command)
    try {
        Get-Command $Command -ErrorAction Stop | Out-Null
        return $true
    }
    catch {
        return $false
    }
}

# 1. Check CMake
Write-Host "Checking CMake..." -ForegroundColor Yellow
if (Test-Command "cmake") {
    $cmakeVersion = cmake --version
    Write-Host "  ✓ CMake found: $($cmakeVersion[0])" -ForegroundColor Green
} else {
    Write-Host "  ✗ CMake not found. Please install CMake 3.15 or later." -ForegroundColor Red
    Write-Host "    Download from: https://cmake.org/download/" -ForegroundColor Gray
    $errors++
}
Write-Host ""

# 2. Check C++ Compiler
Write-Host "Checking C++ Compiler..." -ForegroundColor Yellow
$vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $vsWhere) {
    $vsPath = & $vsWhere -latest -property installationPath
    if ($vsPath) {
        Write-Host "  ✓ Visual Studio found at: $vsPath" -ForegroundColor Green
        
        # Check for VS 2022 or later
        $vsVersion = & $vsWhere -latest -property installationVersion
        Write-Host "    Version: $vsVersion" -ForegroundColor Cyan
        
        if ([version]$vsVersion -lt [version]"17.0") {
            Write-Host "    ⚠ Visual Studio 2022 (17.0+) recommended for C++23 support" -ForegroundColor Yellow
            $warnings++
        }
    } else {
        Write-Host "  ✗ Visual Studio not found. Please install Visual Studio 2022 or later." -ForegroundColor Red
        Write-Host "    Download from: https://visualstudio.microsoft.com/" -ForegroundColor Gray
        Write-Host "    Required workload: Desktop development with C++" -ForegroundColor Gray
        $errors++
    }
} else {
    Write-Host "  ✗ Visual Studio not found. Please install Visual Studio 2022 or later." -ForegroundColor Red
    Write-Host "    Download from: https://visualstudio.microsoft.com/" -ForegroundColor Gray
    Write-Host "    Required workload: Desktop development with C++" -ForegroundColor Gray
    $errors++
}
Write-Host ""

# 3. Check Git
Write-Host "Checking Git..." -ForegroundColor Yellow
if (Test-Command "git") {
    $gitVersion = git --version
    Write-Host "  ✓ Git found: $gitVersion" -ForegroundColor Green
} else {
    Write-Host "  ⚠ Git not found. Git is recommended for dependency management." -ForegroundColor Yellow
    Write-Host "    Download from: https://git-scm.com/downloads" -ForegroundColor Gray
    $warnings++
}
Write-Host ""

# 4. Check Python
Write-Host "Checking Python..." -ForegroundColor Yellow
if (Test-Command "python") {
    $pythonVersion = python --version
    Write-Host "  ✓ Python found: $pythonVersion" -ForegroundColor Green
} elseif (Test-Command "python3") {
    $pythonVersion = python3 --version
    Write-Host "  ✓ Python found: $pythonVersion" -ForegroundColor Green
} else {
    Write-Host "  ⚠ Python not found. Python is required for Python bindings." -ForegroundColor Yellow
    Write-Host "    Download from: https://www.python.org/downloads/" -ForegroundColor Gray
    $warnings++
}
Write-Host ""

# 5. Check Disk Space
Write-Host "Checking Disk Space..." -ForegroundColor Yellow
$drive = $PSScriptRoot.Substring(0, 1)
$disk = Get-PSDrive -Name $drive
$freeSpaceGB = [math]::Round($disk.Free / 1GB, 2)
Write-Host "  Drive $drive`: $freeSpaceGB GB free" -ForegroundColor Cyan

if ($freeSpaceGB -lt 2) {
    Write-Host "  ⚠ Low disk space. Recommended: 2GB+ free space" -ForegroundColor Yellow
    $warnings++
} else {
    Write-Host "  ✓ Sufficient disk space" -ForegroundColor Green
}
Write-Host ""

# 6. Check Memory
Write-Host "Checking System Memory..." -ForegroundColor Yellow
$totalMemoryGB = [math]::Round((Get-CimInstance Win32_ComputerSystem).TotalPhysicalMemory / 1GB, 2)
Write-Host "  Total Memory: $totalMemoryGB GB" -ForegroundColor Cyan

if ($totalMemoryGB -lt 4) {
    Write-Host "  ⚠ Low memory. Recommended: 4GB+ RAM" -ForegroundColor Yellow
    $warnings++
} else {
    Write-Host "  ✓ Sufficient memory" -ForegroundColor Green
}
Write-Host ""

# 7. Check Internet Connection
Write-Host "Checking Internet Connection..." -ForegroundColor Yellow
try {
    $response = Invoke-WebRequest -Uri "https://github.com" -UseBasicParsing -TimeoutSec 5 -ErrorAction Stop
    Write-Host "  ✓ Internet connection available" -ForegroundColor Green
} catch {
    Write-Host "  ⚠ Cannot reach GitHub. Dependency downloads may fail." -ForegroundColor Yellow
    Write-Host "    Check your internet connection or firewall settings." -ForegroundColor Gray
    $warnings++
}
Write-Host ""

# Summary
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Summary" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan

if ($errors -eq 0 -and $warnings -eq 0) {
    Write-Host "✓ All prerequisites met! You can proceed with the build." -ForegroundColor Green
    exit 0
} elseif ($errors -eq 0) {
    Write-Host "⚠ $warnings warning(s) found. Build may work but with limitations." -ForegroundColor Yellow
    exit 0
} else {
    Write-Host "✗ $errors error(s) found. Please fix the issues before building." -ForegroundColor Red
    exit 1
}
