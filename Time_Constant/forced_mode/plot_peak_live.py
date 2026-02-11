#!/usr/bin/env python3
import subprocess
import time
from collections import deque

import numpy as np
import matplotlib.pyplot as plt

CMD = ["./forced_mode"]

# How much history to show (seconds)
WINDOW_SEC = 120

def parse_fft_line(line: str):
    # Expected: FFT,cycle_id,Fs,mag1,mag2,...
    parts = line.strip().split(",")
    if len(parts) < 4:
        return None
    cycle_id = int(parts[1])
    Fs = float(parts[2])
    mags = np.array([float(x) for x in parts[3:]], dtype=float)
    return cycle_id, Fs, mags

def main():
    proc = subprocess.Popen(
        CMD,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        bufsize=1,
        universal_newlines=True,
    )

    # store (t, peak_mag, peak_freq)
    t0 = time.time()
    ts = deque()
    peak_mags = deque()
    peak_freqs = deque()

    plt.ion()
    fig, ax = plt.subplots()
    line_mag, = ax.plot([], [], marker="o")
    ax.set_xlabel("Time (s)")
    ax.set_ylabel("Peak magnitude (a.u.)")
    ax.set_title("Live FFT peak magnitude (scrolling)")
    ax.grid(True)

    try:
        for raw in proc.stdout:
            raw = raw.strip()
            if not raw:
                continue

            if not raw.startswith("FFT,"):
                continue

            parsed = parse_fft_line(raw)
            if not parsed:
                continue

            cycle_id, Fs, mags = parsed

            # Infer frequency axis
            K = len(mags)      # bins printed: 1..K
            M = 2 * K          # feature length assumption used in your earlier code
            freqs = (np.arange(1, K + 1) * Fs) / M

            # Peak (ignore NaNs just in case)
            if mags.size == 0 or not np.isfinite(mags).any():
                continue

            idx = int(np.nanargmax(mags))
            peak_mag = float(mags[idx])
            peak_freq = float(freqs[idx])

            now = time.time() - t0

            ts.append(now)
            peak_mags.append(peak_mag)
            peak_freqs.append(peak_freq)

            # Drop old points outside window
            while ts and (now - ts[0] > WINDOW_SEC):
                ts.popleft()
                peak_mags.popleft()
                peak_freqs.popleft()

            # Update plot
            line_mag.set_data(list(ts), list(peak_mags))
            ax.relim()
            ax.autoscale_view()

            ax.set_title(f"Peak mag (cycle {cycle_id})  |  f_peak = {peak_freq:.2f} Hz")
            plt.pause(0.001)

    except KeyboardInterrupt:
        print("\nStopping...")
    finally:
        proc.terminate()
        try:
            proc.wait(timeout=1)
        except Exception:
            pass

if __name__ == "__main__":
    main()
