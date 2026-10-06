#include "ui/Formats.h"

#include <cmath>

namespace ui {
namespace format {

namespace {

// `value` times 10^digits, rounded half away from zero, as a decimal text with `digits` places.
std::string fixed(double value, int digits) {
  if (!std::isfinite(value)) return "--";
  double scale = 1;
  for (int i = 0; i < digits; ++i) scale *= 10;
  const bool minus = value < 0;
  const long long total = static_cast<long long>(std::fabs(value) * scale + 0.5);
  const long long whole = total / static_cast<long long>(scale);
  std::string fraction = std::to_string(total % static_cast<long long>(scale));
  fraction.insert(0, static_cast<size_t>(digits) - fraction.size(), '0');
  const bool zero = total == 0;
  return std::string(minus && !zero ? "-" : "") + std::to_string(whole) + "." + fraction;
}

}  // namespace

std::string degrees(double value) { return fixed(value, 5); }

std::string tenths(float value) { return fixed(static_cast<double>(value), 1); }

std::string age(uint32_t seconds, i18n::Translator& t) {
  if (seconds < 90) return t.get("age.now");
  if (seconds < 3600) return t.get("age.minutes", {{"n", static_cast<int>(seconds / 60)}});
  if (seconds < 86400) return t.get("age.hours", {{"n", static_cast<int>(seconds / 3600)}});
  return t.plural("age.days", seconds / 86400);
}

std::string ageUnknown(i18n::Translator& t) { return t.get("age.unknown"); }

std::string ageSince(uint32_t lastSeen, uint32_t now, i18n::Translator& t) {
  if (lastSeen == 0 || now == 0 || now < lastSeen) return ageUnknown(t);
  return age(now - lastSeen, t);
}

}  // namespace format
}  // namespace ui
