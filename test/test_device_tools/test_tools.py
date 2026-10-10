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
    """The device's side: it answers each command line with scripted lines. The answers are bytes on one stream,
    read by line or by count like the real port, so that blank lines and exact lengths are tested too."""

    def __init__(self, answers):
        self.answers = answers  # command prefix -> a list of lines, or one list per attempt (the last is repeated)
        self.attempts = {}
        self.written = b""
        self.pending = b""
        self.closed = False

    def reset_input_buffer(self):
        self.pending = b""

    def write(self, data):
        self.written += data
        for line in data.decode("latin-1").split("\n"):
            for prefix, lines in self.answers.items():
                if line.startswith(prefix):
                    if lines and isinstance(lines[0], list):
                        n = self.attempts.get(prefix, 0)
                        self.attempts[prefix] = n + 1
                        lines = lines[min(n, len(lines) - 1)]
                    self.pending += "".join(l + "\n" for l in lines).encode("latin-1")

    def flush(self):
        pass

    def readline(self):
        end = self.pending.find(b"\n")
        if end < 0:
            line, self.pending = self.pending, b""
            return line
        line, self.pending = self.pending[:end + 1], self.pending[end + 1:]
        return line

    def read(self, count):
        data, self.pending = self.pending[:count], self.pending[count:]
        return data

    def close(self):
        self.closed = True


def run(argv, answers):
    ser = FakeSerial(answers)
    deviceconfig.main(argv, opener=lambda port: ser)
    return ser


def capture(function):
    import io
    from contextlib import redirect_stderr, redirect_stdout

    buffer = io.StringIO()
    with redirect_stdout(buffer), redirect_stderr(io.StringIO()):
        function()
    return buffer.getvalue()


def capture_both(function):
    """(stdout, stderr)"""
    import io
    from contextlib import redirect_stderr, redirect_stdout

    out, err = io.StringIO(), io.StringIO()
    with redirect_stdout(out), redirect_stderr(err):
        function()
    return out.getvalue(), err.getvalue()


failures = []


def check(name, ok, detail=""):
    print(("ok   " if ok else "FAIL ") + name)
    if not ok:
        failures.append(name)
        print(detail)


check("the checksum is the 16-bit sum", deviceconfig.checksum(b"abc") == 294 and deviceconfig.checksum(b"\xff" * 300) == (255 * 300) & 0xFFFF)

out = capture(lambda: run(["--port", "x", "list"], {"CONFIG LIST": ["[log] noise", "CONFIG LIST BEGIN", "settings.toml 5", "CONFIG LIST END"]}))
check("list prints the files and skips log lines", out == "settings.toml 5\n", out)

# The content as the device sends it: the header, the bytes, a line break, "CONFIG END". Blank lines are content.
TEXT = b"[child]\nname = \"Alex\"\n\n# the school\n\n[wifi]\n"
HEAD = f"CONFIG CONTENT settings.toml {len(TEXT)} {deviceconfig.checksum(TEXT)} from 0 of {len(TEXT)}"
CONTENT = TEXT.decode().split("\n")[:-1] + [""]  # the lines whose joining gives TEXT, then the break before END

out = capture(lambda: run(["--port", "x", "read", "settings.toml"], {"CONFIG READ settings.toml": [HEAD] + CONTENT + ["CONFIG END"]}))
check("read prints the file byte for byte, blank lines included", out == TEXT.decode(), repr(out))

out, err = capture_both(lambda: run(["--port", "x", "read", "settings.toml"],
                                    {"CONFIG READ settings.toml": ["[log] noise", HEAD] + CONTENT + ["CONFIG END"]}))
check("read skips the device's own log lines before the answer", out == TEXT.decode(), repr(out))

DISTURBED = [HEAD, "[child]", "[log] a line of the device in the middle", 'name = "Alex"', "", "# the school", "", "[wifi]", "CONFIG END"]
out, err = capture_both(lambda: run(["--port", "x", "read", "settings.toml"],
                                    {"CONFIG READ settings.toml": [DISTURBED, [HEAD] + CONTENT + ["CONFIG END"]]}))
check("read repeats a read that a log line disturbed and prints only the good one",
      out == TEXT.decode() and "disturbed" in err, repr((out, err)))

try:
    run(["--port", "x", "read", "settings.toml"], {"CONFIG READ settings.toml": [DISTURBED]})
    check("read gives up after three disturbed tries", False)
except SystemExit as e:
    check("read gives up after three disturbed tries", "three tries" in str(e), str(e))

SYSTEM_LOG = "system" + ".log"  # built: a quoted word with a dot looks like a catalog key
TAIL = b"line b\nline c\n"
out, err = capture_both(lambda: run(["--port", "x", "read", SYSTEM_LOG],
                                    {f"CONFIG READ {SYSTEM_LOG}": [f"CONFIG CONTENT {SYSTEM_LOG} {len(TAIL)} {deviceconfig.checksum(TAIL)} from 100 of 114",
                                                                "line b", "line c", "", "CONFIG END"]}))
check("read of the end of a long log says where it begins", out == TAIL.decode() and "begins 100 bytes earlier" in err, repr((out, err)))

