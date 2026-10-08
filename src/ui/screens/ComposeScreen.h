#pragma once

// Writing a message to the person in the session (`Session::recipient`), by SMS or, when the session
// says so, over the radio. Sending goes out at once; the core says where it went in the notice line
// ("Message is out", "Message waits for network"), and the page goes home. When the core cannot take
// the message (the queue is full) the text stays.
//
// **Free text is the profile's switch** (`Profile::freeText`). Without it the page offers a short
// list of ready-made messages instead, shown in the child's language and sent in the *parents'*
// (`Profile::smsLanguage`), because the person who reads an SMS is the parent. Choosing one sends it.
// With free text the child types at once, and the ready-made messages are one key away: while nothing
// is typed the action key is "Templates", which opens the same list; Back, or typing a letter, goes back
// to the text (owner, 2026-10-08: with free text the ready-made messages were gone).
//
// **The character set.** The child can type what the display can draw: Latin-1, the euro sign, `Œ œ Ÿ`
// and the interface symbols ([ui-text.md](ui-text.md)); anything else is not taken, because the child
// could not read what was sent. Received text outside the set is shown as the font's box. The counter
// counts characters, not bytes: 160 while everything is plain ASCII, **70** as soon as one character is
// not (an SMS with such a character is sent in a wider coding), and the page says why the limit fell. A
// character that does not fit is not taken; nothing is cut. The history keeps at most 160 bytes of a
// text, so that is the second limit.
//
// A draft stays while the page is left (to choose another recipient) and is gone when it is sent. With
// no recipient chosen the page starts with the first call target (or the first contact), and says so
// when there is nobody.
//
// School time does not stop sending: a message to the parents is not what school time is about.

#include <string>

#include "ui/AppScreen.h"
#include "ui/ListView.h"

namespace ui {

class ComposeScreen : public AppScreen {
public:
  using AppScreen::AppScreen;

  static constexpr size_t kMaxChars = 160;     // plain text
  static constexpr size_t kMaxCharsWide = 70;  // with any character outside ASCII
  static constexpr size_t kMaxBytes = 160;     // what the history keeps

  void onEnter() override;

  const std::string& text() const { return text_; }
  size_t characters() const;
  size_t limit() const;

protected:
  void draw(Display& display, const Rect& area) override;
  SoftKeys keys() override;
  bool onInput(const Input& input, const Rect& area) override;

private:
  bool canned() { return !context().app.profile().freeText || templatesShown_; }
  bool type(uint32_t codePoint);
  void deleteLast();
  bool send(const std::string& body);
  void fillCanned();
  Rect recipientLine(const Rect& area);
  // Where the list of ready-made messages is, under the recipient and its title.
  Rect cannedArea(const Rect& area);

  std::string text_;
  bool templatesShown_ = false;  // with free text: the list was opened with the key "Templates"
  ListView list_;
  std::vector<std::string> cannedBodies_;
};

}  // namespace ui
