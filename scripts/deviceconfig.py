#!/usr/bin/env python3
"""Configure the device over the USB cable, without taking the SD card out.

  python3 scripts/deviceconfig.py list
  python3 scripts/deviceconfig.py read settings.toml
  python3 scripts/deviceconfig.py read system.log --all
  python3 scripts/deviceconfig.py write secrets.toml my-secrets.toml [--restart]
  python3 scripts/deviceconfig.py battery
  python3 scripts/deviceconfig.py sms +491701111111 "test message"
  python3 scripts/deviceconfig.py restart

Over the cable only settings.toml, contacts.toml and secrets.toml can be read and written, and the logs
system.log, system.old.log, messages.log, energy.log and energy.old.log read. Everything else on the card is the parents' page's business
(scripts/webbridge.py). A new file takes effect after a restart.

`read` prints the file byte for byte (of a long log the last 8 KB, and says where they start); the device sends the
length and a checksum with it, and a read that a log line of the device disturbed is repeated. A file that does not
exist reads like an empty one. `read <log> --all` prints the whole file: it asks for it in pieces of 8 KB from the
start (`CONFIG READ <file> <from>`) over the one open port, checks each piece the same way and goes on until the
end the device names; a log that grows meanwhile is read to its new end. `sms` says "accepted" when the text was sent or is waiting for the network; only an
empty text, one that is too long, or a full queue is refused. `battery` reads the fuel gauge and the charger
without writing anything and prints one line for each value.

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
LOGS = ("system" + ".log", "system" + ".old" + ".log", "messages" + ".log", "energy" + ".log", "energy" + ".old" + ".log")  # built: a quoted word with a dot looks like a catalog key
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


def read_exact(ser, count, timeout=20.0):
    """Exactly `count` bytes, or fewer when the device stops sending; blank lines and line ends are content here."""
    data = b""
    end = time.time() + timeout
    while len(data) < count and time.time() < end:
        data += ser.read(count - len(data))
    return data


def read_piece(ser, name, offset=None):
    """One answer of the device: (bytes, first, total). The device announces length and checksum, so a read that one
    of its own log lines disturbed is noticed and repeated instead of being printed with the line inside it."""
    command = f"CONFIG READ {name}" if offset is None else f"CONFIG READ {name} {offset}"
    for attempt in range(3):
        send(ser, command)
        head, _ = read_until(ser, "CONFIG CONTENT")
        fields = head.split()  # CONFIG CONTENT <file> <length> <sum> from <first> of <total>
        length, expected, first, total = int(fields[3]), int(fields[4]), int(fields[6]), int(fields[8])
        data = read_exact(ser, length)
        read_until(ser, "CONFIG END", timeout=5)
        if len(data) == length and checksum(data) == expected:
            return data, first, total
        print("The answer was disturbed (a log line of the device in it?). Reading again.", file=sys.stderr)
    sys.exit("The file could not be read without a disturbance in three tries.")


def read_whole(ser, name):
    """All of the file, piece after piece from 0. The bytes are joined before they are decoded, because a piece may
    end in the middle of a character."""
    data = b""
    while True:
        piece, first, total = read_piece(ser, name, len(data))
        if first != len(data):
            # A firmware before the offset ignores it and answers the end again: joining those would never finish.
            sys.exit(f"The device answered from byte {first} instead of {len(data)}: its firmware cannot read from an offset.")
        data += piece
        if not piece or len(data) >= total:
            return data, total


def do_read(ser, name, whole=False):
    """Prints the file byte for byte: by default what one answer holds (of a long log its end), with `whole` all of it."""
    if whole:
        data, total = read_whole(ser, name)
        sys.stdout.write(data.decode("utf-8", "replace"))
        if total == 0:
            print("(the file is empty or does not exist)", file=sys.stderr)
        elif len(data) < total:
            print(f"(only {len(data)} of {total} bytes: the device gave no more)", file=sys.stderr)
        return
    data, first, total = read_piece(ser, name)
    length = len(data)
    sys.stdout.write(data.decode("utf-8", "replace"))
    if length == 0:
        print("(the file is empty or does not exist)", file=sys.stderr)
    elif first > 0:
        print(f"(the last {length} bytes of {total}: the file begins {first} bytes earlier)", file=sys.stderr)


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
    if answer.split()[-1] != str(len(data)):
        sys.exit(f"The device stored {answer.split()[-1]} bytes, not {len(data)}.")
    if restart:
        do_restart(ser)


def do_restart(ser):
    send(ser, "CONFIG RESTART")
    read_until(ser, "CONFIG OK restart")
    print("The device is restarting.")


def do_battery(ser):
    send(ser, "BATTERY")
    read_until(ser, "CONFIG BATTERY BEGIN")
    _, lines = read_until(ser, "CONFIG BATTERY END")
    for line in lines:
        print(line)


def do_sms(ser, number, text):
    if "\n" in text or "\r" in text:
        sys.exit("A line break in the text is written \\n: the console takes one line.")
    send(ser, f"SMS {number} {text}")
    answer, _ = read_until(ser, "CONFIG OK", timeout=10)
    print(answer)
    if answer.endswith("refused"):
        sys.exit("The device refused the text: it is empty or too long, or the queue of waiting texts is full.")


def build_parser():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--port", help="serial port of the device (default: found automatically)")
    sub = p.add_subparsers(dest="cmd", required=True)
    sub.add_parser("list", help="the files in /config with their sizes")
    r = sub.add_parser("read", help="print a file (of a long log the end)")
    r.add_argument("file", choices=READABLE)
    r.add_argument("--all", "--whole", dest="whole", action="store_true", help="the whole file, read in pieces")
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
            do_read(ser, args.file, args.whole)
        elif args.cmd == "write":
            do_write(ser, args.file, args.source, args.restart)
        elif args.cmd == "battery":
            do_battery(ser)
        elif args.cmd == "sms":
            do_sms(ser, args.number, args.text)
        elif args.cmd == "restart":
            do_restart(ser)
    finally:
        ser.close()


if __name__ == "__main__":
    main()
