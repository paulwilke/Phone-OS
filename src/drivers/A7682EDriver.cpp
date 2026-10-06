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

// While a call runs, the speaker belongs to the modem, from the first dial tone and ring on. Otherwise
// not, or the amplifier would stay on and draw current. One place, so that no path forgets it.
void A7682EDriver::setState(ModemState s) {
  const bool wasCall = callState();
  state_ = s;
  if (callState() != wasCall) transport_.setAudioToModem(callState());
}

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
  // A call does not survive it. One being set up failed because of the restart (the real hang: no
  // answer for about seven seconds after ATD, then *ATREADY); a connected one ended; a ringing one is gone.
  if (callState()) {
    if (state_ == ModemState::Dialing) queueEvent(CallEventType::DialFailed, 0, "*ATREADY | " + callFacts(), DialFailReason::ModemRestarted);
    else if (state_ == ModemState::InCall) queueEvent(CallEventType::Ended, (transport_.now() - connectedAtMs_) / 1000, "");
    else queueEvent(CallEventType::Missed, 0, "");
  }
  callPhone_.clear();
  callOutgoing_ = callConnected_ = dialPending_ = atdSent_ = answerPending_ = false;
  chupPending_ = 0;
  diagPending_ = false;
  clccPoll_.clear();
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
        if (volume_ != 4) volumePending_ = true;  // the initialisation set the default; the child's choice comes back
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

    case Job::CallDiag:
      // The failed dial is reported now, with the network's own reason attached by step().
      diagPending_ = false;
      pushEvent(diagEvent_);
      break;

    case Job::Clock:
    case Job::Dial:
    case Job::Answer:
    case Job::Hangup:
    case Job::CallPoll:
    case Job::Volume:
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

    // -----------------------------------------------------------------------
    // Dialling. The IMS is asked once more right before ATD: the state of up to 30 seconds ago may be
    // gone, and ATD without the IMS hangs the modem (docs/hardware-lessons.md). Only a fresh "registered"
    // lets the call go out.
    case Job::Dial:
      if (!line) {
        dialImsSeen_ = false;
        send("AT+CIREG?");
        expect(config_.commandTimeoutMs);
        return;
      }
      if (!answered) return;
      if (stepIndex_ == 0) {
        if (state_ != ModemState::Dialing) {  // hangup() came in between: nothing was sent, nothing to undo
          finishJob(false);
          return;
        }
        if (!dialImsSeen_ || !imsRegistered_) {
          failDial(DialFailReason::NotReady, dialImsSeen_ ? "IMS not registered, ATD not sent" : "IMS state could not be read, ATD not sent");
          finishJob(false);
          return;
        }
        stepIndex_ = 1;
        snprintf(buf, sizeof(buf), "ATD%s;", callPhone_.c_str());
        atdSent_ = true;
        send(buf);
        expect(config_.dialTimeoutMs);
        return;
      }
      if (type == at::LineType::Ok) {
        clccPoll_.set(transport_.now(), config_.callPollMs);
        finishJob(true);
      } else {
        if (state_ == ModemState::Dialing) failDial(DialFailReason::Refused, line);
        finishJob(false);
      }
      return;

    case Job::Answer:
      if (!line) {
        send("ATA");
        expect(config_.commandTimeoutMs);
        return;
      }
      if (!answered) return;
      if (type == at::LineType::Ok) {
        if (state_ == ModemState::RingingIn) connectCall();
      } else {
        log(std::string("Call: the modem refused to answer (") + line + ")");
      }
      finishJob(type == at::LineType::Ok);
      return;

    case Job::Hangup:
      if (!line) {
        send("AT+CHUP");
        expect(config_.commandTimeoutMs);
        return;
      }
      if (answered) finishJob(type == at::LineType::Ok);
      return;

    case Job::Volume:
      if (!line) {
        snprintf(buf, sizeof(buf), "AT+COUTGAIN=%d", volume_);
        send(buf);
        expect(config_.commandTimeoutMs);
        return;
      }
      if (answered) finishJob(type == at::LineType::Ok);
      return;

    // While a call is being set up the call list says how far it is: 2 dialling, 3 ringing at the other
    // end, 0 active. A state 0 is the connection even when no VOICE CALL: BEGIN comes.
    case Job::CallPoll:
      if (!line) {
        clccState_ = -1;
        send("AT+CLCC");
        expect(config_.commandTimeoutMs);
        return;
      }
      if (!answered) return;
      if (type == at::LineType::Ok && state_ == ModemState::Dialing && atdSent_) {
        if (clccState_ == 0) {
          connectCall();
        } else if (clccState_ == 3 && !ringingLogged_) {
          ringingLogged_ = true;
          log("Call: ringing at the other end");
        } else if (clccState_ < 0 && clccSeen_) {
          callEnded("call no longer in the call list", 0);  // gone without a word
        }
      }
      if (state_ == ModemState::Dialing) clccPoll_.set(transport_.now(), config_.callPollMs);
      finishJob(true);
      return;

    // After a failed outgoing call the network is asked why (AT+CEER). The event is reported only
    // afterwards, so that the reason is in the log: the calls of 2026-10-01 showed only "did not connect".
    case Job::CallDiag:
      if (!line) {
        send("AT+CEER");
        expect(config_.commandTimeoutMs);
        return;
      }
      if (strncmp(line, "+CEER:", 6) == 0) {
        const char* v = line + 6;
        while (*v == ' ') ++v;
        diagEvent_.reason += std::string(" | ") + v;
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
  if (job_ == Job::Dial && state_ == ModemState::Dialing) {
    // No answer to ATD: the call may exist all the same, so it is hung up for safety.
    if (stepIndex_ == 1) ++chupPending_;
    failDial(DialFailReason::Unknown, stepIndex_ == 1 ? "no answer to ATD" : "no answer to the IMS question, ATD not sent");
  }
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
        imsMode_ = r.mode;
        imsInfo_ = r.info;
        imsExt_ = r.ext;
        if (job_ == Job::Dial) dialImsSeen_ = true;
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
  if (callState() && type != at::LineType::Ok) {
    // Since the call began, write down what the modem says: on 2026-10-02 calls ended after two seconds
    // with CEER "0 Unknown", and nobody saw what came before.
    if (callTrace_.size() < 160) callTrace_ += (callTrace_.empty() ? "" : "/") + std::string(line);
  }
  onCallLine(line, type);
  if (type == at::LineType::Ok || type == at::LineType::Error || type == at::LineType::Cpin ||
      type == at::LineType::Other || type == at::LineType::Clcc) {
    step(line, type);
  }
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
  if (state_ != ModemState::Ready && !callState()) return;

  // What the application asked for comes before the periodic questions, and a call before everything:
  // the volume is changed in the middle of a call and must take effect at once.
  if (chupPending_ > 0) {
    --chupPending_;
    startJob(Job::Hangup);
    return;
  }
  if (diagPending_) {
    startJob(Job::CallDiag);
    return;
  }
  if (answerPending_ && state_ == ModemState::RingingIn) {
    answerPending_ = false;
    startJob(Job::Answer);
    return;
  }
  if (volumePending_) {
    volumePending_ = false;
    startJob(Job::Volume);
    return;
  }
  if (dialPending_ && state_ == ModemState::Dialing) {
    dialPending_ = false;
    startJob(Job::Dial);
    return;
  }
  if (state_ == ModemState::Dialing && atdSent_ && clccPoll_.expired(now)) {
    clccPoll_.clear();
    startJob(Job::CallPoll);
    return;
  }
  if (state_ != ModemState::Ready) return;  // the rest waits until the call is over

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

// ---------------------------------------------------------------------------
// Calls
// ---------------------------------------------------------------------------

void A7682EDriver::pushEvent(const CallEvent& e) {
  if (events_.size() >= 16) events_.pop_front();  // the newest are the final ones: they are kept
  events_.push_back(e);
}

void A7682EDriver::queueEvent(CallEventType type, uint32_t duration, const std::string& reason, DialFailReason why) {
  CallEvent e;
  e.type = type;
  e.phone = callPhone_;
  e.duration = duration;
  e.reason = reason;
  e.failReason = why;
  pushEvent(e);
}

bool A7682EDriver::pollCallEvent(CallEvent& out) {
  if (events_.empty()) return false;
  out = events_.front();
  events_.pop_front();
  return true;
}

// What the IMS looked like, for the log of a failed call: the last AT+CIREG? answer and the type of
// the IMS bearer. Without them a failure like the one of 2026-10-02 is a riddle.
std::string A7682EDriver::callFacts() const {
  std::string facts = imsKnown_ ? "CIREG " + std::to_string(imsMode_) + "," + std::to_string(imsInfo_) + (imsExt_ >= 0 ? "," + std::to_string(imsExt_) : "")
                                : "CIREG unknown";
  facts += pdn8Seen_ ? std::string(" PDN8 ") + pdn8Type_ : " PDN8 not listed";
  return facts;
}

void A7682EDriver::startCall(ModemState state, const std::string& phone, bool outgoing) {
  callPhone_ = phone;
  callOutgoing_ = outgoing;
  callConnected_ = false;
  atdSent_ = false;
  clccSeen_ = false;
  ringingLogged_ = false;
  callTrace_.clear();
  setState(state);
}

void A7682EDriver::endCall() {
  callPhone_.clear();
  callOutgoing_ = callConnected_ = dialPending_ = atdSent_ = answerPending_ = false;
  clccPoll_.clear();
  clccSeen_ = false;
  ringingLogged_ = false;
  callTrace_.clear();
  setState(ModemState::Ready);
}

void A7682EDriver::connectCall() {
  if (callConnected_ || !callState()) return;
  callConnected_ = true;
  connectedAtMs_ = transport_.now();
  setState(ModemState::InCall);
  queueEvent(CallEventType::Connected, 0, "");
  log("Call: connected");
}

void A7682EDriver::failDial(DialFailReason why, const std::string& line) {
  log("Call: dial failed, " + line);
  queueEvent(CallEventType::DialFailed, 0, line + " | " + callFacts(), why);
  endCall();
}

// NO CARRIER, or "VOICE CALL: END", or the call gone from the list. What it means depends on how far the
// call had come: a ringing call is missed, a connected one ends, one that never connected failed. The
// failed one asks the network why before it is reported (see CallDiag).
void A7682EDriver::callEnded(const char* line, uint32_t seconds) {
  if (state_ == ModemState::RingingIn) {
    queueEvent(CallEventType::Missed, 0, "");
    endCall();
  } else if (state_ == ModemState::InCall) {
    const uint32_t measured = (transport_.now() - connectedAtMs_) / 1000;
    queueEvent(CallEventType::Ended, seconds > 0 ? seconds : measured, "");
    endCall();
  } else if (state_ == ModemState::Dialing && atdSent_) {
    // Never connected: the network refused the call. With the SIM of 2026-09-30 (registration 7, SMS only)
    // every call ended exactly so; reported as "ended" it looked like a conversation to the child.
    diagEvent_ = CallEvent();
    diagEvent_.type = CallEventType::DialFailed;
    diagEvent_.phone = callPhone_;
    diagEvent_.reason = std::string(line) + " | lines: " + callTrace_ + " | " + callFacts();
    diagPending_ = true;
    log(std::string("Call: dial failed, ") + line);
    endCall();
  }
}

void A7682EDriver::onCallLine(const char* line, at::LineType type) {
  switch (type) {
    case at::LineType::Ring:
      // A ring in any other state is the end of a call waiting or an echo of an old one.
      if (state_ == ModemState::Ready) {
        startCall(ModemState::RingingIn, std::string(), false);
        queueEvent(CallEventType::Incoming, 0, "");
        log("Call: incoming");
      }
      return;

    case at::LineType::Clip: {
      char phone[at::kPhoneSize];
      if (state_ != ModemState::RingingIn) return;
      if (at::parseClip(line, at::Charset::Ucs2, phone, sizeof(phone)) || at::parseClip(line, at::Charset::Gsm, phone, sizeof(phone))) {
        // The number is known only with +CLIP, after the first ring: the call is reported again with it.
        callPhone_ = phone;
        queueEvent(CallEventType::Incoming, 0, "");
      }
      return;
    }

    case at::LineType::Clcc: {
      at::CallListEntry e;
      if (state_ == ModemState::Dialing && atdSent_ && at::parseClcc(line, at::Charset::Ucs2, &e) && e.direction == 0) {
        clccState_ = e.state;
        clccSeen_ = true;
      }
      return;
    }

    case at::LineType::VoiceCallBegin:
      // Connected only when a call exists that is waiting for it: one we dialled (ATD went out, which only
      // happens with a fresh "IMS registered") or one we are answering. Without the IMS the modem sometimes
      // reports BEGIN at once instead of failing, and the product showed "in a call" while nothing was dialled.
      if (state_ == ModemState::Dialing && atdSent_) connectCall();
      else if (state_ == ModemState::RingingIn && job_ == Job::Answer) connectCall();
      else if (state_ != ModemState::InCall) log("Call: VOICE CALL BEGIN without a call, ignored");
      return;

    case at::LineType::VoiceCallEnd: {
      uint32_t seconds = 0;
      at::parseVoiceCallEnd(line, &seconds);
      if (callState()) callEnded(line, seconds);
      return;
    }

    case at::LineType::NoCarrier:
      if (callState()) callEnded(line, 0);
      return;

    case at::LineType::Busy:
      if (state_ == ModemState::Dialing && atdSent_) {
        queueEvent(CallEventType::Busy, 0, "");
        endCall();
      }
      return;

    case at::LineType::NoAnswer:
      if (state_ == ModemState::Dialing && atdSent_) {
        queueEvent(CallEventType::NoAnswer, 0, "");
        endCall();
      }
      return;

    default:
      return;
  }
}

// ---- from the application -----------------------------------------------------------------------------------

bool A7682EDriver::dial(const std::string& phone) {
  // Never without the IMS, and never a call on top of another. A dial that is being set up refuses a
  // second one until its first step is over, so that nothing is sent twice.
  if (state_ != ModemState::Ready || job_ == Job::Dial) return false;
  if (!at::isValidDialString(phone.c_str()) || voiceStatus() != VoiceStatus::Ready) return false;
  startCall(ModemState::Dialing, phone, true);
  dialPending_ = true;
  log("Call: dialling");
  return true;
}

bool A7682EDriver::answer() {
  if (state_ != ModemState::RingingIn || answerPending_) return false;
  answerPending_ = true;
  return true;
}

// Ends the call for the application at once; the modem is told afterwards (AT+CHUP), which may wait for
// a job that is running. What the modem then says about the call (NO CARRIER, END) finds no call and is ignored.
bool A7682EDriver::hangup() {
  if (!callState()) return false;
  if (state_ == ModemState::RingingIn) {
    queueEvent(CallEventType::Missed, 0, "");
    ++chupPending_;
  } else if (state_ == ModemState::InCall) {
    queueEvent(CallEventType::Ended, (transport_.now() - connectedAtMs_) / 1000, "");
    ++chupPending_;
  } else {
    // A dial we cancel ends with Ended and no duration. If ATD has not gone out, nothing was sent and there
    // is nothing to hang up.
    queueEvent(CallEventType::Ended, 0, "");
    if (atdSent_) ++chupPending_;
  }
  endCall();
  return true;
}

bool A7682EDriver::setCallVolume(int level) {
  if (level < 0) level = 0;
  if (level > 7) level = 7;
  if (level != volume_) {
    volume_ = level;
    volumePending_ = true;
  }
  return true;
}
