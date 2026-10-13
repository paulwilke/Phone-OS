#pragma once

// The scripted modem for the driver tests: everything a healthy A7682E answers during start-up, with
// the one thing a test varies set on the struct before apply(). A test then reads like the sequence
// it checks. The answers are built after 3GPP TS 27.007 and what the previous driver met on the
// device; real captures belong here as further scripts.

#include <string>
#include <vector>

#include <drivers/A7682EDriver.h>
#include <drivers/Ucs2.h>
#include <fakes/FakeAtTransport.h>

namespace modemtest {

const char* const kApn = "internet.example";
const char* const kPin = "0000";

struct Script {
  // What AT+CGDCONT? lists. The default is a modem that has context 1 right and the IMS bearer on 8.
  std::vector<std::string> contexts = {"+CGDCONT: 1,\"IPV4V6\",\"internet.example\",\"0.0.0.0\",0,0",
                                       "+CGDCONT: 8,\"IPV4V6\",\"ims\",\"0.0.0.0\",0,0"};
  std::string cgdcontWriteFinal = "OK";
  std::string cfun0Final = "OK";
  std::string cfun1Final = "OK";
  // AT+CPIN? answers in turn (the last stays): a status line, or an error as the final line.
  struct Cpin { std::string line; std::string final; };
  std::vector<Cpin> cpin = {{"+CPIN: READY", "OK"}};
  std::string pin = kPin;              // the PIN the card accepts
  std::string pinFinal = "OK";         // what the modem answers to it
  std::vector<std::string> creg = {"+CREG: 0,1"};   // AT+CREG? answers in turn (the last stays)
  std::string cops = "+COPS: 0,0,\"Example Mobile\",7";
  std::string cclk = "+CCLK: \"26/09/15,14:32:10+08\"";
  std::string csq = "+CSQ: 23,99";
  std::vector<std::string> cireg = {"+CIREG: 1,1,15"};  // answers in turn (the last stays)
  std::string ciregFinal = "OK";
  int networkMode = 2;
  // Calls. The number the test dials; the answers of AT+CLCC in turn ("" is an empty list; the last stays);
  // what ATD and ATA answer; the network's reason for AT+CEER ("" = none).
  std::string number = "+491701111111";
  std::vector<std::string> clcc = {""};
  std::string atdFinal = "OK";
  std::string ataFinal = "OK";
  std::string ceer = "";
  // Messages. What AT+CMGL="ALL" lists (header and text lines); every AT+CMGS=<length> is answered with the
  // prompt and its PDU with the line in `smsFinal` (an error line is a refused sending).
  std::vector<std::string> inbox;
  std::string smsFinal = "OK";
  std::string contextsFinal = "OK";   // the final line of AT+CGDCONT?
  bool cpinSilent = false;            // the card is not asked about at all: no answer
  std::string silent;                 // a command that is never answered (the modem hangs on it)

  void apply(FakeAtTransport& t) const {
    // Every answer goes through here, so that the one command in `silent` gets none.
    const auto reply = [&](const std::string& command, std::initializer_list<std::string> lines, const std::string& final = "OK") {
      if (command == silent) t.silence(command);
      else t.reply(command, lines, final);
    };
    for (const char* c : {"AT", "ATE0", "AT+CMEE=2", "AT+CMGF=1", "AT+CSCS=\"UCS2\"", "AT+CNMI=2,1,0,0,0",
                          "AT+CTZU=1", "AT+CLIP=1", "AT+CSDVC=3", "AT+COUTGAIN=4", "AT+COPS=3,0"}) {
      reply(c, {});
    }
    reply("AT+CNMP=" + std::to_string(networkMode), {});
    if (cpinSilent) t.silence("AT+CPIN?");
    else
      for (const Cpin& c : cpin) {
        if (c.line.empty()) reply("AT+CPIN?", {}, c.final);
        else reply("AT+CPIN?", {c.line}, c.final);
      }
    reply("AT+CPIN=\"" + pin + "\"", {}, pinFinal);
    // The answer of the context list is delivered as its own lines.
    if (contexts.empty()) reply("AT+CGDCONT?", {}, contextsFinal);
    else if (contexts.size() == 1) reply("AT+CGDCONT?", {contexts[0]}, contextsFinal);
    else reply("AT+CGDCONT?", {contexts[0], contexts[1]}, contextsFinal);
    reply(std::string("AT+CGDCONT=1,\"IPV4V6\",\"") + kApn + "\"", {}, cgdcontWriteFinal);
    reply("AT+CFUN=0", {}, cfun0Final);
    reply("AT+CFUN=1", {}, cfun1Final);
    for (const std::string& c : creg) reply("AT+CREG?", {c});
    reply("AT+COPS?", {cops});
    reply("AT+CCLK?", {cclk});
    reply("AT+CSQ", {csq});
    for (const std::string& c : cireg) reply("AT+CIREG?", {c}, ciregFinal);
    reply("ATD" + number + ";", {}, atdFinal);
    reply("ATA", {}, ataFinal);
    reply("AT+CHUP", {});
    for (const std::string& c : clcc) {
      if (c.empty()) reply("AT+CLCC", {});
      else reply("AT+CLCC", {c});
    }
    if (ceer.empty()) reply("AT+CEER", {});
    else reply("AT+CEER", {"+CEER: " + ceer});
    for (int level = 0; level <= 7; ++level) reply("AT+COUTGAIN=" + std::to_string(level), {});
    t.replyLines("AT+CMGL=\"ALL\"", inbox);
    for (int slot = 0; slot < 10; ++slot) reply("AT+CMGD=" + std::to_string(slot), {});
    reply("AT+CSCLK=1", {});
    reply("AT+CPOF", {});
    reply("AT+CMGF=0", {});
    t.setDefaultBodyAnswer({"+CMGS: 12"}, smsFinal);
    t.replyPromptToPrefix("AT+CMGS=");
  }
};

// The driver and its modem, and a clock that runs in steps of 50 ms while the driver polls.
struct Rig {
  FakeAtTransport t;
  A7682EDriver::Config config;
  A7682EDriver driver;
  std::vector<std::string> logs;

