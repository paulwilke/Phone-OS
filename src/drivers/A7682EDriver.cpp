#include "A7682EDriver.h"

#include <cctype>
#include <cstdio>
#include <cstring>

namespace {

// The initialisation, in a fixed order. ATE0 first, or every command comes back as an echo and
// confuses the line reading.
const char* const kInitCommands[] = {
    "ATE0",
    "AT+CMEE=2",           // errors as words; 1 would give bare numbers, 2 is also the default
    "AT+CMGF=1",           // SMS in text mode
    "AT+CSCS=\"UCS2\"",    // umlauts and emoji unharmed
    "AT+CSMP=17,167,0,8",  // outgoing SMS as UCS2
    "AT+CNMI=2,1,0,0,0",   // a new SMS is announced as +CMTI, not delivered inline
    "AT+CTZU=1",           // take the network time
    "AT+CLIP=1",           // the caller's number on an incoming call
    "AT+CSDVC=3",          // sound through the speaker (1 would be the earpiece)
    "AT+COUTGAIN=4",       // medium volume (0 … 7); the A76XX series has no AT+CLVL
    "AT+COPS=3,0",         // the operator's name as text
};
constexpr int kInitCommandCount = (int)(sizeof(kInitCommands) / sizeof(kInitCommands[0]));
// "AT", the list, and last the network mode: it depends on the SIM and the tariff, so it comes
// from the configuration and not from the fixed list.
constexpr int kInitSteps = kInitCommandCount + 2;
constexpr int kSimQueryAttempts = 12;  // about so many seconds the modem takes after power-on
constexpr size_t kMaxLogLines = 32;

bool isApnCharacter(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '_';
}

bool equalsIgnoreCase(const char* a, const char* b) {
  for (; *a && *b; ++a, ++b)
    if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return false;
  return *a == *b;
}

bool validPin(const std::string& pin) {
  if (pin.size() < 4 || pin.size() > 8) return false;
  for (char c : pin)
    if (c < '0' || c > '9') return false;
  return true;
}

}  // namespace

A7682EDriver::A7682EDriver(AtTransport& transport, const Config& config) : transport_(transport), config_(config) {
  if (!validPin(config_.simPin)) config_.simPin.clear();
}

bool A7682EDriver::apnUsable() const {
  if (config_.apn.empty() || config_.apn.size() > 100) return false;
  for (char c : config_.apn)
    if (!isApnCharacter(c)) return false;
  return true;
}

void A7682EDriver::log(const std::string& line) {
  if (log_.size() >= kMaxLogLines) log_.pop_front();
  log_.push_back(line);
}

bool A7682EDriver::pollLog(std::string& line) {
  if (log_.empty()) return false;
  line = log_.front();
  log_.pop_front();
  return true;
}

void A7682EDriver::setState(ModemState s) { state_ = s; }

void A7682EDriver::send(const char* command) {
  transport_.write(command, strlen(command));
  transport_.write("\r\n", 2);
}

void A7682EDriver::expect(uint32_t timeoutMs) { jobDeadline_.set(transport_.now(), timeoutMs); }

VoiceStatus A7682EDriver::voiceStatus() const {
  if (!registered_) return VoiceStatus::Unknown;
  if (smsOnly_) return VoiceStatus::SmsOnly;
  if (!imsKnown_) return VoiceStatus::Unknown;
  return imsRegistered_ ? VoiceStatus::Ready : VoiceStatus::ImsMissing;
}

int A7682EDriver::signalBars() const { return registered_ ? at::csqToBars(rssi_) : 0; }

bool A7682EDriver::networkTime(at::NetworkTime& out) const {
  if (!timeKnown_) return false;
  out = time_;
  return true;
}

// ---------------------------------------------------------------------------
// Start
// ---------------------------------------------------------------------------

