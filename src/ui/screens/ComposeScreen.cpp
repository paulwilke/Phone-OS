#include "ui/screens/ComposeScreen.h"

#include <string_view>

#include "ui/Fonts.h"
#include "ui/Text.h"
#include "ui/Wording.h"

namespace ui {

namespace {

// The ready-made messages, in the order they are offered. The keys start with `sms.` so that the
// catalog check holds them to the characters an SMS can carry.
const char* const kCannedKeys[] = {"sms.canned.on-my-way", "sms.canned.arrived", "sms.canned.call-me",
                                   "sms.canned.late", "sms.canned.ok", "sms.canned.thanks"};

void appendUtf8(std::string& out, uint32_t c) {
  if (c < 0x80) {
    out += static_cast<char>(c);
  } else if (c < 0x800) {
    out += static_cast<char>(0xC0 | (c >> 6));
    out += static_cast<char>(0x80 | (c & 0x3F));
  } else if (c < 0x10000) {
    out += static_cast<char>(0xE0 | (c >> 12));
    out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
    out += static_cast<char>(0x80 | (c & 0x3F));
  } else {
    out += static_cast<char>(0xF0 | (c >> 18));
    out += static_cast<char>(0x80 | ((c >> 12) & 0x3F));
    out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
    out += static_cast<char>(0x80 | (c & 0x3F));
  }
}

}  // namespace

size_t ComposeScreen::characters() const {
  size_t n = 0, pos = 0;
  while (pos < text_.size()) {
    text::decode(text_, pos);
    ++n;
  }
  return n;
}

size_t ComposeScreen::limit() const {
  for (unsigned char c : text_)
    if (c >= 0x80) return kMaxCharsWide;
  return kMaxChars;
}

void ComposeScreen::onEnter() {
  if (!session().recipient.empty()) return;
  // Nobody chosen (the key under the screen opens this page directly): the first person the child
  // may write to.
  const Profile& profile = context().app.profile();
  const std::string first = profile.sosRecipient(std::string());
  if (!first.empty()) session().recipient = first;
  else if (profile.contacts.count() > 0) session().recipient = profile.contacts.at(0).phone;
  session().viaRadio = false;
}

bool ComposeScreen::type(uint32_t c) {
  // Only what the display can draw: a control character, an emoji or a letter of another alphabet is
  // not taken.
  if (c < 0x20 || c == 0x7F || font(FontSize::Text).find(c) == nullptr) return false;
  std::string next = text_;
  appendUtf8(next, c);
  if (next.size() > kMaxBytes) return false;
  const std::string before = text_;
  text_ = next;
  const bool fits = characters() <= limit();
  if (!fits) text_ = before;   // the 71st character after a first accent is not taken either
  return fits;
}

void ComposeScreen::deleteLast() {
  if (text_.empty()) return;
  size_t pos = text_.size();
  do {
    --pos;
  } while (pos > 0 && (static_cast<unsigned char>(text_[pos]) & 0xC0) == 0x80);
  text_.erase(pos);
}

Rect ComposeScreen::recipientLine(const Rect& area) {
  const int h = font(FontSize::Text).lineHeight() + 4;
  return {area.x, area.y + font(FontSize::Small).lineHeight() + 2, area.w, h};
}

Rect ComposeScreen::cannedArea(const Rect& area) {
  const Font& small = font(FontSize::Small);
  const std::string title = t().get("compose.canned");
  const int top = recipientLine(area).bottom() + 4 +
                  static_cast<int>(text::wrap(small, title, area.w - 8).size()) * small.lineHeight() + 2;
  return {area.x, top, area.w, area.bottom() - top};
}

void ComposeScreen::fillCanned() {
  std::vector<ListItem> items;
  cannedBodies_.clear();
  for (const char* key : kCannedKeys) {
    items.push_back({t().get(key), false});
    // The words that go out are the parents' language, not the screen's.
    cannedBodies_.push_back(context().translator.getIn(context().app.profile().smsLanguage, key));
  }
  list_.setItems(std::move(items));
}

void ComposeScreen::draw(Display& display, const Rect& area) {
  const Font& small = font(FontSize::Small);
  const Font& body = font(FontSize::Text);
  const bool isCanned = canned();

  // The header: "To", and the counter when there is something to count.
  std::string counter;
  if (!isCanned) counter = std::to_string(characters()) + "/" + std::to_string(limit());
  const int counterWidth = counter.empty() ? 0 : text::drawRight(display, small, area.right() - 4, area.y, counter);
  const std::string to = t().get(session().viaRadio ? "compose.to-radio" : "compose.to");
  text::draw(display, small, area.x + 4, area.y, text::ellipsize(small, to, area.w - 12 - counterWidth));

  // The recipient: a name, or a number (never shortened), and a tap on it opens the choice.
  const Rect line = recipientLine(area);
  const std::string who = session().recipient.empty()
                              ? t().get("compose.unknown-to")
                              : wording::peerName(session().recipient, context().app.profile().contacts, t());
  const bool isNumber = !session().recipient.empty() && !session().viaRadio &&
                        context().app.profile().contacts.find(session().recipient.c_str()) == nullptr;
  text::draw(display, body, area.x + 4, line.y + 2, isNumber ? who : text::ellipsize(body, who, area.w - 8));
  display.fillRect({area.x, line.bottom(), area.w, 1}, true);

  int y = line.bottom() + 4;
  if (isCanned) {
    const std::string title = t().get("compose.canned");
    for (std::string_view l : text::wrap(small, title, area.w - 8)) {
      text::draw(display, small, area.x + 4, y, l);
      y += small.lineHeight();
    }
    fillCanned();
    list_.draw(display, cannedArea(area));
    return;
  }

  // The text, wrapped, with a bar after it as the cursor; the note below says why the limit fell.
  const bool wide = limit() == kMaxCharsWide;
  const int bottom = area.bottom() - (wide ? small.lineHeight() * 2 + 2 : 0);
  const std::string shown = text_ + "_";
  for (std::string_view l : text::wrap(body, shown, area.w - 8)) {
    if (y + body.lineHeight() > bottom) break;
    text::draw(display, body, area.x + 4, y, l);
    y += body.lineHeight();
  }
  if (wide) {
    const std::string note = t().get("compose.limit-note");
    int ny = area.bottom() - small.lineHeight() * 2;
    for (std::string_view l : text::wrap(small, note, area.w - 8)) {
      text::draw(display, small, area.x + 4, ny, l);
      ny += small.lineHeight();
    }
  }
}

SoftKeys ComposeScreen::keys() {
  SoftKeys keys;
  if (canned()) {
    keys.back = t().get("key.back");
    keys.scroll = true;
    keys.action = t().get("key.send");
    return keys;
  }
  keys.back = t().get(text_.empty() ? "key.back" : "key.delete");
  if (!text_.empty() && !session().recipient.empty()) keys.action = t().get("key.send");
  return keys;
}

bool ComposeScreen::send(const std::string& body) {
  if (session().recipient.empty() || body.empty()) return false;
  const bool sent = session().viaRadio ? app().sendRadio(session().recipient, body) : app().send(session().recipient, body);
  if (!sent) return false;   // the core said why (a full queue); the text stays
  text_.clear();
  session().clear();
  // Back to the start, not to the list the child came from.
  nav().home();
  return true;
}

bool ComposeScreen::onInput(const Input& input, const Rect& area) {
  if (input.kind == Input::Kind::Touch && recipientLine(area).contains(input.x, input.y) && !session().viaRadio) {
    nav().open(screen::kRecipient);
    return true;
  }
  if (canned()) {
    fillCanned();
    if (input.isKey(Key::Back)) {
      nav().back();
      return true;
    }
    bool chosen = false;
    const bool used = list_.handle(input, cannedArea(area), &chosen);
    if (chosen) send(cannedBodies_[list_.selected()]);
    return used;
  }
  if (input.kind == Input::Kind::Touch) return false;
  if (input.kind == Input::Kind::Text) {
    type(input.character);
    return true;
  }
  if (input.isKey(Key::Back)) {
    if (text_.empty()) nav().back();
    else deleteLast();
    return true;
  }
  if (input.isKey(Key::Select)) return send(text_);
  return false;
}

}  // namespace ui