ENERGY_LOG = "energy" + ".log"
LINE = b"1791489827 600 63 3754 -42 939 modem-sleep\n"
out, err = capture_both(lambda: run(["--port", "x", "read", ENERGY_LOG],
                                    {f"CONFIG READ {ENERGY_LOG}": [f"CONFIG CONTENT {ENERGY_LOG} {len(LINE)} {deviceconfig.checksum(LINE)} from 0 of {len(LINE)}",
                                                                 LINE.decode().rstrip("\n"), "", "CONFIG END"]}))
check("the energy log can be read over the cable", out == LINE.decode(), repr((out, err)))

out, err = capture_both(lambda: run(["--port", "x", "read", "contacts.toml"],
                                    {"CONFIG READ contacts.toml": ["CONFIG CONTENT contacts.toml 0 0 from 0 of 0", "", "CONFIG END"]}))
check("read of a file that is empty or missing says so", out == "" and "empty or does not exist" in err, repr((out, err)))

with tempfile.NamedTemporaryFile(delete=False) as f:
    f.write(b"language = \"en\"\n")
    source = f.name
ser = None


def do_write():
    global ser
    ser = run(["--port", "x", "write", "secrets.toml", source, "--restart"],
              {"CONFIG WRITE": ["CONFIG READY 16"], "language": ["CONFIG OK secrets.toml 16"], "CONFIG RESTART": ["CONFIG OK restart"]})


out = capture(do_write)
os.unlink(source)
header = f"CONFIG WRITE secrets.toml 16 {deviceconfig.checksum(b'language = \"en\"' + b'\n')}\n".encode()
check("write sends the announced length and checksum, then the bytes, then the restart",
      ser.written.startswith(header) and b"language = \"en\"\n" in ser.written and ser.written.endswith(b"CONFIG RESTART\n"), ser.written)
check("write says the device restarts", "CONFIG OK secrets.toml 16" in out and "restarting" in out, out)

try:
    run(["--port", "x", "restart"], {"CONFIG RESTART": []})
    check("restart waits for the device's answer", False)
except SystemExit as e:
    check("restart waits for the device's answer", "No answer" in str(e), str(e))
out = capture(lambda: run(["--port", "x", "restart"], {"CONFIG RESTART": ["CONFIG OK restart"]}))
check("restart says the device restarts once it said so", out == "The device is restarting.\n", out)

with tempfile.NamedTemporaryFile(delete=False) as g:
    g.write(b"x = 1\n")
    short = g.name
try:
    run(["--port", "x", "write", "settings.toml", short], {"CONFIG WRITE": ["CONFIG READY 6"], "x = 1": ["CONFIG OK settings.toml 5"]})
    check("write notices that the device stored another length", False)
except SystemExit as e:
    check("write notices that the device stored another length", "stored 5 bytes, not 6" in str(e), str(e))
os.unlink(short)

out = capture(lambda: run(["--port", "x", "sms", "+491701111111", "line one\\nline two"],
                          {"SMS": ["[log] noise", "CONFIG OK sms +491701111111 17 chars accepted"]}))
check("sms prints the device's answer", out == "CONFIG OK sms +491701111111 17 chars accepted\n", out)
try:
    run(["--port", "x", "sms", "+491701111111", "test"], {"SMS": ["CONFIG OK sms +491701111111 4 chars refused"]})
    check("a text the device refused is an error of the tool", False)
except SystemExit as e:
    check("a text the device refused is an error of the tool", "refused" in str(e), str(e))
try:
    run(["--port", "x", "sms", "+491701111111", "one\ntwo"], {})
    check("a real line break in a text is refused before it reaches the console", False)
except SystemExit as e:
    check("a real line break in a text is refused before it reaches the console", "\\n" in str(e), str(e))

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

raw = webbridge.build_request("PUT", "/api/settings", {"authorization": "Basic x", "x-phone-os": "1", "cookie": "no"}, b"{}")
check("the bridge forwards only the headers that matter and sets the length",
      raw == b"PUT /api/settings HTTP/1.1\r\nauthorization: Basic x\r\nx-phone-os: 1\r\ncontent-length: 2\r\n\r\n{}", raw)
check("a request that does not fit the device is refused", webbridge.build_request("PUT", "/x", {}, b"x" * 9300) is None)
check("the largest request that fits is forwarded", webbridge.build_request("GET", "/", {}, b"") is not None)

raw = webbridge.build_request("GET", "/api/status", {"authorization": "Basic x"}, b"")
ser = FakeSerial({"CONFIG WEB": ["CONFIG READY %d" % len(raw)], "GET /api/status": ["[log] web: noise", "CONFIG WEB 17", "HTTP/1.1 200 OK\r\n"]})
answer = webbridge.Bridge(ser).forward(raw)
check("the bridge announces the request, sends it whole and returns the response of the announced length",
      ser.written.startswith(b"CONFIG WEB %d\n" % len(raw)) and ser.written.endswith(raw) and answer == b"HTTP/1.1 200 OK\r\n", repr((ser.written, answer)))
try:
    webbridge.Bridge(FakeSerial({"CONFIG WEB": ["CONFIG ERROR no web service"]})).forward(raw)
    check("the bridge reports an error line of the device", False)
except RuntimeError as e:
    check("the bridge reports an error line of the device", "no web service" in str(e), str(e))

sys.exit(1 if failures else 0)
