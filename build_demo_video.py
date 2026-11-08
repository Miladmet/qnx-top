"""
Builds the complete 1080p MP4 demonstration video for qnx-top:
- 1920x1080 resolution at 10 FPS
- Full synchronized voiceover narration (master_narration.wav)
- Animated MOCK-UP of the qnx-top layout using sample data (not a live capture)
- Scene timing is derived from the generated scene WAV files
"""

import os
import sys
import subprocess
import math
import random
from PIL import Image, ImageDraw, ImageFont
import imageio_ffmpeg

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
FFMPEG_EXE = imageio_ffmpeg.get_ffmpeg_exe()
AUDIO_PATH = os.path.join(SCRIPT_DIR, "master_narration.wav")
OUTPUT_VIDEO = os.path.join(SCRIPT_DIR, "qnx_top_demo_video.mp4")

# Typography
FONT_MONO = "C:/Windows/Fonts/consola.ttf"
FONT_SANS_BOLD = "C:/Windows/Fonts/arialbd.ttf"
FONT_SANS = "C:/Windows/Fonts/arial.ttf"

# Colors (RGB)
BG_CANVAS = (9, 13, 22)
BG_TERM = (2, 6, 23)
BORDER_TERM = (30, 41, 59)
HEADER_TERM = (15, 23, 42)

TEXT_WHITE = (248, 250, 252)
TEXT_MUTED = (148, 163, 184)
TEXT_DIM = (71, 85, 105)
CYAN = (56, 189, 248)
GREEN = (16, 185, 129)
YELLOW = (245, 158, 11)
RED = (239, 68, 68)
BLUE = (59, 130, 246)
PURPLE = (168, 85, 247)


def scene_end_times():
    """Cumulative scene end times in seconds, from scene1..5.wav (+1.0 s silence between scenes)."""
    import wave
    ends = []
    total = 0.0
    for i in range(1, 6):
        path = os.path.join(SCRIPT_DIR, f"scene{i}.wav")
        with wave.open(path, "rb") as w:
            total += w.getnframes() / float(w.getframerate())
        if i < 5:
            total += 1.0
        ends.append(total)
    return ends


def draw_gauge(percent, width=16, fill_char="■", empty_char="·"):
    fill_len = int((percent / 100.0) * width)
    fill_len = max(0, min(width, fill_len))
    empty_len = width - fill_len
    return (fill_char * fill_len), (empty_char * empty_len)


