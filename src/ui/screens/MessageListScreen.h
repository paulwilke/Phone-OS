#pragma once

// The history: what came in and what went out, one list each, and one message at a time to read.
//
//   list      two lines a row: who and when (and the marks), under it the start of the text
//   message   the whole text, wrapped, scrolled by the arrows, with Reply or Call back
//   delete    "Delete this message?" with Keep and Delete
//
// **Entries of the device are worded from codes** (`wording::systemEntry`), so a missed call
// yesterday reads in today's language, and a contact renamed since is renamed in it. They are marked
// "!", radio messages "~", and what has not been read yet "✉".
//
// What the marks of an outgoing message say: "(waiting)" while it sits in the queue; over the radio,
// "arrived" or "unconfirmed" when the network said so; an SMS gets no answer from the network, and
// nothing is written for it.
//
// The time is that of the clock's zone: "14:32" for today, the date in front of it for an earlier day,
// "--:--" for a message that came before the time was known. A name too long for its line is shortened
// with "..."; a number never is.
//
// **Reply** is offered for a message that came in from someone the child may write to (a contact, or
// anybody where the profile allows free recipients), over the radio if it came by radio. **Call back**
// is offered for an entry of the device about a number the child may call (a missed call). Neither
// is offered for anything else.
//
// Opening a message marks the history read (the store has one mark, "everything up to here").
// The letters O (the other list) and X (delete) are shortcuts on the keyboard, written in the hints.

#include <vector>

#include "ui/AppScreen.h"
#include "ui/TextBlock.h"

namespace ui {

class MessageListScreen : public AppScreen {
public:
  using AppScreen::AppScreen;

  void onEnter() override;

  // For tests: the state of the page.
  enum class Mode { List, Message, Delete };
  Mode mode() const { return mode_; }
  bool showingSent() const { return sent_; }

protected:
  void draw(Display& display, const Rect& area) override;
  SoftKeys keys() override;
  bool onInput(const Input& input, const Rect& area) override;
  std::vector<Rect> targets(const Rect& area) override;

private:
  enum class Reply { None, Write, Call };

  std::vector<const Message*> visible() const;
  const Message* selected() const;
  Reply replyFor(const Message& message) const;
  std::string head(const Message& message, bool forRow, int width);
  std::string detailHead(const Message& message, int width);
  Rect textArea(const Rect& area, const Message& message);
  std::string body(const Message& message);
  int headerHeight(const Rect& area);
  int rowHeight() const;
  size_t rowsOnShow(const Rect& area);
  void clampSelection();
  void open();
  void remove();

  Mode mode_ = Mode::List;
  Mode before_ = Mode::List;  // where "Keep" returns to
  bool sent_ = false;
  size_t selection_ = 0;
  size_t first_ = 0;
  TextBlock text_;
  // The words `text_` holds: asked again at every frame, because the language may have changed since
  // the message was opened and an entry of the device is worded when it is drawn.
  std::string worded_;
};

}  // namespace ui
