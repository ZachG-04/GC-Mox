#!/usr/bin/env python3
import subprocess
import numpy as np
import matplotlib.pyplot as plt
import time

CMD = ["./forced_2_fft"]

latest_fft = {
    "0x76": None,
    "0x77": None,
    "Fs": None
}

def parse_fft(line):
    parts = line.strip().split(",")

    if len(parts) < 6:
        return None

    if parts[0] != "FFT":
        return None

    t_ms = float(parts[1])
    addr = parts[2]
    Fs = float(parts[3])
    mags = np.array([float(x) for x in parts[4:]])

    return addr, Fs, mags


def main():
    proc = subprocess.Popen(
        CMD,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        bufsize=1
    )

    plt.ion()
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(10,4))

    l1, = ax1.plot([], [], marker="o")
    l2, = ax2.plot([], [], marker="o", color="orange")

    ax1.set_title("Sensor 0x76 FFT")
    ax2.set_title("Sensor 0x77 FFT")

    for ax in (ax1, ax2):
        ax.set_xlabel("Frequency (Hz)")
        ax.set_ylabel("Magnitude")
        ax.grid(True)

    last_draw = 0

    try:
        for raw in proc.stdout:
            raw = raw.strip()

            parsed = parse_fft(raw)
            if not parsed:
                continue

            addr, Fs, mags = parsed
            latest_fft[addr] = mags
            latest_fft["Fs"] = Fs

            if latest_fft["0x76"] is None or latest_fft["0x77"] is None:
                continue

            N = (len(mags) - 1) * 2
            freqs = np.arange(len(mags)) * Fs / N

            now = time.monotonic()
            if now - last_draw < 0.05:
                continue

            l1.set_data(freqs, latest_fft["0x76"])
            l2.set_data(freqs, latest_fft["0x77"])

            ax1.relim()
            ax1.autoscale_view()
            ax2.relim()
            ax2.autoscale_view()

            plt.pause(0.001)
            last_draw = now

    except KeyboardInterrupt:
        pass
    finally:
        proc.terminate()


if __name__ == "__main__":
    main()

