# `qnx-top`: Real-Time Microkernel Telemetry & IPC Profiler for QNX® 8.0

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Target: QNX 8.0](https://img.shields.io/badge/Target-QNX%20Neutrino%208.0-red.svg)](https://qnx.software/en/developers/get-started/qnx-everywhere)
[![Hackathon Category](https://img.shields.io/badge/Category-Developer%20Tooling%20%26%20Host%20Tools-orange.svg)](#)
[![CI Build](https://github.com/Miladmet/qnx-top/actions/workflows/ci.yml/badge.svg)](https://github.com/Miladmet/qnx-top/actions)
[![Commercial Consulting](https://img.shields.io/badge/Services-QNX%208.0%20Consulting-success.svg)](docs/commercial/CONSULTING.md)

**`qnx-top`** is a high-performance terminal observability tool and microkernel profiler built specifically for **BlackBerry QNX Neutrino 8.0 (QNX Everywhere)**. 

While traditional Unix tools (`top`, `htop`) focus solely on monolithic CPU/RAM metrics, `qnx-top` inspects the core foundation of QNX microkernel systems: **synchronous message-passing latency, channel blocking states, Adaptive Partitioning budgets, and `/dev` resource manager trees**.

---

## 📺 Demonstration Video & Highlights

* **Hackathon Demonstration Video:** [Watch on YouTube](https://youtu.be/DEMO_VIDEO_ID) *(2 minutes 47 seconds — generated via `build_demo_video.py`)*
* **Category:** Developer Tooling & Host Tools
* **Compatibility:** QNX 8.0 (x86_64, aarch64, QEMU Virtual Target)

```
==========================================================================================
 QNX-TOP v1.0.0 │ QNX NEUTRINO 8.0 [LIVE KERNEL] │ Cores: 4
==========================================================================================
Core 0: [64.2%] (CtxSw:   3920/s)   │   Core 1: [32.1%] (CtxSw:   2140/s)
Core 2: [51.4%] (CtxSw:   3310/s)   │   Core 3: [14.8%] (CtxSw:   1020/s)
RAM: 812 MB / 4096 MB (19.8%) │ ResMgr Pool: 48200 KB │ Total Msg Rate: 97420 msgs/sec
------------------------------------------------------------------------------------------
PID    TID  PRI   STATE             CPU%    CHID   PULSES/S   COMMAND                  
1      1    255   RUNNING            0.8%  0x01   2410       [kernel/procmgr]         
124    1    21    RECEIVE            2.4%  0x03   84102      /sbin/devc-ser8250       
256    1    30    RECEIVE            4.1%  0x04   12940      /sbin/io-pkt-v6-hc       
312    1    18    REPLY-blocked      1.2%  0x02   312        /sbin/devb-eide (Stall: 12.4ms)
480    1    40    RUNNING           14.2%  0x08   450119     /usr/bin/safety_actuator 

[Channel & Connection Matrix (Synchronous MsgSend/MsgReceive)]
CHID     OWNER PID    RECEIVE THREAD     BOUND CLIENTS    MSG/SEC    STATUS                
------------------------------------------------------------------------------------------
0x01     1            TID 2  (PRI 250)   16               12400      DETERMINISTIC         
0x02     312          TID 1  (PRI 18 )   3                140        STALL (12.4ms)        
0x03     124          TID 1  (PRI 21 )   5                1820       DETERMINISTIC         
0x04     256          TID 1  (PRI 30 )   22               28950      DETERMINISTIC         
0x08     480          TID 1  (PRI 40 )   2                50000      DETERMINISTIC         
```

---

## 🚀 Key Features Built for QNX 8.0

1. **Native Multi-Thread State Matrix:**
   - Traverses `/proc/<pid>` and iterates all threads (`DCMD_PROC_STATUS`).
   - Surfaces QNX-specific scheduling states: `RECEIVE-blocked`, `REPLY-blocked`, `SEND-blocked`, `NANOSLEEP`, and `READY`.
   - Flags threads stalled waiting on remote server processes (`REPLY-blocked`) with delta millisecond latency tracking.
2. **Channel & Connection Matrix (`chid` / `coid`):**
   - Live inspection of synchronous `MsgSend()`, `MsgReceive()`, and `MsgReply()` queues.
   - Detects channel bottlenecks and priority inversion conditions across client-server pipelines.
3. **Adaptive Partitioning System (APS) Monitor:**
   - Real-time monitoring of CPU partition budgets (`Safety_Critical`, `Networking_IO`, `System_Kernel`).
   - Visual verification of hard real-time scheduling boundaries under high CPU stress.
4. **Native QNX Resource Manager (`/dev/qnx_top`):**
   - Implements a full QNX resource manager with dedicated background POSIX dispatch thread (`dispatch_block` $\rightarrow$ `dispatch_handler`).
   - Clean POSIX `_IO_READ` semantics with EOF handling and dynamic heap allocation.
   - Allows any local process or remote host tool to inspect telemetry via `cat /dev/qnx_top`.

---

## 🛠️ Build and Run Instructions

### Prerequisites
* QNX Software Development Platform (SDP) 8.0 (Free access via [QNX Everywhere](https://qnx.software/en/developers/get-started/qnx-everywhere)).
* CMake 3.20+ and QNX Toolchain (`qcc`).
* *(Optional)* QEMU for x86_64 target emulation.

### 1. Building for QNX 8.0 Target
```bash
# 1. Source QNX SDP 8.0 Environment
source /opt/qnx800/qnxsdp-env.sh

# 2. Configure with QNX Toolchain
cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/qnx800-x86_64.toolchain.cmake -DCMAKE_BUILD_TYPE=Release

# 3. Compile native binary
cmake --build build -j$(nproc)
```

### 2. Building and Testing on Host (Linux / macOS)
`qnx-top` builds cleanly in Host Compatibility Mode for local testing and CI verification:
```bash
# Configure, build, and run unit tests
cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

### 3. Running in QEMU (1-Command Verification)
```bash
# Launch pre-configured QNX 8.0 QEMU virtual machine
./scripts/run_qemu.sh

# Inside the QNX 8.0 Target Shell:
qnx-top -d 1.0
```

### 4. Running the Standalone Interactive Terminal Profiler
If evaluating on a host machine without an active QNX toolchain:
```bash
python demo_qnx_top.py
```

---

## 📁 Repository Structure

```
qnx-top/
├── CMakeLists.txt                 # Multi-platform CMake build configuration
├── LICENSE                        # MIT Open Source License
├── README.md                      # Technical documentation & usage
├── demo_qnx_top.py                # Standalone interactive Python TUI
├── index.html                     # Web landing page
├── cmake/                         # QNX 8.0 toolchain files (x86_64 and aarch64)
├── include/                       # Public C headers (qnx_top.h)
├── src/                           # Native C implementation (collector, resmgr, main)
├── tests/                         # Unit tests (state mapping & snapshot validation)
├── scripts/                       # Turnkey QEMU launch and build scripts
└── docs/
    └── commercial/                # Services catalog & consulting documentation
```

---

## 💼 Commercial Support & Specialized Consulting

Building a mission-critical Software-Defined Vehicle (SDV), medical robot, or aerospace control system on QNX 8.0?

We offer fixed-scope engineering packages and advisory retainers:
* **Real-Time Jitter & Microkernel Latency Audits** (Identifying and resolving deadline spikes and IPC stalls)
* **Custom QNX 8.0 Resource Manager Architecture** (Turnkey `/dev` drivers with POSIX I/O handlers)
* **Fractional QNX Systems Architect Retainer**

👉 **[View Full Consulting Packages & Rates in docs/commercial/CONSULTING.md](docs/commercial/CONSULTING.md)**  
👉 **Inquiries:** `consulting@qnx-top.dev`

---

## 📄 License

This project is licensed under the **MIT License** — see the [LICENSE](LICENSE) file for details.

*QNX, Neutrino, and Momentics are trademarks or registered trademarks of BlackBerry Limited in the United States and other countries.*
