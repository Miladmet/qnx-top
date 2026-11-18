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

if ! command -v qemu-system-x86_64 &> /dev/null; then
    echo "[!] qemu-system-x86_64 not found in PATH."
    echo "    Please install QEMU or add it to PATH."
    exit 1
fi

if [ ! -f "$QEMU_IMAGE" ]; then
    echo ""
    echo "[!] QNX disk image not found at: $QEMU_IMAGE"
    echo ""
    echo "To obtain the QNX 8.0 Virtual Target disk image:"
    echo "1. Register for free at: https://qnx.software/en/developers/get-started/qnx-everywhere"
    echo "2. Download the QNX 8.0 Virtual Target (qcow2 format)"
    echo "3. Save it to: $QEMU_IMAGE (or specify QNX_IMAGE=/path/to/image.qcow2)"
    echo ""
    exit 1
fi

qemu-system-x86_64 \
    -M q35,accel=kvm:tcg \
    -cpu host \
    -smp "$SMP_CORES" \
    -m "$MEM_SIZE" \
    -drive file="$QEMU_IMAGE",if=virtio,format=qcow2 \
    -netdev user,id=net0,hostfwd=tcp::2222-:22,hostfwd=tcp::8080-:8080 \
    -device virtio-net-pci,netdev=net0 \
    -nographic
