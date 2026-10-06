#pragma once

// The reading side of the AT protocol of the A7682E, apart from the UART: lines out of the
// byte stream, what each line is, the answers of the commands the driver asks, and the
// multi-line SMS answers. Plain C++ without Arduino; the fixed buffers are bounded, and the
// fuzz test feeds them random bytes under AddressSanitizer.
//
// Rules the code keeps because the device taught them:
//   * a line is classified whole, never by a substring of the raw stream: an SMS text that
//     reads "BUSY" is not a busy signal;
//   * every answer is read line by line, so ten messages of 160 characters (2 KB) are no
//     problem for a 512-byte line buffer;
//   * a number that does not fit is refused, never shortened: a shortened number is another
//     person's number;
//   * a message counts as received only when its end is seen; a truncated answer leaves the
//     last message on the SIM, and the next read brings it again.

#include <cstddef>
#include <cstdint>

#include "../hal/Modem.h"

namespace at {

// The charset the modem was told with AT+CSCS. It decides how numbers and texts are coded
// in the answers.
enum class Charset { Gsm, Ucs2 };

constexpr size_t kMaxPhoneChars = 23;  // a number is 1 … 23 characters (the AT+CMGS limit)
constexpr size_t kPhoneSize = kMaxPhoneChars + 1;

// ---------------------------------------------------------------------------
// Time limits
// ---------------------------------------------------------------------------

// A point in time on the transport's wrapping millisecond clock. It compares differences,
// so it survives the wrap after about 49 days; a deadline that has not been set never
// expires.
class Deadline {
public:
  void set(uint32_t now, uint32_t afterMs) {
    at_ = now + afterMs;
    armed_ = true;
  }
  void clear() { armed_ = false; }
  bool armed() const { return armed_; }
  bool expired(uint32_t now) const { return armed_ && (int32_t)(now - at_) >= 0; }

private:
  uint32_t at_ = 0;
  bool armed_ = false;
};

// ---------------------------------------------------------------------------
// Lines out of the byte stream
// ---------------------------------------------------------------------------

class LineReader {
public:
  // A UCS2 text is ONE line of four hex digits per character: 160 characters are 640 bytes, and
  // the longest text the parser keeps (SmsResponseParser::kMaxRaw = 1400) must arrive whole, or
  // its end would be cut off without a trace. A multiple of four, so that a longer line is cut
  // between two characters.
  static constexpr size_t kMaxLine = 1500;

  // Takes one byte; true when a line is complete. It stays readable through line() until the
  // next feed(). Empty lines are reported. A NUL byte is line noise and is dropped, because
  // it would end the C string early.
  bool feed(char c);

  const char* line() const { return buf_; }

  // The line last reported was longer than kMaxLine and was cut.
  bool truncated() const { return lastTruncated_; }

  // After AT+CMGS the modem sends "> " without a line end. In this mode a "> " at the start
  // of a line is reported at once, as the line ">".
  void setPromptMode(bool on) { promptMode_ = on; }

