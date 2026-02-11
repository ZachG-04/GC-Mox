#!/usr/bin/env python3
import subprocess
import threading
import queue
import csv
import datetime as dt
import time
import matplotlib.pyplot as plt

CMD = ["./forced_2_gas_ratio"]
OUT_CSV = "ratio_log.csv"

MAX_POINTS = 1500
REDRAW_EVERY = 0.05

event_q = queue.Queue()


# ---------------- Label input ----------------

def input_thread():
    while True:
        try:
            s = input().strip()
        except EOFError:
            return
        if s:
            event_q.put(s)


# ---------------- Main ----------------

def main():

    current_label = input("Starting label (air etc): ").strip() or "unknown"
    print("Type new label + Enter to mark switch\n")

    proc = subprocess.Popen(
        CMD,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        bufsize=1
    )

    threading.Thread(target=input_thread, daemon=True).start()

    csv_file = open(OUT_CSV, "w", newline="")
    writer = csv.writer(csv_file)
    writer.writerow(["type", "wall_time", "time_s", "addr", "ratio", "label"])
    csv_file.flush()

    plt.ion()
    fig, ax = plt.subplots(figsize=(8,4))

    line76, = ax.plot([], [], label="Output", linewidth=2)
    line77, = ax.plot([], [], label="Input", linewidth=2)

    ax.set_xlabel("Time (s)")
    ax.set_ylabel("High / Low ratio")
    ax.set_title("Thermal modulation drift-removed signal")
    ax.grid(True)
    ax.legend()

    t = []
    r76 = []
    r77 = []

    last76 = None
    last77 = None

    last_redraw = 0

    try:
        for raw in proc.stdout:
            raw = raw.strip()
            if not raw.startswith("RATIO,"):
                continue

            parts = raw.split(",")
            if len(parts) != 4:
                continue

            try:
                ts = float(parts[1]) / 1000.0
                addr = parts[2]
                val = float(parts[3])
            except:
                continue

            if addr == "0x76":
                last76 = val
            elif addr == "0x77":
                last77 = val
            else:
                continue

            if last76 is not None and last77 is not None:
                t.append(ts)
                r76.append(last76)
                r77.append(last77)

                if len(t) > MAX_POINTS:
                    t[:] = t[-MAX_POINTS:]
                    r76[:] = r76[-MAX_POINTS:]
                    r77[:] = r77[-MAX_POINTS:]

            now_wall = dt.datetime.now().isoformat(timespec="seconds")
            writer.writerow(["DATA", now_wall, f"{ts:.3f}", addr, f"{val:.6f}", current_label])

            # label switch
            while not event_q.empty():
                lab = event_q.get()
                current_label = lab
                mark_t = t[-1] if t else ts
                ax.axvline(mark_t, linestyle="--", alpha=0.6)

                ymin, ymax = ax.get_ylim()
                ax.text(mark_t, ymax*0.95, lab, rotation=90, va="top", fontsize=9)

                writer.writerow(["EVENT", now_wall, f"{mark_t:.3f}", "", "", lab])

            now = time.monotonic()
            if now - last_redraw > REDRAW_EVERY and len(t) > 3:
                line76.set_data(t, r76)
                line77.set_data(t, r77)
                ax.set_xlim(t[0], t[-1])
                ax.relim()
                ax.autoscale_view(scalex=False)

                plt.draw()
                plt.pause(0.001)

                last_redraw = now

    except KeyboardInterrupt:
        print("\nStopping")

    finally:
        proc.terminate()
        csv_file.close()
        print("Saved:", OUT_CSV)


if __name__ == "__main__":
    main()
