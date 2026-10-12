#pragma once

// The rig that runs the Modem contract against the A7682E driver on a scripted modem. The contract talks to a
// Modem that settles at once; the driver is a polled state machine on a byte line, so the rig stands between
// them: it lets the driver run (in steps of 50 ms of scripted time) after what the contract does, and it turns
// the contract's hooks ("the network appears", "the other side answers") into what a modem would say.
//
// Not everything of the world can be set while the modem runs: a card that changes makes the modem restart,
// as a real one does, and a modem cannot be told to lack volume control (setVolumeSupported is refused, and the
// test that needs it is ignored).

#include <memory>
#include <string>

#include <drivers/A7682EDriver.h>
#include <drivers/Ucs2.h>
#include <fakes/FakeAtTransport.h>

class DriverModemRig {
public:
  DriverModemRig() {
    config_.apn = "internet.example";
    // No PIN is stored: a card that wants one stays PinRequired, as the contract describes it.
    config_.registrationTimeoutMs = 3600000;   // the contract's "no network" tests wait longer than 90 seconds
    driver_.reset(new A7682EDriver(t_, config_));
    adapter_.reset(new Adapter(*this));
    scriptStatic();
    scriptWorld();
  }

  Modem& modem() { return *adapter_; }
  void advanceSeconds(uint32_t s) { pump(s * 1000); }

