#!/usr/bin/env python3
import numpy as np
import math
import matplotlib.pyplot as plt

LOG = "sweep_2.csv"

def lockin_amp(t_s, x, f_hz):
    x = x - np.mean(x)  # remove DC
    w = 2 * math.pi * f_hz
    s = np.sin(w * t_s)
    c = np.cos(w * t_s)
    a_s = 2 * np.mean(x * s)
    a_c = 2 * np.mean(x * c)
    return math.sqrt(a_s*a_s + a_c*a_c)

def main():
    results = {"0x76": [], "0x77": []}  # list of (f_hz, amp)

    current = None  # dict: half_ms, f_hz, data_by_addr
    with open(LOG, "r") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue

            if line.startswith("header,"):
                continue

            if line.startswith("SWEEP,"):
                _, half_ms, f_hz, cycles, Fs = line.split(",")
                current = {
                    "half_ms": int(half_ms),
                    "f_hz": float(f_hz),
                    "data": {"0x76": {"t": [], "x": []},
                             "0x77": {"t": [], "x": []}}
                }
                continue

            if line.startswith("ENDSWEEP,"):
                if current:
                    f_hz = current["f_hz"]
                    for addr in ["0x76", "0x77"]:
                        t = np.array(current["data"][addr]["t"], dtype=float)
                        x = np.array(current["data"][addr]["x"], dtype=float)
                        if len(t) < 20:
                            continue

                        # drop first 1s of each sweep segment (extra warmup safety)
                        t = t / 1000.0
                        keep = t > (t[0] + 1.0)
                        t2 = t[keep] - t[keep][0]
                        x2 = x[keep]

                        if len(t2) < 20:
                            continue

                        amp = lockin_amp(t2, x2, f_hz)
                        results[addr].append((f_hz, amp))

                        print(f"{addr}  f={f_hz:.3f} Hz  amp={amp:.3f}")

                current = None
                continue

            # sample line: t_ms,addr,heater_C,gas_ohm
            if current:
                parts = line.split(",")
                if len(parts) != 4:
                    continue
                try:
                    t_ms = float(parts[0])
                    addr = parts[1].strip()
                    gas = float(parts[3])
                except ValueError:
                    continue
                if addr in current["data"]:
                    current["data"][addr]["t"].append(t_ms)
                    current["data"][addr]["x"].append(gas)

    plt.figure()

    for addr, marker in [("0x76", "o"), ("0x77", "s")]:
        arr = results[addr]
        if not arr:
            continue
        arr.sort(key=lambda z: z[0])
        freqs = np.array([z[0] for z in arr])
        amps  = np.array([z[1] for z in arr])
        plt.plot(freqs, amps, marker=marker, label=addr)

    plt.xlabel("Square-wave frequency (Hz)")
    plt.ylabel("Response amplitude (a.u., lock-in, DC removed)")
    plt.title("BME69x frequency response (both sensors)")
    plt.grid(True)
    plt.legend()
    plt.show()

if __name__ == "__main__":
    main()
