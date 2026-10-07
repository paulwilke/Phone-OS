#!/usr/bin/env python3
"""Configure the device over the USB cable, without taking the SD card out.

  python3 scripts/deviceconfig.py list
  python3 scripts/deviceconfig.py read settings.toml
  python3 scripts/deviceconfig.py write secrets.toml my-secrets.toml [--restart]
  python3 scripts/deviceconfig.py battery
  python3 scripts/deviceconfig.py sms +491701111111 "test message"
  python3 scripts/deviceconfig.py restart

Over the cable only settings.toml, contacts.toml and secrets.toml can be read and written, and the logs
system.log, system.old.log and messages.log read. Everything else on the card is the parents' page's business
(scripts/webbridge.py). A new file takes effect after a restart.

`battery` reads the fuel gauge and the charger without writing anything and prints one line for each value.

The port is found automatically (/dev/cu.usbmodem*, /dev/ttyACM*, /dev/ttyUSB*); --port overrides it.
Needs pyserial, which PlatformIO brings along:

  ~/.platformio/penv/bin/python scripts/deviceconfig.py list

Opening the serial port resets the ESP32 (the USB bridge restarts it), so the first command after opening
waits a moment for the device to come back; the firmware does not need the port to be open to work.
"""

import argparse
import glob
import sys
import time

WRITABLE = ("settings.toml", "contacts.toml", "secrets.toml")
LOGS = ("system" + ".log", "system" + ".old" + ".log", "messages" + ".log")  # built: a quoted word with a dot looks like a catalog key
READABLE = WRITABLE + LOGS
BOOT_WAIT = 3.0  # seconds the device needs after the port was opened


def find_port():
    for pattern in ("/dev/cu.usbmodem*", "/dev/ttyACM*", "/dev/ttyUSB*"):
        hits = sorted(glob.glob(pattern))
        if hits:
            return hits[0]
    sys.exit("No device found. Pass --port.")


def open_port(path):
    import serial  # imported late so that the protocol functions can be tested without pyserial

    ser = serial.Serial()
    ser.port = path
    ser.baudrate = 115200
    ser.timeout = 1
    ser.dtr = True  # the console of the ESP32-S3 talks only when the host says it is connected
    ser.rts = False
    ser.open()
    return ser


def checksum(data: bytes) -> int:
    """The 16-bit sum the device checks a write against. It catches a cut-off or garbled transfer, not an attacker."""
    return sum(data) & 0xFFFF


def read_until(ser, marker, timeout=5.0):
    """Reads lines until one starts with `marker`; the device's own log lines in between are skipped. Returns the
    marker line and the lines before it. Exits on an error line of the device."""
    end = time.time() + timeout
    seen = []
    while time.time() < end:
        line = ser.readline().decode("utf-8", "replace").rstrip("\r\n")
        if not line:
            continue
        if line.startswith("CONFIG ERROR"):
            sys.exit(line)
        if line.startswith(marker):
            return line, seen
        seen.append(line)
    sys.exit(f"No answer to '{marker}' (does the firmware run with the console?)")


def send(ser, text):
    ser.reset_input_buffer()
    ser.write((text + "\n").encode())
    ser.flush()


def do_list(ser):
    send(ser, "CONFIG LIST")
    read_until(ser, "CONFIG LIST BEGIN")
    _, lines = read_until(ser, "CONFIG LIST END")
    for line in lines:
        print(line)


def do_read(ser, name):
    send(ser, f"CONFIG READ {name}")
    head, _ = read_until(ser, "CONFIG CONTENT")
    length = int(head.split()[3])
    _, lines = read_until(ser, "CONFIG END", timeout=20)
    text = "\n".join(lines)
    print(text)
    # The final line break is lost when the lines are joined; anything beyond it would be a real loss.
    if len(text.encode()) + 1 < length:
        print(f"(note: {length} bytes reported, {len(text.encode())} read)", file=sys.stderr)


def do_write(ser, name, source, restart):
    data = open(source, "rb").read()
    if not data:
        sys.exit("The file is empty.")
    send(ser, f"CONFIG WRITE {name} {len(data)} {checksum(data)}")
    read_until(ser, "CONFIG READY")
    # In small pieces with a short pause: the device's receive buffer is finite, and an overrun looks like a
    # truncated file.
    for i in range(0, len(data), 64):
        ser.write(data[i:i + 64])
        ser.flush()
        time.sleep(0.01)
    answer, _ = read_until(ser, "CONFIG OK", timeout=15)
    print(answer)
    if restart:
        send(ser, "CONFIG RESTART")
        print("The device is restarting.")


def do_battery(ser):
    send(ser, "BATTERY")
    read_until(ser, "CONFIG BATTERY BEGIN")
    _, lines = read_until(ser, "CONFIG BATTERY END")
    for line in lines:
        print(line)


def do_sms(ser, number, text):
    send(ser, f"SMS {number} {text}")
    answer, _ = read_until(ser, "CONFIG OK", timeout=10)
    print(answer)


def build_parser():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--port", help="serial port of the device (default: found automatically)")
    sub = p.add_subparsers(dest="cmd", required=True)
    sub.add_parser("list", help="the files in /config with their sizes")
    r = sub.add_parser("read", help="print a file (of a long log the end)")
    r.add_argument("file", choices=READABLE)
    w = sub.add_parser("write", help="replace a configuration file")
    w.add_argument("file", choices=WRITABLE)
    w.add_argument("source", help="the local file to send")
    w.add_argument("--restart", action="store_true", help="restart the device afterwards")
    sub.add_parser("battery", help="read the fuel gauge and the charger (writes nothing)")
    s = sub.add_parser("sms", help="send a text by hand")
    s.add_argument("number")
    s.add_argument("text", help="\\n in the text is a line break")
    sub.add_parser("restart", help="restart the device")
    return p


def main(argv=None, opener=open_port):
    args = build_parser().parse_args(argv)
    ser = opener(args.port or find_port())
    try:
        time.sleep(BOOT_WAIT if opener is open_port else 0)
        if args.cmd == "list":
            do_list(ser)
        elif args.cmd == "read":
            do_read(ser, args.file)
        elif args.cmd == "write":
            do_write(ser, args.file, args.source, args.restart)
        elif args.cmd == "battery":
            do_battery(ser)
        elif args.cmd == "sms":
            do_sms(ser, args.number, args.text)
        elif args.cmd == "restart":
            send(ser, "CONFIG RESTART")
            print("The device is restarting.")
    finally:
        ser.close()


if __name__ == "__main__":
    main()
