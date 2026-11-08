# QNX® 8.0 Engineering Advisory & Specialized Consulting Services

> Professional engineering services, architecture reviews, and hard real-time latency optimization for BlackBerry QNX® Neutrino® 8.0 deployments.

As the creators of **`qnx-top`**, we provide specialized, fixed-scope engineering consulting for Tier-1 automotive suppliers, medical device manufacturers, robotics companies, and defense contractors.

---

## 1. Productized Consulting Packages

To eliminate procurement friction, we offer **fixed-scope, fixed-fee deliverables** that engineering leads can approve quickly without prolonged corporate purchasing cycles.

```
┌─────────────────────────────────┐   ┌─────────────────────────────────┐   ┌─────────────────────────────────┐
│       TIER 1: AUDIT & JITTER    │   │      TIER 2: RESMGR SPRINT      │   │    TIER 3: ARCHITECTURE RETAINER│
│            $2,950 USD           │   │            $5,800 USD           │   │         $4,500 / month          │
│   Turnaround: 3 - 5 Days        │   │   Turnaround: 1 - 2 Weeks       │   │   15-20 Hours Dedicated / Mo    │
└────────────────┬────────────────┘   └────────────────┬────────────────┘   └────────────────┬────────────────┘
                 │                                     │                                     │
                 ▼                                     ▼                                     ▼
 • Microkernel IPC profile             • Turnkey `/dev` resource manager     • Ongoing PR & code reviews
 • Priority inversion & stall check    • Thread pool & pulse handler         • Architecture design reviews
 • Written remediation plan with code  • Functional test harness & docs      • Direct Slack / Teams access
```

---

### Package 1: Real-Time Jitter & Microkernel Latency Audit
* **Price:** `$2,950 USD` (One-time, fixed scope)
* **Turnaround:** 3–5 business days
* **Ideal For:** Teams encountering unexpected latency spikes, missed deadlines, or intermittent system lockups.
* **Deliverables:**
  1. **IPC & Channel Bottleneck Analysis:** Full inspection of client-server message passing (`MsgSend`, `MsgReceive`, `MsgReply`), identifying threads stalled in `REPLY-blocked` or `RECEIVE-blocked` states.
  2. **Priority Inversion & Scheduling Assessment:** Review of thread priorities, mutex inheritance policies, and Adaptive Partitioning System (APS) allocations.
  3. **Executive Action Report:** A detailed technical document pinpointing root causes with line-by-line code recommendations and diffs to eliminate deadline jitter.

---

### Package 2: Custom QNX 8.0 Resource Manager Architecture Sprint
* **Price:** `$5,800 USD` (One-time, fixed scope)
* **Turnaround:** 7–10 business days
* **Ideal For:** Teams needing to expose custom hardware, sensors, CAN/Ethernet buses, or proprietary algorithms through standard POSIX `/dev` endpoints.
* **Deliverables:**
  1. **Production-Ready Resource Manager Skeleton:** Modern, multi-threaded C/C++ resource manager implementation using QNX 8.0 `resmgr_attach()`.
  2. **POSIX Handler Implementations:** Complete handlers for `_IO_READ`, `_IO_WRITE`, `_IO_DEVCTL`, and non-blocking pulse events (`_IO_PULSE`).
  3. **Privilege Dropping & Sandboxing:** Implementation of QNX `procmgr_ability()` to ensure least-privilege security compliance.
  4. **QEMU Test Harness & Benchmarking Suite:** Automated regression tests runnable in CI/CD.

---

### Package 3: Fractional QNX Real-Time Architect (Retainer)
* **Price:** `$4,500 USD / month` (3-month initial commitment)
* **Capacity:** 15–20 hours / month
* **Ideal For:** Fast-moving engineering teams building Software-Defined Vehicles (SDVs), surgical robots, or industrial controllers needing senior QNX expertise on demand.
* **Deliverables:**
  - Weekly architectural review calls with your engineering team.
  - Asynchronous code reviews for pull requests targeting QNX SDP 8.0.
  - Direct integration support for QNX Everywhere, QEMU simulation, and BSP bring-up.
  - Priority emergency response for critical blocker bugs (< 24h turnaround).

---

## 2. Enterprise Procurement & Security Standards

* **Standard Mutual NDA:** We provide a standard mutual Non-Disclosure Agreement before receiving proprietary code, or we can sign your organization's standard corporate NDA.
* **IP Cleanliness:** 100% of all code, documentation, and artifacts generated during the engagement are assigned exclusively to your organization under work-for-hire provisions.
* **Billing & Tax Forms:** Invoicing via Stripe (Credit Card, ACH, SEPA) or Direct International Wire Transfer. Form W-9 (US entities) and Form W-8BEN (International entities) available upon request.

---

## 3. How to Engage

1. **Email:** Send a brief description of your project and timeline to:  
   `consulting@qnx-top.dev` *(or your direct contact email)*
2. **Book a Technical Discovery Call:**  
   Schedule a 25-minute technical discovery session directly on [our booking calendar](https://cal.com/your-username/qnx-advisory).
3. **Execution:** Upon NDA execution and package selection, work begins within 48 hours.

---
*Disclaimer: QNX, Neutrino, and Momentics are registered trademarks of BlackBerry Limited. This consulting practice operates as an independent technical advisory service specializing in BlackBerry QNX technologies.*
