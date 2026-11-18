@echo off
rem ==============================================================================
rem Turnkey Windows QEMU Launch Script for QNX 8.0 Virtual Target
rem ==============================================================================

set "MEM_SIZE=4096M"
set "SMP_CORES=4"
set "QEMU_IMAGE=images\qnx800-virtual-target.qcow2"

rem Locate QEMU binary
set "QEMU_BIN=qemu-system-x86_64"
where %QEMU_BIN% >nul 2>nul
if %ERRORLEVEL% neq 0 (
    if exist "C:\Program Files\qemu\qemu-system-x86_64.exe" (
        set "QEMU_BIN=C:\Program Files\qemu\qemu-system-x86_64.exe"
    )
)

echo ======================================================================
echo  QNX Neutrino 8.0 Virtual Target Emulator Launcher
echo  QEMU Executable: %QEMU_BIN%
echo ======================================================================

if not exist "%QEMU_IMAGE%" (
    echo.
    echo [!] QNX disk image not found at: %QEMU_IMAGE%
    echo.
    echo To get the virtual target disk image:
    echo 1. Register for free at: https://qnx.software/en/developers/get-started/qnx-everywhere
    echo 2. Download the QNX 8.0 Virtual Target (qcow2)
    echo 3. Save it to: %~dp0..\images\qnx800-virtual-target.qcow2
    echo.
    echo In the meantime, you can run the standalone host profiler:
    echo python demo_qnx_top.py
    echo.
    pause
    exit /b 1
)

echo Booting QNX 8.0 with %MEM_SIZE% RAM and %SMP_CORES% Cores...
"%QEMU_BIN%" ^
    -M q35 ^
    -smp %SMP_CORES% ^
    -m %MEM_SIZE% ^
    -drive file="%QEMU_IMAGE%",if=virtio,format=qcow2 ^
    -netdev user,id=net0,hostfwd=tcp::2222-:22 ^
    -device virtio-net-pci,netdev=net0 ^
    -nographic

pause