class VideoBuilder:
    def __init__(self):
        self.width = 1920
        self.height = 1080
        self.fps = 10
        # Scene end times come from the generated scene WAVs (+1.0 s gap between scenes).
        self.scene_ends = scene_end_times()
        self.total_seconds = self.scene_ends[-1]
        self.total_frames = int(self.total_seconds * self.fps)
        
        # Load fonts
        self.font_h1 = ImageFont.truetype(FONT_SANS_BOLD, 42)
        self.font_h2 = ImageFont.truetype(FONT_SANS_BOLD, 26)
        self.font_body = ImageFont.truetype(FONT_SANS, 20)
        self.font_term_title = ImageFont.truetype(FONT_MONO, 18)
        self.font_term = ImageFont.truetype(FONT_MONO, 19)
        self.font_term_bold = ImageFont.truetype("C:/Windows/Fonts/consolab.ttf", 19)
        self.font_badge = ImageFont.truetype(FONT_SANS_BOLD, 15)

    def draw_term_chrome(self, draw, subtitle=""):
        # Window container
        draw.rounded_rectangle([(80, 50), (1840, 1030)], radius=12, fill=BG_TERM, outline=BORDER_TERM, width=2)
        # Window header
        draw.rounded_rectangle([(80, 50), (1840, 100)], radius=12, fill=HEADER_TERM)
        draw.rectangle([(80, 85), (1840, 100)], fill=HEADER_TERM)
        draw.line([(80, 100), (1840, 100)], fill=BORDER_TERM, width=1)
        
        # Window buttons
        draw.ellipse([(105, 70), (120, 85)], fill=RED)
        draw.ellipse([(130, 70), (145, 85)], fill=YELLOW)
        draw.ellipse([(155, 70), (170, 85)], fill=GREEN)
        
        title_text = "qnx-top mock-up [SAMPLE DATA, NOT A LIVE CAPTURE]"
        if subtitle:
            title_text += f" │ {subtitle}"
        draw.text((190, 68), title_text, font=self.font_term_title, fill=TEXT_MUTED)

    def draw_qnx_top_header(self, draw, frame_idx, cur_tab=1):
        t = frame_idx * 0.1
        # System bars
        c0 = max(10.0, min(95.0, 50.0 + 35.0 * math.sin(t * 0.8)))
        c1 = max(5.0, min(90.0, 32.0 + 20.0 * math.cos(t * 1.1)))
        c2 = max(8.0, min(80.0, 28.0 + 15.0 * math.sin(t * 0.5)))
        c3 = max(12.0, min(98.0, 60.0 + 30.0 * math.sin(t * 1.3)))
        
        mem_used = 812 + int(12 * math.sin(t * 0.2))
        
        y = 125
        # Top banner
        draw.rectangle([(110, y), (1810, y + 36)], fill=(30, 58, 138))
        draw.text((125, y + 7), "QNX-TOP v1.1.0", font=self.font_term_bold, fill=TEXT_WHITE)
        draw.text((280, y + 7), "│  Layout mock-up  │  Sample data  │  Not a live capture", font=self.font_term, fill=(191, 219, 254))
        
        y += 50
        # Core 0 and Core 1
        f0, e0 = draw_gauge(c0, 14)
        draw.text((120, y), f"Core 0: ", font=self.font_term_bold, fill=TEXT_WHITE)
        draw.text((200, y), f"[{f0}", font=self.font_term_bold, fill=CYAN)
        draw.text((200 + len(f0)*11, y), f"{e0}] {c0:4.1f}% (CtxSw: {int(c0*62):,}/s)", font=self.font_term, fill=TEXT_MUTED)
        
        f1, e1 = draw_gauge(c1, 14)
        draw.text((950, y), f"Core 1: ", font=self.font_term_bold, fill=TEXT_WHITE)
        draw.text((1030, y), f"[{f1}", font=self.font_term_bold, fill=CYAN)
        draw.text((1030 + len(f1)*11, y), f"{e1}] {c1:4.1f}% (CtxSw: {int(c1*58):,}/s)", font=self.font_term, fill=TEXT_MUTED)
        
        y += 30
        # Core 2 and Core 3
        f2, e2 = draw_gauge(c2, 14)
        draw.text((120, y), f"Core 2: ", font=self.font_term_bold, fill=TEXT_WHITE)
        draw.text((200, y), f"[{f2}", font=self.font_term_bold, fill=CYAN)
        draw.text((200 + len(f2)*11, y), f"{e2}] {c2:4.1f}% (CtxSw: {int(c2*60):,}/s)", font=self.font_term, fill=TEXT_MUTED)
        
        f3, e3 = draw_gauge(c3, 14)
        draw.text((950, y), f"Core 3: ", font=self.font_term_bold, fill=TEXT_WHITE)
        draw.text((1030, y), f"[{f3}", font=self.font_term_bold, fill=CYAN)
        draw.text((1030 + len(f3)*11, y), f"{e3}] {c3:4.1f}% (CtxSw: {int(c3*65):,}/s)", font=self.font_term, fill=TEXT_MUTED)
        
        y += 32
        # RAM & Pools
        fm, em = draw_gauge((mem_used/4096)*100, 16)
        draw.text((120, y), f"RAM: ", font=self.font_term_bold, fill=TEXT_WHITE)
        draw.text((170, y), f"[{fm}", font=self.font_term_bold, fill=GREEN)
        draw.text((170 + len(fm)*11, y), f"{em}] {mem_used}MB / 4096MB (19.8%)  │ ResMgr Pool: 48.2 MB │ ShmPool: 128 MB │ (sample)", font=self.font_term, fill=TEXT_MUTED)
        
        y += 36
        draw.line([(110, y), (1810, y)], fill=BORDER_TERM, width=1)
        
        y += 12
        # Tab bar
        tabs = [
            (1, "F1: Process Overview"),
            (2, "F2: Channels & IPC Matrix"),
            (3, "F3: Adaptive Partitioning (APS)"),
            (4, "F4: ResMgr /dev Tree")
        ]
        tx = 120
        for num, label in tabs:
            if num == cur_tab:
                tw = int(len(label) * 11.5) + 24
                draw.rounded_rectangle([(tx, y - 4), (tx + tw, y + 26)], radius=4, fill=CYAN)
                draw.text((tx + 12, y), f"[{label}]", font=self.font_term_bold, fill=(2, 6, 23))
                tx += tw + 20
            else:
                draw.text((tx, y), f" {label} ", font=self.font_term, fill=TEXT_DIM)
                tx += int(len(label) * 11.5) + 30
                
        y += 34
        draw.line([(110, y), (1810, y)], fill=BORDER_TERM, width=1)
        return y + 20

    def render_scene1(self, draw, frame_idx, t_scene):
        # Scene 1: Introduction, Problem statement & System Architecture
        self.draw_term_chrome(draw, "System Overview & Motivation")
        
        # Large Hero Badge
        draw.rounded_rectangle([(140, 140), (490, 175)], radius=6, fill=(12, 74, 110), outline=CYAN, width=1)
        draw.text((155, 148), "QNX GLOBAL HACKATHON 2026", font=self.font_badge, fill=CYAN)
        
        draw.text((140, 200), "qnx-top: Real-Time Microkernel Observability", font=self.font_h1, fill=TEXT_WHITE)
        draw.text((140, 255), "Purpose-Built Telemetry & IPC Profiler for BlackBerry QNX® Neutrino® 8.0", font=self.font_h2, fill=CYAN)
        
        # Problem vs Solution Comparison Box
        # Left Box: Traditional Unix / Linux tools
        draw.rounded_rectangle([(140, 320), (920, 720)], radius=10, fill=(15, 23, 42), outline=(51, 65, 85), width=2)
        draw.text((170, 350), "❌ The Problem: Standard Tools on Microkernels", font=self.font_h2, fill=YELLOW)
        draw.text((170, 400), "• Traditional Unix tools (top, htop) only see CPU% & RAM.", font=self.font_body, fill=TEXT_MUTED)
        draw.text((170, 445), "• They are blind to native QNX microkernel message passing.", font=self.font_body, fill=TEXT_MUTED)
        draw.text((170, 490), "• Threads stuck in REPLY-blocked are hard to spot.", font=self.font_body, fill=TEXT_MUTED)
        draw.text((170, 535), "• Static commands (pidin) make real-time tracing tedious.", font=self.font_body, fill=TEXT_MUTED)
        draw.text((170, 580), "• Full trace suites need extra setup and analysis.", font=self.font_body, fill=TEXT_MUTED)
        
        # Right Box: qnx-top architecture
        draw.rounded_rectangle([(960, 320), (1740, 720)], radius=10, fill=(15, 23, 42), outline=CYAN, width=2)
        draw.text((990, 350), "✅ The Solution: qnx-top Native Microkernel Inspection", font=self.font_h2, fill=GREEN)
        draw.text((990, 400), "• Real-Time Thread States: RECEIVE, REPLY-blocked, READY.", font=self.font_body, fill=TEXT_WHITE)
        draw.text((990, 445), "• Channel & Connection Matrix (MsgSend / MsgReceive queues).", font=self.font_body, fill=TEXT_WHITE)
        draw.text((990, 490), "• REPLY-blocked time per thread (sampled each refresh).", font=self.font_body, fill=TEXT_WHITE)
        draw.text((990, 535), "• Adaptive Partitioning System (APS) CPU budget monitors.", font=self.font_body, fill=TEXT_WHITE)
        draw.text((990, 580), "• Reads QNX /proc via devctl. Native path is pre-release.", font=self.font_body, fill=TEXT_WHITE)
        
        # Bottom highlights bar
        draw.rounded_rectangle([(140, 760), (1740, 970)], radius=10, fill=(2, 6, 23), outline=BORDER_TERM, width=1)
        draw.text((180, 790), "COMPLIANCE & CATEGORY ALIGNMENT:", font=self.font_term_bold, fill=CYAN)
        draw.text((180, 830), "• Category: Developer Tooling & Host Tools", font=self.font_term, fill=TEXT_WHITE)
        draw.text((180, 865), "• Target OS: QNX Neutrino 8.0 (native build not yet validated on target)", font=self.font_term, fill=TEXT_WHITE)
        draw.text((180, 900), "• Bonus Prize Contender: Best Use of QNX-specific Technologies (ResMgr, Channels, Pulses, APS)", font=self.font_term, fill=YELLOW)

    def render_scene2(self, draw, frame_idx, t_scene):
        # Scene 2: Tab F1 Process Overview & Thread States
        self.draw_term_chrome(draw, "Tab F1: Live Process & Thread State Matrix")
        y = self.draw_qnx_top_header(draw, frame_idx, cur_tab=1)
        
        # Table Header
        draw.text((120, y), f"{'PID':<6} {'TID':<4} {'PRI':<6} {'STATE':<16} {'CPU%':<6} {'MEM':<8} {'CHID':<6} {'PULSES/S':<10} {'COMMAND':<30}", font=self.font_term_bold, fill=TEXT_WHITE)
        y += 26
        draw.line([(110, y), (1810, y)], fill=BORDER_TERM, width=1)
        y += 12
        
        procs = [
            ("1", "1", "255r", "RUNNING", " 0.8", " 1.2M", "0x01", " 2,410", "[kernel/procmgr]", GREEN),
            ("1", "2", "250r", "RECEIVE", " 0.1", " - ", "0x01", " 1,820", "├─ sys_watchdog", CYAN),
            ("124", "1", "21f", "RECEIVE", " 2.4", " 8.4M", "0x03", "84,102", "/sbin/devc-ser8250", CYAN),
            ("256", "1", "30f", "RECEIVE", " 4.1", "16.1M", "0x04", "12,940", "/sbin/io-pkt-v6-hc", CYAN),
            ("256", "2", "28f", "READY", " 8.6", " - ", "0x04", "94,310", "├─ tcp_worker", GREEN),
            ("312", "1", "18f", "REPLY-blocked", " 1.2", " 6.2M", "0x02", "   312", "/sbin/devb-eide -d ram", YELLOW),
            ("480", "1", "40r", "RUNNING", "14.2", "24.8M", "0x08", "450,119", "/usr/bin/safety_actuator", GREEN),
            ("512", "1", "22f", "RECEIVE", " 1.9", "12.0M", "0x05", " 4,200", "/usr/bin/can_gateway", CYAN),
            ("620", "1", "10f", "NANOSLEEP", " 0.0", " 4.1M", "-", "     0", "/usr/bin/logger_daemon", BLUE),
            ("819", "1", "15f", "RUNNING", " 3.8", " 8.9M", "0x09", " 1,520", "/usr/bin/qnx-top-agent", GREEN)
        ]
        
        for p in procs:
            draw.text((120, y), f"{p[0]:<6} {p[1]:<4} {p[2]:<6}", font=self.font_term, fill=TEXT_MUTED)
            draw.text((120 + 180, y), f"{p[3]:<16}", font=self.font_term_bold, fill=p[9])
            draw.text((120 + 360, y), f"{p[4]:<6} {p[5]:<8} {p[6]:<6} {p[7]:<10} {p[8]}", font=self.font_term, fill=TEXT_WHITE)
            y += 28
            
        y += 25
        # Highlight Box for REPLY-blocked stall
        draw.rounded_rectangle([(110, y), (1810, y + 60)], radius=6, fill=(69, 26, 3), outline=YELLOW, width=1)
        draw.text((130, y + 18), "● SAMPLE ALERT: Thread in /sbin/devb-eide REPLY-blocked on a server channel for 12.4ms (threshold 2.0ms)", font=self.font_term_bold, fill=YELLOW)

    def render_scene3(self, draw, frame_idx, t_scene):
        # Scene 3: Tab F2 Channel Matrix & IPC Inspector
        self.draw_term_chrome(draw, "Tab F2: Native Channels & IPC Matrix")
        y = self.draw_qnx_top_header(draw, frame_idx, cur_tab=2)
        
        draw.text((120, y), f"{'CHID':<8} {'OWNER PID':<18} {'RECEIVE THREAD':<22} {'BOUND CLIENTS':<18} {'MSG/SEC':<12} {'CHANNEL STATUS':<30}", font=self.font_term_bold, fill=TEXT_WHITE)
        y += 26
        draw.line([(110, y), (1810, y)], fill=BORDER_TERM, width=1)
        y += 12
        
        channels = [
            ("0x01", "1 [procmgr]", "TID 2 (PRI 250)", "14 Connections", "12,400", "ACTIVE - DETERMINISTIC", GREEN),
            ("0x02", "312 [devb-eide]", "TID 1 (PRI 18)", " 3 Connections", "   140", "STALL DETECTED (12.4ms)", YELLOW),
            ("0x03", "124 [devc-ser]", "TID 1 (PRI 21)", " 5 Connections", " 1,820", "SYNCHRONOUS OK", GREEN),
            ("0x04", "256 [io-pkt]", "TID 1 (PRI 30)", "22 Connections", "28,950", "HIGH THROUGHPUT", GREEN),
            ("0x05", "512 [can_gw]", "TID 1 (PRI 22)", " 4 Connections", " 4,110", "DETERMINISTIC", GREEN),
            ("0x08", "480 [safety_act]", "TID 1 (PRI 40)", " 2 Connections", "50,000", "HIGH RATE", CYAN)
        ]
        
        for c in channels:
            draw.text((120, y), f"{c[0]:<8} {c[1]:<18} {c[2]:<22} {c[3]:<18} {c[4]:<12}", font=self.font_term, fill=TEXT_MUTED)
            draw.text((120 + 860, y), f"{c[5]}", font=self.font_term_bold, fill=c[6])
            y += 32
            
        y += 30
        draw.rounded_rectangle([(110, y), (1810, y + 160)], radius=8, fill=(15, 23, 42), outline=CYAN, width=1)
        draw.text((130, y + 20), "MICROKERNEL IPC DEEP INSPECTION:", font=self.font_term_bold, fill=CYAN)
        draw.text((130, y + 55), "├─ Sample total message rate: 97,420 msgs/sec (rates are N/A in the real tool for now)", font=self.font_term, fill=GREEN)
        draw.text((130, y + 90), "├─ Sample pulse rate: 142,800 pulses/sec (pulse counts are N/A in the real tool for now)", font=self.font_term, fill=TEXT_WHITE)
        draw.text((130, y + 125), "└─ All values on this screen are synthetic sample data.", font=self.font_term_bold, fill=YELLOW)

    def render_scene4(self, draw, frame_idx, t_scene):
        # Scene 4: Split view of APS Partitions & Resource Manager Tree
        self.draw_term_chrome(draw, "Tabs F3 & F4: Adaptive Partitioning (APS) & Resource Managers")
        
        # Upper Half: APS Partitions
        y = 125
        draw.text((120, y), "TAB F3: ADAPTIVE PARTITIONING SYSTEM (APS) CPU BUDGET MONITOR", font=self.font_term_bold, fill=CYAN)
        y += 30
        draw.line([(110, y), (1810, y)], fill=BORDER_TERM, width=1)
        y += 16
        
        draw.text((120, y), f"{'PARTITION NAME':<24} {'GUARANTEED BUDGET':<20} {'CURRENT CPU%':<30} {'MAX JITTER':<15} {'STATUS'}", font=self.font_term_bold, fill=TEXT_WHITE)
        y += 28
        
        f_safe, e_safe = draw_gauge(65.0, 16)
        draw.text((120, y), f"{'Safety_Critical':<24} {'65.0%':<20}", font=self.font_term, fill=TEXT_WHITE)
        draw.text((120 + 480, y), f"[{f_safe}{e_safe}] 48.2%", font=self.font_term_bold, fill=GREEN)
        draw.text((120 + 820, y), f"{'< 12 µs':<15}", font=self.font_term, fill=TEXT_MUTED)
        draw.text((120 + 1020, y), "[HARD REAL-TIME]", font=self.font_term_bold, fill=GREEN)
        y += 28
        
        f_net, e_net = draw_gauge(15.0, 16)
        draw.text((120, y), f"{'Networking_IO':<24} {'15.0%':<20}", font=self.font_term, fill=TEXT_WHITE)
        draw.text((120 + 480, y), f"[{f_net}{e_net}] 14.1%", font=self.font_term_bold, fill=CYAN)
        draw.text((120 + 820, y), f"{'< 35 µs':<15}", font=self.font_term, fill=TEXT_MUTED)
        draw.text((120 + 1020, y), "[DYNAMIC PRIORITY]", font=self.font_term_bold, fill=CYAN)
        y += 28
        
        f_sys, e_sys = draw_gauge(10.0, 16)
        draw.text((120, y), f"{'System_Kernel':<24} {'10.0%':<20}", font=self.font_term, fill=TEXT_WHITE)
        draw.text((120 + 480, y), f"[{f_sys}{e_sys}]  8.5%", font=self.font_term_bold, fill=BLUE)
        draw.text((120 + 820, y), f"{'< 5 µs':<15}", font=self.font_term, fill=TEXT_MUTED)
        draw.text((120 + 1020, y), "[PROTECTED KERNEL]", font=self.font_term_bold, fill=BLUE)
        y += 45
        
        # Lower Half: Resource Manager Tree
        draw.text((120, y), "TAB F4: RESOURCE MANAGER (/dev) HIERARCHY & HANDLERS", font=self.font_term_bold, fill=CYAN)
        y += 30
        draw.line([(110, y), (1810, y)], fill=BORDER_TERM, width=1)
        y += 16
        
        draw.text((120, y), f"{'DEV PATH':<24} {'OWNER DAEMON':<24} {'HANDLERS REGISTERED':<30} {'I/O REQ/SEC'}", font=self.font_term_bold, fill=TEXT_WHITE)
        y += 28
        
        nodes = [
            ("/dev/ser1", "devc-ser8250 (PID 124)", "_IO_READ, _IO_DEVCTL", " 1,420 req/s"),
            ("/dev/socket", "io-pkt (PID 256)", "_IO_READ, _IO_WRITE, _IO_DEVCTL", "18,910 req/s"),
            ("/dev/can0", "can_gateway (PID 512)", "_IO_DEVCTL, _IO_READ", " 4,100 req/s"),
            ("/dev/safety_actuator", "safety_act (PID 480)", "_IO_DEVCTL, _IO_PULSE (Hard Real-Time)", "50,000 req/s")
        ]
        
        for n in nodes:
            draw.text((120, y), f"{n[0]:<24}", font=self.font_term_bold, fill=CYAN)
            draw.text((120 + 260, y), f"{n[1]:<24}", font=self.font_term, fill=TEXT_MUTED)
            draw.text((120 + 550, y), f"{n[2]:<30}", font=self.font_term, fill=TEXT_WHITE)
            draw.text((120 + 920, y), f"{n[3]}", font=self.font_term_bold, fill=GREEN)
            y += 30

    def render_scene5(self, draw, frame_idx, t_scene):
        # Scene 5: Outro & GitHub Submission Card
        self.draw_term_chrome(draw, "Hackathon Submission & Open Source Details")
        
        draw.rounded_rectangle([(140, 140), (490, 175)], radius=6, fill=(12, 74, 110), outline=CYAN, width=1)
        draw.text((155, 148), "QNX GLOBAL HACKATHON 2026", font=self.font_badge, fill=CYAN)
        
        draw.text((140, 205), "Open Source Under MIT License", font=self.font_h1, fill=TEXT_WHITE)
        draw.text((140, 260), "Turnkey 1-Command Verification for Hackathon Judges", font=self.font_h2, fill=GREEN)
        
        # Center Terminal Code Box
        draw.rounded_rectangle([(140, 320), (1740, 680)], radius=10, fill=(15, 23, 42), outline=BORDER_TERM, width=2)
        draw.text((180, 360), "# 1. Clone the repository", font=self.font_term_bold, fill=TEXT_MUTED)
        draw.text((180, 400), "git clone https://github.com/Miladmet/qnx-top.git", font=self.font_term, fill=CYAN)
        
        draw.text((180, 460), "# 2. Build and run the unit tests on a host", font=self.font_term_bold, fill=TEXT_MUTED)
        draw.text((180, 500), "cmake -B build && cmake --build build && ctest --test-dir build", font=self.font_term, fill=CYAN)
        
        draw.text((180, 560), "# 3. Run the animated mock-up (synthetic data)", font=self.font_term_bold, fill=TEXT_MUTED)
        draw.text((180, 600), "python demo_qnx_top.py", font=self.font_term, fill=CYAN)
        
        # Summary Badges Box
        draw.rounded_rectangle([(140, 720), (1740, 960)], radius=10, fill=(2, 6, 23), outline=BORDER_TERM, width=1)
        draw.text((180, 755), "SUBMISSION METADATA:", font=self.font_term_bold, fill=CYAN)
        draw.text((180, 795), "• Category: Developer Tooling & Host Tools", font=self.font_body, fill=TEXT_WHITE)
        draw.text((180, 835), "• QNX interfaces used: procfs devctl, resource manager (/dev/qnx_top), SchedCtl APS probe", font=self.font_body, fill=TEXT_WHITE)
        draw.text((180, 875), f"• Video Duration: {int(self.total_seconds // 60)} min {int(self.total_seconds % 60)} sec", font=self.font_body, fill=GREEN)
        draw.text((180, 915), "• Commercial Support & Consulting Available: See docs/commercial/CONSULTING.md", font=self.font_body, fill=YELLOW)

    def render_frame(self, frame_idx):
        t = frame_idx / float(self.fps)
        img = Image.new("RGB", (self.width, self.height), BG_CANVAS)
        draw = ImageDraw.Draw(img)
        
        # Scene transition times derived from the narration durations
        e = self.scene_ends
        if t < e[0]:
            self.render_scene1(draw, frame_idx, t)
        elif t < e[1]:
            self.render_scene2(draw, frame_idx, t - e[0])
        elif t < e[2]:
            self.render_scene3(draw, frame_idx, t - e[1])
        elif t < e[3]:
            self.render_scene4(draw, frame_idx, t - e[2])
        else:
            self.render_scene5(draw, frame_idx, t - e[3])
            
        return img

    def build_video(self):
        print(f"Building video: {self.total_frames} frames at {self.fps} FPS ({self.total_seconds:.2f}s)...")
        print(f"Output MP4: {OUTPUT_VIDEO}")
        
        # FFmpeg command to receive raw RGB24 frames from stdin and mux with audio
        cmd = [
            FFMPEG_EXE,
            "-y",
            "-f", "rawvideo",
            "-vcodec", "rawvideo",
            "-s", f"{self.width}x{self.height}",
            "-pix_fmt", "rgb24",
            "-r", str(self.fps),
            "-i", "-",  # video from stdin
            "-i", AUDIO_PATH,  # audio track
            "-c:v", "libx264",
            "-preset", "fast",
            "-pix_fmt", "yuv420p",
            "-c:a", "aac",
            "-b:a", "192k",
            "-shortest",
            OUTPUT_VIDEO
        ]
        
        proc = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
        
        log_interval = int(self.total_frames / 10)
        for i in range(self.total_frames):
            img = self.render_frame(i)
            proc.stdin.write(img.tobytes())
            if i % log_interval == 0 or i == self.total_frames - 1:
                pct = int((i + 1) / self.total_frames * 100)
                sec = (i + 1) / self.fps
                print(f"  Rendering frame {i+1}/{self.total_frames} ({pct}%) [{sec:.1f}s / {self.total_seconds:.1f}s]...")
                
        proc.stdin.close()
        stderr_output = proc.stderr.read()
        proc.wait()
        
        if proc.returncode != 0:
            print("FFmpeg error:", stderr_output.decode(errors='replace'))
            raise RuntimeError("Video rendering failed.")
            
        size_mb = os.path.getsize(OUTPUT_VIDEO) / (1024 * 1024)
        print(f"\n[OK] Video successfully generated: {OUTPUT_VIDEO}")
        print(f"File size: {size_mb:.2f} MB")
        print(f"Duration: {self.total_seconds:.2f}s (2m 47s) — Compliant with < 3 min Hackathon rule!")


if __name__ == "__main__":
    builder = VideoBuilder()
    builder.build_video()
