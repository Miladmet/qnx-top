"""
Generates the 5 audio narration segments using Windows System.Speech.Synthesis
and stitches them together into master_narration.wav.
"""

import subprocess
import os
import wave

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))

SCENES = [
    {
        "id": "scene1",
        "title": "Introduction",
        "text": (
            "When building mission-critical software on QNX 8.0, performance is not just about CPU percentages "
            "or RAM usage. In a true microkernel architecture, system health depends on message-passing latency, "
            "channel blocking states, and Adaptive Partitioning budgets. Standard Unix tools cannot inspect microkernel "
            "primitives, and static commands make tracking real-time bottlenecks tedious. This is qnx-top: a lightweight, "
            "real-time terminal observability tool and IPC profiler designed specifically for QNX Neutrino 8.0."
        )
    },
    {
        "id": "scene2",
        "title": "Processes & Thread States",
        "text": (
            "Connected directly to our QNX 8.0 target running inside QEMU, qnx-top queries the microkernel debug and proc "
            "interfaces with zero real-time jitter. In the process view, qnx-top surfaces genuine QNX-native thread states "
            "rather than generic POSIX abstractions. We observe real-time priority levels, pulse event frequencies, and "
            "instant identification of threads in RECEIVE-blocked or REPLY-blocked conditions across all four CPU cores."
        )
    },
    {
        "id": "scene3",
        "title": "Channels & Message Passing",
        "text": (
            "Here is the defining feature of qnx-top: the native Channel and Connection Matrix. In QNX, all client-server "
            "communication flows through synchronous MsgSend and MsgReceive calls. qnx-top inspects every active Channel ID, "
            "tracking message throughput and instantaneous latency. Notice this live alert: a worker thread in the storage "
            "manager is REPLY-blocked on Channel 2 for over 12 milliseconds. qnx-top immediately exposes the server channel "
            "causing the bottleneck and verifies that priority inheritance is active, preventing priority inversion from stalling critical tasks."
        )
    },
    {
        "id": "scene4",
        "title": "Adaptive Partitioning & ResMgr",
        "text": (
            "Next, we inspect QNX's Adaptive Partitioning System. Under heavy multi-core load, qnx-top monitors CPU partition "
            "guarantees in real time, validating that our Safety-Critical partition maintains its 65 percent guaranteed execution "
            "budget with under 12 microseconds of scheduling jitter. Switching to the Resource Manager tree, qnx-top catalogs every "
            "registered path under dev, mapping POSIX pathname resolution directly to owner PIDs and active handler functions."
        )
    },
    {
        "id": "scene5",
        "title": "Conclusion & Outro",
        "text": (
            "qnx-top is completely open source under the MIT License, targets QNX 8.0 Everywhere on both x86_64 and ARM64, and "
            "includes turnkey QEMU launch scripts so judges can verify it with a single command. Bringing modern, real-time "
            "observability to the QNX microkernel. Thank you."
        )
    }
]


def generate_audio():
    durations = []
    wav_files = []
    
    for i, sc in enumerate(SCENES):
        out_wav = os.path.join(SCRIPT_DIR, f"{sc['id']}.wav")
        wav_files.append(out_wav)
        
        # Clean text for PowerShell string
        escaped_text = sc["text"].replace("'", "''")
        ps_cmd = (
            f"Add-Type -AssemblyName System.Speech; "
            f"$s = New-Object System.Speech.Synthesis.SpeechSynthesizer; "
            f"$s.SelectVoiceByHints('Male'); "
            f"$s.Rate = 0; "
            f"$s.SetOutputToWaveFile('{out_wav}'); "
            f"$s.Speak('{escaped_text}'); "
            f"$s.Dispose()"
        )
        
        print(f"Synthesizing {sc['id']} ({sc['title']})...")
        subprocess.run(["powershell", "-NoProfile", "-Command", ps_cmd], check=True)
        
        with wave.open(out_wav, "rb") as w:
            dur = w.getnframes() / float(w.getframerate())
            durations.append(dur)
            print(f"  -> Duration: {dur:.2f}s")
            
    # Combine into master_narration.wav with 1.0s silence between scenes
    master_path = os.path.join(SCRIPT_DIR, "master_narration.wav")
    print(f"\nCombining into {master_path}...")
    
    first_w = wave.open(wav_files[0], "rb")
    params = first_w.getparams()
    first_w.close()
    
    # 1 second of silence
    silence_frames = b"\x00" * (params.sampwidth * params.nchannels * int(params.framerate * 1.0))
    
    with wave.open(master_path, "wb") as out_w:
        out_w.setparams(params)
        for i, wf in enumerate(wav_files):
            with wave.open(wf, "rb") as in_w:
                out_w.writeframes(in_w.readframes(in_w.getnframes()))
            if i < len(wav_files) - 1:
                out_w.writeframes(silence_frames)
                durations[i] += 1.0  # account for silence gap
                
    with wave.open(master_path, "rb") as mw:
        total_dur = mw.getnframes() / float(mw.getframerate())
        print(f"Total Narration Duration: {total_dur:.2f} seconds ({int(total_dur // 60)}m {int(total_dur % 60)}s)")
        
    return durations, total_dur


if __name__ == "__main__":
    generate_audio()
