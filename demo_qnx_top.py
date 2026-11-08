"""
qnx-top: Real-time Terminal Observability & Microkernel IPC Inspector for QNX Neutrino 8.0
Hackathon Demo / Visualizer Script
Zero-dependency Python 3 script using ANSI color rendering.
Compatible with Windows PowerShell, Windows Terminal, Linux, and macOS.
"""

import sys
import os
import time
import random
import math

# Force UTF-8 on Windows stdout to ensure clean rendering of box-drawing characters
try:
    if hasattr(sys.stdout, 'reconfigure'):
        sys.stdout.reconfigure(encoding='utf-8')
except Exception:
    pass

# Initialize Colorama on Windows to enable native ANSI processing
try:
    import colorama
    colorama.init(autoreset=False)
except ImportError:
    pass

# Enable ANSI escape sequences on Windows consoles if needed
if os.name == 'nt':
    try:
        import ctypes
        kernel32 = ctypes.windll.kernel32
        h_out = kernel32.GetStdHandle(-11)
        mode = ctypes.c_ulong()
        kernel32.GetConsoleMode(h_out, ctypes.byref(mode))
        kernel32.SetConsoleMode(h_out, mode.value | 0x0004 | 0x0001)
    except Exception:
        pass

# ANSI Color Codes
RESET = "\033[0m"
BOLD = "\033[1m"
DIM = "\033[2m"

# Foregrounds
FG_BLACK = "\033[30m"
FG_RED = "\033[31m"
FG_GREEN = "\033[32m"
FG_YELLOW = "\033[33m"
FG_BLUE = "\033[34m"
FG_MAGENTA = "\033[35m"
FG_CYAN = "\033[36m"
FG_WHITE = "\033[37m"

# High-intensity Foregrounds
FG_HI_RED = "\033[91m"
FG_HI_GREEN = "\033[92m"
FG_HI_YELLOW = "\033[93m"
FG_HI_BLUE = "\033[94m"
FG_HI_MAGENTA = "\033[95m"
FG_HI_CYAN = "\033[96m"
FG_HI_WHITE = "\033[97m"

# Backgrounds
BG_BLACK = "\033[40m"
BG_BLUE = "\033[44m"
BG_CYAN = "\033[46m"
BG_DARK_GRAY = "\033[100m"

CURSOR_HOME = "\033[H"
HIDE_CURSOR = "\033[?25l"
SHOW_CURSOR = "\033[?25h"


def draw_gauge(percent, width=16, fill_char="■", empty_char="·", color=FG_HI_CYAN):
    fill_len = int((percent / 100.0) * width)
    fill_len = max(0, min(width, fill_len))
    empty_len = width - fill_len
    
    if percent > 85:
        c = FG_HI_RED
    elif percent > 65:
        c = FG_HI_YELLOW
    else:
        c = color
        
    return f"{c}{fill_char * fill_len}{DIM}{empty_char * empty_len}{RESET}"


