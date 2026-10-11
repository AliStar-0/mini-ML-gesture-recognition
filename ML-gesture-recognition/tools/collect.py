import argparse
import sys
import time
from pathlib import Path

import serial
from serial.tools import list_ports

N_FEATURES = 300
DATA_DIR = Path(__file__).resolve().parent.parent / "data"

ESP32_USB_VENDORS = {0x10C4, 0x1A86, 0x0403, 0x303A}


def pick_port(requested):
    if requested:
        return requested
    ports = sorted(list_ports.comports(), key=lambda p: p.device)
    if not ports:
        sys.exit("No serial port found. Is the board plugged in?")
    if len(ports) == 1:
        return ports[0].device

    usb = [p for p in ports if getattr(p, "vid", None) is not None]
    known = [p for p in usb if p.vid in ESP32_USB_VENDORS]
    for candidates in (known, usb):
        if len(candidates) == 1:
            print(f"Using {candidates[0].device} ({candidates[0].description}).")
            return candidates[0].device

    print("Several serial ports found:")
    for i, p in enumerate(ports, 1):
        print(f"  {i}. {p.device}  {p.description}")
    print("Tip: unplug the board and run this again to see which one disappears.")
    while True:
        answer = input(f"Which one is the ESP32? (1-{len(ports)}): ").strip()
        if answer.isdigit() and 1 <= int(answer) <= len(ports):
            return ports[int(answer) - 1].device


def reconnect(port, baud, wait_seconds=60):
    print(f"\n  Lost the connection to {port}. The USB plug probably moved.")
    print("  Check the cable. Waiting up to a minute for the board to come back...")
    deadline = time.time() + wait_seconds
    while time.time() < deadline:
        time.sleep(1)
        try:
            return serial.Serial(port, baud, timeout=1)
        except serial.SerialException:
            pass
    return None


def main():
    parser = argparse.ArgumentParser(description="Record gestures from the board into data/<label>.csv.")
    parser.add_argument("--label", required=True, help="gesture name, e.g. circle")
    parser.add_argument("--count", type=int, default=30, help="how many to record (default 30)")
    parser.add_argument("--port", help="serial port, e.g. COM5 or /dev/ttyUSB0")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--add", action="store_true",
                        help="add to an existing file without asking")
    args = parser.parse_args()

    label = args.label.strip().lower()
    if not label.replace("_", "").isalnum():
        sys.exit("Use only letters, digits and underscores in the label.")

    DATA_DIR.mkdir(exist_ok=True)
    path = DATA_DIR / f"{label}.csv"
    existing = sum(1 for _ in path.open()) if path.exists() else 0

    mode = "a"
    if existing and not args.add:
        answer = ""
        while answer not in ("a", "s"):
            answer = input(f"{path.name} already has {existing} recordings. "
                           "Add to them or start over? (a/s): ").strip().lower()
        if answer == "s":
            mode, existing = "w", 0

    port = pick_port(args.port)
    try:
        ser = serial.Serial(port, args.baud, timeout=1)
    except serial.SerialException as err:
        sys.exit(f"Could not open {port}: {err}\nClose the PlatformIO serial monitor and try again.")

    print(f"Recording '{label}' on {port} into {path.name} ({existing} already saved).")
    print("Hold the button, draw the gesture, release. Ctrl+C to stop.\n")

    saved = 0
    try:
        with path.open(mode) as out:
            while saved < args.count:
                try:
                    raw = ser.readline()
                except serial.SerialException:
                    try:
                        ser.close()
                    except Exception:
                        pass
                    ser = reconnect(port, args.baud)
                    if ser is None:
                        print("  The board did not come back. Plug it in and run this again;")
                        print("  choose 'a' to keep what is already saved.")
                        break
                    print("  Reconnected. Carry on.\n")
                    continue

                line = raw.decode("utf-8", errors="replace").strip()
                if not line:
                    continue
                if not line.startswith("G,"):
                    print("  board:", line.lstrip("# "))
                    continue

                values = line[2:].split(",")
                try:
                    [float(v) for v in values]
                except ValueError:
                    values = []
                if len(values) != N_FEATURES:
                    print("  garbled line, ignored. Do that one again.")
                    continue

                out.write(",".join(values) + "\n")
                out.flush()
                saved += 1
                print(f"[{saved}/{args.count}] saved")
    except KeyboardInterrupt:
        print("\nStopped.")
    finally:
        if ser is not None:
            try:
                ser.close()
            except Exception:
                pass

    print(f"\n{path.name} now has {existing + saved} gestures.")


if __name__ == "__main__":
    main()
