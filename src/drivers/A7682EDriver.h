#pragma once

// Driver for the 4G modem A7682E: start-up, SIM, the data bearer for the IMS, registration,
// signal, operator and network time. Plain C++ without Arduino: bytes, time and the power lines
// come through AtTransport, the reading of the answers through at:: (AtParser), so the whole
// course is tested against a scripted modem (FakeAtTransport).
//
// There is no task and no queue of its own: poll() is a state machine that never blocks and is
// called from the main loop, so there is one user of the line. One job runs at a time; what
// else is wanted waits as a flag.
//
// The VoLTE rule that this driver exists for (docs/hardware-lessons.md): outgoing calls hung the
// modem two seconds after ATD whenever the IMS was not registered. The IMS bearer is created
// from PDP context 1, so the driver defines that context itself, as IPV4V6 with the configured
// APN, and cycles the radio once; it asks AT+CIREG? and reports the answer as voiceStatus().
//
// The character set is UCS2: umlauts and emoji arrive unharmed, and the text lines of an answer
// are pure hex, which makes a URC in the middle of an answer unmistakable.

#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>
#include <vector>

#include "AtParser.h"
#include "AtTransport.h"
#include "SmsPdu.h"
#include "../hal/Modem.h"

class A7682EDriver : public Modem {
public:
  struct Config {
    // The APN of PDP context 1. Empty: the modem's own contexts are left alone (nothing is
    // written, the radio is not cycled) and the log says so once per start. An APN with a
    // character that does not belong in one is not sent either.
    std::string apn;
    // The PIN of the SIM, digits only, 4 to 8; anything else is discarded, because it ends up
    // inside an AT command. Empty: the card has no PIN, or it is not known.
    std::string simPin;

    uint32_t probeDelayMs = 500;  // first ask whether the modem already runs
    // After the power key. On the device the A7682E needed more than three seconds; with 3000 the
    // initialisation ended in an error (2026-09-30).
    uint32_t powerOnDelayMs = 6000;
    // So often "AT" is asked before giving up. A fixed wait does not fit: a running modem answers
    // at once, a cold one needs seconds, and how many depends on what it is doing.
    int atAttempts = 12;
    uint32_t atRetryDelayMs = 1000;
    uint32_t restartSettleMs = 3000;  // after a restart the modem reported, before asking again
    uint32_t commandTimeoutMs = 5000;
    uint32_t smsSendTimeoutMs = 60000;  // AT+CMGS may take long: it waits for the network
    uint32_t radioTimeoutMs = 30000;  // AT+CFUN switches the radio off and on; each may take seconds
    uint32_t registrationTimeoutMs = 90000;
    uint32_t operatorScanTimeoutMs = 120000;  // AT+COPS=? searches for a long time
    // After so many fruitless AT+CREG? rounds (every 2 s) the network is chosen by hand. On
    // 2026-09-30 a foreign SIM found no network in Germany with AT+COPS=0, while
    // AT+COPS=1,2,"<code>" registered within seconds. Thirty rounds, about a minute: with eight (16 s) a cold
    // modem, after five hours switched off, had not registered on its home network by itself, and the choice by
    // hand fired at home (2026-10-16). A foreign SIM pays the longer wait once per start.
    int autoRegisterAttempts = 30;
    // A modem that sits on a foreign network (roaming) or on one chosen by hand does not go back to its own by
    // itself while the foreign one still carries: on the German-French border it stayed on the French network
    // for a long time after coming home (2026-10). So often, when nothing else is going on, it is told to
    // deregister and search again, which prefers the home network. Fifteen minutes: every search costs a few
    // seconds without a network.
    uint32_t reselectIntervalMs = 15 * 60 * 1000;
    // AT+CNMP: the A7682E knows 2 (automatic), 13 (GSM only) and 38 (LTE only), no 3G.
    // Automatic is the default: LTE covers better, and GSM sending draws up to two amperes, with
    // which the modem restarted once in the middle of a call.
    int networkMode = 2;
    // ATD normally answers at once. When the IMS is missing the modem hangs for about seven seconds
    // and then restarts (*ATREADY): the limit must be longer than that, or a restart would be reported as
    // a call that did not answer.
    uint32_t dialTimeoutMs = 15000;
    uint32_t callPollMs = 1000;          // AT+CLCC while a call is being set up
    // An incoming call is reported once, with the caller's number from +CLIP. A number that does not come
    // (withheld) is waited for this long, and the call is then reported without one.
    uint32_t incomingNumberWaitMs = 1500;
    uint32_t signalIntervalMs = 30000;   // AT+CSQ, the network, the operator and the IMS
    uint32_t clockIntervalMs = 3600000;  // AT+CCLK
  };

