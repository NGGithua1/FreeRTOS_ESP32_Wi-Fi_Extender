#---@neph---

import csv
import os
import sys
import time
from datetime import datetime
from pathlib import Path

import serial
from serial import SerialException


# -------------------- Configuration --------------------

SERIAL_PORT = "COM3"       # Change to your ESP32's actual COM port
BAUD_RATE = 115200          # Must match Serial.begin(115200)
READ_TIMEOUT = 0.1            # Seconds
FLUSH_INTERVAL = 1          # Flush buffered CSV data every second

# Store logs beside this script, inside the logs folder.
LOG_DIR = Path(__file__).resolve().parent / "logs"
LOG_FILE = LOG_DIR / "extender_log.csv"

CSV_COLUMNS = ["timestamp", "message_type", "message"]


def classify_message(message: str) -> str:
    """Classify common ESP32 serial output without modifying the message."""
    text = message.strip()

    if text.startswith("[AP]"):
        return "AP_EVENT"
    if text.startswith("[MONITOR]"):
        return "MONITOR"
    if text.startswith("---"):
        return "DIAGNOSTICS"
    if text.startswith("Upstream state:"):
        return "CONNECTION_STATE"
    if text.startswith("Upstream IP:"):
        return "UPSTREAM_IP"
    if text.startswith("RSSI:"):
        return "RSSI"
    if text.startswith("Connected duration:"):
        return "CONNECTION_DURATION"
    if text.startswith("AP clients:"):
        return "AP_CLIENTS"
    if text.startswith("NAPT enabled:"):
        return "NAPT_STATE"
    if text.startswith("Last disconnect reason:"):
        return "DISCONNECT_REASON"
    if text.startswith("Reconnect attempts:"):
        return "RECONNECT_ATTEMPTS"
    if text.startswith("Free heap:") or text.startswith("Minimum free heap:"):
        return "MEMORY"
    if "error" in text.lower() or "failed" in text.lower():
        return "ERROR"
    if not text:
        return "EMPTY_LINE"

    return "RAW"


def prepare_log_file():
    """Create the log directory and CSV header if the file is new/empty."""
    LOG_DIR.mkdir(parents=True, exist_ok=True)

    needs_header = not LOG_FILE.exists() or LOG_FILE.stat().st_size == 0

    file_handle = LOG_FILE.open(
        mode="a",
        newline="",
        encoding="utf-8",
    )

    writer = csv.DictWriter(file_handle, fieldnames=CSV_COLUMNS)

    if needs_header:
        writer.writeheader()
        file_handle.flush()
        os.fsync(file_handle.fileno())

    return file_handle, writer


def main():
    file_handle = None
    ser = None

    try:
        file_handle, writer = prepare_log_file()

        print(f"CSV log: {LOG_FILE}")
        print(f"Opening {SERIAL_PORT} at {BAUD_RATE} baud...")
        print("Close Arduino IDE Serial Monitor before continuing.")
        print("Press Ctrl+C to stop logging safely.\n")

        ser = serial.Serial(
            port=SERIAL_PORT,
            baudrate=BAUD_RATE,
            timeout=READ_TIMEOUT,
        )

        # Opening a serial connection may reset some ESP32 boards.
        print("Serial connection opened. Waiting for output...\n")

        last_flush = time.monotonic()

        while True:
            raw_line = ser.readline()

            if raw_line:
                # Preserve the received text, removing only line endings.
                message = raw_line.decode(
                    "utf-8", errors="replace"
                ).rstrip("\r\n")

                timestamp = datetime.now().astimezone().isoformat(
                    timespec="milliseconds"
                )

                record = {
                    "timestamp": timestamp,
                    "message_type": classify_message(message),
                    "message": message,
                }

                writer.writerow(record)

                # Show the same captured record in the VS Code terminal.
                print(f"[{timestamp}] {message}")

            if time.monotonic() - last_flush >= FLUSH_INTERVAL:
                file_handle.flush()
                last_flush = time.monotonic()

    except KeyboardInterrupt:
        print("\nStopping logger...")

    except (SerialException, OSError) as exc:
        print(f"\nLogger error: {exc}", file=sys.stderr)
        print(
            "Check the COM port, USB connection, and whether "
            "another application is using the port.",
            file=sys.stderr,
        )

    finally:
        if file_handle is not None:
            file_handle.flush()
            os.fsync(file_handle.fileno())
            file_handle.close()

        if ser is not None and ser.is_open:
            ser.close()

        print("Serial port and CSV file closed.")


if __name__ == "__main__":
    main()
