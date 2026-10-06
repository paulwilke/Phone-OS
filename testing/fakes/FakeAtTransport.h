#pragma once

// A scripted modem behind the AtTransport seam: a table of "command → answer" and unsolicited
// lines at chosen times, so that a whole start-up, a call or a restart is a readable script in
// the test and the driver is exercised through the real byte protocol.
//
// The modem answers what the driver writes, line by line:
//   reply(command, {lines…}, final = "OK")   the lines, then the final line ("OK", "ERROR",
//                                            "+CME ERROR: 30" …), each framed as the modem does
//                                            (CR LF line CR LF). Several replies to the same
//                                            command are used in turn; the last one stays, so a
//                                            poll that repeats keeps getting it.
//   replyLines(command, vector, final)      the same with the lines in a vector
//   replyAfter(delayMs, command, …)          the same, but the answer comes after the delay
//   clearReplies(command)                    forgets the scripted answers of a command (to script new ones)
//   silence(command)                         no answer at all (a hang, for timeout tests)
//   replyPrompt(command)                     "\r\n> " without a line end; the text that follows,
//                                            ended by Ctrl-Z (0x1A), is answered by the next
//                                            replyToBody(); Esc (0x1B) cancels without an answer
//   replyToBody(lines, final = "OK")        queued; one is used for each text that ends in Ctrl-Z
//   setDefaultBodyAnswer(lines, final)      what a text is answered with when none is queued (default: none)
// A command nobody scripted is answered "ERROR" and recorded in unexpected(), so a test can say
// that the driver sent nothing it should not have.
//
// Without a request:
//   unsolicited(atMs, line)                  a line (a URC) at the given time on this transport's
//                                            clock, "\r\n" framed
//   restart()                                the modem restarts now: answers still on the way are
//                                            lost, a partial command is forgotten, and
//                                            "*ATREADY: 1" arrives
// Echo: setEcho(true) makes the modem repeat each command line, as before ATE0; the script's
// "ATE0" turns it off.
// The supply: power(true/false) is recorded (powered()); a pulse on the power key (pwrkeyPulses())
// wakes a modem that was set cold with setCold(true), which ignores everything until then, as a
// modem does that has never been started; a pulse on a running modem switches it off, as it does.
// The sound path: setAudioToModem() is recorded (audioToModem(), audioSwitches()); so is the sleep
// line (dtr(), true = high = the modem may sleep).
// Time: advance(ms) moves the clock (setNow() starts it anywhere, to test the wrap).
// Reading back: powered(), pwrkeyPulses(), audioToModem(), dtr(), escapes() (Escape bytes received: a cancelled text), sent() (every command line, without its line end), bodies() (every text sent
// after a prompt, without the Ctrl-Z), unexpected(), pendingOutput() (bytes not read yet).

#include <cstdint>
#include <deque>
#include <initializer_list>
#include <map>
#include <string>
#include <vector>

#include <drivers/AtTransport.h>

class FakeAtTransport : public AtTransport {
public:
  void write(const char* data, size_t len) override {
    for (size_t i = 0; i < len; ++i) takeByte(data[i]);
  }

  int read() override {
    deliverDue();
    if (out_.empty()) return -1;
    const unsigned char c = (unsigned char)out_.front();
    out_.pop_front();
    return c;
  }

  uint32_t now() override { return now_; }

  void power(bool on) override { powered_ = on; }
  void setDtr(bool high) override { dtr_ = high; }
  void setAudioToModem(bool on) override {
    audio_ = on;
    ++audioSwitches_;
  }
  void pwrkeyPulse() override {
    ++pulses_;
    if (cold_) {
      cold_ = false;
      const std::string ready = "\r\n*ATREADY: 1\r\n";
      out_.insert(out_.end(), ready.begin(), ready.end());
    } else {
      cold_ = true;  // a pulse on a running modem switches it off
      out_.clear();
    }
  }

  // Scripting.
  void reply(const std::string& command, std::initializer_list<std::string> lines, const std::string& final = "OK") {
    queue(command, 0, frame(lines, final), false);
  }
  // The same with the lines in a vector (an answer whose length the test decides).
  void replyLines(const std::string& command, const std::vector<std::string>& lines, const std::string& final = "OK") {
    queue(command, 0, frame(lines, final), false);
  }
  void replyAfter(uint32_t delayMs, const std::string& command, std::initializer_list<std::string> lines,
                  const std::string& final = "OK") {
    queue(command, delayMs, frame(lines, final), false);
  }
  void silence(const std::string& command) { queue(command, 0, std::string(), false); }
  void replyPrompt(const std::string& command) { queue(command, 0, "\r\n> ", true); }
  void replyToBody(std::initializer_list<std::string> lines, const std::string& final = "OK") {
    bodyAnswers_.push_back(frame(lines, final));
  }
  void setDefaultBodyAnswer(std::initializer_list<std::string> lines, const std::string& final = "OK") {
    defaultBodyAnswer_ = frame(lines, final);
  }
  void unsolicited(uint32_t atMs, const std::string& line) { timed_.push_back({atMs, "\r\n" + line + "\r\n"}); }
  void restart() {
    out_.clear();
    timed_.clear();
    line_.clear();
    inBody_ = false;
    const std::string ready = "\r\n*ATREADY: 1\r\n";
    out_.insert(out_.end(), ready.begin(), ready.end());
  }
  // Forgets what was scripted for a command, so that a test can change the world while the modem runs.
  void clearReplies(const std::string& command) { answers_.erase(command); }
  void setEcho(bool on) { echo_ = on; }
  void setCold(bool cold) { cold_ = cold; }
  void advance(uint32_t ms) { now_ += ms; }
  void setNow(uint32_t ms) { now_ = ms; }