  bool setNetwork(const std::string& op, int bars, VoiceStatus voice) {
    operator_ = op;
    bars_ = bars;
    voice_ = voice;
    scriptWorld();
    if (driver_->state() == ModemState::Registering || driver_->state() == ModemState::Ready) pump(35000);
    return true;
  }
  bool setSim(SimStatus s) {
    sim_ = s;
    scriptWorld();
    // A card that changes makes the modem restart; the driver starts again and meets the new card.
    if (driver_->state() != ModemState::Off) {
      t_.restart();
      pump(200);                      // the driver sees the restart before it is asked whether it has settled
      pumpUntilSettled(60000);
    }
    return true;
  }
  bool injectSms(const std::string& phone, const std::string& body, uint32_t timestamp) {
    const int slot = ++slot_ % 10;
    char hexPhone[256], hexBody[2048];
    text::utf8ToUcs2Hex(phone.c_str(), hexPhone, sizeof(hexPhone));
    text::utf8ToUcs2Hex(body.c_str(), hexBody, sizeof(hexBody));
    t_.clearReplies("AT+CMGR=" + std::to_string(slot));
    t_.reply("AT+CMGR=" + std::to_string(slot),
             {std::string("+CMGR: \"REC UNREAD\",\"") + hexPhone + "\",\"\",\"" + scts(timestamp) + "\"", hexBody});
    urc("+CMTI: \"SM\"," + std::to_string(slot));
    pump(3000);
    return true;
  }
  bool injectIncomingCall(const std::string& phone) {
    if (driver_->state() != ModemState::Ready || driver_->voiceStatus() != VoiceStatus::Ready) return false;
    char hex[256];
    text::utf8ToUcs2Hex(phone.c_str(), hex, sizeof(hex));
    urc("RING");
    urc(std::string("+CLIP: \"") + hex + "\",145,,,,0");
    pump(200);
    return true;
  }
  bool remoteAnswer() {
    if (driver_->state() == ModemState::Dialing && waitForAtd()) {
      urc("VOICE CALL: BEGIN");
      pump(200);
    }
    return true;
  }
  bool remoteBusy() {
    if (driver_->state() == ModemState::Dialing && waitForAtd()) {
      urc("BUSY");
      pump(200);
    }
    return true;
  }
  bool remoteNoAnswer() {
    if (driver_->state() == ModemState::Dialing && waitForAtd()) {
      urc("NO ANSWER");
      pump(200);
    }
    return true;
  }
  bool failDial(const std::string& reason, DialFailReason why) {
    if (driver_->state() != ModemState::Dialing) return true;
    switch (why) {
      case DialFailReason::Refused: {
        t_.clearReplies(atd());
        t_.reply(atd(), {}, reason.empty() ? "ERROR" : reason);    // the modem refuses ATD itself
        pump(4000);
        break;
      }
      case DialFailReason::NotReady:
        world_ims_ = false;                                        // the question right before ATD finds the IMS gone
        scriptWorld();
        pump(4000);
        world_ims_ = true;
        scriptWorld();
        break;
      case DialFailReason::ModemRestarted:
        t_.restart();
        pump(200);
        pumpUntilSettled(60000);
        break;
      default:                                                     // the network refuses the call after ATD
        if (waitForAtd()) {
          urc("NO CARRIER");
          pump(3000);
        }
        break;
    }
    return true;
  }
  bool restartDuringCall() {
    t_.restart();
    pump(200);
    pumpUntilSettled(60000);
    return true;
  }
  bool remoteHangup() {
    switch (driver_->state()) {
      case ModemState::InCall:
      case ModemState::RingingIn:
        urc("NO CARRIER");
        pump(3000);
        break;
      case ModemState::Dialing:
        if (waitForAtd()) {
          urc("NO ANSWER");
          pump(200);
        }
        break;
      default: break;
    }
    return true;
  }
  bool failNextSms(const std::string& reason) {
    t_.replyToBody({}, reason);
    return true;
  }
  bool setVolumeSupported(bool supported) { return supported; }   // a modem cannot be made to lack the command

private:
  // The Modem the contract sees: every call goes to the driver, and the driver then gets the time it needs.
  class Adapter : public Modem {
  public:
    explicit Adapter(DriverModemRig& rig) : rig_(rig) {}
    void begin() override {
      rig_.driver_->begin();
      rig_.pumpUntilSettled(40000);
    }
    void shutdown() override {
      rig_.driver_->shutdown();
      rig_.pump(10000);
    }
    void sleep() override {
      rig_.driver_->sleep();
      rig_.pump(2000);
    }
    void wake() override {
      rig_.driver_->wake();
      rig_.pump(2000);
    }
    bool sendSms(const std::string& phone, const std::string& body) override {
      const bool ok = rig_.driver_->sendSms(phone, body);
      if (ok) rig_.pump(6000);
      return ok;
    }
    bool pollSms(SmsMessage& out) override { return rig_.driver_->pollSms(out); }
    bool pollSendFailure(SmsFailure& out) override { return rig_.driver_->pollSendFailure(out); }
    bool dial(const std::string& phone) override {
      const bool ok = rig_.driver_->dial(phone);
      if (ok) rig_.dialled_ = phone;
      return ok;
    }
    bool answer() override {
      const bool ok = rig_.driver_->answer();
      if (ok) rig_.pump(1000);
      return ok;
    }
    bool hangup() override {
      const bool ok = rig_.driver_->hangup();
      if (ok) rig_.pump(1000);
      return ok;
    }
    bool pollCallEvent(CallEvent& out) override { return rig_.driver_->pollCallEvent(out); }
    bool setCallVolume(int level) override {
      const bool ok = rig_.driver_->setCallVolume(level);
      rig_.pump(1000);
      return ok;
    }
    int callVolume() const override { return rig_.driver_->callVolume(); }
    bool query(const std::string& command) override {
      const bool ok = rig_.driver_->query(command);
      if (ok) rig_.pump(2000);
      return ok;
    }
    bool pollQueryResult(std::string& out) override { return rig_.driver_->pollQueryResult(out); }
    ModemState state() const override { return rig_.driver_->state(); }
    int signalBars() const override { return rig_.driver_->signalBars(); }
    VoiceStatus voiceStatus() const override { return rig_.driver_->voiceStatus(); }
    std::string operatorName() const override { return rig_.driver_->operatorName(); }
    SimStatus simStatus() const override { return rig_.driver_->simStatus(); }

  private:
    DriverModemRig& rig_;
  };

  std::string atd() const { return "ATD" + dialled_ + ";"; }
  void urc(const std::string& line) { t_.unsolicited(t_.now(), line); }

  void pump(uint32_t ms) {
    for (uint32_t spent = 0; spent < ms; spent += 50) {
      t_.advance(50);
      driver_->poll();
    }
  }
  // Runs until the modem is Ready, in Error, or the time is up: what a start settles to.
  void pumpUntilSettled(uint32_t maxMs) {
    for (uint32_t spent = 0; spent < maxMs; spent += 50) {
      t_.advance(50);
      driver_->poll();
      if (driver_->state() == ModemState::Ready || driver_->state() == ModemState::Error) return;
    }
  }
  // Lets the driver get as far as sending ATD, which is when the other side can do something.
  bool waitForAtd() {
    for (uint32_t spent = 0; spent < 4000; spent += 50) {
      for (const std::string& c : t_.sent())
        if (c == atd()) return true;
      t_.advance(50);
      driver_->poll();
    }
    return false;
  }

