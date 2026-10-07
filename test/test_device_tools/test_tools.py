#!/usr/bin/env python3
"""Tests of scripts/deviceconfig.py and scripts/webbridge.py against a scripted console: the framing of a write,
the checksum, the answers of the device, an error line, and the raw request the bridge builds.

    python3 test/test_device_tools/test_tools.py
"""
import importlib.util
import os
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def load(name):
    spec = importlib.util.spec_from_file_location(name, os.path.join(ROOT, "scripts", name + ".py"))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


deviceconfig = load("deviceconfig")
webbridge = load("webbridge")


class FakeSerial:
    """The device's side: it answers each command line with scripted lines."""

    def __init__(self, answers):
        self.answers = answers  # command prefix -> list of lines
        self.written = b""
        self.pending = []
        self.closed = False

    def reset_input_buffer(self):
        self.pending = []

    def write(self, data):
        self.written += data
        for line in data.decode("latin-1").split("\n"):
            for prefix, lines in self.answers.items():
                if line.startswith(prefix):
                    self.pending += [l + "\n" for l in lines]

    def flush(self):
        pass

    def readline(self):
        return self.pending.pop(0).encode() if self.pending else b""

    def close(self):
        self.closed = True


def run(argv, answers):
    ser = FakeSerial(answers)
    deviceconfig.main(argv, opener=lambda port: ser)
    return ser


def capture(function):
    import io
    from contextlib import redirect_stdout

    buffer = io.StringIO()
    with redirect_stdout(buffer):
        function()
    return buffer.getvalue()


failures = []


def check(name, ok, detail=""):
    print(("ok   " if ok else "FAIL ") + name)
    if not ok:
        failures.append(name)
        print(detail)


check("the checksum is the 16-bit sum", deviceconfig.checksum(b"abc") == 294 and deviceconfig.checksum(b"\xff" * 300) == (255 * 300) & 0xFFFF)

out = capture(lambda: run(["--port", "x", "list"], {"CONFIG LIST": ["[log] noise", "CONFIG LIST BEGIN", "settings.toml 5", "CONFIG LIST END"]}))
check("list prints the files and skips log lines", out == "settings.toml 5\n", out)

out = capture(lambda: run(["--port", "x", "read", "settings.toml"],
                          {"CONFIG READ settings.toml": ["CONFIG CONTENT settings.toml 3 294 from 0 of 3", "abc", "CONFIG END"]}))
check("read prints the content", out == "abc\n", out)

with tempfile.NamedTemporaryFile(delete=False) as f:
    f.write(b"language = \"en\"\n")
    source = f.name
ser = None


def do_write():
    global ser
    ser = run(["--port", "x", "write", "secrets.toml", source, "--restart"],
              {"CONFIG WRITE": ["CONFIG READY 16"], "language": ["CONFIG OK secrets.toml 16"]})


out = capture(do_write)
os.unlink(source)
header = f"CONFIG WRITE secrets.toml 16 {deviceconfig.checksum(b'language = \"en\"' + b'\n')}\n".encode()
check("write sends the announced length and checksum, then the bytes, then the restart",
      ser.written.startswith(header) and b"language = \"en\"\n" in ser.written and ser.written.endswith(b"CONFIG RESTART\n"), ser.written)
check("write says the device restarts", "CONFIG OK secrets.toml 16" in out and "restarting" in out, out)

out = capture(lambda: run(["--port", "x", "battery"], {"BATTERY": ["CONFIG BATTERY BEGIN", "gauge: voltage 4118 mV", "CONFIG BATTERY END"]}))
check("battery prints the lines between the markers", out == "gauge: voltage 4118 mV\n", out)

try:
    run(["--port", "x", "list"], {"CONFIG LIST": ["CONFIG ERROR no card"]})
    check("an error line of the device ends the tool", False)
except SystemExit as e:
    check("an error line of the device ends the tool", "no card" in str(e), str(e))

try:
    deviceconfig.build_parser().parse_args(["write", deviceconfig.LOGS[0], "x"])
    check("a log cannot be written over the cable", False)
except SystemExit:
    check("a log cannot be written over the cable", True)
try:
    deviceconfig.build_parser().parse_args(["read", "../secrets.toml"])
    check("no path can be read", False)
except SystemExit:
    check("no path can be read", True)

check("the tool has a help text", "settings.toml" in deviceconfig.build_parser().format_help())

raw = webbridge.build_request("PUT", "/api/settings", {"authorization": "Basic x", "x-phoneplus": "1", "cookie": "no"}, b"{}")
check("the bridge forwards only the headers that matter and sets the length",
      raw == b"PUT /api/settings HTTP/1.1\r\nauthorization: Basic x\r\nx-phoneplus: 1\r\ncontent-length: 2\r\n\r\n{}", raw)
check("a request that does not fit the device is refused", webbridge.build_request("PUT", "/x", {}, b"x" * 9300) is None)
check("the largest request that fits is forwarded", webbridge.build_request("GET", "/", {}, b"") is not None)

sys.exit(1 if failures else 0)