class QnxTopDemo:
    def __init__(self):
        self.tick = 0
        self.current_tab = 1
        self.auto_cycle = True
        self.cycle_interval = 7.0  # seconds per tab in auto demo mode
        self.last_tab_change = time.time()
        
        # Simulated system state
        self.cores = [24.0, 48.0, 15.0, 62.0]
        self.ctxsw = [3200, 4510, 1980, 5120]
        self.mem_used = 812
        self.mem_total = 4096
        
        # State counters for animations
        self.stalled_ms = 12.4

    def update_metrics(self):
        self.tick += 1
        t = self.tick * 0.1
        
        # Update CPU cores with natural oscillations
        self.cores[0] = max(10.0, min(95.0, 45.0 + 35.0 * math.sin(t * 0.8) + random.uniform(-3, 3)))
        self.cores[1] = max(5.0, min(90.0, 30.0 + 20.0 * math.cos(t * 1.1) + random.uniform(-2, 2)))
        self.cores[2] = max(8.0, min(80.0, 25.0 + 15.0 * math.sin(t * 0.5) + random.uniform(-2, 2)))
        self.cores[3] = max(12.0, min(98.0, 55.0 + 30.0 * math.sin(t * 1.3) + random.uniform(-4, 4)))
        
        for i in range(4):
            self.ctxsw[i] = int(self.cores[i] * 65 + random.randint(-60, 60))
            
        self.mem_used = 812 + int(15 * math.sin(t * 0.2))
        self.stalled_ms = round(12.4 + 1.8 * math.sin(t * 1.5), 1)
        
        # Auto cycle tabs for demo video recording
        if self.auto_cycle and (time.time() - self.last_tab_change) > self.cycle_interval:
            self.current_tab = (self.current_tab % 4) + 1
            self.last_tab_change = time.time()

    def render_header(self):
        now_str = time.strftime("%H:%M:%S")
        out = []
        out.append(f"{BG_BLUE}{FG_HI_WHITE}{BOLD}  QNX-TOP v1.0  │  Target: QNX Neutrino 8.0.0 (x86_64 SMP)  │  Host: QEMU-Virt  │  {now_str}  {RESET}")
        
        # Cores display
        c0 = f"Core 0: {draw_gauge(self.cores[0], 14)} {self.cores[0]:4.1f}% (CtxSw: {self.ctxsw[0]:,}/s)"
        c1 = f"Core 1: {draw_gauge(self.cores[1], 14)} {self.cores[1]:4.1f}% (CtxSw: {self.ctxsw[1]:,}/s)"
        c2 = f"Core 2: {draw_gauge(self.cores[2], 14)} {self.cores[2]:4.1f}% (CtxSw: {self.ctxsw[2]:,}/s)"
        c3 = f"Core 3: {draw_gauge(self.cores[3], 14)} {self.cores[3]:4.1f}% (CtxSw: {self.ctxsw[3]:,}/s)"
        
        out.append(f"  {c0}   │   {c1}")
        out.append(f"  {c2}   │   {c3}")
        
        # Memory & Microkernel status line
        mem_pct = (self.mem_used / self.mem_total) * 100
        mem_gauge = draw_gauge(mem_pct, 16, color=FG_HI_GREEN)
        out.append(f"  RAM: {mem_gauge} {self.mem_used}MB / {self.mem_total}MB ({mem_pct:.1f}%) │ ResMgr Pool: {FG_HI_CYAN}48.2 MB{RESET} │ ShmPool: {FG_HI_CYAN}128 MB{RESET} │ Status: {FG_HI_GREEN}REAL-TIME OPTIMAL{RESET}")
        out.append(f"{DIM}{'─' * 96}{RESET}")
        
        # Tab Bar
        tabs = [
            (1, "F1: Process Overview"),
            (2, "F2: Channels & IPC Matrix"),
            (3, "F3: Adaptive Partitioning (APS)"),
            (4, "F4: ResMgr /dev Tree")
        ]
        tab_str = "  "
        for num, label in tabs:
            if num == self.current_tab:
                tab_str += f"{BG_CYAN}{FG_BLACK}{BOLD} [{label}] {RESET}  "
            else:
                tab_str += f"{DIM}  {label}  {RESET}  "
        out.append(tab_str)
        out.append(f"{DIM}{'─' * 96}{RESET}")
        return "\n".join(out)

    def render_tab1_processes(self):
        lines = []
        lines.append(f" {BOLD}{'PID':<6} {'TID':<4} {'PRI':<5} {'STATE':<16} {'CPU%':<6} {'MEM':<8} {'CHID':<6} {'PULSES':<8} {'COMMAND':<25}{RESET}")
        lines.append(f" {DIM}{'─'*94}{RESET}")
        
        procs = [
            ("1", "1", "255r", f"{FG_GREEN}RUNNING{RESET}", " 0.8", " 1.2M", "0x01", "2,410", "[kernel/procmgr]"),
            ("1", "2", "250r", f"{FG_CYAN}RECEIVE{RESET}", " 0.1", " - ", "0x01", "1,820", "├─ sys_watchdog"),
            ("124", "1", "21f", f"{FG_CYAN}RECEIVE{RESET}", " 2.4", " 8.4M", "0x03", "84,102", "/sbin/devc-ser8250"),
            ("256", "1", "30f", f"{FG_CYAN}RECEIVE{RESET}", " 4.1", "16.1M", "0x04", "12,940", "/sbin/io-pkt-v6-hc"),
            ("256", "2", "28f", f"{FG_GREEN}READY{RESET}", " 8.6", " - ", "0x04", "94,310", "├─ tcp_worker"),
            ("312", "1", "18f", f"{FG_HI_YELLOW}REPLY-blocked{RESET}", " 1.2", " 6.2M", "0x02", "312", "/sbin/devb-eide -d ram"),
            ("480", "1", "40r", f"{FG_HI_GREEN}RUNNING{RESET}", "14.2", "24.8M", "0x08", "450,119", "/usr/bin/safety_actuator"),
            ("512", "1", "22f", f"{FG_CYAN}RECEIVE{RESET}", " 1.9", "12.0M", "0x05", "4,200", "/usr/bin/can_gateway"),
            ("620", "1", "10f", f"{FG_BLUE}NANOSLEEP{RESET}", " 0.0", " 4.1M", "-", "0", "/usr/bin/logger_daemon"),
            ("819", "1", "15f", f"{FG_GREEN}RUNNING{RESET}", " 3.8", " 8.9M", "0x09", "1,520", "/usr/bin/qnx-top-agent")
        ]
        
        for p in procs:
            lines.append(f" {p[0]:<6} {p[1]:<4} {p[2]:<5} {p[3]:<25} {p[4]:<6} {p[5]:<8} {p[6]:<6} {p[7]:<8} {BOLD}{p[8]}{RESET}")
            
        lines.append(f"\n {FG_HI_YELLOW}● NOTICE:{RESET} Thread in {BOLD}/sbin/devb-eide{RESET} is {FG_HI_YELLOW}REPLY-blocked{RESET} on Channel 0x02 (Latency: {self.stalled_ms}ms)")
        return "\n".join(lines)

    def render_tab2_channels(self):
        lines = []
        lines.append(f" {BOLD}{'CHID':<6} {'OWNER PID':<10} {'RECEIVE THREAD':<18} {'BOUND CLIENTS':<15} {'MSG/SEC':<10} {'STATUS':<20}{RESET}")
        lines.append(f" {DIM}{'─'*94}{RESET}")
        
        channels = [
            ("0x01", "1 [procmgr]", "TID 2 (PRI 250)", "14 Connections", "12,400", f"{FG_GREEN}ACTIVE - DETERMINISTIC{RESET}"),
            ("0x02", "312 [devb-eide]", "TID 1 (PRI 18)", " 3 Connections", "   140", f"{FG_HI_YELLOW}STALL DETECTED ({self.stalled_ms}ms){RESET}"),
            ("0x03", "124 [devc-ser]", "TID 1 (PRI 21)", " 5 Connections", " 1,820", f"{FG_GREEN}SYNCHRONOUS OK{RESET}"),
            ("0x04", "256 [io-pkt]", "TID 1 (PRI 30)", "22 Connections", "28,950", f"{FG_GREEN}HIGH THROUGHPUT{RESET}"),
            ("0x05", "512 [can_gw]", "TID 1 (PRI 22)", " 4 Connections", " 4,110", f"{FG_GREEN}DETERMINISTIC{RESET}"),
            ("0x08", "480 [safety_act]", "TID 1 (PRI 40)", " 2 Connections", "50,000", f"{FG_HI_CYAN}HARD REAL-TIME (20µs){RESET}")
        ]
        
        for c in channels:
            lines.append(f" {BOLD}{c[0]:<6}{RESET} {c[1]:<10} {c[2]:<18} {c[3]:<15} {c[4]:<10} {c[5]}")
            
        lines.append(f"\n {BOLD}IPC Analysis & Message Queue Profiling:{RESET}")
        lines.append(f" ├─ Synchronous MsgSend/MsgReceive Cycles: {FG_HI_GREEN}97,420 msgs/sec{RESET} (Zero-copy buffers)")
        lines.append(f" ├─ Pulse Event Deliveries (ISR/Timers):  {FG_HI_CYAN}142,800 pulses/sec{RESET} (Non-blocking)")
        lines.append(f" └─ {FG_HI_YELLOW}Priority Inversion Guard:{RESET} Channel 0x02 has inherited priority 18 -> boosted.")
        return "\n".join(lines)

    def render_tab3_aps(self):
        lines = []
        lines.append(f" {BOLD}{'PARTITION NAME':<22} {'BUDGET':<10} {'ACTUAL USAGE':<30} {'PEAK':<8} {'CRITICAL FLAGS'}{RESET}")
        lines.append(f" {DIM}{'─'*94}{RESET}")
        
        p1_use = min(65.0, 48.0 + 10.0 * math.sin(self.tick * 0.2))
        p2_use = min(25.0, 18.0 + 4.0 * math.cos(self.tick * 0.3))
        p3_use = min(10.0, 8.5 + 1.2 * math.sin(self.tick * 0.1))
        
        lines.append(f" {BOLD}{'System_Kernel':<22}{RESET} {' 10.0%':<10} {draw_gauge(p3_use*10, 16, color=FG_HI_BLUE)} {p3_use:4.1f}%     {' 9.8%':<8} {FG_GREEN}[PROTECTED]{RESET}")
        lines.append(f" {BOLD}{'Safety_Critical':<22}{RESET} {' 65.0%':<10} {draw_gauge(p1_use*1.5, 16, color=FG_HI_GREEN)} {p1_use:4.1f}%     {'62.4%':<8} {FG_HI_GREEN}[HARD REAL-TIME]{RESET}")
        lines.append(f" {BOLD}{'Networking_IO':<22}{RESET} {' 15.0%':<10} {draw_gauge(p2_use*4.0, 16, color=FG_HI_CYAN)} {p2_use:4.1f}%     {'14.9%':<8} {FG_CYAN}[DYNAMIC]{RESET}")
        lines.append(f" {BOLD}{'Background_Telemetry':<22}{RESET} {' 10.0%':<10} {draw_gauge(35.0, 16, color=FG_HI_YELLOW)} {' 6.2%':<9} {' 8.9%':<8} {FG_YELLOW}[BEST EFFORT]{RESET}")
        
        lines.append(f"\n {BOLD}Partitioning Guarantees under Stress:{RESET}")
        lines.append(f" • Guarantee: Safety_Critical partition guaranteed min {BOLD}65.0%{RESET} of CPU execution window.")
        lines.append(f" • Bankruptcy Policy: {FG_HI_GREEN}Active (Graceful partition throttling enabled){RESET}")
        lines.append(f" • Sched Period: {BOLD}100 ms{RESET} | Tick Size: {BOLD}1 ms{RESET} | Max jitter: {FG_HI_GREEN}< 12 µs{RESET}")
        return "\n".join(lines)

    def render_tab4_resmgr(self):
        lines = []
        lines.append(f" {BOLD}{'PATH':<24} {'OWNER':<20} {'MAJOR/MINOR':<14} {'HANDLERS REGISTERED':<22} {'I/O REQ/S'}{RESET}")
        lines.append(f" {DIM}{'─'*94}{RESET}")
        
        nodes = [
            ("/dev/null", "procmgr (PID 1)", "1, 0", "_IO_READ, _IO_WRITE", "   12"),
            ("/dev/zero", "procmgr (PID 1)", "1, 1", "_IO_READ", "    0"),
            ("/dev/ser1", "devc-ser8250 (PID 124)", "4, 64", "_IO_READ, _IO_DEVCTL", "1,420"),
            ("/dev/socket", "io-pkt (PID 256)", "8, 0", "_IO_READ, _IO_WRITE, _IO_DEVCTL", "18,910"),
            ("/dev/hd0", "devb-eide (PID 312)", "2, 0", "_IO_READ, _IO_WRITE", "  420"),
            ("/dev/can0", "can_gateway (PID 512)", "12, 0", "_IO_DEVCTL, _IO_READ", "4,100"),
            ("/dev/safety_actuator", "safety_act (PID 480)", "14, 0", "_IO_DEVCTL, _IO_PULSE", "50,000")
        ]
        
        for n in nodes:
            lines.append(f" {FG_HI_CYAN}{n[0]:<24}{RESET} {n[1]:<20} {n[2]:<14} {n[3]:<22} {BOLD}{n[4]}{RESET}")
            
        lines.append(f"\n {BOLD}Resource Manager Integrity Check:{RESET}")
        lines.append(f" • Registered prefix paths: {FG_HI_GREEN}14 mounted{RESET} | Dynamic handlers: {FG_HI_GREEN}OK{RESET}")
        lines.append(f" • resmgr_attach() status: Full POSIX pathname resolution mapped to native QNX microkernel IPC.")
        return "\n".join(lines)

    def render_footer(self):
        sec_left = max(0, int(self.cycle_interval - (time.time() - self.last_tab_change)))
        return (
            f"{DIM}{'─'*96}{RESET}\n"
            f" {BG_DARK_GRAY}{FG_HI_WHITE} AUTO-DEMO MODE: Cycling tab in {sec_left}s │ Press Ctrl+C to Exit │ Frame: {self.tick:04d} {RESET}"
        )

    def run(self):
        # Clear screen once at startup
        os.system('cls' if os.name == 'nt' else 'clear')
        print(HIDE_CURSOR, end="")
        try:
            while True:
                self.update_metrics()
                
                header = self.render_header()
                if self.current_tab == 1:
                    body = self.render_tab1_processes()
                elif self.current_tab == 2:
                    body = self.render_tab2_channels()
                elif self.current_tab == 3:
                    body = self.render_tab3_aps()
                elif self.current_tab == 4:
                    body = self.render_tab4_resmgr()
                else:
                    body = ""
                    
                footer = self.render_footer()
                
                # Write to top-left without causing infinite vertical scrolling
                frame = f"{CURSOR_HOME}{header}\n\n{body}\n\n{footer}"
                sys.stdout.write(frame)
                sys.stdout.flush()
                time.sleep(0.3)
        except KeyboardInterrupt:
            pass
        finally:
            print(SHOW_CURSOR + RESET + "\n\nExited cleanly.")


if __name__ == "__main__":
    demo = QnxTopDemo()
    demo.run()