// Ask first, press later. A pulse on the power key of a running modem switches it OFF, and after a
// reset of the processor (flashing, watchdog, reset key) the modem still runs, because its supply
// hangs on the expander and not on the processor. Until 2026-09-30 the previous code pulsed
// blindly: every second start put the modem to sleep, and nobody could call.
void A7682EDriver::begin() {
  if (state_ != ModemState::Off) return;
  transport_.power(true);
  setState(ModemState::PoweringOn);
  waitUntil_.set(transport_.now(), config_.probeDelayMs);
  reader_.reset();
  job_ = Job::None;
  jobDeadline_.clear();
  responded_ = false;
  atTries_ = 0;
  pwrkeyTried_ = false;
  restarts_ = 0;
  registered_ = smsOnly_ = false;
  imsKnown_ = false;
  operator_.clear();
  simStatus_ = SimStatus::Unknown;
  pinSentThisBoot_ = false;
  simTries_ = 0;
  simRetry_.clear();
  manualSelectDone_ = false;
  apnNoticeLogged_ = false;
  log("Modem: switched on");
}

// *ATREADY after the modem had answered means it restarted by itself: a bad SIM contact, a supply
// that dips under the current of a transmission. Whatever was going on is gone, so the course
// starts again; the data bearer is read first and written only when it differs.
void A7682EDriver::onRestart() {
  ++restarts_;
  log("Modem restarted by itself (" + std::to_string(restarts_) + " since start)");
  reader_.reset();
  job_ = Job::None;
  stepIndex_ = 0;
  jobDeadline_.clear();
  registerWindow_.clear();
  nextSignal_.clear();
  nextClock_.clear();
  simRetry_.clear();
  responded_ = false;
  atTries_ = 0;
  pwrkeyTried_ = false;
  registered_ = smsOnly_ = false;
  imsKnown_ = false;
  lastLoggedIms_ = -1;
  operator_.clear();
  simStatus_ = SimStatus::Unknown;
  pinSentThisBoot_ = false;
  simTries_ = 0;
  manualSelectDone_ = false;
  setState(ModemState::PoweringOn);
  waitUntil_.set(transport_.now(), config_.restartSettleMs);
}

// ---------------------------------------------------------------------------
// Jobs
// ---------------------------------------------------------------------------

void A7682EDriver::startJob(Job job) {
  job_ = job;
  stepIndex_ = 0;
  jobDeadline_.clear();
  step(nullptr, at::LineType::Empty);      // the first step sends the command
}

void A7682EDriver::beginRegistering(bool startWindow) {
  setState(ModemState::Registering);
  if (startWindow) registerWindow_.set(transport_.now(), config_.registrationTimeoutMs);
  else registerWindow_.clear();
  registerTries_ = 0;
  waitUntil_.set(transport_.now(), 2000);
  startJob(Job::Register);
}

void A7682EDriver::finishJob(bool ok) {
  const Job done = job_;
  job_ = Job::None;
  stepIndex_ = 0;
  jobDeadline_.clear();
  const uint32_t now = transport_.now();

  switch (done) {
    case Job::Init:
      if (ok) {
        log("Modem: initialised");
        startJob(Job::SimCheck);  // the card first, then the network
      } else if (!pwrkeyTried_ && !responded_) {
        // Twice asked in vain: now the power key is worth it. Not the first time: it would switch a
        // running modem off. And never once the modem has answered: then it runs.
        if (++atTries_ < 2) {
          setState(ModemState::PoweringOn);
          waitUntil_.set(now, config_.atRetryDelayMs);
        } else {
          pwrkeyTried_ = true;
          transport_.pwrkeyPulse();
          log("Modem: power key pressed, it did not answer");
          setState(ModemState::PoweringOn);
          waitUntil_.set(now, config_.powerOnDelayMs);
        }
      } else if (++atTries_ < config_.atAttempts) {
        // After the key, or when the modem stopped answering in the middle: keep asking. How long a
        // cold start takes depends on what the modem is doing, so a fixed wait does not fit.
        setState(ModemState::PoweringOn);
        waitUntil_.set(now, config_.atRetryDelayMs);
      } else {
        setState(ModemState::Error);
        log("Modem not responding, giving up");
      }
      break;

    case Job::SimCheck:
      if (simRetry_.armed()) break;  // not done: stay in Initializing, pickNextJob() asks again
      // An answer that is not understood holds nothing up: the modem registers without our help.
      // Only what the SIM refuses outright is an error, which the interface names.
      if (simStatus_ == SimStatus::Missing || simStatus_ == SimStatus::PinRequired || simStatus_ == SimStatus::PinWrong ||
          simStatus_ == SimStatus::PukRequired) {
        setState(ModemState::Error);
      } else {
        startJob(Job::DataBearer);
      }
      break;

    case Job::DataBearer:
      // Whatever happened there, registration goes on: a modem whose bearer could not be set is
      // still a modem for messages.
      beginRegistering(true);
      break;

    case Job::Register:
      if (ok && registered_) {
        startJob(Job::Ims);
      } else if (registerWindow_.expired(now)) {
        setState(ModemState::Error);
        log("Cellular: registration failed, no network found");
      } else if (++registerTries_ >= config_.autoRegisterAttempts && !manualSelectDone_ && registerWindow_.armed()) {
        // The automatic search finds nothing. Look which networks can be reached and choose the first
        // by hand. The search needs time of its own, and the registration its full window after it:
        // without this extension the window ran out in the middle of the search and the modem went to
        // Error (seen on the device, 2026-09-30).
        manualSelectDone_ = true;
        registerWindow_.set(now, config_.operatorScanTimeoutMs + config_.registrationTimeoutMs);
        log("Cellular: automatic network search found nothing, choosing by hand");
        startJob(Job::ScanOperators);
      }
      break;

    case Job::ScanOperators:
      if (ok && pickedOperator_[0]) startJob(Job::SelectOperator);
      else startJob(Job::Register);
      break;

    case Job::SelectOperator:
      startJob(Job::Register);
      break;

    case Job::Ims:
      // Registered, and the question about the IMS is answered (or was not, which voiceStatus() then
      // says as "unknown"): now the modem is ready.
      setState(ModemState::Ready);
      nextSignal_.set(now, config_.signalIntervalMs);
      nextClock_.set(now, config_.clockIntervalMs);
      break;

    case Job::Signal:
      if (!registered_ && state_ == ModemState::Ready) {
        log("Cellular: network lost, registering again");
        beginRegistering(false);  // no window: a device in the woods is not in error
      }
      break;

    case Job::Clock:
    case Job::None:
      break;
  }
}

