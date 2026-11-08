# `qnx-top`: Real-Time Microkernel Telemetry & IPC Profiler for QNX® 8.0

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Target: QNX 8.0](https://img.shields.io/badge/Target-QNX%20Neutrino%208.0-red.svg)](https://qnx.software/en/developers/get-started/qnx-everywhere)
[![Hackathon Category](https://img.shields.io/badge/Category-Developer%20Tooling%20%26%20Host%20Tools-orange.svg)](#)
[![CI Build](https://github.com/Miladmet/qnx-top/actions/workflows/ci.yml/badge.svg)](https://github.com/Miladmet/qnx-top/actions)
[![Commercial Consulting](https://img.shields.io/badge/Services-QNX%208.0%20Consulting-success.svg)](docs/commercial/CONSULTING.md)

**`qnx-top`** is a terminal diagnostic tool for **BlackBerry QNX Neutrino 8.0 (QNX Everywhere)**.

Unlike generic `top`-style tools, it looks at QNX microkernel behaviour: which threads are **REPLY-blocked** on which **server channel**, for how long, plus per-process and per-thread CPU deltas, resident memory, APS presence and the `/dev` namespace.

> **Metric honesty:** every number is either measured or printed as `N/A`. Nothing is estimated or filled in with a default. On a non-QNX host the program runs in a clearly labelled **simulated** mode (see below).

---

## What is measured on a QNX 8.0 target

| Area | Source | Status |
|---|---|---|
| Process list, parent PID, thread count | `/proc/<pid>/as` + `DCMD_PROC_INFO` | Measured |
| Process name | `DCMD_PROC_MAPDEBUG_BASE` | Measured |
| Per-thread priority, policy, scheduler state, last CPU | `DCMD_PROC_TIDSTATUS` (all threads, up to 32 per process; the rest are counted but not listed) | Measured (`last_cpu` only if the SDP header has it) |
| Per-process and per-thread CPU% | Delta of `sutime` between samples, divided by wall time and core count (share of total machine capacity) | Measured (`N/A` on the first sample) |
| Address-space size | `fstat()` on `/proc/<pid>/as` | Measured |
| Resident memory (RSS) | `DCMD_PROC_PAGEDATA`: total size of page runs flagged `PG_HWMAPPED` | Measured; shared pages count in every process that maps them. `N/A` if the page map cannot be read |
| REPLY-block duration | **Kernel:** `nsec_since_block` when the SDP header has it (detected by CMake), millisecond resolution. **Fallback:** time since qnx-top **first observed** the thread in `STATE_REPLY`, a lower bound | Measured. Every value is labelled `(kernel)` or `(sampled)` |
| Server channel for a blocked client | `blocked.connect.pid` / `blocked.connect.chid` of the REPLY-blocked thread | Measured |
| Server receive thread / priority | Thread in `STATE_RECEIVE` with matching `blocked.channel.chid` | Measured when such a thread is seen |
| Core count | `syspage` | Measured |
| Total / used RAM | `sysconf(_SC_PHYS_PAGES / _SC_AVPHYS_PAGES)` | Measured when the libc provides them, else `N/A` |
| APS present or not | `SchedCtl(SCHED_APS_QUERY_PARTITION)` probe | Detected only: `Active (per-partition budgets N/A)` or `Inactive / Not Configured` |
| `/dev` entries | `readdir("/dev")` | Names only |

**Reported as `N/A` (not implemented):** per-core utilization and context-switch rates (need `TraceEvent`), channel message rates, pulse counts, resource-manager / shared-memory pool sizes, APS partition budgets and jitter, `/dev` owner / handler / request-rate details, priority-inversion detection.

> The native QNX code path has **not yet been compiled or run against the QNX SDP 8.0 headers on a real target**. The host build, tests and CI cover only the simulated path and the platform-independent stall tracker. Treat native output as unvalidated until you have run it on your target.

### Sample output on a QNX target (illustrative layout)

```
==========================================================================================
 QNX-TOP v1.1.0 | QNX NEUTRINO 8.0 [LIVE KERNEL] | Cores: 4
==========================================================================================
Per-core CPU% / CtxSw: N/A (requires TraceEvent instrumentation)
RAM: 812 MB / 4096 MB (19.8%) | ResMgr Pool: N/A KB | Total Msg Rate: N/A msgs/sec
REPLY-block time source: kernel (nsec_since_block), sampled lower bound as fallback
------------------------------------------------------------------------------------------
PID    TID  PRI   STATE             CPU%     RSS      CHID   PULSES     COMMAND
------------------------------------------------------------------------------------------
1      1    255   RUNNING           0.8%     1.1M     0x01   N/A        /proc/boot/procnto-smp-instr
124    1    21    RECEIVE           2.4%     8.0M     0x03   N/A        /sbin/devc-ser8250
312    1    18    REPLY-blocked     1.2%     6.0M     -      N/A        /sbin/devb-eide [waiting on PID 124 CHID 0x03: 12.4ms (kernel)]

[Server Channels With REPLY-Blocked Clients]
CHID     SERVER PID SERVER                 RECEIVE THREAD   BLOCKED CLIENTS MSG/SEC    STATUS
------------------------------------------------------------------------------------------
0x03     124        /sbin/devc-ser8250     TID 1 (PRI 21)   1              N/A        STALL 12.4ms (kernel)

[Adaptive Partitioning System (APS)]
Adaptive Partitioning: Inactive / Not Configured

[/dev Entries]
* /dev/ser1                Owner: N/A                Handlers: N/A                       (Req/s: N/A)
```

The numbers above are an example of the layout, not a recording. Run with `-b` for a single snapshot.

### Thread view (`-t`, or `-p <pid>` for one process)

```
PID 312  /sbin/devb-eide  (PPID 1)  VSIZE 12.0M  RSS 6.0M  CPU 1.2%  threads 1
  TID  PRI  POLICY    STATE          CPU%     CPU# BLOCKED ON
  1    18   FIFO      REPLY-blocked  1.2%     0    server PID 124 CHID 0x03, waiting 12.4ms (kernel)
```

Thread CPU% values are shares of total machine capacity, so they add up to the process CPU%.

### How REPLY-block time is measured

- **`(kernel)`:** the kernel's own blocked-time counter (`nsec_since_block`). CMake checks for the field in the SDP headers at configure time and prints which mode was chosen. Force sampling with `-DQNX_TOP_FORCE_SAMPLED_STALL=ON`.
- **`(sampled)`:** shown as `>=Nms`. It is the time since qnx-top first saw the thread blocked, so it is a lower bound and starts at 0 on the first sighting. It is used when the field is missing or the kernel reports 0 (its resolution is about 1 ms).
- A channel is flagged `STALL` when its longest-waiting client exceeds 2 ms, whichever source applies.

### Host (simulated) mode

When built outside QNX the header reads `HOST MODE [SIMULATED TELEMETRY]`. All values then come from a fixed demo data set, so the UI and tests can run anywhere. They say nothing about the machine you are on.

---

## Resource manager: `/dev/qnx_top`

On QNX the tool registers a resource manager that serves a text summary on `read()`:

- Dispatch loop runs on a dedicated thread (`dispatch_block` then `dispatch_handler`).
- `read()` returns the report once and then EOF (tracks `ocb->offset`).
- The snapshot and report buffers are heap allocated.
- Permissions are `0644`.
- The same `N/A` rules apply to its output: `cat /dev/qnx_top`.

Collector state is shared between the monitor loop and the resource-manager thread. It is protected by a mutex, and the unit tests exercise concurrent access (CI also runs them under ThreadSanitizer).

---

## Build and Run Instructions

### Prerequisites
* QNX Software Development Platform (SDP) 8.0 (free access via [QNX Everywhere](https://qnx.software/en/developers/get-started/qnx-everywhere)) for the native build.
* CMake 3.20+ and the QNX toolchain (`qcc`).
* *(Optional)* QEMU for x86_64 target emulation. A QNX VM image is **not** included in this repository.

### 1. Building for QNX 8.0 Target
```bash
# 1. Source QNX SDP 8.0 Environment
source /opt/qnx800/qnxsdp-env.sh

# 2. Configure with QNX Toolchain
cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/qnx800-x86_64.toolchain.cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo

# 3. Compile native binary
cmake --build build -j$(nproc)
```

### 2. Building and Testing on Host (Linux / macOS)
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```
Use `-DQNX_TOP_WERROR=ON` to treat warnings as errors (CI does). The tests use their own `TEST_ASSERT` macro, so they also run in Release (`NDEBUG`) builds. The default build type is `RelWithDebInfo`.

### 3. Running
```bash
qnx-top            # refresh every 1 s
qnx-top -d 0.5     # 0.5 s refresh
qnx-top -b         # one snapshot, no screen clear
qnx-top -n 10      # exit after 10 refreshes
qnx-top -t         # thread view: one row per thread
qnx-top -p 312     # only PID 312, with its threads
```

### 4. Terminal mock-up (Python)
`demo_qnx_top.py` is an **animated mock-up with synthetic data**, used for the demo video. It does not read from any system.
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
├── demo_qnx_top.py                # Animated mock-up with synthetic data (not a monitor)
├── index.html                     # Web landing page
├── cmake/                         # QNX 8.0 toolchain files (x86_64 and aarch64)
├── include/                       # Public C headers (qnx_top.h)
├── src/                           # Native C implementation (collector, resmgr, main)
├── tests/                         # Unit tests (states, snapshot, stall tracker, concurrency)
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