  static std::string scts(uint32_t epoch) {
    // Unix time to "yy/MM/dd,hh:mm:ss+00" (the civil date from the day number, after Howard Hinnant).
    const int64_t days = epoch / 86400;
    const uint32_t rest = epoch % 86400;
    const int64_t z = days + 719468;
    const int64_t era = z / 146097;
    const int64_t doe = z - era * 146097;
    const int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const int64_t mp = (5 * doy + 2) / 153;
    const int d = (int)(doy - (153 * mp + 2) / 5 + 1);
    const int m = (int)(mp < 10 ? mp + 3 : mp - 9);
    const int y = (int)(yoe + era * 400 + (m <= 2));
    char buf[40];
    snprintf(buf, sizeof(buf), "%02d/%02d/%02d,%02d:%02d:%02d+00", y % 100, m, d, (int)(rest / 3600), (int)(rest / 60 % 60), (int)(rest % 60));
    return buf;
  }

  // What does not change while a test runs: the initialisation, messages, calls, the questions of the diagnosis.
  void scriptStatic() {
    for (const char* c : {"AT", "ATE0", "AT+CMEE=2", "AT+CMGF=1", "AT+CSCS=\"UCS2\"", "AT+CNMI=2,1,0,0,0", "AT+CTZU=1",
                          "AT+CLIP=1", "AT+CSDVC=3", "AT+COUTGAIN=4", "AT+COPS=3,0", "AT+CNMP=2", "AT+CMGF=0",
                          "AT+CCLK?", "AT+CHUP", "ATA", "AT+CLCC", "AT+CEER", "AT+CSCLK=1", "AT+CPOF", "AT+CFUN=0", "AT+CFUN=1"}) {
      t_.reply(c, {});
    }
    t_.reply("AT+CCLK?", {"+CCLK: \"26/09/15,14:32:10+08\""});
    t_.reply("AT+CGDCONT?", {"+CGDCONT: 1,\"IPV4V6\",\"internet.example\"", "+CGDCONT: 8,\"IPV4V6\",\"ims\""});
    t_.reply("AT+CSCLK?", {"+CSCLK: 1"});
    for (int level = 0; level <= 7; ++level) t_.reply("AT+COUTGAIN=" + std::to_string(level), {});
    t_.reply("AT+CMGL=\"ALL\"", {});
    for (int slot = 0; slot < 10; ++slot) t_.reply("AT+CMGD=" + std::to_string(slot), {});
    t_.setDefaultBodyAnswer({"+CMGS: 12"});
    t_.replyPromptToPrefix("AT+CMGS=");
    for (const char* n : {"+491701111111", "+491702222222"}) t_.reply(std::string("ATD") + n + ";", {});
  }

  // What the contract changes: the network, the card.
  void scriptWorld() {
    for (const char* c : {"AT+CPIN?", "AT+CREG?", "AT+COPS?", "AT+CSQ", "AT+CIREG?"}) t_.clearReplies(c);
    switch (sim_) {
      case SimStatus::Ready:
      case SimStatus::Unknown: t_.reply("AT+CPIN?", {"+CPIN: READY"}); break;
      case SimStatus::PinRequired:
      case SimStatus::PinWrong: t_.reply("AT+CPIN?", {"+CPIN: SIM PIN"}); break;
      case SimStatus::PukRequired: t_.reply("AT+CPIN?", {"+CPIN: SIM PUK"}); break;
      case SimStatus::Missing: t_.reply("AT+CPIN?", {}, "+CME ERROR: SIM not inserted"); break;
    }
    const bool network = !operator_.empty();
    t_.reply("AT+CREG?", {network ? (voice_ == VoiceStatus::SmsOnly ? "+CREG: 0,7" : "+CREG: 0,1") : "+CREG: 0,2"});
    t_.reply("AT+COPS?", {"+COPS: 0,0,\"" + operator_ + "\",7"});
    static const int kRssi[] = {0, 3, 8, 13, 18, 23};
    t_.reply("AT+CSQ", {"+CSQ: " + std::to_string(kRssi[bars_ < 0 ? 0 : bars_ > 5 ? 5 : bars_]) + ",99"});
    const bool ims = world_ims_ && voice_ != VoiceStatus::ImsMissing;
    t_.reply("AT+CIREG?", {ims ? "+CIREG: 1,1,15" : "+CIREG: 1,0,0"});
  }

  FakeAtTransport t_;
  A7682EDriver::Config config_;
  std::unique_ptr<A7682EDriver> driver_;
  std::unique_ptr<Adapter> adapter_;
  std::string operator_ = "Provider";
  int bars_ = 3;
  VoiceStatus voice_ = VoiceStatus::Ready;
  SimStatus sim_ = SimStatus::Ready;
  bool world_ims_ = true;
  std::string dialled_;
  int slot_ = 0;
};