bool A7682EDriver::contextOneIsRight() const {
  return context1Seen_ && strcmp(context1_.type, "IPV4V6") == 0 && equalsIgnoreCase(context1_.apn, config_.apn.c_str());
}

void A7682EDriver::logIms() {
  std::string line = imsRegistered_ ? "Cellular: IMS registered" : "Cellular: IMS not registered, calls cannot be made";
  line += pdn8Seen_ ? std::string(" (PDN8 ") + pdn8Type_ + ")" : " (PDN8 not listed)";
  log(line);
  lastLoggedIms_ = imsRegistered_ ? 1 : 0;
}

void A7682EDriver::step(const char* line, at::LineType type) {
  char buf[160];
  const bool answered = type == at::LineType::Ok || type == at::LineType::Error;

  switch (job_) {
    // -----------------------------------------------------------------------
    case Job::Init: {
      // Step 0 is a bare "AT" to synchronise, then the list, then the network mode. A command the
      // modem refuses is no reason to give up: not every modem knows every command.
      if (!line) {
        send("AT");
        expect(config_.commandTimeoutMs);
        return;
      }
      if (!answered) return;
      if (stepIndex_ == 0) responded_ = true;
      ++stepIndex_;
      if (stepIndex_ >= kInitSteps) {
        finishJob(true);
        return;
      }
      if (stepIndex_ <= kInitCommandCount) {
        send(kInitCommands[stepIndex_ - 1]);
      } else {
        snprintf(buf, sizeof(buf), "AT+CNMP=%d", config_.networkMode);
        send(buf);
      }
      expect(config_.commandTimeoutMs);
      return;
    }

    // -----------------------------------------------------------------------
    // Ask the SIM and, if it wants one, give it the PIN. Step 0 asks, step 1 judges the answer to
    // the PIN. The PIN is sent once per start of the modem and never again after a rejection: the
    // card counts and locks itself after three wrong tries.
    case Job::SimCheck:
      if (!line) {
        simStatus_ = SimStatus::Unknown;
        send("AT+CPIN?");
        expect(config_.commandTimeoutMs);
        return;
      }
      if (stepIndex_ == 0) {
        if (type == at::LineType::Cpin) {
          SimStatus s;
          if (at::parseCpin(line, &s)) simStatus_ = s;
          return;
        }
        if (type == at::LineType::Error) {
          // "SIM not inserted" is unambiguous. Any other error means the modem does not know yet: on the
          // device it answers "SIM failure" in the first seconds and then "+CPIN: READY". Ask again
          // after a second instead of missing a card with a PIN.
          if (at::isSimNotInserted(line)) {
            simStatus_ = SimStatus::Missing;
            log("SIM: no card");
          } else if (++simTries_ < kSimQueryAttempts) {
            simRetry_.set(transport_.now(), 1000);
          } else {
            log("SIM: state not known, carrying on");
          }
          finishJob(true);
          return;
        }
        if (type != at::LineType::Ok) return;

        if (simStatus_ == SimStatus::PinRequired) {
          if (pinRejected_) simStatus_ = SimStatus::PinWrong;  // not again
          if (config_.simPin.empty() || pinRejected_ || pinSentThisBoot_) {
            log(pinRejected_ ? "SIM: PIN was rejected before, it is not tried again" : "SIM: PIN required but none is stored");
            finishJob(true);  // without a usable PIN there is no network
            return;
          }
          pinSentThisBoot_ = true;
          stepIndex_ = 1;
          snprintf(buf, sizeof(buf), "AT+CPIN=\"%s\"", config_.simPin.c_str());
          send(buf);
          log("SIM: PIN sent");
          expect(config_.commandTimeoutMs);
          return;
        }
        if (simStatus_ == SimStatus::PukRequired) log("SIM: PUK required, the card is locked");
        else if (simStatus_ == SimStatus::Ready) log("SIM: ready");
        finishJob(true);
        return;
      }
      if (type == at::LineType::Ok) {
        simStatus_ = SimStatus::Ready;
        log("SIM: PIN accepted");
        finishJob(true);
      } else if (type == at::LineType::Error) {
        simStatus_ = SimStatus::PinWrong;
        pinRejected_ = true;
        log("SIM: PIN rejected, it is not tried again");
        finishJob(true);
      }
      return;

    // -----------------------------------------------------------------------
    // The default bearer. The IMS bearer is created from PDP context 1, so context 1 has to be
    // IPV4V6 with the operator's APN: read the contexts, write only when context 1 differs, switch
    // the radio off and on so that the network builds the bearer again. Context 8 (the IMS bearer
    // itself) is never touched: setting it by hand did not help, its type follows context 1.
    //   step 0 AT+CGDCONT?   1 AT+CGDCONT=1,…   2 AT+CFUN=0   3 AT+CFUN=1
    case Job::DataBearer:
      if (!line) {
        if (!apnUsable()) {
          if (!apnNoticeLogged_) {
            log(config_.apn.empty() ? "Cellular: no APN set, the data contexts stay as the modem has them"
                                    : "Cellular: the APN has a character that does not belong in one, it is not sent");
            apnNoticeLogged_ = true;
          }
          finishJob(true);
          return;
        }
        context1Seen_ = false;
        send("AT+CGDCONT?");
        expect(config_.commandTimeoutMs);
        return;
      }
      if (!answered) return;
      switch (stepIndex_) {
        case 0:
          if (type == at::LineType::Error) {
            log("Cellular: the data contexts could not be read, they stay as they are");
            finishJob(true);
          } else if (contextOneIsRight()) {
            finishJob(true);  // nothing to write: the modem's memory is not worn on every start
          } else {
            stepIndex_ = 1;
            snprintf(buf, sizeof(buf), "AT+CGDCONT=1,\"IPV4V6\",\"%s\"", config_.apn.c_str());
            send(buf);
            expect(config_.commandTimeoutMs);
          }
          return;
        case 1:
          if (type == at::LineType::Error) {
            // For example "no network service" while the radio is off: the context stays as it is.
            log(std::string("Cellular: data context 1 could not be set (") + line + ")");
            finishJob(true);
          } else {
            stepIndex_ = 2;
            send("AT+CFUN=0");
            expect(config_.radioTimeoutMs);
          }
          return;
        case 2:
          // Whatever the answer: the radio must come back on.
          stepIndex_ = 3;
          cfunTries_ = 0;
          send("AT+CFUN=1");
          expect(config_.radioTimeoutMs);
          return;
        default:
          if (type == at::LineType::Ok) {
            log("Cellular: data context 1 written as IPV4V6, radio switched off and on");
            finishJob(true);
          } else if (++cfunTries_ < 3) {
            send("AT+CFUN=1");
            expect(config_.radioTimeoutMs);
          } else {
            log("Cellular: the radio could not be switched on again");
            finishJob(false);
          }
          return;
      }

    // -----------------------------------------------------------------------
    // The search for operators. The list comes as one line and can be long; the first code is enough.
    case Job::ScanOperators:
      if (!line) {
        pickedOperator_[0] = '\0';
        send("AT+COPS=?");
        expect(config_.operatorScanTimeoutMs);
        return;
      }
      if (answered) finishJob(pickedOperator_[0] != '\0');
      return;

    case Job::SelectOperator:
      if (!line) {
        snprintf(buf, sizeof(buf), "AT+COPS=1,2,\"%s\"", pickedOperator_);
        send(buf);
        expect(config_.operatorScanTimeoutMs);
        return;
      }
      if (answered) finishJob(type == at::LineType::Ok);
      return;

    // -----------------------------------------------------------------------
    // Registration: the status, and once registered the operator, the network time and the signal.
    case Job::Register:
      if (!line) {
        send("AT+CREG?");
        expect(config_.commandTimeoutMs);
        return;
      }
      if (!answered) return;
      if (!registered_) {
        finishJob(false);  // not yet: pickNextJob() asks again
        return;
      }
      if (stepIndex_ == 0) {
        stepIndex_ = 1;
        send("AT+COPS?");
        expect(config_.commandTimeoutMs);
      } else if (stepIndex_ == 1) {
        stepIndex_ = 2;
        send("AT+CCLK?");
        expect(config_.commandTimeoutMs);
      } else if (stepIndex_ == 2) {
        stepIndex_ = 3;
        send("AT+CSQ");  // the bars are right from the start, not after the first periodic question
        expect(config_.commandTimeoutMs);
      } else {
        finishJob(true);
      }
      return;

    // -----------------------------------------------------------------------
    // The IMS, once after registration: AT+CIREG? says whether it is registered, AT+CGDCONT? what type
    // the IMS bearer (context 8) has, for the log.
    case Job::Ims:
      if (!line) {
        send("AT+CIREG?");
        expect(config_.commandTimeoutMs);
        return;
      }
      if (!answered) return;
      if (stepIndex_ == 0) {
        stepIndex_ = 1;
        pdn8Seen_ = false;
        send("AT+CGDCONT?");
        expect(config_.commandTimeoutMs);
        return;
      }
      if (imsKnown_) logIms();
      else log("Cellular: the state of the IMS is not known");
      finishJob(true);
      return;

    // -----------------------------------------------------------------------
    // The periodic question, every 30 seconds: signal, the network (it may have changed by itself:
    // abroad on 2026-10-03 the device kept showing the home operator, because nobody asked after the
    // modem had booked itself into another), the operator, and the IMS.
    case Job::Signal:
      if (!line) {
        send("AT+CSQ");
        expect(config_.commandTimeoutMs);
        return;
      }
      if (!answered) return;
      if (stepIndex_ == 0) {
        stepIndex_ = 1;
        send("AT+CREG?");
      } else if (stepIndex_ == 1) {
        stepIndex_ = 2;
        send("AT+COPS?");
      } else if (stepIndex_ == 2 && registered_) {
        stepIndex_ = 3;
        send("AT+CIREG?");
      } else {
        // The IMS can come and go without a restart; that is worth one line, not one per poll.
        if (stepIndex_ == 3 && imsKnown_ && lastLoggedIms_ != (imsRegistered_ ? 1 : 0)) logIms();
        finishJob(true);
        return;
      }
      expect(config_.commandTimeoutMs);
      return;

    case Job::Clock:
      if (!line) {
        send("AT+CCLK?");
        expect(config_.commandTimeoutMs);
        return;
      }
      if (answered) finishJob(true);
      return;

    case Job::None:
      return;
  }
}

