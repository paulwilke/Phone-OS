#pragma once

// What the screens of one task hand to each other: the screen that asks whom a message goes to
// leaves the answer here, and the screen that writes it reads it. The application owns one Session
// and gives it to every screen with the ScreenContext; a screen never holds a pointer to another.
//
// It is plain data and holds no words. Nothing in it survives a restart, and nothing in it is a
// setting: what the child may do is the profile's.

#include <string>

namespace ui {

struct Session {
  // The one a message is being written to: a phone number, or the name of a radio contact.
  std::string recipient;
  // The recipient is a radio contact and the message goes over the radio.
  bool viaRadio = false;

  void clear() {
    recipient.clear();
    viaRadio = false;
  }
};

}  // namespace ui
