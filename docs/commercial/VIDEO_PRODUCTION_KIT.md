# `qnx-top` Demo Video Production Kit & Submission Package

This document describes how to record, narrate, and publish the demonstration video.

> **Important:** the on-screen content is an animated **mock-up with synthetic data** (`demo_qnx_top.py` or `build_demo_video.py`). It is not a capture from a QNX target, and the narration says so. Do not describe the video as showing live kernel telemetry. Replace the mock-up with a real capture once the native build has been validated on a target.

---

## 1. Compliance Checklist for the Video

Check each item against the hackathon rules yourself before submitting. Nothing here has been verified.

| Rule Requirement | Status | Notes |
| :--- | :---: | :--- |
| **Duration < 3 Minutes** | To verify | `build_demo_video.py` derives the length from the narration WAVs and prints it. Confirm the final MP4. |
| **Functioning on Target** | **Not met** | The video shows a mock-up, not the tool running on QNX. A real capture is needed if the rules require it. |
| **Platform** | To do | Upload to YouTube as Public or Unlisted, then add the link to the submission. |
| **No Copyrighted Music** | To verify | Voiceover only. |
| **English Narration/Subs** | To verify | English script below. |

---

## 2. Recording Instructions

### Option A: Regenerate the mock-up video
```powershell
python synthesize_narration.py   # writes scene1..5.wav and master_narration.wav (Windows speech synthesis)
python build_demo_video.py       # writes qnx_top_demo_video.mp4 (needs Pillow and imageio-ffmpeg)
```
Scene timing is taken from the generated WAV files, so the video length follows the narration.

### Option B: Screen-record the animated mock-up
1. Use Windows Terminal at 1920x1080, Cascadia Code or Consolas 16-18 pt.
2. Start your recorder (`Win + Alt + R` or OBS), then run `python demo_qnx_top.py` (or `run_demo.bat`).
3. It cycles through four tabs of synthetic data: processes, channels, APS, `/dev`.

### Option C (preferred once available): record the real tool
Build for QNX 8.0, run `qnx-top` on a target or QEMU VM, and record that. Metrics that are not collected will show `N/A`.

---

## 3. Narration Script

This is the text in `synthesize_narration.py`. Keep the two in sync.

### Scene 1: Introduction
> *"When building mission-critical software on QNX 8.0, CPU and RAM numbers are only part of the picture. In a microkernel system, much of the story is in message passing: which threads are blocked waiting for a reply, and from which server. Standard Unix tools do not show that. This is qnx-top, an open source diagnostic tool for QNX Neutrino 8.0. Please note: the screens in this video are an animated mock-up with sample data, not a live capture. The native collector is pre-release."*

### Scene 2: Processes and thread states
> *"This is the process view. For each process, qnx-top shows the scheduler state of its threads, such as RECEIVE, READY, or REPLY-blocked, along with priority and CPU percentage. On a QNX target, this information comes from the proc file system, using devctl calls. The numbers shown here are sample data."*

### Scene 3: Channels
> *"The channel view groups REPLY-blocked client threads by the server channel they are waiting on. In this sample, a thread in the storage manager has been waiting on a server channel for over twelve milliseconds, so the channel is flagged as stalled. qnx-top measures this by timing how long a thread stays in the REPLY state between samples. It does not detect priority inversion yet, and message rates are not collected yet."*

### Scene 4: APS and `/dev`
> *"On a real target, qnx-top detects whether the Adaptive Partitioning scheduler is active, and lists the entries under slash dev. Per-partition budgets, and owner and handler details for dev entries, are not collected yet and appear as N slash A. The partition and resource manager screens in this video show sample data to illustrate the intended layout."*

### Scene 5: Outro
> *"qnx-top is open source under the MIT License. A host build with unit tests is included, and validating the native QNX build on a real target is the next step. Thank you."*

---

## 4. YouTube Upload Metadata

* **Video Title:** `qnx-top: REPLY-blocked thread and IPC diagnostics for QNX 8.0 (pre-release, mock-up demo)`
* **Category:** Science & Technology
* **Visibility:** Public (or Unlisted)
* **Video Description:**
```
qnx-top is an open-source diagnostic tool for QNX Neutrino 8.0.

NOTE: the screens in this video are an animated mock-up with sample data, not a live capture.
The native QNX collector is pre-release and has not yet been validated on a target.

What the tool is designed to show on a QNX target:
- Per-thread scheduler state (RECEIVE, REPLY-blocked, READY, ...) from the proc file system
- Client threads REPLY-blocked per server channel, with time blocked (sampled)
- Per-process CPU% from sampled deltas
- Whether the Adaptive Partitioning scheduler is active
- A listing of /dev entries and a text endpoint at /dev/qnx_top

Not implemented yet (shown as N/A): per-core load and context switches, message and pulse rates,
APS partition budgets, /dev owner and handler details, priority inversion detection.

Repository: https://github.com/Miladmet/qnx-top
License: MIT License
```
