#!/bin/bash
# ==============================================================================
# Turnkey QEMU Launch Script for QNX 8.0 (QNX Everywhere Virtual Target)
# ==============================================================================

set -e

TARGET_ARCH=${1:-"x86_64"}
QEMU_IMAGE=${QNX_IMAGE:-"./images/qnx800-virtual-target.qcow2"}
MEM_SIZE=${QEMU_MEM:-"4096M"}
SMP_CORES=${QEMU_SMP:-"4"}

echo "======================================================================"
echo " Launching QNX Neutrino 8.0 Virtual Target in QEMU ($TARGET_ARCH)"
echo " Memory: $MEM_SIZE | SMP Cores: $SMP_CORES"
echo "======================================================================"

if command -v qemu-system-x86_64 &> /dev/null; then
    qemu-system-x86_64 \
        -M q35,accel=kvm:tcg \
        -cpu host \
        -smp "$SMP_CORES" \
        -m "$MEM_SIZE" \
        -drive file="$QEMU_IMAGE",if=virtio,format=qcow2 \
        -netdev user,id=net0,hostfwd=tcp::2222-:22,hostfwd=tcp::8080-:8080 \
        -device virtio-net-pci,netdev=net0 \
        -nographic
else
    echo "[!] qemu-system-x86_64 not found in PATH."
    echo "    To test qnx-top without QEMU, run the interactive host visualizer:"
    echo "    python demo_qnx_top.py"
fi
