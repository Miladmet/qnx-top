# `qnx-top`: Real-Time Microkernel Telemetry & IPC Inspector for QNX® 8.0

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Target: QNX 8.0](https://img.shields.io/badge/Target-QNX%20Neutrino%208.0-red.svg)](https://qnx.software/en/developers/get-started/qnx-everywhere)
[![Hackathon Category](https://img.shields.io/badge/Category-Developer%20Tooling%20%26%20Host%20Tools-orange.svg)](#)
[![Commercial Consulting](https://img.shields.io/badge/Services-QNX%208.0%20Consulting-success.svg)](CONSULTING.md)

**`qnx-top`** is a high-performance terminal observability tool and microkernel profiler built specifically for **BlackBerry QNX Neutrino 8.0 (QNX Everywhere)**. 

While traditional Unix tools (`top`, `htop`) focus solely on monolithic CPU/RAM metrics, `qnx-top` inspects the core foundation of QNX microkernel systems: **synchronous message-passing latency, channel blocking states, Adaptive Partitioning budgets, and `/dev` resource manager trees**.

---

## 📺 Demonstration Video & Highlights

* **Hackathon Demonstration Video:** [Watch on YouTube](https://youtube.com/watch?v=DEMO_VIDEO_ID) *(< 3 minutes)*
* **Category:** Developer Tooling & Host Tools
* **Compatibility:** QNX 8.0 (x86_64, aarch64, QEMU Virtual Target)

```
  QNX-TOP v1.0  │  Target: QNX Neutrino 8.0.0 (x86_64 SMP)  │  Host: QEMU-Virt  │  11:42:15  
  Core 0: [■■■■■■■■■■······] 64.2% (CtxSw: 3,920/s)   │   Core 1: [■■■■■···········] 32.1% (CtxSw: 2,140/s)
  Core 2: [■■■■■■■■········] 51.4% (CtxSw: 3,310/s)   │   Core 3: [■■··············] 14.8% (CtxSw: 1,020/s)
  RAM: [■■■■················] 812MB / 4096MB (19.8%)  │ ResMgr Pool: 48.2 MB │ Microkernel: HEALTHY
──────────────────────────────────────────────────────────────────────────────────────────────────
   [F1: Process Overview]    F2: Channels & IPC Matrix    F3: Adaptive Partitioning    F4: ResMgr Tree   
──────────────────────────────────────────────────────────────────────────────────────────────────
 PID    TID  PRI   STATE             CPU%   MEM      CHID   PULSES   COMMAND                  
 1      1    255r  RUNNING           0.8    1.2M     0x01   2,410    [kernel/procmgr]         
 124    1    21f   RECEIVE           2.4    8.4M     0x03   84,102   /sbin/devc-ser8250       
 256    1    30f   RECEIVE           4.1    16.1M    0x04   12,940   /sbin/io-pkt-v6-hc       
 312    1    18f   REPLY-blocked     1.2    6.2M     0x02   312      /sbin/devb-eide          
 480    1    40r   RUNNING          14.2   24.8M     0x08   450,119  /usr/bin/safety_actuator 
```

---

## 🚀 Key Features Built for QNX 8.0

1. **Native Thread Blocking States:**
   - Detects QNX-specific scheduling states: `RECEIVE-blocked`, `REPLY-blocked`, `SEND-blocked`, `NANOSLEEP`, and `READY`.
   - Flags threads stalled waiting on remote server processes (`REPLY-blocked`) with real-time microsecond latency tracking.
2. **Channel & Connection Matrix (`chid` / `coid`):**
   - Live inspection of synchronous `MsgSend()`, `MsgReceive()`, and `MsgReply()` queues.
   - Immediate detection of channel bottlenecks and priority inversion conditions.
3. **Adaptive Partitioning System (APS) Monitor:**
   - Real-time gauge bars of CPU partition budgets (`Safety_Critical`, `Networking_IO`, `System_Kernel`).
   - Visual verification of hard real-time scheduling boundaries under high CPU stress.
4. **Resource Manager (`/dev`) Explorer:**
   - Catalog of all registered POSIX pathnames mapped to owner PIDs and active handlers (`_IO_READ`, `_IO_WRITE`, `_IO_DEVCTL`).

---

## 🛠️ Build and Run Instructions

### Prerequisites
* QNX Software Development Platform (SDP) 8.0 (Free access via [QNX Everywhere](https://qnx.software/en/developers/get-started/qnx-everywhere)).
* QEMU for x86_64 or aarch64 target emulation.
* CMake 3.22+ and QNX Toolchain (`qcc`).

### 1. Building for QNX 8.0 Target
```bash
# Set up QNX SDP 8.0 Environment
source /opt/qnx800/qnxsdp-env.sh

# Configure and compile with QNX qcc compiler
mkdir build && cd build
cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/qnx800-x86_64.toolchain.cmake ..
make -j$(nproc)
```

### 2. Running in QEMU (1-Command Verification)
```bash
# Launch pre-configured QNX 8.0 QEMU virtual machine
./scripts/run_qemu.sh

# Inside the QNX 8.0 Target Shell:
qnx-top
```

### 3. Running the Standalone Interactive Visualizer (Host Demo)
If evaluating on a host machine without an active QNX target:
```bash
python demo_qnx_top.py
```

---

## 💼 Commercial Support & Specialized Consulting

Building a mission-critical Software-Defined Vehicle (SDV), medical robot, or aerospace control system on QNX 8.0?

We offer fixed-scope engineering packages and advisory retainers:
* **Real-Time Jitter & Microkernel Latency Audits** (Identifying and resolving deadline spikes and IPC stalls)
* **Custom QNX 8.0 Resource Manager Architecture** (Turnkey `/dev` drivers with POSIX I/O handlers)
* **Fractional QNX Systems Architect Retainer**

👉 **[View Full Consulting Packages & Rates in CONSULTING.md](CONSULTING.md)**  
👉 **Inquiries:** `consulting@qnx-top.dev`

---

## 📄 License

This project is licensed under the **MIT License** — see the [LICENSE](LICENSE) file for details.

*QNX, Neutrino, and Momentics are trademarks or registered trademarks of BlackBerry Limited in the United States and other countries.*