  explicit A7682EDriver(AtTransport& transport) : A7682EDriver(transport, Config()) {}
  A7682EDriver(AtTransport& transport, const Config& config);

  // ---- the Modem capability ----------------------------------------------------------------------------------------
  // Switches the modem on and starts. A second call changes nothing.
  void begin() override;
  // AT+CPOF, then the supply off. The call that is on ends as the capability says, what was queued but not
  // sent is reported as failed, and the state is Off when the job is done.
  void shutdown() override;
  // Sleeping keeps the modem registered (AT+CSCLK=1, DTR high). It starts when nothing else is to be done,
  // and the modem is woken by whatever needs it: a message to send, a call, a question, wake().
  void sleep() override;
  void wake() override;

  // True means accepted, not delivered: false when the modem is not ready to send (not registered, off or
  // going off), the number is not a dial string, the text is empty, not valid UTF-8 or longer than
  // kMaxSmsBytes, or four messages are already waiting. The text goes out in PDU mode (SmsPdu.h): 7-bit
  // (160 places) when all its characters are in the GSM alphabet, umlauts included, else UCS2 (70); a longer
  // text is one concatenated message that the receiving phone shows as one.
  bool sendSms(const std::string& phone, const std::string& body) override;
  bool pollSms(SmsMessage& out) override;
  bool pollSendFailure(SmsFailure& out) override;

  // A pure question for the diagnosis: only "AT+<name>?" and "AT+<name>=?" are accepted (see isQuery).
  bool query(const std::string& command) override;
  bool pollQueryResult(std::string& out) override;
  static bool isQuery(const std::string& command);

  // From the main loop; never blocks.
  void poll();

  ModemState state() const override { return state_; }
  SimStatus simStatus() const override { return simStatus_; }
  VoiceStatus voiceStatus() const override;
  // 0 … 5; 0 while not registered.
  int signalBars() const override;
  // The short network name; empty while not registered.
  std::string operatorName() const override { return operator_; }
  // The network time of AT+CCLK; false until the modem has one.
  bool networkTime(at::NetworkTime& out) const;
  // How often the modem restarted by itself since begin() (*ATREADY after it had answered).
  int restartCount() const { return restarts_; }

  // ---- calls ---------------------------------------------------------------------------------------------
  // Same meaning as in the Modem capability (src/hal/Modem.h). A dial is refused (false, and nothing
  // is sent) unless the modem is Ready, the number is a dial string and voiceStatus() is Ready; before
  // ATD goes out the IMS is asked once more, because the state of 30 seconds ago may be gone.
  bool dial(const std::string& phone) override;
  bool answer() override;
  bool hangup() override;
  bool pollCallEvent(CallEvent& out) override;
  // Call volume 0 … 7, clamped; takes effect at once, also in the middle of a call.
  bool setCallVolume(int level) override;
  int callVolume() const override { return volume_; }

  // The driver's own log lines (English, technical, no PIN and no number), oldest first, each once.
  // The host writes them through its log, which applies the level; the driver has no log of its
  // own so that it needs nothing above hal/.
  bool pollLog(std::string& line);

private:
  // What the driver is doing. One at a time.
  enum class Job : uint8_t { None, Init, SimCheck, ReadImsi, DataBearer, Register, ScanOperators, SelectOperator, Ims, Signal, Clock,
                             Dial, Answer, Hangup, CallPoll, CallDiag, Volume,
                             SendSms, ReadSms, DrainInbox, Query, Sleep, Wake, Shutdown, Reselect, Automatic };

