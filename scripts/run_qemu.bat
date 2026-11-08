@echo off
rem ==============================================================================
rem Turnkey Windows QEMU Launch Script for QNX 8.0 Virtual Target
rem ==============================================================================

set "MEM_SIZE=4096M"
set "SMP_CORES=4"
set "QEMU_IMAGE=images\qnx800-virtual-target.qcow2"

echo ======================================================================
echo  Launching QNX Neutrino 8.0 Virtual Target in QEMU (x86_64)
echo  Memory: %MEM_SIZE% ^| SMP Cores: %SMP_CORES%
echo ======================================================================

where qemu-system-x86_64 >nul 2>nul
if %ERRORLEVEL% equ 0 (
    qemu-system-x86_64 ^
        -M q35 ^
        -smp %SMP_CORES% ^
        -m %MEM_SIZE% ^
        -drive file="%QEMU_IMAGE%",if=virtio,format=qcow2 ^
        -netdev user,id=net0,hostfwd=tcp::2222-:22 ^
        -device virtio-net-pci,netdev=net0 ^
        -nographic
) else (
    echo [!] qemu-system-x86_64 is not installed or not in PATH.
    echo     You can run the interactive standalone host profiler directly:
    echo     python demo_qnx_top.py
    pause
)