  const std::vector<std::string>& sent() const { return sent_; }
  const std::vector<std::string>& bodies() const { return bodies_; }
  const std::vector<std::string>& unexpected() const { return unexpected_; }
  size_t pendingOutput() const { return out_.size(); }
  bool powered() const { return powered_; }
  int escapes() const { return escapes_; }
  bool audioToModem() const { return audio_; }
  bool dtr() const { return dtr_; }
  int audioSwitches() const { return audioSwitches_; }
  int pwrkeyPulses() const { return pulses_; }

private:
  struct Answer {
    uint32_t delayMs;
    std::string bytes;
    bool prompt;
  };
  struct Timed {
    uint32_t atMs;
    std::string bytes;
  };

  static std::string frame(std::initializer_list<std::string> lines, const std::string& final) {
    return frame(std::vector<std::string>(lines), final);
  }
  static std::string frame(const std::vector<std::string>& lines, const std::string& final) {
    // The lines of one answer follow each other without a gap; the final line has one before it.
    std::string bytes;
    bool first = true;
    for (const std::string& l : lines) {
      bytes += first ? "\r\n" : "";
      first = false;
      bytes += l + "\r\n";
    }
    if (!final.empty()) bytes += "\r\n" + final + "\r\n";
    return bytes;
  }
  void queue(const std::string& command, uint32_t delayMs, const std::string& bytes, bool prompt) {
    answers_[command].push_back({delayMs, bytes, prompt});
  }
  // What is due moves to the output, in the order it was scripted.
  void deliverDue() {
    for (size_t i = 0; i < timed_.size();) {
      if ((int32_t)(now_ - timed_[i].atMs) >= 0) {
        out_.insert(out_.end(), timed_[i].bytes.begin(), timed_[i].bytes.end());
        timed_.erase(timed_.begin() + (long)i);
      } else {
        ++i;
      }
    }
  }
  void emit(const Answer& a) {
    if (a.delayMs == 0) out_.insert(out_.end(), a.bytes.begin(), a.bytes.end());
    else timed_.push_back({now_ + a.delayMs, a.bytes});
  }

  void takeByte(char c) {
    if (inBody_) {
      if (skipLf_ && c == '\n') {
        skipLf_ = false;
        return;
      }
      skipLf_ = false;
      if (c == 0x1A) {
        bodies_.push_back(line_);
        line_.clear();
        inBody_ = false;
        if (!bodyAnswers_.empty()) {
          out_.insert(out_.end(), bodyAnswers_.front().begin(), bodyAnswers_.front().end());
          bodyAnswers_.pop_front();
        } else {
          out_.insert(out_.end(), defaultBodyAnswer_.begin(), defaultBodyAnswer_.end());
        }
      } else if (c == 0x1B) {
        ++escapes_;
        line_.clear();
        inBody_ = false;
      } else {
        line_ += c;
      }
      return;
    }
    if (c == '\n' && skipLf_) {  // the LF behind the CR of a command line is part of its end
      skipLf_ = false;
      return;
    }
    skipLf_ = false;
    if (c == 0x1A || c == 0x1B) {  // Ctrl-Z and Escape mean nothing outside a text, but an Escape is counted
      if (c == 0x1B) ++escapes_;
      return;
    }
    if (c != '\r' && c != '\n') {
      line_ += c;
      return;
    }
    if (line_.empty()) return;
    const std::string command = line_;
    line_.clear();
    skipLf_ = (c == '\r');
    sent_.push_back(command);
    if (cold_ || !powered_) return;   // nobody is listening
    if (echo_) {
      out_.insert(out_.end(), command.begin(), command.end());
      out_.push_back('\r');
      out_.push_back('\n');
    }
    if (command == "ATE0") echo_ = false;

    auto it = answers_.find(command);
    if (it == answers_.end()) {
      unexpected_.push_back(command);
      const std::string err = "\r\nERROR\r\n";
      out_.insert(out_.end(), err.begin(), err.end());
      return;
    }
    // The last answer stays; earlier ones are used once.
    const Answer a = it->second.front();
    if (it->second.size() > 1) it->second.pop_front();
    emit(a);
    if (a.prompt) inBody_ = true;
  }

  uint32_t now_ = 0;
  bool echo_ = false;
  bool cold_ = false;
  bool powered_ = true;  // a bench modem is on until the driver says otherwise
  int pulses_ = 0;
  bool audio_ = false;
  bool dtr_ = false;
  int escapes_ = 0;
  int audioSwitches_ = 0;
  bool inBody_ = false;
  bool skipLf_ = false;
  std::string line_;
  std::deque<char> out_;
  std::vector<Timed> timed_;
  std::map<std::string, std::deque<Answer>> answers_;
  std::deque<std::string> bodyAnswers_;
  std::string defaultBodyAnswer_;
  std::vector<std::string> sent_, bodies_, unexpected_;
};
