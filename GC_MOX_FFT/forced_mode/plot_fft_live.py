# Create Method log fft data to a CSV
#100 points not 10
# Square wave
# Remove DC
import subprocess
import numpy as np
import matplotlib.pyplot as plt
from collections import deque

CMD = ["./forced_mode"]

def parse_fft_line(line: str):
    # FFT,cycle_id,Fs,mag1,mag2,...  (magk is bin k)
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
    )

    plt.ion()
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(10, 9))

    # Spectrum plot
    spec_line, = ax1.plot([], [], marker="o")
    ax1.set_xlabel("Frequency (Hz)")
    ax1.set_ylabel("Magnitude (a.u.)")
    ax1.grid(True)

    # Scrolling peak history
    history_len = 120
    hist_cycles = deque(maxlen=history_len)
    hist_peak_f = deque(maxlen=history_len)
    hist_peak_m = deque(maxlen=history_len)

    peak_line, = ax2.plot([], [], marker="o")
    ax2.set_xlabel("Cycle # (recent)")
    ax2.set_ylabel("Peak magnitude")
    ax2.grid(True)

    last_cycle = None

    try:
        for raw in proc.stdout:
            raw = raw.strip()
            if not raw:
                continue

            if raw.startswith("FFT,"):
                parsed = parse_fft_line(raw)
                if not parsed:
                    continue

                cycle_id, Fs, mags = parsed

                # In C: y length M=20 => bins printed k=1..10 => mags len = 10
                # frequency for bin k is k*Fs/M; we can infer M = 2*len(mags)
                K = len(mags)
                M = 2 * (K - 1)
                freqs = (np.arange(0, K) * Fs) / M

                # Update spectrum
                spec_line.set_data(freqs, mags)
                ax1.relim()
                ax1.autoscale_view()
                ax1.set_title(f"Live FFT spectrum (cycle {cycle_id}, Fs={Fs:.1f} Hz, fmax={Fs/2:.1f} Hz)")

                # Peak tracking
                peak_idx = int(np.argmax(mags))
                peak_f = freqs[peak_idx]
                peak_m = mags[peak_idx]

                hist_cycles.append(cycle_id)
                hist_peak_f.append(peak_f)
                hist_peak_m.append(peak_m)

                # Plot peak magnitude vs time (cycle)
                x = np.arange(len(hist_peak_m))
                peak_line.set_data(x, list(hist_peak_m))
                ax2.relim()
                ax2.autoscale_view()
                ax2.set_title(f"Peak bin: {peak_f:.1f} Hz (mag {peak_m:.4f})")

                plt.pause(0.001)

                last_cycle = cycle_id

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
