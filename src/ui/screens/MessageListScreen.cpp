#include "ui/screens/MessageListScreen.h"

#include <string_view>

#include "ui/Fonts.h"
#include "ui/Shapes.h"
#include "ui/Symbols.h"
#include "ui/Text.h"
#include "ui/Wording.h"

namespace ui {

namespace {

constexpr int kRowGap = 2;

// The marks that say what became of an outgoing message.
std::string deliveryMarks(const Message& m, i18n::Translator& t) {
  if (m.dir != MsgDir::Out) return std::string();
  if (m.pending) return " " + t.get("messages.waiting");
  if (m.delivery == MsgDelivery::Confirmed) return " " + t.get("messages.delivered");
  if (m.delivery == MsgDelivery::Unconfirmed) return " " + t.get("messages.unconfirmed");
  return std::string();
}

}  // namespace

void MessageListScreen::onEnter() {
  mode_ = Mode::List;
  sent_ = false;
  selection_ = 0;
  first_ = 0;
}

// Newest first, the ones of the list on show.
std::vector<const Message*> MessageListScreen::visible() const {
  const MessageStore& store = context().app.history();
  std::vector<const Message*> out;
  for (size_t i = store.count(); i-- > 0;)
    if ((store.at(i).dir == MsgDir::Out) == sent_) out.push_back(&store.at(i));
  return out;
}

const Message* MessageListScreen::selected() const {
  const std::vector<const Message*> list = visible();
  if (list.empty()) return nullptr;
  return list[selection_ < list.size() ? selection_ : list.size() - 1];
}

void MessageListScreen::clampSelection() {
  const size_t n = visible().size();
  if (selection_ >= n) selection_ = n ? n - 1 : 0;
  if (first_ > selection_) first_ = selection_;
}

MessageListScreen::Reply MessageListScreen::replyFor(const Message& m) const {
  if (m.dir != MsgDir::In || m.peer[0] == '\0') return Reply::None;
  const Profile& profile = context().app.profile();
  if (m.source == MsgSource::System) return profile.mayCall(m.peer) ? Reply::Call : Reply::None;
  // Over the radio the peer is a name, and the radio's contacts are not the book's.
  if (m.source == MsgSource::Radio) return profile.freeText ? Reply::Write : Reply::None;
  return (profile.freeText || profile.contacts.find(m.peer) != nullptr) ? Reply::Write : Reply::None;
}

// "✉ Parent One  14:32 (waiting)": the marks, the name (shortened when the line is too long), the time.
std::string MessageListScreen::head(const Message& m, bool forRow, int width) {
  const Font& small = font(FontSize::Small);
  const AppCore::State::WallClock at = app().wallClockOf(m.time);
  const std::string time = wording::messageTime(at, state().wall, t());
  const std::string marks = deliveryMarks(m, t());
  std::string lead;
  if (forRow) {
    if (!m.read && m.dir == MsgDir::In) lead += std::string(symbol::kEnvelope) + " ";
    if (m.source == MsgSource::Radio) lead += "~ ";
    if (m.source == MsgSource::System) lead += "! ";
  }
  if (m.source == MsgSource::System) return lead + time;
  const phone::ContactBook& book = context().app.profile().contacts;
  const std::string name = m.dir == MsgDir::In ? wording::peerName(m.peer, book, t()) : t().get("messages.you");
  const bool isNumber = m.dir == MsgDir::In && book.find(m.peer) == nullptr && m.peer[0] != '\0' && m.source != MsgSource::Radio;
  const std::string rest = "  " + time + marks;
  const int room = width - 8 - text::width(small, lead + rest);
  return lead + (isNumber ? name : text::ellipsize(small, name, room)) + rest;
}

std::string MessageListScreen::body(const Message& m) {
  if (m.source == MsgSource::System) return wording::systemEntry(m, context().app.profile().contacts, t());
  return m.text;
}

int MessageListScreen::rowHeight() const {
  return font(FontSize::Small).lineHeight() + font(FontSize::Text).lineHeight() + kRowGap;
}

int MessageListScreen::headerHeight(const Rect& area) {
  const std::string title = t().get(sent_ ? "messages.view-sent" : "messages.view-inbox");
  return static_cast<int>(text::wrap(font(FontSize::Small), title, area.w - 8).size()) * font(FontSize::Small).lineHeight() + 4;
}

size_t MessageListScreen::rowsOnShow(const Rect& area) {
  const int rows = (area.h - headerHeight(area)) / rowHeight();
  return rows > 0 ? static_cast<size_t>(rows) : 1;
}

// "14:32 from Parent One" with the marks of an outgoing message; the name is shortened inside the
// line, a number is not.
std::string MessageListScreen::detailHead(const Message& m, int width) {
  const Font& small = font(FontSize::Small);
  const std::string time = wording::messageTime(app().wallClockOf(m.time), state().wall, t());
  if (m.source == MsgSource::System) return time;
  const phone::ContactBook& book = context().app.profile().contacts;
  const char* key = m.dir == MsgDir::In ? "messages.from" : "messages.to";
  std::string name = wording::peerName(m.peer, book, t());
  const std::string marks = deliveryMarks(m, t());
  std::string line = t().get(key, {{"time", time.c_str()}, {"name", name.c_str()}});
  const bool isNumber = book.find(m.peer) == nullptr && m.peer[0] != '\0' && m.source != MsgSource::Radio;
  if (!isNumber && text::width(small, line + marks) > width - 8) {
    const int room = width - 8 - text::width(small, t().get(key, {{"time", time.c_str()}, {"name", ""}}) + marks);
    name = text::ellipsize(small, name, room);
    line = t().get(key, {{"time", time.c_str()}, {"name", name.c_str()}});
  }
  return line + marks;
}

// The text of a message lies between its head and the hint about deleting.
Rect MessageListScreen::textArea(const Rect& area, const Message& m) {
  const Font& small = font(FontSize::Small);
  const int headHeight = static_cast<int>(text::wrap(small, detailHead(m, area.w), area.w - 8).size()) * small.lineHeight() + 4;
  const std::string hint = t().get("messages.delete-hint");
  const int hintHeight = static_cast<int>(text::wrap(small, hint, area.w - 8).size()) * small.lineHeight();
  return {area.x, area.y + headHeight, area.w, area.h - headHeight - hintHeight - 2};
}

void MessageListScreen::draw(Display& display, const Rect& area) {
  const Font& small = font(FontSize::Small);
  const Font& body_ = font(FontSize::Text);
  const std::vector<const Message*> list = visible();
  clampSelection();
  if (mode_ != Mode::List && selected() == nullptr) mode_ = Mode::List;

  if (mode_ == Mode::Delete) {
    const std::string title = t().get("messages.delete-title");
    int y = area.y + 8;
    for (std::string_view line : text::wrap(body_, title, area.w - 8)) {
      text::draw(display, body_, area.x + 4, y, line);
      y += body_.lineHeight();
    }
    y += 6;
    const Message& m = *selected();
    const std::string who = m.source == MsgSource::System ? body(m) : head(m, false, area.w);
    for (std::string_view line : text::wrap(small, who, area.w - 8)) {
      text::draw(display, small, area.x + 4, y, line);
      y += small.lineHeight();
    }
    return;
  }

  if (mode_ == Mode::Message) {
    const Message& m = *selected();
    int y = area.y;
    const std::string headline = detailHead(m, area.w);   // the lines are views into it
    for (std::string_view l : text::wrap(small, headline, area.w - 8)) {
      text::draw(display, small, area.x + 4, y, l);
      y += small.lineHeight();
    }
    const Rect textRect = textArea(area, m);
    const std::string words = body(m);
    if (words != worded_) {
      text_.setText(words);
      worded_ = words;
    }
    text_.draw(display, textRect);
    const std::string hint = t().get("messages.delete-hint");
    int hy = textRect.bottom() + 2;
    for (std::string_view l : text::wrap(small, hint, area.w - 8)) {
      text::draw(display, small, area.x + 4, hy, l);
      hy += small.lineHeight();
    }
    return;
  }

  // The list: a header that says which list, and how to change.
  const std::string title = t().get(sent_ ? "messages.view-sent" : "messages.view-inbox");
  int y = area.y;
  for (std::string_view l : text::wrap(small, title, area.w - 8)) {
    text::draw(display, small, area.x + 4, y, l);
    y += small.lineHeight();
  }
  display.fillRect({area.x, y + 1, area.w, 1}, true);
  y = area.y + headerHeight(area);

  if (list.empty()) {
    text::draw(display, body_, area.x + 6, y + 4, t().get(sent_ ? "messages.none-sent" : "messages.none"));
    return;
  }
  const size_t rows = rowsOnShow(area);
  if (selection_ < first_) first_ = selection_;
  if (selection_ >= first_ + rows) first_ = selection_ + 1 - rows;
  for (size_t i = first_; i < list.size() && i < first_ + rows; ++i) {
    const Message& m = *list[i];
    const Rect row{area.x, y, area.w, rowHeight()};
    const bool isSelected = i == selection_;
    if (isSelected) display.fillRect({row.x, row.y, row.w, row.h - 1}, true);
    text::draw(display, small, row.x + 4, row.y, head(m, true, row.w), !isSelected);
    // The start of the text is the user's words: shortened with dots when too long.
    const std::string b = body(m);
    text::draw(display, body_, row.x + 4, row.y + small.lineHeight(), text::ellipsize(body_, b, row.w - 8), !isSelected);
    if (!isSelected) display.fillRect({row.x, row.bottom() - 1, row.w, 1}, true);
    y += rowHeight();
  }
}

SoftKeys MessageListScreen::keys() {
  SoftKeys keys;
  keys.back = t().get(mode_ == Mode::Delete ? "messages.keep" : "key.back");
  const std::vector<const Message*> list = visible();
  switch (mode_) {
    case Mode::List:
      keys.scroll = list.size() > 1;
      if (!list.empty()) keys.action = t().get("messages.read");
      break;
    case Mode::Message:
      keys.scroll = true;
      if (const Message* m = selected()) {
        const Reply r = replyFor(*m);
        if (r == Reply::Write) keys.action = t().get("messages.reply");
        if (r == Reply::Call) keys.action = t().get("messages.call-back");
      }
      break;
    case Mode::Delete: keys.action = t().get("key.delete"); break;
  }
  return keys;
}

void MessageListScreen::open() {
  const Message* m = selected();
  if (!m) return;
  mode_ = Mode::Message;
  app().markAllRead();
  worded_ = body(*m);
  text_.setText(worded_);
}

void MessageListScreen::remove() {
  if (const Message* m = selected()) app().deleteMessage(m->seq);
  mode_ = Mode::List;
  clampSelection();
}

bool MessageListScreen::onInput(const Input& input, const Rect& area) {
  const std::vector<const Message*> list = visible();
  const bool hasX = input.kind == Input::Kind::Text && (input.character == 'x' || input.character == 'X');
  const bool hasO = input.kind == Input::Kind::Text && (input.character == 'o' || input.character == 'O');

  if (mode_ == Mode::Delete) {
    if (input.isKey(Key::Select)) {
      remove();
      return true;
    }
    if (input.isKey(Key::Back)) {
      mode_ = before_;
      return true;
    }
    return false;
  }

  if (hasX && !list.empty()) {
    before_ = mode_;
    mode_ = Mode::Delete;
    return true;
  }

  if (mode_ == Mode::Message) {
    if (input.isKey(Key::Back)) {
      mode_ = Mode::List;
      return true;
    }
    if (input.isKey(Key::Select)) {
      const Message* m = selected();
      if (!m) return false;
      switch (replyFor(*m)) {
        case Reply::Write:
          session().recipient = m->peer;
          session().viaRadio = m->source == MsgSource::Radio;
          nav().open(screen::kCompose);
          return true;
        case Reply::Call:
          if (app().dial(m->peer)) nav().open(screen::kCall);
          return true;
        case Reply::None: return false;
      }
    }
    const Message* shown = selected();
    return shown && text_.handle(input, textArea(area, *shown));
  }

  // The list.
  if (hasO || (input.kind == Input::Kind::Touch && input.y < area.y + headerHeight(area))) {
    sent_ = !sent_;
    selection_ = 0;
    first_ = 0;
    return true;
  }
  if (input.isKey(Key::Back)) {
    nav().back();
    return true;
  }
  if (list.empty()) return false;
  if (input.isKey(Key::Up)) {
    if (selection_ > 0) --selection_;
    return true;
  }
  if (input.isKey(Key::Down)) {
    if (selection_ + 1 < list.size()) ++selection_;
    return true;
  }
  if (input.isKey(Key::Select)) {
    open();
    return true;
  }
  if (input.kind == Input::Kind::Touch) {
    const int top = area.y + headerHeight(area);
    if (input.y < top) return false;
    const size_t index = first_ + static_cast<size_t>((input.y - top) / rowHeight());
    if (index >= list.size() || (input.y - top) / rowHeight() >= static_cast<int>(rowsOnShow(area))) return false;
    selection_ = index;
    open();
    return true;
  }
  return false;
}

}  // namespace ui
