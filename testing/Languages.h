#pragma once

// Runs a test body once per language, so that a layout is checked in every language the
// interface ships in and in the pseudo-language (src/i18n/Pseudo.h):
//
//   languages::forEach([](const std::string& language, i18n::Translator& t) {
//     // draw something with t.get("home.inbox"), compare, assert !fb.clipped()
//   });
//
// The languages are every catalog in lang/ (read from disk, like the device reads them, so a
// language added there is tested without touching any test) in the order of their file names,
// then the pseudo-language. The translator passed to the body has all of them loaded and the
// current one selected. A catalog that does not load fails the test with its file name.
//
// Run the tests from the repository root, as `pio test -e native` does.

#include <unity.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <i18n/Pseudo.h>
#include <i18n/Translator.h>

namespace languages {

// The codes forEach() visits, in order: the shipped languages, then the pseudo-language.
inline std::vector<std::string> codes(const std::string& dir = "lang") {
  std::vector<std::string> out;
  for (const auto& entry : std::filesystem::directory_iterator(dir))
    if (entry.path().extension() == ".toml") out.push_back(entry.path().stem().string());
  std::sort(out.begin(), out.end());
  TEST_ASSERT_TRUE_MESSAGE(!out.empty(), "no catalogs found: run the tests from the repository root");
  out.push_back(i18n::kPseudoCode);
  return out;
}

// A translator holding every shipped catalog and the pseudo-language made from English.
inline i18n::Translator loaded(const std::string& dir = "lang") {
  i18n::Translator translator;
  i18n::Catalog english;
  for (const std::string& code : codes(dir)) {
    if (code == i18n::kPseudoCode) continue;
    const std::string path = dir + "/" + code + ".toml";
    std::ifstream in(path);
    std::stringstream text;
    text << in.rdbuf();
    i18n::Catalog catalog;
    if (!catalog.parse(text.str().data(), text.str().size())) TEST_FAIL_MESSAGE(("cannot load " + path).c_str());
    translator.add(catalog);
    if (code == i18n::Translator::kReference) english = catalog;
  }
  TEST_ASSERT_TRUE_MESSAGE(english.size() > 0, "lang/en.toml is missing");
  translator.add(i18n::pseudoCatalog(english));
  return translator;
}

template <class Body>
void forEach(Body&& body, const std::string& dir = "lang") {
  i18n::Translator translator = loaded(dir);
  for (const std::string& code : codes(dir)) {
    TEST_ASSERT_TRUE_MESSAGE(translator.select(code), code.c_str());
    body(code, translator);
  }
}

}  // namespace languages