  void send(const char* command);
  void startJob(Job job);
  void finishJob(bool ok);
  void step(const char* line, at::LineType type);  // a line for the running job
  void timedOut();                                 // the running job got no answer in time
  void onLine(const char* line);
  void onRestart();
  void pickNextJob();
  void setState(ModemState s);
  void expect(uint32_t timeoutMs);
  void log(const std::string& line);
  void searchOn();
  void beginRegistering(bool startWindow);
  bool apnUsable() const;
  // The APN in force: the configured one, else the one the operator list gives for the SIM (decision D-9).
  const std::string& apn() const { return config_.apn.empty() ? chosenApn_ : config_.apn; }
  bool contextOneIsRight() const;
  void logIms();

  // Calls
  bool callState() const { return state_ == ModemState::Dialing || state_ == ModemState::RingingIn || state_ == ModemState::InCall; }
  void pushEvent(const CallEvent& e);
  void queueEvent(CallEventType type, uint32_t duration, const std::string& reason, DialFailReason why = DialFailReason::Unknown);
  void startCall(ModemState state, const std::string& phone, bool outgoing);
  void endCall();
  void connectCall();
  void failDial(DialFailReason why, const std::string& line);
  void callEnded(const char* line, uint32_t seconds);
  std::string callFacts() const;
  void onCallLine(const char* line, at::LineType type);
  void reportIncoming();

  // Messages
  struct Outgoing {
    std::string phone;
    std::vector<sms::Part> parts;    // the PDUs, each one SMS
    sms::Coding coding = sms::Coding::Gsm7;
  };
  bool wantsSomething() const;
  void noteSendFailure(const std::string& phone, const std::string& reason);
  void smsFail(const std::string& reason);
  void smsRestoreTextMode();
  void smsStartPart();
  void queueSms(const SmsMessage& sms);
  void finishPowerOff();

  AtTransport& transport_;
  Config config_;
  at::LineReader reader_;

  ModemState state_ = ModemState::Off;
  Job job_ = Job::None;
  int stepIndex_ = 0;
  at::Deadline jobDeadline_;
  at::Deadline waitUntil_;  // PoweringOn: until the modem is asked; Registering: the next CREG round
  at::Deadline registerWindow_;
  at::Deadline nextSignal_;
  at::Deadline nextReselect_;  // armed while roaming or on a network chosen by hand
  at::Deadline nextClock_;
  std::deque<std::string> log_;

  // Start-up
  bool responded_ = false;     // the modem has answered since it was switched on (an *ATREADY after it is a restart)
  int atTries_ = 0;
  bool pwrkeyTried_ = false;
  int restarts_ = 0;
  int registerTries_ = 0;
  bool manualSelectDone_ = false;
  char pickedOperator_[12] = {};

  // SIM
  SimStatus simStatus_ = SimStatus::Unknown;
  // A rejected PIN is never sent again, not even after a restart of the modem: the SIM counts and
  // locks after three wrong tries.
  bool pinRejected_ = false;
  bool pinSentThisBoot_ = false;  // at most one try per start of the modem
  int simTries_ = 0;
  at::Deadline simRetry_;

  // The data bearer
  bool apnNoticeLogged_ = false;
  std::string chosenApn_;       // from the operator list, when no APN is configured
  bool apnLookedUp_ = false;    // the SIM was asked once for its operator since the start
  // The operator code (MCC and MNC) of the SIM, as the operator list gives it; empty when the list does not know
  // it or the SIM was not asked (a configured APN). A network chosen by hand with this code is the home network.
  std::string homeOperator_;
  at::PdpContext context1_;
  bool context1Seen_ = false;
  char pdn8Type_[12] = {};
  bool pdn8Seen_ = false;
  int cfunTries_ = 0;

