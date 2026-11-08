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
            "When building mission-critical software on QNX 8.0, CPU and RAM numbers are only part of the picture. "
            "In a microkernel system, much of the story is in message passing: which threads are blocked waiting for "
            "a reply, and from which server. Standard Unix tools do not show that. This is qnx-top, an open source "
            "diagnostic tool for QNX Neutrino 8.0. Please note: the screens in this video are an animated mock-up "
            "with sample data, not a live capture. The native collector is pre-release."
        )
    },
    {
        "id": "scene2",
        "title": "Processes & Thread States",
        "text": (
            "This is the process view. For each process, qnx-top shows the scheduler state of its threads, such as "
            "RECEIVE, READY, or REPLY-blocked, along with priority and CPU percentage. On a QNX target, this "
            "information comes from the proc file system, using devctl calls. The numbers shown here are sample data."
        )
    },
    {
        "id": "scene3",
        "title": "Channels & Message Passing",
        "text": (
            "The channel view groups REPLY-blocked client threads by the server channel they are waiting on. "
            "In this sample, a thread in the storage manager has been waiting on a server channel for over twelve "
            "milliseconds, so the channel is flagged as stalled. qnx-top measures this by timing how long a thread "
            "stays in the REPLY state between samples. It does not detect priority inversion yet, and message "
            "rates are not collected yet."
        )
    },
    {
        "id": "scene4",
        "title": "Adaptive Partitioning & ResMgr",
        "text": (
            "On a real target, qnx-top detects whether the Adaptive Partitioning scheduler is active, and lists the "
            "entries under slash dev. Per-partition budgets, and owner and handler details for dev entries, are not "
            "collected yet and appear as N slash A. The partition and resource manager screens in this video show "
            "sample data to illustrate the intended layout."
        )
    },
    {
        "id": "scene5",
        "title": "Conclusion & Outro",
        "text": (
            "qnx-top is open source under the MIT License. A host build with unit tests is included, and validating "
            "the native QNX build on a real target is the next step. Thank you."
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