  // Forgets a partial line (after a modem restart nothing of it can be trusted).
  void reset();

private:
  char buf_[kMaxLine + 1] = {};
  size_t len_ = 0;
  bool overflow_ = false;
  bool lastTruncated_ = false;
  bool promptMode_ = false;
};

// ---------------------------------------------------------------------------
// What a line is
// ---------------------------------------------------------------------------

enum class LineType {
  Empty, Ok, Error, Prompt,
  Ring, Clip, NoCarrier, Busy, NoAnswer, VoiceCallBegin, VoiceCallEnd, Cmti,
  Cmgl, Cmgr, Cmgs, Csq, Creg, Cops, Cclk, Cpin, Cireg, Cgdcont, Clcc,
  AtReady,  // *ATREADY: the modem has (re)started
  Other
};

LineType classify(const char* line);

// ---------------------------------------------------------------------------
// Answers and URCs, one line each
// ---------------------------------------------------------------------------

// +CLIP: "<number>",145,,,,0. False when the number is missing, unreadable or too long.
bool parseClip(const char* line, Charset cs, char* phone, size_t phoneSize);

// +CMTI: "SM",<index>
bool parseCmti(const char* line, int* index);

// VOICE CALL: END[: <duration>] in the forms "m:ss", "h:mm:ss" and "hhmmss"; 0 when absent.
bool parseVoiceCallEnd(const char* line, uint32_t* seconds);

// +CSQ: <rssi>,<ber>
bool parseCsq(const char* line, int* rssi);
int csqToBars(int rssi);  // 0 … 5; 99 (unknown) gives 0

// +CREG: <n>,<stat>[,…], the answer of AT+CREG?. The driver asks; it does not switch the URCs on.
struct Registration {
  int stat = 0;
  // 1 home network, 5 roaming, 6 and 7 "SMS only" (registered for SMS, no voice). 6 and 7
  // count as registered: the device's job is messages, and on 2026-09-30 a SIM at stat 7
  // sent its SMS all the same.
  bool registered() const { return stat == 1 || stat == 5 || stat == 6 || stat == 7; }
  bool smsOnly() const { return stat == 6 || stat == 7; }
};
bool parseCreg(const char* line, Registration* out);

// +CIREG: <mode>,<reg_info>[,<ext_info>], the answer of AT+CIREG?. reg_info 1 means the IMS is registered; ext_info
// (15 on the device that works) says over which kind of bearer, -1 when absent.
struct ImsRegistration {
  int mode = 0;
  int info = 0;
  int ext = -1;
  bool registered() const { return info == 1; }
};
bool parseCireg(const char* line, ImsRegistration* out);

// +CGDCONT: <cid>,"<type>","<apn>",… (one line per context; the later fields are not needed).
// False when a field does not fit: an APN cut short could look equal to the configured one.
struct PdpContext {
  int cid = 0;
  char type[12] = {};
  char apn[101] = {};
};
bool parseCgdcont(const char* line, PdpContext* out);

// +CLCC: <idx>,<dir>,<stat>,<mode>,<mpty>[,"<number>",<type>]. The number may be missing
// (withheld); then `phone` is empty and the line is still valid. stat: 0 active, 1 held,
// 2 dialling, 3 alerting (ringing at the other end), 4 incoming, 5 waiting, 6 disconnected.
struct CallListEntry {
  int index = 0;
  int direction = 0;  // 0 outgoing, 1 incoming
  int state = 0;
  char phone[kPhoneSize] = {};
};
bool parseClcc(const char* line, Charset cs, CallListEntry* out);

// +CPIN: READY | SIM PIN | SIM PUK. Anything else (SIM PIN2, PH-NET PIN …) is false: the SIM is
// then neither known to be usable nor known to want a PIN.
bool parseCpin(const char* line, SimStatus* out);
// The modem answers AT+CPIN? with "+CME ERROR: SIM not inserted" for a missing card; any other
// error in the first seconds ("SIM failure") only means "not known yet".
bool isSimNotInserted(const char* errorLine);

// +COPS: <mode>[,<format>,"<name>"[,<act>]] → the name (empty when the line carries none).
// A network that answers with its code ("26201") instead of the text gets the name from
// operatorNameForCode(). False when the line has no quoted name.
bool parseCopsName(const char* line, char* name, size_t nameSize);
// The first operator code (MCC and MNC, five or six digits in quotes) of an AT+COPS=? list;
// names such as "o2 - de" are never five digits.
bool parseFirstOperatorCode(const char* line, char* out, size_t outSize);
// For the codes of the countries a device here is likely to meet the brand name; every other
// string is returned as it is.
const char* operatorNameForCode(const char* code);

// +CCLK: "yy/MM/dd,hh:mm:ss±zz" (zz in quarter hours) → Unix time in UTC and the zone in
// minutes. A year before 2020 or after 2069 is a modem that has no network time yet (its
// clock starts in 1970 or 2004): false.
struct NetworkTime {
  uint32_t epochUtc = 0;
  int tzOffsetMinutes = 0;
};
bool parseCclk(const char* line, NetworkTime* out);
// The same timestamp as the SMS service centre writes it ("yy/MM/dd,hh:mm:ss±zz").
bool parseScts(const char* s, uint32_t* epochUtc);

// ---------------------------------------------------------------------------
// Commands
// ---------------------------------------------------------------------------

// AT+CMGS="<number>" for 1 … 23 characters of 0-9 * # and a leading +; everything else, a
// buffer that is too small included, is false and leaves `out` empty.
bool buildCmgsCommand(const char* phone, Charset cs, char* out, size_t outSize);

// ---------------------------------------------------------------------------
// The answer of AT+CMGL and AT+CMGR
// ---------------------------------------------------------------------------

struct ParsedSms {
  int index = -1;  // the storage slot for AT+CMGD; +CMGR does not carry it
  SmsMessage msg;
  bool senderValid = false;  // the number is readable and fits; otherwise msg.phone is empty
  bool bodyTruncated = false;
};

enum class Feed {
  Consumed,     // the line belongs to the answer
  Unsolicited,  // it does not (a RING in between): the caller treats it as a URC
  Done          // OK or an error was seen; the answer is complete
};

// Fed line by line, it puts finished messages into an array the caller owns. The driver
// deletes from the SIM only afterwards, and only the messages that landed here (index >= 0).
// A message is finished when the next header or the final OK follows.
//
// Limit in the GSM charset: a text line that reads exactly "OK" or "ERROR" cannot be told from
// the status line. In UCS2 texts are pure hex, so this confusion does not exist and a URC
// between two messages is recognised as one; that is why UCS2 is the working mode.
class SmsResponseParser {
public:
  static constexpr size_t kMaxRaw = 1400;  // raw text per message; divisible by 4

  SmsResponseParser() = default;
  SmsResponseParser(Charset cs, ParsedSms* out, size_t capacity) { reset(cs, out, capacity); }

  // The same instance for the next answer.
  void reset(Charset cs, ParsedSms* out, size_t capacity);

  Feed feedLine(const char* line);

  size_t count() const { return count_; }      // messages put away
  size_t dropped() const { return dropped_; }  // finished, but no room left: do not delete them
  bool finished() const { return finished_; }
  bool ok() const { return ok_; }

private:
  void startMessage(const char* line, bool isCmgl);
  void appendBody(const char* line);
  void finishMessage();

  Charset cs_ = Charset::Ucs2;
  ParsedSms* out_ = nullptr;
  size_t capacity_ = 0;
  size_t count_ = 0;
  size_t dropped_ = 0;
  bool finished_ = false;
  bool ok_ = false;

  bool inMessage_ = false;
  ParsedSms cur_;
  char raw_[kMaxRaw + 1] = {};
  size_t rawLen_ = 0;
  bool rawOverflow_ = false;
  bool firstBodyLine_ = true;
};

}  // namespace at