  // Calls. One at a time; the driver reports each exactly once, in the order of the Modem contract.
  std::deque<CallEvent> events_;
  std::string callPhone_;
  bool callOutgoing_ = false;
  bool callConnected_ = false;
  uint32_t connectedAtMs_ = 0;
  bool dialPending_ = false;      // Dialing, but ATD has not gone out yet
  bool atdSent_ = false;
  bool dialImsSeen_ = false;      // the answer to the IMS question asked just before ATD
  bool answerPending_ = false;
  bool incomingReported_ = true;  // a ringing call is reported once, when its number is known
  at::Deadline incomingWait_;
  int chupPending_ = 0;           // AT+CHUP still to be sent, however the call ended for the application
  bool volumePending_ = false;
  int volume_ = 4;                // as the initialisation sets it (AT+COUTGAIN=4)
  at::Deadline clccPoll_;
  int clccState_ = -1;            // the state of the call in the last AT+CLCC, -1 when it was not listed
  bool clccSeen_ = false;         // the call has been listed at least once
  bool ringingLogged_ = false;
  std::string callTrace_;         // what the modem said since the call began, for the log of a failure
  bool diagPending_ = false;      // a failed dial waits for AT+CEER before it is reported
  CallEvent diagEvent_;
  int imsMode_ = 0, imsInfo_ = 0, imsExt_ = -1;  // the last AT+CIREG? answer, for the log of a failure

  // Messages
  static constexpr size_t kOutboxSize = 4;
  static constexpr size_t kInboxSize = 6;
  static constexpr size_t kMaxFailures = 16;
  std::deque<Outgoing> outbox_;
  std::deque<SmsMessage> inbox_;
  std::deque<SmsFailure> failures_;
  std::deque<int> cmtiPending_;      // storage slots announced by +CMTI, to be read
  bool drainInboxPending_ = false;   // read what lies on the SIM: after registration and after a message that did not parse
  at::ParsedSms parsed_[2];
  at::SmsResponseParser smsParser_;
  int readingIndex_ = -1;
  int deleteIndex_[2] = {-1, -1};
  size_t deleteCount_ = 0;
  size_t deleteCursor_ = 0;
  size_t partIndex_ = 0;
  uint8_t smsReference_ = 0;  // the concatenation reference of the last text that needed several parts
  bool smsFailed_ = false;

  // Questions, sleep, shutdown
  std::string queryCommand_;
  std::string queryResult_;
  bool queryPending_ = false;
  bool queryDone_ = false;
  bool asleep_ = false;
  bool sleepPending_ = false;
  bool wakePending_ = false;
  bool shutdownPending_ = false;
  bool shuttingDown_ = false;

  // Network
  bool registered_ = false;
  bool smsOnly_ = false;
  bool roaming_ = false;         // +CREG says 5 or 7
  bool manualOperator_ = false;  // the network was chosen by hand (AT+COPS=1): the modem stays there until told otherwise
  // The home network was chosen by hand: the modem is set back to automatic (AT+COPS=0) when it is idle, without
  // deregistering, so that it can follow the child across a border.
  bool automaticPending_ = false;
  std::string operator_;
  std::string loggedOperator_;  // the network is logged when it changes, not at every question
  // The modem gave a blank name: the operator is asked for as its code (AT+COPS=3,2), which the
  // table of at::operatorNameForCode() turns into a name. Kept across restarts of the modem: it is
  // how this SIM and modem answer.
  bool operatorByCode_ = false;
  const char* operatorQuery() const { return operatorByCode_ ? "AT+COPS=3,2;+COPS?" : "AT+COPS?"; }
  int rssi_ = 99;
  bool imsKnown_ = false;
  bool imsRegistered_ = false;
  int lastLoggedIms_ = -1;  // -1 not logged yet, else 0 or 1: the IMS is logged when it changes
  bool timeKnown_ = false;
  at::NetworkTime time_;
};
