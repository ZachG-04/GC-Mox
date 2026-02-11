#!/usr/bin/env python3
"""
plot_gas_live.py

- Runs ./forced_mode (your C binary)
- Live plots gas resistance vs time
- Requires an initial label at startup
- Lets you change label while running (type new label + Enter)
- Adds a vertical marker + label text on the plot when label changes
- Logs everything to CSV (data rows include current label; event rows record label switches)
"""

import subprocess
import threading
import queue
import csv
import datetime as dt
import time
import matplotlib.pyplot as plt


CMD = ["./forced_mode"]          # name of your compiled C binary
OUT_CSV = "turps2.csv"
MAX_POINTS_ON_SCREEN = 1200

event_q = queue.Queue()


def input_thread():
    """Wait for user to type a new label"""
    while True:
        try:
            s = input().strip()
        except EOFError:
            return
        if s:
            event_q.put(s)


def main():
    # Require starting label
    current_label = input("Starting material label (e.g. air): ").strip()
    if not current_label:
        current_label = "unknown"

    print(f"Current label = {current_label}")
    print("Type a NEW label and press Enter to mark a change.")
    print("Press Ctrl+C to stop.\n")

    # Start C program
    proc = subprocess.Popen(
        CMD,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        bufsize=1,
    )

    threading.Thread(target=input_thread, daemon=True).start()

    # CSV setup
    with open(OUT_CSV, "w", newline="") as csv_file:
        writer = csv.writer(csv_file)
        writer.writerow([
            "row_type", "wall_time", "time_s",
            "gas_ohm", "temp_C", "hum_pct",
            "press_Pa", "status", "label"
        ])
        csv_file.flush()

        # Plot setup
        plt.ion()
        fig, ax = plt.subplots()
        line, = ax.plot([], [], lw=1)
        ax.set_xlabel("Time (s)")
        ax.set_ylabel("Gas resistance (ohm)")
        ax.set_title("BME69x Gas resistance vs time")
        ax.grid(True)

        t_s = []
        gas = []
        got_header = False
        last_redraw = 0.0

        try:
            for raw in proc.stdout:
                raw = raw.strip()
                if not raw:
                    continue

                if raw.startswith("t_ms,"):
                    got_header = True
                    continue

                if not got_header:
                    continue

                parts = raw.split(",")
                if len(parts) < 2:
                    continue

                try:
                    t_ms = float(parts[0])
                    gas_ohm = float(parts[1])
                except ValueError:
                    continue

                ts = t_ms / 1000.0
                t_s.append(ts)
                gas.append(gas_ohm)

                if len(t_s) > MAX_POINTS_ON_SCREEN:
                    t_s[:] = t_s[-MAX_POINTS_ON_SCREEN:]
                    gas[:] = gas[-MAX_POINTS_ON_SCREEN:]

                now_wall = dt.datetime.now().isoformat(timespec="seconds")

                writer.writerow([
                    "DATA", now_wall, f"{ts:.3f}", f"{gas_ohm:.3f}",
                    "", "", "", "", current_label
                ])

                if len(t_s) % 25 == 0:
                    csv_file.flush()

                while not event_q.empty():
                    new_label = event_q.get()
                    current_label = new_label
                    event_t = t_s[-1]

                    ax.axvline(event_t, ls="--", lw=1)
                    ymin, ymax = ax.get_ylim()
                    ax.text(
                        event_t, ymax,
                        current_label,
                        rotation=90,
                        va="top",
                        ha="right",
                        fontsize=9,
                        bbox=dict(boxstyle="round", alpha=0.3)
                    )

                    print(f"[MARK] {current_label} at t={event_t:.2f}s")
                    writer.writerow([
                        "EVENT", now_wall, f"{event_t:.3f}",
                        "", "", "", "", "", current_label
                    ])
                    csv_file.flush()

                now = time.monotonic()
                if now - last_redraw > 0.1:
                    line.set_data(t_s, gas)
                    ax.relim()
                    ax.autoscale_view()
                    plt.pause(0.001)
                    last_redraw = now

        except KeyboardInterrupt:
            print("\nStopping...")
        finally:
            proc.terminate()


if __name__ == "__main__":
    main()
