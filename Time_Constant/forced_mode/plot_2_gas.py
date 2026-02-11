#!/usr/bin/env python3
"""
plot_2_gas_twin_live.py

- Runs ./forced_2_gas
- Parses lines: t_ms,addr,gas_ohm,temp_C,hum_pct,press_Pa,status
- Live plots BOTH sensors (0x76 on left axis, 0x77 on right axis using twinx)
- Label system: type new label + Enter to mark graph + CSV
- Saves CSV (appends by default)
"""

import subprocess
import threading
import queue
import csv
import datetime as dt
import time
import matplotlib.pyplot as plt
import os
import sys

CMD = ["./forced_2_gas"]
OUT_CSV = "petroleum2_23_12_16_10am.csv"
APPEND_CSV = False 

MAX_POINTS_ON_SCREEN = 1500
REDRAW_EVERY_S = 0.10

event_q = queue.Queue()


def input_thread():
    """Background thread: user types a new label + Enter."""
    while True:
        try:
            s = input().strip()
        except EOFError:
            return
        if s:
            event_q.put(s)


def parse_line(line: str):
    """
    Returns dict with keys:
      t_ms, addr, gas_ohm, temp_C, hum_pct, press_Pa, status
    or:
      "HEADER"
    or:
      None
    """
    parts = line.strip().split(",")
    if not parts:
        return None

    if parts[0] == "t_ms":
        return "HEADER"

    if len(parts) < 7:
        return None

    try:
        t_ms = float(parts[0])
        addr = parts[1].strip()
        gas_ohm = float(parts[2])
        temp_C = float(parts[3])
        hum_pct = float(parts[4])
        press_Pa = float(parts[5])
        status = parts[6].strip()
        return {
            "t_ms": t_ms,
            "addr": addr,
            "gas_ohm": gas_ohm,
            "temp_C": temp_C,
            "hum_pct": hum_pct,
            "press_Pa": press_Pa,
            "status": status,
        }
    except ValueError:
        return None


def main():
    current_label = input("Starting material label (e.g. air): ").strip() or "unknown"
    print(f"\nCurrent label = {current_label}")
    print("Type a NEW label + Enter to mark/switch. Ctrl+C to stop.\n")

    proc = subprocess.Popen(
        CMD,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        bufsize=1,
        universal_newlines=True,
    )

    threading.Thread(target=input_thread, daemon=True).start()

    # CSV open (append or write)
    file_exists = os.path.exists(OUT_CSV)
    mode = "a" if (APPEND_CSV and file_exists) else "w"
    csv_file = open(OUT_CSV, mode, newline="")
    writer = csv.writer(csv_file)

    if mode == "w":
        writer.writerow([
            "row_type", "wall_time", "time_s", "addr",
            "gas_ohm", "temp_C", "hum_pct", "press_Pa", "status",
            "label"
        ])
        csv_file.flush()

    # --- Plot setup ---
    plt.ion()
    fig, ax = plt.subplots()
    a1 = ax.twinx()

    # Make sure the window actually shows
    plt.show(block=False)

    line76, = ax.plot([], [], linewidth=1, label="Output")
    line77, = a1.plot([], [], linewidth=1, label="Input", color="orange")

    ax.set_xlabel("Time (s)")
    ax.set_ylabel("Gas resistance (ohm) - Output")
    a1.set_ylabel("Gas resistance (ohm) - Input", color="orange")

    a1.tick_params(axis="y", colors="orange")
    
    ax.set_title("Dual BME69x Gas Resistance (twin axes)")
    ax.grid(True)

    # Combined legend (from both axes)
    lines = [line76, line77]
    labels = [l.get_label() for l in lines]
    ax.legend(lines, labels, loc="upper left")

    # Data buffers
    t_s = []
    gas76 = []
    gas77 = []

    # "last known" per sensor so we only add points when both are present
    last76 = None
    last77 = None
    last_ts = None

    last_redraw = 0.0

    def add_marker(x_time: float, label_text: str):
        
        ax.axvline(x_time, linestyle="--", linewidth=1)
        a1.axvline(x_time, linestyle="--", linewidth=1)

        # Place text near top of each axis (inside view)
        yminL, ymaxL = ax.get_ylim()
        y_text_L = ymaxL - 0.05 * (ymaxL - yminL)
        ax.text(
            x_time, y_text_L, label_text,
            rotation=90, va="top", ha="right", fontsize=9,
            bbox=dict(boxstyle="round,pad=0.2", alpha=0.3)
        )

        yminR, ymaxR = a1.get_ylim()
        y_text_R = ymaxR - 0.05 * (ymaxR - yminR)
        a1.text(
            x_time, y_text_R, label_text,
            rotation=90, va="top", ha="right", fontsize=9,
            bbox=dict(boxstyle="round,pad=0.2", alpha=0.3)
        )

    try:
        if proc.stdout is None:
            print("No stdout from process.")
            return 1

        for raw in proc.stdout:
            raw = raw.strip()
            if not raw:
                continue

            parsed = parse_line(raw)
            if parsed == "HEADER":
                continue
            if parsed is None:
                continue

            ts = parsed["t_ms"] / 1000.0
            addr = parsed["addr"]
            g = parsed["gas_ohm"]

            # Update last-known values
            if addr == "0x76":
                last76 = g
            elif addr == "0x77":
                last77 = g
            else:
                continue

            # If we have both values, store one synchronized point
            if last76 is not None and last77 is not None:
                
                t_s.append(ts)
                gas76.append(last76)
                gas77.append(last77)
                last_ts = ts

                # Trim
                if len(t_s) > MAX_POINTS_ON_SCREEN:
                    t_s[:] = t_s[-MAX_POINTS_ON_SCREEN:]
                    gas76[:] = gas76[-MAX_POINTS_ON_SCREEN:]
                    gas77[:] = gas77[-MAX_POINTS_ON_SCREEN:]

            # CSV log (log every line with its addr + current_label)
            now_wall = dt.datetime.now().isoformat(timespec="seconds")
            writer.writerow([
                "DATA", now_wall, f"{ts:.3f}", addr,
                f"{parsed['gas_ohm']:.3f}",
                f"{parsed['temp_C']:.2f}",
                f"{parsed['hum_pct']:.2f}",
                f"{parsed['press_Pa']:.2f}",
                parsed["status"],
                current_label
            ])

            # flush sometimes
            if len(t_s) and (len(t_s) % 25 == 0):
                csv_file.flush()

            # Handle label switches
            while not event_q.empty():
                new_label = event_q.get().strip()
                if not new_label:
                    continue

                current_label = new_label
                event_t = last_ts if last_ts is not None else ts

                add_marker(event_t, current_label)
                print(f"[MARK] {current_label} at t={event_t:.2f}s")

                writer.writerow(["EVENT", now_wall, f"{event_t:.3f}", "", "", "", "", "", "", current_label])
                csv_file.flush()

            # redraw (need to autoscale BOTH axes)
            now = time.monotonic()
            if (now - last_redraw) >= REDRAW_EVERY_S and len(t_s) > 2:
                line76.set_data(t_s, gas76)
                line77.set_data(t_s, gas77)

                ax.relim()
                ax.autoscale_view()

                a1.relim()
                a1.autoscale_view()

                plt.pause(0.001)
                last_redraw = now

    except KeyboardInterrupt:
        print("\nStopping...")

    finally:
        try:
            proc.terminate()
        except Exception:
            pass
        try:
            csv_file.flush()
            csv_file.close()
        except Exception:
            pass
        print(f"Saved CSV: {OUT_CSV}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