// A command that got no answer in time ends its job, not the device.
void A7682EDriver::timedOut() {
  if (job_ == Job::DataBearer && stepIndex_ == 2) {
    // The radio is off and CFUN=0 did not answer: switch it on regardless.
    stepIndex_ = 3;
    cfunTries_ = 0;
    send("AT+CFUN=1");
    expect(config_.radioTimeoutMs);
    return;
  }
  if (job_ == Job::DataBearer && stepIndex_ == 3 && ++cfunTries_ < 3) {
    send("AT+CFUN=1");
    expect(config_.radioTimeoutMs);
    return;
  }
  if (job_ == Job::DataBearer) log("Cellular: no answer while setting the data context");
  finishJob(false);
}

// ---------------------------------------------------------------------------
// Lines
// ---------------------------------------------------------------------------

// Answers to our questions may turn up anywhere, so they are taken here, wherever they come; the
// job steps then only watch for the final line.
void A7682EDriver::onLine(const char* line) {
  const at::LineType type = at::classify(line);
  if (type == at::LineType::Empty) return;

  switch (type) {
    case at::LineType::AtReady:
      // The first one after power-on is the boot. One after the modem had answered is a restart.
      if (responded_ && state_ != ModemState::Off && state_ != ModemState::PoweringOn) onRestart();
      return;

    case at::LineType::Creg: {
      at::Registration r;
      if (at::parseCreg(line, &r)) {
        registered_ = r.registered();
        smsOnly_ = r.smsOnly();
        if (!registered_) {
          operator_.clear();  // no network, no operator: the old name would be wrong
          imsKnown_ = false;
        }
      }
      return;
    }

    case at::LineType::Cops: {
      // During the search the same line is something else: the list of networks in reach.
      if (job_ == Job::ScanOperators) {
        at::parseFirstOperatorCode(line, pickedOperator_, sizeof(pickedOperator_));
        return;
      }
      char name[24];
      if (at::parseCopsName(line, name, sizeof(name)) && registered_) operator_ = name;
      return;
    }

    case at::LineType::Cclk: {
      at::NetworkTime t;
      if (at::parseCclk(line, &t)) {
        time_ = t;
        timeKnown_ = true;
      }
      return;
    }

    case at::LineType::Csq:
      at::parseCsq(line, &rssi_);
      return;

    case at::LineType::Cireg: {
      at::ImsRegistration r;
      if (at::parseCireg(line, &r)) {
        imsKnown_ = true;
        imsRegistered_ = r.registered();
      }
      return;
    }

    case at::LineType::Cgdcont: {
      at::PdpContext c;
      if (!at::parseCgdcont(line, &c)) return;
      if (c.cid == 1) {
        context1_ = c;
        context1Seen_ = true;
      } else if (c.cid == 8) {
        strcpy(pdn8Type_, c.type);
        pdn8Seen_ = true;
      }
      return;
    }

    default:
      break;
  }
  if (type == at::LineType::Ok || type == at::LineType::Error || type == at::LineType::Cpin) step(line, type);
}