  explicit Rig(const A7682EDriver::Config& c) : config(c), driver(t, c) {}
  Rig() : Rig(defaultConfig()) {}

  static A7682EDriver::Config defaultConfig() {
    A7682EDriver::Config c;
    c.apn = kApn;
    c.simPin = kPin;
    return c;
  }

  void drain() {
    std::string line;
    while (driver.pollLog(line)) logs.push_back(line);
  }
  void run(uint32_t ms) {
    for (uint32_t spent = 0; spent < ms; spent += 50) {
      t.advance(50);
      driver.poll();
      drain();
    }
  }
  // Runs until the state is reached or the time is up; true when it was reached.
  bool runTo(ModemState state, uint32_t maxMs = 400000) {
    for (uint32_t spent = 0; spent < maxMs && driver.state() != state; spent += 50) {
      t.advance(50);
      driver.poll();
      drain();
    }
    return driver.state() == state;
  }
  // Starts the driver and runs it to Ready.
  bool start(uint32_t maxMs = 400000) {
    driver.begin();
    return runTo(ModemState::Ready, maxMs);
  }

  int count(const std::string& command) const {
    int n = 0;
    for (const std::string& c : t.sent())
      if (c == command) ++n;
    return n;
  }
  int countPrefix(const std::string& prefix) const {
    int n = 0;
    for (const std::string& c : t.sent())
      if (c.compare(0, prefix.size(), prefix) == 0) ++n;
    return n;
  }
  // The position of the first such command, -1 when it was not sent.
  int indexOf(const std::string& command) const {
    for (size_t i = 0; i < t.sent().size(); ++i)
      if (t.sent()[i] == command) return (int)i;
    return -1;
  }
  // A message announced by +CMTI and read with AT+CMGR: the modem has it in slot `slot`.
  void injectSms(int slot, const std::string& phone, const std::string& body, const std::string& scts = "26/09/15,14:32:10+08") {
    char hexPhone[256], hexBody[2048];
    text::utf8ToUcs2Hex(phone.c_str(), hexPhone, sizeof(hexPhone));
    text::utf8ToUcs2Hex(body.c_str(), hexBody, sizeof(hexBody));
    t.reply("AT+CMGR=" + std::to_string(slot),
            {std::string("+CMGR: \"REC UNREAD\",\"") + hexPhone + "\",\"\",\"" + scts + "\"", hexBody});
    urc("+CMTI: \"SM\"," + std::to_string(slot));
  }
  // Feeds a line of the modem (a URC) now and lets the driver read it.
  void urc(const std::string& line, uint32_t thenRunMs = 100) {
    t.unsolicited(t.now(), line);
    run(thenRunMs);
  }
  std::vector<CallEvent> events() {
    std::vector<CallEvent> out;
    CallEvent e;
    while (driver.pollCallEvent(e)) out.push_back(e);
    return out;
  }
  bool logged(const std::string& part) const {
    for (const std::string& l : logs)
      if (l.find(part) != std::string::npos) return true;
    return false;
  }
  int loggedCount(const std::string& part) const {
    int n = 0;
    for (const std::string& l : logs)
      if (l.find(part) != std::string::npos) ++n;
    return n;
  }
};

}  // namespace modemtest
