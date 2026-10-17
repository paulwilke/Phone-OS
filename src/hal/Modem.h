#pragma once

// Capability: cellular. The application knows this interface, no AT commands and no
// modem model. Another modem means a new driver that fulfils this interface.
//
// Everything is polled and never blocks: events are queued by the implementation and
// fetched with the poll functions, each event exactly once.

#include <cstddef>
#include <cstdint>
#include <string>

// The longest SMS text the interface carries, in bytes. Splitting into parts is the
// modem's business.
constexpr size_t kMaxSmsBytes = 320;

struct SmsMessage {
  std::string phone;
  std::string body;
  uint32_t timestamp = 0;  // Unix time from the network, 0 = unknown
};

// A message the modem did **not** manage to send, with the reason it named itself
// (for example "+CMS ERROR: 304"). Without this report every accepted message counted
// as sent, and a rejected emergency message looked on the screen like a delivered one.
// The reason is for the log, not for the child.
struct SmsFailure {
  std::string phone;
  std::string reason;
};

// What the modem is doing.
//   Off          no power
//   PoweringOn   the supply is on, the modem is starting
//   Initializing configuration commands are running
//   Registering  searching for a network
//   Ready        registered with a network and idle: SMS and calls are possible
//   Error        the modem or the SIM prevents operation (a locked or missing SIM
//                belongs here, see SimStatus)
//   Dialing      an outgoing call, not answered yet
//   RingingIn    an incoming call, not answered yet
//   InCall       a call is connected
enum class ModemState { Off, PoweringOn, Initializing, Registering, Ready, Error, Dialing, RingingIn, InCall };

// The events of a call. Every call produces a sequence of events that ends in exactly
// one final event:
//
//   incoming call   Incoming, then Connected and Ended, or Missed
//   outgoing call   (nothing at first), then Connected and Ended, or one of Busy,
//                   NoAnswer, DialFailed
//
// Ended, Missed, Busy, NoAnswer and DialFailed are final: nothing follows for that
// call. Ended is delivered once, whichever side hung up; its `duration` is the length of
// the connected call in seconds (0 for all other events). A call we cancel with
// hangup() while it is still dialling ends with Ended and duration 0, without a
// Connected before it; one we refuse while it rings is Missed. Switching the modem off
// during a call ends it the same way.
enum class CallEventType { Incoming, Connected, Ended, Missed, Busy, NoAnswer, DialFailed };

// Why a dialled call failed, as a code the application words from the catalogs. Busy and
// NoAnswer are events of their own and have no code here.
//   Unknown          the modem named nothing the driver can classify; the application
//                    derives the words from the state of the network
//   NotReady         the IMS was not registered: telephony was not ready
//   ModemRestarted   the modem restarted while the call was being set up
//   Refused          the network or the modem refused the call
//   NoNetwork        there was no network
//   SmsOnly          the network registered the SIM for SMS only
enum class DialFailReason { Unknown, NotReady, ModemRestarted, Refused, NoNetwork, SmsOnly };

// `reason` is the modem's own line when it names one (for example "+CME ERROR: ..."):
// for the log, not for the child. `failReason` is meaningful for DialFailed only.
struct CallEvent {
  CallEventType type = CallEventType::Ended;
  std::string phone;  // the other party, empty when the network withholds it
  uint32_t duration = 0;
  std::string reason;
  DialFailReason failReason = DialFailReason::Unknown;
};

// Whether calls are possible. Registering for the network is not enough: without the
// IMS the modem hangs two seconds after ATD, so telephony needs both.
//   Unknown     not asked yet (no verdict is no complaint: nothing is shown)
//   Ready       registered, and the IMS is registered: calls work
//   SmsOnly     registered for SMS only, a restriction of the network
//   ImsMissing  registered, but the IMS is not: telephony is not ready, usually for a short time
enum class VoiceStatus { Unknown, Ready, SmsOnly, ImsMissing };

// What the SIM card tells the modem. Without it a SIM with a PIN looked like "no
// network": the modem does not register until the PIN is entered, and until then
// everything is silent.
enum class SimStatus {
  Unknown,      // not asked, or no answer: the modem tries anyway
  Ready,
  Missing,      // no card
  PinRequired,  // the card wants its PIN and we have none (or it is invalid)
  PinWrong,     // the stored PIN was rejected; we never try again
  PukRequired,  // the card is locked and can only be saved with the PUK: not the device's business
};

class Modem {
public:
  virtual ~Modem() = default;

  // Powers up and starts registering. Safe to call when already on.
  virtual void begin() = 0;
  // Powers the modem down completely.
  virtual void shutdown() = 0;
  // Sleeping keeps the modem registered; it is woken by an incoming call or SMS, or by wake().
  virtual void sleep() = 0;
  virtual void wake() = 0;

  // Hands a message to the modem. True means *accepted*, not delivered: false when the
  // modem is not ready to send, the number is empty or no dial string (digits, '*', '#'
  // and a leading '+': a number as the parents wrote it, "+49 170 1111111", is refused;
  // the application hands over phone::dialable()), or the text is empty or longer
  // than kMaxSmsBytes. A message that is accepted and then refused by the network is
  // reported once through pollSendFailure().
  virtual bool sendSms(const std::string& phone, const std::string& body) = 0;
  // The next received message, oldest first; false when none is waiting.
  virtual bool pollSms(SmsMessage& out) = 0;
  // The next failed sending, in the order they failed, each exactly once.
  virtual bool pollSendFailure(SmsFailure& out) = 0;

  // False when no call can be started (not Ready, an empty number or no dial string, as for sendSms, or voiceStatus() is
  // SmsOnly or ImsMissing: nothing is sent to the modem then); the state is unchanged.
  virtual bool dial(const std::string& phone) = 0;
  // Answers a ringing call; false when nothing is ringing.
  virtual bool answer() = 0;
  // Ends a call: refuses a ringing one, cancels a dialling one, hangs up a connected
  // one. False when there is no call.
  virtual bool hangup() = 0;
  // The next call event, in the order the network delivered them (see CallEventType).
  virtual bool pollCallEvent(CallEvent& out) = 0;

  // Call volume 0 … 7; a level outside is clamped to it. Sound runs through the
  // modem's codec, so it sets the volume as well. False when the modem cannot;
  // callVolume() is -1 then.
  virtual bool setCallVolume(int level) = 0;
  virtual int callVolume() const = 0;

  // A pure question to the modem, for the diagnosis over USB: which power-saving modes
  // exist and how are they set? Only read forms ("AT+...?") are accepted; anything else
  // returns false and is not sent, because a query must never change anything.
  // pollQueryResult delivers the answer lines joined by "|" once, then false.
  virtual bool query(const std::string& command) = 0;
  virtual bool pollQueryResult(std::string& out) = 0;

  virtual ModemState state() const = 0;
  // 0 … 5; 0 when not registered.
  virtual int signalBars() const = 0;
  // SmsOnly and ImsMissing: writing works, calls do not, and the user must see which of
  // the two it is, so that "no calls" is not read as "no network".
  virtual VoiceStatus voiceStatus() const = 0;
  // The short network name; empty when not registered.
  virtual std::string operatorName() const = 0;
  virtual SimStatus simStatus() const = 0;
};
