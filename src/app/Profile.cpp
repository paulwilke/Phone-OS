#include "Profile.h"

#include "PhoneNumber.h"

bool Profile::mayCall(const std::string& number) const {
  if (contacts.isKnown(number.c_str())) return true;
  for (const std::string& target : callTargets) {
    if (phone::same(target.c_str(), number.c_str(), contacts.countryCode())) return true;
  }
  return false;
}

bool Profile::mayReceiveFrom(const std::string& number) const { return mayCall(number); }

std::string Profile::sosRecipient(const std::string& target) const {
  if (!target.empty()) return target;
  for (const std::string& t : callTargets) {
    if (!t.empty()) return t;
  }
  return std::string();
}
