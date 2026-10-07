#!/usr/bin/env python3
"""Open the device's parents' page in a browser over the USB cable: no Wi-Fi needed.

  python3 scripts/webbridge.py            # then open http://localhost:8081
  python3 scripts/webbridge.py --port-http 9000 --serial /dev/cu.usbmodem1101

A small web server on this computer (127.0.0.1 only) forwards every browser request through the USB console
(`CONFIG WEB`) to the device. The device runs the same checks as over Wi-Fi: the page, the password
(web_password in secrets.toml), the lock-out after wrong passwords and the X-Phoneplus header. What the page
shows is the device's real configuration. The cable replaces the transport and nothing else.

The serial port is exclusive: while the bridge runs, deviceconfig.py cannot talk to the device. Stop with
Ctrl-C. Opening the serial port resets the ESP32, so the bridge waits a moment for it.

Needs pyserial, which PlatformIO brings along:

  ~/.platformio/penv/bin/python scripts/webbridge.py
"""

import argparse
import glob
import socketserver
import sys
import threading
import time

MAX_REQUEST = 9300  # the head and the largest body the device's parser accepts
FORWARD = ("authorization", "x-phoneplus", "content-type")


def find_port():
    for pattern in ("/dev/cu.usbmodem*", "/dev/ttyACM*", "/dev/ttyUSB*"):
        hits = sorted(glob.glob(pattern))
        if hits:
            return hits[0]
    sys.exit("No device found. Pass --serial.")


def open_port(path):
    import serial

    ser = serial.Serial()
    ser.port = path
    ser.baudrate = 115200
    ser.timeout = 1
    ser.dtr = True  # the console of the ESP32-S3 talks only when the host says it is connected
    ser.rts = False
    ser.open()
    return ser


def build_request(method, target, headers, body):
    """The raw HTTP request the device gets: the request line, the headers that matter, the body. None when it
    would not fit the device's limit."""
    out = [f"{method} {target} HTTP/1.1"]
    for name in FORWARD:
        if name in headers:
            out.append(f"{name}: {headers[name]}")
    if method in ("POST", "PUT") or body:
        out.append(f"content-length: {len(body)}")
    raw = ("\r\n".join(out) + "\r\n\r\n").encode("latin-1") + body
    return raw if len(raw) <= MAX_REQUEST else None


class Bridge:
    def __init__(self, ser):
        self.ser = ser
        self.lock = threading.Lock()  # one request on the cable at a time

    def _read_until(self, prefix, timeout):
        end = time.time() + timeout
        while time.time() < end:
            line = self.ser.readline().decode("utf-8", "replace").rstrip("\r\n")
            if line.startswith("CONFIG ERROR"):
                raise RuntimeError(line)
            if line.startswith(prefix):
                return line
        raise RuntimeError(f"no answer to '{prefix}' (is the firmware new enough for CONFIG WEB?)")

    def forward(self, raw):
        with self.lock:
            self.ser.reset_input_buffer()
            self.ser.write(f"CONFIG WEB {len(raw)}\n".encode())
            self.ser.flush()
            self._read_until("CONFIG READY", 5)
            # Small pieces with a short pause: the device's receive buffer is finite, and an overrun looks
            # like a truncated request.
            for i in range(0, len(raw), 128):
                self.ser.write(raw[i:i + 128])
                self.ser.flush()
                time.sleep(0.004)
            line = self._read_until("CONFIG WEB ", 20)
            n = int(line.split()[2])
            data = bytearray()
            end = time.time() + 30
            while len(data) < n and time.time() < end:
                data += self.ser.read(n - len(data))
            if len(data) < n:
                raise RuntimeError(f"response cut short ({len(data)} of {n} bytes)")
            return bytes(data)


class Handler(socketserver.StreamRequestHandler):
    bridge = None

    def handle(self):
        try:
            request_line = self.rfile.readline(2048).decode("latin-1")
            if not request_line.strip():
                return
            method, target, _ = request_line.split(" ", 2)
            headers = {}
            while True:
                line = self.rfile.readline(4096).decode("latin-1")
                if line in ("\r\n", "\n", ""):
                    break
                name, _, value = line.partition(":")
                headers[name.strip().lower()] = value.strip()
            length = int(headers.get("content-length", "0") or 0)
            body = self.rfile.read(length) if length else b""
            raw = build_request(method, target, headers, body)
            if raw is None:
                self.wfile.write(b"HTTP/1.1 413 Payload Too Large\r\nContent-Length: 0\r\nConnection: close\r\n\r\n")
                return
            self.wfile.write(self.bridge.forward(raw))
        except Exception as e:  # the browser gets a readable 502, the terminal the reason
            message = str(e).encode()
            print("bridge:", e, file=sys.stderr)
            self.wfile.write(b"HTTP/1.1 502 Bad Gateway\r\nContent-Type: text/plain\r\nContent-Length: " +
                             str(len(message)).encode() + b"\r\nConnection: close\r\n\r\n" + message)


class Server(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--serial", help="serial port of the device (default: found automatically)")
    ap.add_argument("--port-http", type=int, default=8081, help="local port (default 8081)")
    args = ap.parse_args()
    ser = open_port(args.serial or find_port())
    time.sleep(3.0)  # the device restarts when the port is opened
    Handler.bridge = Bridge(ser)
    with Server(("127.0.0.1", args.port_http), Handler) as srv:
        print(f"The device's page: http://localhost:{args.port_http}  (Ctrl-C to stop)")
        try:
            srv.serve_forever()
        except KeyboardInterrupt:
            print("\nstopped")


if __name__ == "__main__":
    main()