void A7682EDriver::pickNextJob() {
  if (job_ != Job::None || state_ == ModemState::Off || state_ == ModemState::Error || state_ == ModemState::PoweringOn) return;
  const uint32_t now = transport_.now();

  if (state_ == ModemState::Initializing) {
    if (simRetry_.armed() && simRetry_.expired(now)) {
      simRetry_.clear();
      startJob(Job::SimCheck);
    }
    return;
  }
  if (state_ == ModemState::Registering) {
    if (registerWindow_.expired(now)) {
      setState(ModemState::Error);
      log("Cellular: registration failed, no network found");
      return;
    }
    // Only ask when nothing runs: otherwise the loop would overwrite a search that is still going.
    if (waitUntil_.expired(now)) {
      waitUntil_.set(now, 2000);  // every two seconds
      startJob(Job::Register);
    }
    return;
  }
  if (state_ != ModemState::Ready) return;

  if (nextSignal_.expired(now)) {
    nextSignal_.set(now, config_.signalIntervalMs);
    startJob(Job::Signal);
    return;
  }
  if (nextClock_.expired(now)) {
    nextClock_.set(now, config_.clockIntervalMs);
    startJob(Job::Clock);
  }
}

void A7682EDriver::poll() {
  // 1. Read what is there.
  for (int c = transport_.read(); c >= 0; c = transport_.read())
    if (reader_.feed((char)c)) onLine(reader_.line());

  // 2. After switching on, wait until the modem answers.
  if (state_ == ModemState::PoweringOn && waitUntil_.expired(transport_.now())) {
    waitUntil_.clear();
    setState(ModemState::Initializing);
    startJob(Job::Init);
    return;
  }

  // 3. A command that hangs ends its job.
  if (job_ != Job::None && jobDeadline_.expired(transport_.now())) {
    timedOut();
    return;
  }

  // 4. Choose the next job.
  pickNextJob();
}
