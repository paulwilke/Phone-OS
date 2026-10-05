#include "Translator.h"

namespace i18n {

bool Translator::add(const Catalog& catalog) {
  if (catalog.code().empty()) return false;
  auto it = languages_.find(catalog.code());
  if (it == languages_.end()) languages_[catalog.code()] = catalog;
  else it->second.merge(catalog);
  return true;
}

bool Translator::addUser(const char* text, size_t length, std::vector<std::string>* errors) {
  Catalog cat;
  if (!cat.parse(text, length)) {
    if (errors) {
      for (const auto& e : cat.errors()) errors->push_back("line " + std::to_string(e.line) + ": " + e.message);
    }
    return false;
  }
  // Placeholders must match the reference, or a message would print "{name}" or crash the layout.
  auto ref = languages_.find(kReference);
  if (ref != languages_.end()) {
    bool bad = false;
    for (const auto& issue : cat.compare(ref->second)) {
      if (issue.what == "placeholders differ from the reference") {
        bad = true;
        if (errors) errors->push_back("`" + issue.key + "`: " + issue.what);
      }
    }
    if (bad) return false;
  }
  return add(cat);
}

bool Translator::select(const std::string& code) {
  if (languages_.count(code)) { current_ = code; return true; }
  const size_t dash = code.find('-');
  if (dash != std::string::npos && languages_.count(code.substr(0, dash))) {
    current_ = code.substr(0, dash);
    return true;
  }
  return false;
}

std::vector<LanguageInfo> Translator::languages() const {
  std::vector<LanguageInfo> out;
  for (const auto& kv : languages_) out.push_back({kv.first, kv.second.name(), kv.second.native()});
  return out;
}

const Entry* Translator::findIn(const std::string& code, const char* key, PluralRule* rule) {
  // The language itself, or its base ("fr-CA" -> "fr").
  auto cur = languages_.find(code);
  if (cur == languages_.end()) {
    const size_t dash = code.find('-');
    if (dash != std::string::npos) cur = languages_.find(code.substr(0, dash));
  }
  if (cur != languages_.end()) {
    if (const Entry* e = cur->second.find(key)) { *rule = cur->second.plural(); return e; }
  }
  auto ref = languages_.find(kReference);
  if (ref != languages_.end()) {
    if (const Entry* e = ref->second.find(key)) {
      ++fallbacks_;  // reached only when the language itself lacks the key
      *rule = ref->second.plural();
      return e;
    }
  }
  missing_.insert(key);
  return nullptr;
}

const char* Translator::text(const char* key) {
  PluralRule rule;
  const Entry* e = find(key, &rule);
  return e ? e->forms[0].c_str() : key;
}

std::string Translator::render(const Entry* e, PluralRule rule, const char* key, bool counted, uint32_t n,
                               const Arg* args, size_t argCount) {
  if (!e) return key;
  size_t idx = 0;
  std::vector<Arg> all(args, args + argCount);
  if (counted) {
    idx = pluralIndex(rule, n);
    if (idx >= e->forms.size()) idx = e->forms.size() - 1;  // a one-form message serves every count
    all.push_back(Arg("n", (int64_t)n));
  }
  char buf[512];
  format(e->forms[idx].c_str(), all.data(), all.size(), buf, sizeof(buf));
  return buf;
}

std::string Translator::get(const char* key, const Arg* args, size_t argCount) {
  return getIn(current_, key, args, argCount);
}

std::string Translator::plural(const char* key, uint32_t n, const Arg* args, size_t argCount) {
  return pluralIn(current_, key, n, args, argCount);
}

std::string Translator::getIn(const std::string& code, const char* key, const Arg* args, size_t argCount) {
  PluralRule rule = PluralRule::Single;
  const Entry* e = findIn(code, key, &rule);
  return render(e, rule, key, false, 0, args, argCount);
}

std::string Translator::pluralIn(const std::string& code, const char* key, uint32_t n, const Arg* args,
                                 size_t argCount) {
  PluralRule rule = PluralRule::Single;
  const Entry* e = findIn(code, key, &rule);
  return render(e, rule, key, true, n, args, argCount);
}

std::vector<std::string> Translator::textsInAllLanguages(const char* key) const {
  std::vector<std::string> out;
  for (const auto& kv : languages_) {
    const Entry* e = kv.second.find(key);
    if (!e) continue;  // a parsed entry always has a form
    bool seen = false;
    for (const std::string& t : out) seen = seen || t == e->forms[0];
    if (!seen) out.push_back(e->forms[0]);
  }
  return out;
}

}  // namespace i18n
