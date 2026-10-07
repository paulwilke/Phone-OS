#pragma once

// Builds a Profile for a test with defaults that suit most of them: the usual three contacts
// and one call target. Configuration is read into the Profile by a later package; until then
// this is how a test says "a profile in which ...".
//
//   Profile p = ProfileBuilder().schoolTime("tue", "14:00", "15:00").radio(true).build();
//
// The numbers and names are invented.

#include <string>
#include <vector>

#include <app/Profile.h>

namespace people {
constexpr const char* kParentOne = "+491701111111";   // a parent
constexpr const char* kParentTwo = "+491702222222";   // a parent
constexpr const char* kGrandma = "+493012345678";     // on the list, not a parent
constexpr const char* kStranger = "+491709999999";    // on no list
}  // namespace people

class ProfileBuilder {
public:
  // The contacts of most tests: two parents and Grandma; the first parent is the one call
  // action, so he is also who the SOS button reaches.
  ProfileBuilder() {
    contact(people::kParentOne, "Parent One", true);
    contact(people::kParentTwo, "Parent Two", true);
    contact(people::kGrandma, "Grandma", false);
    callTargets_ = {people::kParentOne};
  }

  ProfileBuilder& noContacts() {
    contacts_.clear();
    callTargets_.clear();
    return *this;
  }
  ProfileBuilder& contact(const std::string& phone, const std::string& name, bool admin) {
    contacts_ += phone + (admin ? " admin " : " ") + name + "\n";
    return *this;
  }
  ProfileBuilder& callTargets(std::vector<std::string> targets) {
    callTargets_ = std::move(targets);
    return *this;
  }
  ProfileBuilder& schoolTime(const char* days, const char* from, const char* to, bool enabled = true) {
    windows_.push_back({days, from, to, enabled});
    return *this;
  }
  ProfileBuilder& radio(bool allowed) {
    profile_.radioAllowed = allowed;
    return *this;
  }
  // The one radio node whose messages are accepted (`[radio] peer`).
  ProfileBuilder& radioPeer(const char* name) {
    profile_.radioPeer = name;
    return *this;
  }
  ProfileBuilder& freeText(bool allowed) {
    profile_.freeText = allowed;
    return *this;
  }
  ProfileBuilder& parentTools(bool shown) {
    profile_.parentTools = shown;
    return *this;
  }
  ProfileBuilder& messageVibration(bool on) {
    profile_.messageVibration = on;
    return *this;
  }
  ProfileBuilder& callVibration(bool on) {
    profile_.callVibration = on;
    return *this;
  }
  ProfileBuilder& quietCallVibration(bool on) {
    profile_.quietCallVibration = on;
    return *this;
  }
  ProfileBuilder& modemSleep(bool allowed) {
    profile_.modemSleep = allowed;
    return *this;
  }
  ProfileBuilder& childName(const std::string& name) {
    profile_.childName = name;
    return *this;
  }
  ProfileBuilder& smsLanguage(const std::string& code) {
    profile_.smsLanguage = code;
    return *this;
  }
  ProfileBuilder& modemApn(const std::string& apn) {
    profile_.modemApn = apn;
    return *this;
  }
  ProfileBuilder& mapTemplate(const std::string& tmpl) {
    profile_.mapTemplate = tmpl;
    return *this;
  }
  ProfileBuilder& commandNotice(CommandNotice notice) {
    profile_.commandNotice = notice;
    return *this;
  }
  ProfileBuilder& extraTrigger(const std::string& word) {
    profile_.extraTriggers.push_back(word);
    return *this;
  }
  ProfileBuilder& logLevel(loglevel::Level level) {
    profile_.logLevel = level;
    return *this;
  }
  ProfileBuilder& timeZone(const char* name) {
    profile_.timeZone = tz::find(name);
    return *this;
  }

  Profile build() const {
    Profile p = profile_;
    p.contacts.load(contacts_.data(), contacts_.size());
    p.callTargets = callTargets_;
    for (const Window& w : windows_) p.quietHours.add(w.days, w.from, w.to, w.enabled);
    return p;
  }

private:
  struct Window {
    const char *days, *from, *to;
    bool enabled;
  };
  Profile profile_;
  std::string contacts_;
  std::vector<std::string> callTargets_;
  std::vector<Window> windows_;
};
