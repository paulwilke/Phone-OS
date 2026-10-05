// Host tests for the widgets: soft keys, status bar, alert bar, tile grid, list view, text
// block, button and the shapes they share. Layouts run in every language (languages::forEach);
// pictures are kept in English and in the pseudo-language, clipping is asserted in all.
// allow-non-english: the catalogs under test contain German and French.

#include <unity.h>

#include <fstream>
#include <set>
#include <sstream>

#include <Languages.h>
#include <ui/Button.h>
#include <ui/Layout.h>
#include <ui/ListView.h>
#include <ui/Shapes.h>
#include <ui/SoftKeys.h>
#include <ui/StatusBar.h>
#include <ui/Symbols.h>
#include <ui/TextBlock.h>
#include <ui/Text.h>
#include <ui/TileGrid.h>

using namespace ui;
using i18n::Translator;

void setUp() {}
void tearDown() {}

namespace {

// A failure message that names the language and what clipped.
std::string clips(const std::string& language, const FrameBuffer& fb) { return language + ":\n" + fb.describeClips(); }

}  // namespace

// ---- layout --------------------------------------------------------------------------------

void test_the_standard_layout_tiles_the_screen_from_top_to_bottom() {
  const ScreenLayout l = ScreenLayout::standard();
  TEST_ASSERT_TRUE(l.status == (Rect{0, 0, 240, kStatusBarHeight}));
  TEST_ASSERT_EQUAL(l.status.bottom(), l.alert.y);
  TEST_ASSERT_EQUAL(l.alert.bottom() + kContentGap, l.content.y);
  TEST_ASSERT_EQUAL(l.softKeys.y - kContentGap, l.content.bottom());
  TEST_ASSERT_EQUAL(320, l.softKeys.bottom());
  TEST_ASSERT_EQUAL(240, l.content.w);
  // The alert bar is there whether or not it has a text, so the content never moves.
  TEST_ASSERT_EQUAL(kAlertBarHeight, l.alert.h);
}

void test_the_bars_are_high_enough_for_their_font() {
  const int line = font(FontSize::Small).lineHeight();
  TEST_ASSERT_TRUE(kStatusBarHeight >= line + 2);   // text line and the 2-pixel rule
  TEST_ASSERT_TRUE(kAlertBarHeight >= line + 2);    // two pixels of top margin
  TEST_ASSERT_TRUE(kSoftKeysHeight >= line + 5);    // top rule, button frame, text
}

void test_the_symbol_constants_are_the_symbols_of_the_character_set() {
  std::ifstream in("assets/fonts/charset.txt");
  TEST_ASSERT_TRUE(in.good());
  std::map<std::string, uint32_t> codes;
  std::string line;
  while (std::getline(in, line)) {
    std::istringstream words(line.substr(0, line.find('#')));
    std::string kind, hex, name;
    if ((words >> kind >> hex >> name) && kind == "symbol") codes[name] = std::stoul(hex, nullptr, 16);
  }
  const struct { const char* name; const char* utf8; } table[] = {
      {"enter", symbol::kEnter}, {"backspace", symbol::kBackspace}, {"up", symbol::kUp},
      {"down", symbol::kDown}, {"telephone", symbol::kTelephone}, {"envelope", symbol::kEnvelope},
      {"aeroplane", symbol::kAeroplane}, {"bubble", symbol::kBubble}, {"radio", symbol::kRadio},
      {"pin", symbol::kPin}};
  TEST_ASSERT_EQUAL(sizeof table / sizeof *table, codes.size());
  for (const auto& row : table) {
    TEST_ASSERT_TRUE_MESSAGE(codes.count(row.name) == 1, row.name);
    size_t pos = 0;
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(codes[row.name], text::decode(row.utf8, pos), row.name);
    TEST_ASSERT_EQUAL(strlen(row.utf8), pos);
  }
}

// ---- shapes --------------------------------------------------------------------------------

void test_a_round_rectangle_with_radius_zero_is_a_rectangle() {
  FrameBuffer a(20, 20), b(20, 20);
  shapes::fillRoundRect(a, {2, 3, 10, 6}, 0);
  b.fillRect({2, 3, 10, 6}, true);
  TEST_ASSERT_TRUE(a == b);
  FrameBuffer c(20, 20), d(20, 20);
  shapes::outlineRoundRect(c, {2, 3, 10, 6}, 0);
  d.outlineRect({2, 3, 10, 6}, true);
  TEST_ASSERT_TRUE(c == d);
}

void test_rounded_corners_cut_the_corner_pixels_symmetrically() {
  FrameBuffer fb(20, 20);
  const Rect r{2, 2, 12, 10};
  shapes::fillRoundRect(fb, r, 4);
  TEST_ASSERT_FALSE(fb.pixel(2, 2));                        // the corner is cut
  TEST_ASSERT_TRUE(fb.pixel(2 + 6, 2));                     // the straight edge is not
  TEST_ASSERT_TRUE(fb.pixel(2, 2 + 5));
  for (int y = 0; y < 10; ++y)
    for (int x = 0; x < 12; ++x) {
      const bool v = fb.pixel(r.x + x, r.y + y);
      TEST_ASSERT_EQUAL(v, fb.pixel(r.x + 11 - x, r.y + y));       // left-right symmetric
      TEST_ASSERT_EQUAL(v, fb.pixel(r.x + x, r.y + 9 - y));        // top-bottom symmetric
    }
}

void test_an_outline_is_the_fill_without_its_inside_and_leaves_the_background() {
  FrameBuffer fb(30, 30);
  fb.fillRect({0, 0, 30, 30}, true);                    // black background
  shapes::outlineRoundRect(fb, {5, 5, 16, 12}, 4, false);  // a white outline on it
  TEST_ASSERT_TRUE(fb.pixel(12, 10));                   // the inside is untouched: still black
  TEST_ASSERT_FALSE(fb.pixel(12, 5));                   // the edge is white
  TEST_ASSERT_FALSE(fb.pixel(5, 11));
  FrameBuffer fill(30, 30), outline(30, 30);
  shapes::fillRoundRect(fill, {5, 5, 16, 12}, 4);
  shapes::outlineRoundRect(outline, {5, 5, 16, 12}, 4);
  TEST_ASSERT_TRUE(outline.blackPixels() < fill.blackPixels());
  for (int y = 0; y < 30; ++y)
    for (int x = 0; x < 30; ++x)
      if (outline.pixel(x, y)) TEST_ASSERT_TRUE(fill.pixel(x, y));   // the outline lies inside the fill
}

void test_shapes_stay_inside_their_rectangle_even_when_it_is_tiny() {
  for (int w : {0, 1, 2, 3, 5})
    for (int h : {0, 1, 2, 3, 5}) {
      FrameBuffer fb(20, 20);
      const Rect r{5, 5, w, h};
      fb.beginRegion(r);
      shapes::fillRoundRect(fb, r, 6);
      shapes::outlineRoundRect(fb, r, 6);
      fb.endRegion();
      TEST_ASSERT_FALSE_MESSAGE(fb.clipped(), fb.describeClips().c_str());
    }
}

void test_a_radius_beyond_half_the_size_is_the_largest_that_fits() {
  for (const Rect r : {Rect{2, 2, 10, 6}, Rect{2, 2, 7, 12}, Rect{2, 2, 9, 9}}) {
    const int largest = std::min(r.w, r.h) / 2;
    FrameBuffer a(20, 20), b(20, 20), c(20, 20), d(20, 20);
    shapes::fillRoundRect(a, r, 99);
    shapes::fillRoundRect(b, r, largest);
    shapes::outlineRoundRect(c, r, 99);
    shapes::outlineRoundRect(d, r, largest);
    TEST_ASSERT_TRUE(a == b);
    TEST_ASSERT_TRUE(c == d);
  }
}

void test_the_scroll_bar_shows_the_window_and_hides_when_all_is_visible() {
  FrameBuffer fb(10, 100);
  const Rect track{0, 0, 4, 100};
  shapes::drawScrollBar(fb, track, 10, 0, 10);
  TEST_ASSERT_EQUAL(0, fb.blackPixels());
  shapes::drawScrollBar(fb, track, 0, 0, 0);
  TEST_ASSERT_EQUAL(0, fb.blackPixels());

  // 4 of 20 rows shown: the thumb is a fifth of the track, at the top, in the middle, at the end.
  shapes::drawScrollBar(fb, track, 20, 0, 4);
  TEST_ASSERT_TRUE(fb.pixel(0, 0) && fb.pixel(3, 19) && !fb.pixel(0, 20));
  fb.clear();
  shapes::drawScrollBar(fb, track, 20, 16, 4);
  TEST_ASSERT_TRUE(fb.pixel(0, 99) && fb.pixel(3, 80) && !fb.pixel(0, 79));
  fb.clear();
  shapes::drawScrollBar(fb, track, 20, 99, 4);  // beyond the end counts as the end
  TEST_ASSERT_TRUE(fb.pixel(0, 99));
  fb.clear();
  shapes::drawScrollBar(fb, track, 1000, 0, 4);  // the thumb never gets thinner than 4 pixels
  TEST_ASSERT_TRUE(fb.pixel(0, 3) && !fb.pixel(0, 4));
}

// ---- soft keys ------------------------------------------------------------------------------

void test_soft_key_slots_share_the_width() {
  const Rect bar{0, 292, 240, 28};
  SoftKeys two;
  two.back = "Back";
  two.action = "Send";
  SoftKeySlots s = softKeySlots(two, bar);
  TEST_ASSERT_TRUE(s.back == (Rect{0, 292, 120, 28}));
  TEST_ASSERT_TRUE(s.action == (Rect{120, 292, 120, 28}));
  TEST_ASSERT_TRUE(s.up.empty() && s.down.empty());

  SoftKeys four = two;
  four.scroll = true;
  s = softKeySlots(four, bar);
  TEST_ASSERT_TRUE(s.back == (Rect{0, 292, 94, 28}));
  TEST_ASSERT_TRUE(s.up == (Rect{94, 292, 26, 28}));
  TEST_ASSERT_TRUE(s.down == (Rect{120, 292, 26, 28}));
  TEST_ASSERT_TRUE(s.action == (Rect{146, 292, 94, 28}));
}

void test_a_single_button_gets_the_whole_width() {
  const Rect bar{0, 292, 240, 28};
  SoftKeys only;
  only.action = "Hang up";
  SoftKeySlots s = softKeySlots(only, bar);
  TEST_ASSERT_TRUE(s.back.empty());
  TEST_ASSERT_TRUE(s.action == (Rect{0, 292, 240, 28}));
  only.scroll = true;  // volume arrows: still the rest, not less
  s = softKeySlots(only, bar);
  TEST_ASSERT_TRUE(s.action == (Rect{52, 292, 188, 28}));
}

void test_a_tap_acts_like_the_key_of_its_button() {
  const Rect bar{0, 292, 240, 28};
  SoftKeys two;
  two.back = "Back";
  two.action = "Send";
  TEST_ASSERT_TRUE(softKeyAt(two, bar, 10, 310) == Key::Back);
  TEST_ASSERT_TRUE(softKeyAt(two, bar, 230, 310) == Key::Select);
  TEST_ASSERT_FALSE(softKeyAt(two, bar, 10, 200).has_value());   // above the bar
  TEST_ASSERT_FALSE(softKeyAt(two, bar, 10, 320).has_value());   // below it
  TEST_ASSERT_TRUE(softKeyAt(two, bar, 119, 292) == Key::Back);   // the edge between the two
  TEST_ASSERT_TRUE(softKeyAt(two, bar, 120, 292) == Key::Select);

  SoftKeys four = two;
  four.scroll = true;
  TEST_ASSERT_TRUE(softKeyAt(four, bar, 10, 310) == Key::Back);
  TEST_ASSERT_TRUE(softKeyAt(four, bar, 100, 310) == Key::Up);
  TEST_ASSERT_TRUE(softKeyAt(four, bar, 135, 310) == Key::Down);
  TEST_ASSERT_TRUE(softKeyAt(four, bar, 230, 310) == Key::Select);

  SoftKeys none;
  TEST_ASSERT_FALSE(softKeyAt(none, bar, 10, 310).has_value());
  SoftKeys backOnly;
  backOnly.back = "Back";
  TEST_ASSERT_TRUE(softKeyAt(backOnly, bar, 200, 310) == Key::Back);  // it owns the whole bar
}

void test_a_button_label_gives_up_the_space_and_then_the_symbol_when_it_does_not_fit() {
  const Font& f = font(FontSize::Small);
  const std::string word = "Cancel";
  const auto expected = [&](const Rect& slot, const std::string& label) {
    FrameBuffer fb;
    const Rect box{slot.x + 1, slot.y + 3, slot.w - 2, slot.h - 4};
    fb.outlineRect(box, true);
    text::drawCentered(fb, f, {box.x + 2, box.y, box.w - 4, box.h}, box.y + (box.h - f.lineHeight()) / 2 + 1, label);
    return fb;
  };
  const std::string spaced = std::string(symbol::kBackspace) + " " + word;
  const std::string tight = std::string(symbol::kBackspace) + word;
  TEST_ASSERT_TRUE(text::width(f, spaced) > text::width(f, tight) && text::width(f, tight) > text::width(f, word));
  // The slot is the label's width plus the frame (2) and the margin (4); one pixel less and the next form is used.
  const struct { int room; const std::string& label; } cases[] = {
      {text::width(f, spaced), spaced}, {text::width(f, spaced) - 1, tight},
      {text::width(f, tight), tight},   {text::width(f, tight) - 1, word}};
  for (const auto& c : cases) {
    const Rect slot{0, 0, c.room + 6, 28};
    FrameBuffer drawn;
    drawSoftKeyButton(drawn, slot, word, symbol::kBackspace, true);
    TEST_ASSERT_TRUE_MESSAGE(drawn == expected(slot, c.label), std::to_string(c.room).c_str());
  }
  // The action button has the symbol behind the word.
  FrameBuffer action;
  const Rect slot{0, 0, text::width(f, word + " " + symbol::kEnter) + 6, 28};
  drawSoftKeyButton(action, slot, word, symbol::kEnter, false);
  TEST_ASSERT_TRUE(action == expected(slot, word + " " + symbol::kEnter));
  // A button without a word (the arrows) shows its symbol alone.
  FrameBuffer arrow;
  drawSoftKeyButton(arrow, {0, 0, 26, 28}, "", symbol::kUp, true);
  TEST_ASSERT_TRUE(arrow == expected({0, 0, 26, 28}, std::string(symbol::kUp) + " "));   // the space is invisible
}

void test_the_soft_keys_stay_inside_their_buttons_in_every_language() {
  languages::forEach([](const std::string& language, Translator& t) {
    SoftKeys keys;
    keys.back = t.get("key.back");
    keys.action = t.get("key.open");
    for (bool scroll : {false, true}) {
      keys.scroll = scroll;
      const Rect bar = ScreenLayout::standard().softKeys;
      const SoftKeySlots s = softKeySlots(keys, bar);

      // Each label is judged against its own button, not against the bar: a label that spills
      // into its neighbour would still be inside the bar.
      const struct { const Rect& slot; const std::string& word; const char* symbol; bool first; } buttons[] = {
          {s.back, keys.back, symbol::kBackspace, true}, {s.action, keys.action, symbol::kEnter, false}};
      for (const auto& b : buttons) {
        FrameBuffer one;
        one.beginRegion(b.slot);
        drawSoftKeyButton(one, b.slot, b.word, b.symbol, b.first);
        one.endRegion();
        TEST_ASSERT_FALSE_MESSAGE(one.clipped(), clips(language + " " + b.word, one).c_str());
      }

      FrameBuffer whole;
      whole.beginRegion(bar);
      drawSoftKeys(whole, bar, keys);
      whole.endRegion();
      TEST_ASSERT_FALSE_MESSAGE(whole.clipped(), clips(language, whole).c_str());
      languages::expectPicture(whole, std::string("soft_keys_") + (scroll ? "scroll" : "plain"), language, bar);
    }
  });
}

void test_every_standard_soft_key_word_fits_either_button_with_and_without_the_arrows() {
  const char* words[] = {"key.back", "key.open", "key.send", "key.choose", "key.next", "key.delete"};
  languages::forEach([&](const std::string& language, Translator& t) {
    for (const char* key : words) {
      for (bool scroll : {false, true}) {
        // The word as the back button and as the main action, next to the other button.
        for (bool asAction : {false, true}) {
          SoftKeys keys;
          keys.scroll = scroll;
          (asAction ? keys.action : keys.back) = t.get(key);
          (asAction ? keys.back : keys.action) = t.get("key.open");
          const Rect bar = ScreenLayout::standard().softKeys;
          FrameBuffer fb;
          fb.beginRegion(bar);
          drawSoftKeys(fb, bar, keys);
          fb.endRegion();
          TEST_ASSERT_FALSE_MESSAGE(fb.clipped(), clips(language + " " + key, fb).c_str());
        }
      }
    }
  });
}

// ---- status bar -----------------------------------------------------------------------------

void test_the_battery_is_five_bars_rounded_up() {
  const struct { int percent, bars; } table[] = {{-1, 0}, {0, 0}, {1, 1}, {20, 1}, {21, 2}, {40, 2}, {41, 3},
                                               {60, 3}, {61, 4}, {68, 4}, {80, 4}, {81, 5}, {99, 5}, {100, 5}, {250, 5}};
  for (const auto& row : table) TEST_ASSERT_EQUAL_MESSAGE(row.bars, batteryBars(row.percent), std::to_string(row.percent).c_str());
}

void test_the_left_side_names_the_most_important_thing() {
  Translator t = languages::loaded();
  t.select("en");
  StatusModel m;
  m.link = StatusModel::Link::Operator;
  m.operatorName = "Example Mobile";
  StatusLeft left = statusLeft(m, t);
  TEST_ASSERT_EQUAL_STRING("Example Mobile", left.text.c_str());
  TEST_ASSERT_TRUE(left.isOperatorName);

  m.voiceAvailable = false;                       // connected, but only SMS works
  TEST_ASSERT_EQUAL_STRING("SMS only", statusLeft(m, t).text.c_str());
  TEST_ASSERT_FALSE(statusLeft(m, t).isOperatorName);

  m.quietHours = true;                            // school time wins over "SMS only"
  TEST_ASSERT_EQUAL_STRING("School time", statusLeft(m, t).text.c_str());

  m.sim = StatusModel::Sim::PinRequired;          // a SIM problem wins over everything
  TEST_ASSERT_EQUAL_STRING("SIM PIN", statusLeft(m, t).text.c_str());
  m.sim = StatusModel::Sim::Missing;
  TEST_ASSERT_EQUAL_STRING("No SIM", statusLeft(m, t).text.c_str());
  m.sim = StatusModel::Sim::Locked;
  TEST_ASSERT_EQUAL_STRING("SIM locked", statusLeft(m, t).text.c_str());
}

void test_the_left_side_without_an_operator() {
  Translator t = languages::loaded();
  t.select("en");
  StatusModel m;
  TEST_ASSERT_EQUAL_STRING("No network", statusLeft(m, t).text.c_str());
  m.link = StatusModel::Link::Operator;            // connected, but the network sent no name
  TEST_ASSERT_EQUAL_STRING("Network", statusLeft(m, t).text.c_str());   // a general word, not "No network"
  TEST_ASSERT_FALSE(statusLeft(m, t).isOperatorName);                   // so it is never shortened
  t.select("de");
  TEST_ASSERT_EQUAL_STRING("Netz", statusLeft(m, t).text.c_str());
  t.select("fr");
  TEST_ASSERT_EQUAL_STRING("R\xC3\xA9seau", statusLeft(m, t).text.c_str());
  t.select("en");
  m.voiceAvailable = false;                        // the more important word still wins
  TEST_ASSERT_EQUAL_STRING("SMS only", statusLeft(m, t).text.c_str());
  m.voiceAvailable = true;
  m.link = StatusModel::Link::Radio;
  TEST_ASSERT_EQUAL_STRING("Radio", statusLeft(m, t).text.c_str());
  m.link = StatusModel::Link::RadioOnly;
  TEST_ASSERT_EQUAL_STRING("Radio only", statusLeft(m, t).text.c_str());
  m.operatorName = "Example Mobile";               // a name from an earlier connection is not shown without one
  m.link = StatusModel::Link::None;
  TEST_ASSERT_EQUAL_STRING("No network", statusLeft(m, t).text.c_str());
}

void test_the_date_is_written_the_way_the_language_writes_it() {
  Translator t = languages::loaded();
  StatusModel m;
  m.dateKnown = true;
  m.month = 9;
  m.day = 30;
  m.weekday = 2;  // Tuesday
  t.select("en");
  TEST_ASSERT_EQUAL_STRING("Tue 09/30", dateText(m, t, true).c_str());
  TEST_ASSERT_EQUAL_STRING("09/30", dateText(m, t, false).c_str());
  t.select("de");
  TEST_ASSERT_EQUAL_STRING("Di 30.09.", dateText(m, t, true).c_str());
  TEST_ASSERT_EQUAL_STRING("30.09.", dateText(m, t, false).c_str());
  t.select("fr");
  TEST_ASSERT_EQUAL_STRING("mar. 30/09", dateText(m, t, true).c_str());
  TEST_ASSERT_EQUAL_STRING("30/09", dateText(m, t, false).c_str());
}

void test_every_weekday_has_its_own_name_and_the_index_wraps() {
  Translator t = languages::loaded();
  t.select("en");
  StatusModel m;
  m.dateKnown = true;
  m.month = 1;
  m.day = 1;
  const char* names[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  for (int d = 0; d < 7; ++d) {
    m.weekday = d;
    TEST_ASSERT_EQUAL_STRING((std::string(names[d]) + " 01/01").c_str(), dateText(m, t, true).c_str());
  }
  m.weekday = 7;
  TEST_ASSERT_EQUAL_STRING("Sun 01/01", dateText(m, t, true).c_str());
  m.weekday = -1;
  TEST_ASSERT_EQUAL_STRING("Sat 01/01", dateText(m, t, true).c_str());
}

void test_an_unknown_date_is_shown_as_dashes() {
  Translator t = languages::loaded();
  t.select("de");
  StatusModel m;
  TEST_ASSERT_EQUAL_STRING("--.--.", dateText(m, t, true).c_str());
  TEST_ASSERT_EQUAL_STRING("--.--.", dateText(m, t, false).c_str());
}

namespace {

StatusModel model(const char* kind) {
  StatusModel m;
  m.dateKnown = true;
  m.month = 9;
  m.day = 30;
  m.weekday = 2;
  m.batteryPercent = 68;
  m.link = StatusModel::Link::Operator;
  m.operatorName = "Example Mobile";
  const std::string k = kind;
  if (k == "long-name") {
    m.operatorName = "International Mobile Communications of Example Country";
    m.locating = true;  // the GPS note must survive the shortening of the name
  } else if (k == "school") {
    m.quietHours = true;
    m.locating = true;
  } else if (k == "sim-locked") {
    m.sim = StatusModel::Sim::Locked;
    m.locating = true;
    m.batteryPercent = 100;
  } else if (k == "nothing") {
    m.link = StatusModel::Link::None;
    m.dateKnown = false;
    m.batteryPercent = -1;
  } else if (k == "unnamed") {
    m.operatorName.clear();
  } else if (k == "sms-only") {
    m.voiceAvailable = false;
    m.batteryPercent = 3;
  }
  return m;
}

}  // namespace

void test_the_status_bar_fits_its_area_in_every_language() {
  const Rect area = ScreenLayout::standard().status;
  languages::forEach([&](const std::string& language, Translator& t) {
    for (const char* kind : {"plain", "long-name", "school", "sim-locked", "nothing", "sms-only", "unnamed"}) {
      FrameBuffer fb;
      fb.beginRegion(area);
      drawStatusBar(fb, area, model(kind), t);
      fb.endRegion();
      TEST_ASSERT_FALSE_MESSAGE(fb.clipped(), clips(language + " " + kind, fb).c_str());
      languages::expectPicture(fb, std::string("status_bar_") + kind, language, area);
    }
  });
}

void test_the_gps_note_is_drawn_exactly_when_the_position_is_being_determined() {
  const Rect area = ScreenLayout::standard().status;
  Translator t = languages::loaded();
  t.select("en");
  StatusModel m = model("plain");
  m.operatorName = "Mobile";                 // short enough to stay whole next to the GPS note
  FrameBuffer without;
  drawStatusBar(without, area, m, t);
  m.locating = true;
  FrameBuffer with;
  drawStatusBar(with, area, m, t);
  TEST_ASSERT_FALSE(with == without);
  // "GPS" sits right behind the operator name, in the same line.
  const int nameWidth = text::width(font(FontSize::Small), "Mobile");
  FrameBuffer onlyGps;
  text::draw(onlyGps, font(FontSize::Small), 2 + nameWidth, 0, "  GPS");
  for (int y = 0; y < 16; ++y)
    for (int x = 0; x < 240; ++x)
      if (onlyGps.pixel(x, y)) TEST_ASSERT_TRUE(with.pixel(x, y));
}

void test_a_long_operator_name_is_shortened_and_keeps_the_gps_note() {
  const Rect area = ScreenLayout::standard().status;
  Translator t = languages::loaded();
  t.select("en");
  FrameBuffer fb;
  fb.beginRegion(area);
  drawStatusBar(fb, area, model("long-name"), t);
  fb.endRegion();
  TEST_ASSERT_FALSE_MESSAGE(fb.clipped(), fb.describeClips().c_str());
  // The name is cut where the picture ends in dots; the GPS note is behind them.
  FrameBuffer expectedHead;
  const Font& f = font(FontSize::Small);
  StatusModel m = model("long-name");
  const int gps = text::width(f, "  GPS");
  const int date = text::width(f, dateText(m, t, false));
  const int frame = 33;
  const int room = (240 - 2 - frame) - 6 - date - 6 - 2 - 2 - gps;   // the layout's arithmetic, spelled out
  const std::string shown = text::ellipsize(f, m.operatorName, room);
  TEST_ASSERT_TRUE(shown.size() > 3 && shown.size() < m.operatorName.size());
  TEST_ASSERT_EQUAL_STRING("...", shown.c_str() + shown.size() - 3);
}

void test_the_weekday_is_dropped_when_the_sides_meet() {
  const Rect area = ScreenLayout::standard().status;
  Translator t = languages::loaded();
  t.select("fr");
  StatusModel roomy = model("plain");
  roomy.link = StatusModel::Link::Radio;      // "Radio" leaves room for the weekday
  StatusModel tight = model("sim-locked");    // "SIM verrouill\xC3\xA9e" and GPS: the weekday has to go
  FrameBuffer a, b;
  drawStatusBar(a, area, roomy, t);
  drawStatusBar(b, area, tight, t);
  const Font& f = font(FontSize::Small);
  FrameBuffer weekday, noWeekday;
  text::drawRight(weekday, f, 240 - 2 - 33 - 6, 0, dateText(roomy, t, true));
  text::drawRight(noWeekday, f, 240 - 2 - 33 - 6, 0, dateText(tight, t, false));
  for (int y = 0; y < 16; ++y)
    for (int x = 0; x < 240; ++x) {
      if (weekday.pixel(x, y)) TEST_ASSERT_TRUE(a.pixel(x, y));
      if (noWeekday.pixel(x, y)) TEST_ASSERT_TRUE(b.pixel(x, y));
    }
  // And the wider form is not what the tight bar drew.
  FrameBuffer wide;
  text::drawRight(wide, f, 240 - 2 - 33 - 6, 0, dateText(tight, t, true));
  int missing = 0;
  for (int y = 0; y < 16; ++y)
    for (int x = 0; x < 240; ++x)
      if (wide.pixel(x, y) && !b.pixel(x, y)) ++missing;
  TEST_ASSERT_TRUE(missing > 0);
}

void test_the_battery_frame_shows_one_bar_per_fifth() {
  const Rect area = ScreenLayout::standard().status;
  Translator t = languages::loaded();
  t.select("en");
  int previous = -1;
  for (int percent : {-1, 1, 21, 41, 61, 81}) {
    StatusModel m;
    m.batteryPercent = percent;
    FrameBuffer fb;
    drawStatusBar(fb, area, m, t);
    const int frameX = 240 - 2 - 33;
    int black = 0;
    for (int y = 0; y < 16; ++y)
      for (int x = frameX + 2; x < frameX + 31; ++x) black += fb.pixel(x, y) ? 1 : 0;
    TEST_ASSERT_TRUE_MESSAGE(black > previous, std::to_string(percent).c_str());
    previous = black;
  }
  StatusModel zero;
  zero.batteryPercent = 0;
  StatusModel unknown;
  FrameBuffer a, b;
  drawStatusBar(a, area, zero, t);
  drawStatusBar(b, area, unknown, t);
  TEST_ASSERT_TRUE(a == b);   // empty and unknown both show just the frame
}

// ---- alert bar ------------------------------------------------------------------------------

void test_an_empty_alert_draws_nothing_and_a_text_is_inverted() {
  const Rect area = ScreenLayout::standard().alert;
  FrameBuffer fb;
  drawAlertBar(fb, area, "");
  TEST_ASSERT_EQUAL(0, fb.blackPixels());
  drawAlertBar(fb, area, "Parent One is calling");
  TEST_ASSERT_TRUE(fb.pixel(0, area.y) && fb.pixel(239, area.y + area.h - 1));   // filled over the whole bar
  TEST_ASSERT_TRUE(fb.blackPixels() < area.w * area.h);                          // with the text in white
}

void test_an_alert_that_is_too_long_is_shortened_inside_the_bar() {
  const Rect area = ScreenLayout::standard().alert;
  FrameBuffer fb;
  fb.beginRegion(area);
  drawAlertBar(fb, area, "A very long alert text that cannot possibly fit into one line of the alert bar");
  fb.endRegion();
  TEST_ASSERT_FALSE_MESSAGE(fb.clipped(), fb.describeClips().c_str());
}

void test_alerts_with_catalog_texts_fit_in_every_language() {
  const Rect area = ScreenLayout::standard().alert;
  languages::forEach([&](const std::string& language, Translator& t) {
    FrameBuffer fb;
    fb.beginRegion(area);
    drawAlertBar(fb, area, t.get("call.no-answer", {{"name", "Parent One"}}));
    fb.endRegion();
    TEST_ASSERT_FALSE_MESSAGE(fb.clipped(), clips(language, fb).c_str());
    languages::expectPicture(fb, "alert_bar_no_answer", language, area);

    // A countdown is an alert too; the bar shortens whatever does not fit instead of spilling.
    FrameBuffer countdown;
    countdown.beginRegion(area);
    drawAlertBar(countdown, area, t.get("sos.countdown", {{"seconds", 40}}));
    countdown.endRegion();
    TEST_ASSERT_FALSE_MESSAGE(countdown.clipped(), clips(language, countdown).c_str());
  });
}

// ---- tile grid -------------------------------------------------------------------------------

namespace {

std::vector<std::vector<Tile>> homeTiles(Translator& t) {
  std::vector<std::vector<Tile>> pages(2);
  pages[0] = {{t.get("home.phone"), symbol::kTelephone, ""},
              {t.get("home.text"), symbol::kAeroplane, ""},
              {t.get("home.inbox"), symbol::kBubble, "3"},
              {t.get("home.sos"), symbol::kPin, ""}};
  pages[1] = {{t.get("home.radio"), symbol::kRadio, ""}, {t.get("language.title"), nullptr, ""}};
  return pages;
}

const Rect kGridArea{0, 44, 240, 244};

void press(TileGrid& grid, Key key, int times = 1) {
  bool chosen;
  for (int i = 0; i < times; ++i) grid.handle(Input::pressed(key), kGridArea, &chosen);
}

}  // namespace

void test_the_selection_moves_in_reading_order_and_wraps() {
  Translator t = languages::loaded();
  TileGrid grid(2, 3);
  grid.setPages(homeTiles(t));
  TEST_ASSERT_EQUAL(6, grid.count());
  bool chosen = true;
  TEST_ASSERT_TRUE(grid.handle(Input::pressed(Key::Down), kGridArea, &chosen));
  TEST_ASSERT_FALSE(chosen);
  TEST_ASSERT_EQUAL(1, grid.selected());
  press(grid, Key::Up, 2);   // wraps from the first to the last
  TEST_ASSERT_EQUAL(5, grid.selected());
  press(grid, Key::Down);    // and from the last to the first
  TEST_ASSERT_EQUAL(0, grid.selected());
}

void test_the_page_follows_the_selection() {
  Translator t = languages::loaded();
  TileGrid grid(2, 3);
  grid.setPages(homeTiles(t));
  TEST_ASSERT_EQUAL(0, grid.page());
  press(grid, Key::Down, 3);
  TEST_ASSERT_EQUAL(0, grid.page());      // the last tile of the first page
  press(grid, Key::Down);
  TEST_ASSERT_EQUAL(1, grid.page());
  TEST_ASSERT_EQUAL(2, grid.pageCount());
  press(grid, Key::Up);
  TEST_ASSERT_EQUAL(0, grid.page());
}

void test_select_chooses_the_selected_tile_and_other_input_is_not_used() {
  Translator t = languages::loaded();
  t.select("en");
  TileGrid grid(2, 3);
  grid.setPages(homeTiles(t));
  bool chosen = false;
  press(grid, Key::Down);
  TEST_ASSERT_TRUE(grid.handle(Input::pressed(Key::Select), kGridArea, &chosen));
  TEST_ASSERT_TRUE(chosen);
  TEST_ASSERT_EQUAL_STRING("Text", grid.selectedTile().label.c_str());
  TEST_ASSERT_FALSE(grid.handle(Input::pressed(Key::Back), kGridArea, &chosen));
  TEST_ASSERT_FALSE(chosen);
  TEST_ASSERT_FALSE(grid.handle(Input::text('x'), kGridArea, &chosen));
}

void test_a_tap_on_a_tile_selects_and_chooses_it() {
  Translator t = languages::loaded();
  TileGrid grid(2, 3);
  grid.setPages(homeTiles(t));
  bool chosen = false;
  const Rect third = grid.tileRect(kGridArea, 2);
  TEST_ASSERT_TRUE(grid.handle(Input::touch(third.x + 5, third.y + 5), kGridArea, &chosen));
  TEST_ASSERT_TRUE(chosen);
  TEST_ASSERT_EQUAL(2, grid.selected());

  // A tap between the tiles, or below them, hits nothing and changes nothing.
  chosen = true;
  TEST_ASSERT_FALSE(grid.handle(Input::touch(third.right() + 1, third.y + 5), kGridArea, &chosen));
  TEST_ASSERT_FALSE(chosen);
  TEST_ASSERT_FALSE(grid.handle(Input::touch(120, kGridArea.bottom() - 18 - 2), kGridArea, &chosen));   // above the dots
  TEST_ASSERT_EQUAL(2, grid.selected());
}

void test_only_the_tiles_of_the_page_on_show_can_be_tapped() {
  Translator t = languages::loaded();
  TileGrid grid(2, 3);
  grid.setPages(homeTiles(t));
  bool chosen = false;
  press(grid, Key::Down, 4);                              // the second page, which has two tiles
  const Rect fifthSlot = grid.tileRect(kGridArea, 2 + 4);  // a slot of the second page that holds no tile
  TEST_ASSERT_FALSE(grid.handle(Input::touch(fifthSlot.x + 5, fifthSlot.y + 5), kGridArea, &chosen));
  TEST_ASSERT_FALSE(chosen);
  TEST_ASSERT_EQUAL(4, grid.selected());
  const Rect secondTile = grid.tileRect(kGridArea, 5);
  TEST_ASSERT_TRUE(grid.handle(Input::touch(secondTile.x + 5, secondTile.y + 5), kGridArea, &chosen));
  TEST_ASSERT_EQUAL(5, grid.selected());
}

void test_tiles_fill_the_grid_row_by_row_without_overlap() {
  Translator t = languages::loaded();
  TileGrid grid(2, 3);
  grid.setPages(homeTiles(t));
  for (size_t i = 0; i < 4; ++i)
    for (size_t j = i + 1; j < 4; ++j) {
      const Rect a = grid.tileRect(kGridArea, i), b = grid.tileRect(kGridArea, j);
      const bool apart = a.right() + 1 <= b.x || b.right() + 1 <= a.x || a.bottom() + 1 <= b.y || b.bottom() + 1 <= a.y;
      TEST_ASSERT_TRUE(apart);   // one pixel of room for the shadow
    }
  TEST_ASSERT_EQUAL(grid.tileRect(kGridArea, 0).y, grid.tileRect(kGridArea, 1).y);   // two tiles per row
  TEST_ASSERT_TRUE(grid.tileRect(kGridArea, 2).y > grid.tileRect(kGridArea, 0).y);
  TEST_ASSERT_EQUAL(grid.tileRect(kGridArea, 0).x, grid.tileRect(kGridArea, 2).x);
  TEST_ASSERT_TRUE(grid.tileRect(kGridArea, 4) == grid.tileRect(kGridArea, 0));      // the next page starts at the top again
  const Rect last = grid.tileRect(kGridArea, 3);
  TEST_ASSERT_TRUE(last.right() + 1 <= kGridArea.right() && last.bottom() + 1 <= kGridArea.bottom());
}

void test_a_page_holds_at_most_one_grid_and_empty_pages_are_dropped() {
  TileGrid grid(2, 2);
  std::vector<Tile> many(7, Tile{"x", nullptr, ""});
  grid.setPages({many, {}, {}});
  TEST_ASSERT_EQUAL(1, grid.pageCount());
  TEST_ASSERT_EQUAL(4, grid.count());
}

void test_setting_the_tiles_again_keeps_the_selection_while_it_exists() {
  Translator t = languages::loaded();
  TileGrid grid(2, 3);
  grid.setPages(homeTiles(t));
  press(grid, Key::Down, 3);
  grid.setPages(homeTiles(t));
  TEST_ASSERT_EQUAL(3, grid.selected());
  grid.setPages({{Tile{"only", nullptr, ""}}});       // fewer tiles than the selection
  TEST_ASSERT_EQUAL(0, grid.selected());
  grid.resetSelection();
  TEST_ASSERT_EQUAL(0, grid.selected());
}

void test_select_moves_the_selection_to_a_tile_or_to_the_last_one() {
  Translator t = languages::loaded();
  TileGrid grid(2, 3);
  grid.setPages(homeTiles(t));
  grid.select(5);
  TEST_ASSERT_EQUAL(5, grid.selected());
  TEST_ASSERT_EQUAL(1, grid.page());                   // the page follows the selection
  grid.select(2);
  TEST_ASSERT_EQUAL(2, grid.selected());
  grid.select(100);                                     // no such tile: the last one, not nothing
  TEST_ASSERT_EQUAL(grid.count() - 1, grid.selected());
  TileGrid empty(2, 3);
  empty.select(3);
  TEST_ASSERT_EQUAL(0, empty.selected());
}

void test_an_empty_grid_uses_no_input_and_draws_nothing() {
  TileGrid grid(2, 3);
  bool chosen = true;
  TEST_ASSERT_FALSE(grid.handle(Input::pressed(Key::Down), kGridArea, &chosen));
  TEST_ASSERT_FALSE(chosen);
  TEST_ASSERT_FALSE(grid.handle(Input::pressed(Key::Select), kGridArea, &chosen));
  FrameBuffer fb;
  grid.draw(fb, kGridArea);
  TEST_ASSERT_EQUAL(0, fb.blackPixels());
}

void test_the_selected_tile_is_inverted_and_the_others_are_raised() {
  Translator t = languages::loaded();
  TileGrid grid(2, 3);
  grid.setPages(homeTiles(t));
  FrameBuffer fb;
  grid.draw(fb, kGridArea);
  const Rect selected = grid.tileRect(kGridArea, 0), other = grid.tileRect(kGridArea, 1);
  TEST_ASSERT_TRUE(fb.pixel(selected.x + selected.w / 2, selected.y + 3));     // filled inside
  TEST_ASSERT_FALSE(fb.pixel(other.x + other.w / 2, other.y + 3));             // an outline only
  TEST_ASSERT_TRUE(fb.pixel(other.x + other.w / 2, other.y));                  // the frame
  TEST_ASSERT_TRUE(fb.pixel(other.x + other.w / 2, other.bottom()));           // the shadow below
  TEST_ASSERT_TRUE(fb.pixel(other.right(), other.y + other.h / 2));            // and to the right
  TEST_ASSERT_FALSE(fb.pixel(selected.x + selected.w / 2, selected.bottom())); // the selected one has none
}

void test_page_dots_show_how_many_pages_there_are_and_where_we_are() {
  Translator t = languages::loaded();
  TileGrid grid(2, 3);
  grid.setPages(homeTiles(t));
  FrameBuffer one, two;
  grid.draw(one, kGridArea);
  press(grid, Key::Down, 4);
  grid.draw(two, kGridArea);
  const Rect strip{kGridArea.x, kGridArea.bottom() - 18, kGridArea.w, 18};
  TEST_ASSERT_TRUE(one.blackPixels(strip) > 0);
  TEST_ASSERT_FALSE(one.blackPixels(strip) == two.blackPixels(strip) && one.toAscii(strip) == two.toAscii(strip));
  TileGrid single(2, 3);
  single.setPages({{Tile{"only", nullptr, ""}}});
  FrameBuffer none;
  single.draw(none, kGridArea);
  TEST_ASSERT_EQUAL(0, none.blackPixels(strip));   // one page: no dots
}

void test_a_tap_on_a_page_dot_goes_to_that_page_without_choosing_anything() {
  Translator t = languages::loaded();
  TileGrid grid(2, 3);
  grid.setPages(homeTiles(t));
  FrameBuffer fb;
  grid.draw(fb, kGridArea);
  bool chosen = true;
  const Rect second = grid.dotTarget(kGridArea, 1);
  TEST_ASSERT_TRUE(grid.handle(Input::touch(second.x + second.w / 2, second.y + second.h / 2), kGridArea, &chosen));
  TEST_ASSERT_FALSE(chosen);
  TEST_ASSERT_EQUAL(4, grid.selected());               // the first tile of the second page
  TEST_ASSERT_EQUAL(1, grid.page());
  const Rect first = grid.dotTarget(kGridArea, 0);
  TEST_ASSERT_TRUE(grid.handle(Input::touch(first.x + 1, first.bottom() - 1), kGridArea, &chosen));
  TEST_ASSERT_EQUAL(0, grid.selected());
  // The targets are where the dots are drawn, side by side, and do not overlap.
  FrameBuffer none;
  grid.draw(none, kGridArea);
  TEST_ASSERT_TRUE(none.blackPixels(first) > 0 && none.blackPixels(second) > 0);
  TEST_ASSERT_TRUE(first.right() <= second.x);
  TEST_ASSERT_TRUE(kGridArea.contains(first.x, first.y) && kGridArea.contains(second.x, second.bottom() - 1));
  // One page, no dots, nothing to hit.
  TileGrid single(2, 3);
  single.setPages({{Tile{"only", nullptr, ""}}});
  TEST_ASSERT_TRUE(single.dotTarget(kGridArea, 0).empty());
}

void test_the_tiles_stay_inside_their_cells_in_every_language() {
  languages::forEach([](const std::string& language, Translator& t) {
    TileGrid grid(2, 3);
    grid.setPages(homeTiles(t));
    for (int page = 0; page < 2; ++page) {
      const size_t first = page == 0 ? 0 : 4, n = page == 0 ? 4 : 2;
      for (size_t i = 0; i < n; ++i) {
        const Rect cell = grid.tileRect(kGridArea, first + i);
        for (bool selected : {false, true}) {
          FrameBuffer one;
          one.beginRegion({cell.x, cell.y, cell.w + 1, cell.h + 1});   // the cell and its shadow
          TileGrid::drawTile(one, cell, homeTiles(t)[page][i], selected);
          one.endRegion();
          TEST_ASSERT_FALSE_MESSAGE(one.clipped(), clips(language + " " + homeTiles(t)[page][i].label, one).c_str());
        }
      }
      FrameBuffer fb;
      grid.draw(fb, kGridArea);
      languages::expectPicture(fb, "tile_grid_page" + std::to_string(page), language, kGridArea);
      press(grid, Key::Down, 4);
    }
  });
}

// ---- list view -------------------------------------------------------------------------------

namespace {

const Rect kListArea{0, 44, 240, 244};

std::vector<ListItem> numbered(size_t n) {
  std::vector<ListItem> items;
  for (size_t i = 0; i < n; ++i) items.push_back({"Contact " + std::to_string(i), true});
  return items;
}

void press(ListView& list, Key key, int times = 1) {
  bool chosen;
  for (int i = 0; i < times; ++i) list.handle(Input::pressed(key), kListArea, &chosen);
}

}  // namespace

void test_the_list_selection_stops_at_both_ends() {
  ListView list;
  list.setItems(numbered(3));
  press(list, Key::Up);
  TEST_ASSERT_EQUAL(0, list.selected());
  press(list, Key::Down, 5);
  TEST_ASSERT_EQUAL(2, list.selected());
  press(list, Key::Up);
  TEST_ASSERT_EQUAL(1, list.selected());
}

void test_the_window_follows_the_selection_in_a_long_list() {
  ListView list;
  list.setItems(numbered(30));
  const size_t rows = list.visibleRows(kListArea);
  TEST_ASSERT_TRUE(rows >= 5 && rows < 30);
  press(list, Key::Down, static_cast<int>(rows) - 1);
  TEST_ASSERT_EQUAL(0, list.firstVisible());            // the last visible row: no scrolling yet
  press(list, Key::Down);
  TEST_ASSERT_EQUAL(1, list.firstVisible());            // one more: the window moves by one row
  press(list, Key::Down, 100);
  TEST_ASSERT_EQUAL(29, list.selected());
  TEST_ASSERT_EQUAL(30 - rows, list.firstVisible());    // the end of the list fills the window
  press(list, Key::Up, static_cast<int>(rows) - 1);
  TEST_ASSERT_EQUAL(30 - rows, list.firstVisible());    // moving up inside the window does not scroll
  press(list, Key::Up);
  TEST_ASSERT_EQUAL(29 - rows, list.firstVisible());
}

void test_select_and_taps_choose_a_row() {
  ListView list;
  list.setItems(numbered(30));
  bool chosen = false;
  press(list, Key::Down, 2);
  TEST_ASSERT_TRUE(list.handle(Input::pressed(Key::Select), kListArea, &chosen));
  TEST_ASSERT_TRUE(chosen);
  TEST_ASSERT_EQUAL(2, list.selected());

  const Rect row4 = list.rowRect(kListArea, 4);
  TEST_ASSERT_TRUE(list.handle(Input::touch(row4.x + 10, row4.y + 3), kListArea, &chosen));
  TEST_ASSERT_TRUE(chosen);
  TEST_ASSERT_EQUAL(4, list.selected());

  chosen = true;
  TEST_ASSERT_FALSE(list.handle(Input::touch(10, kListArea.y - 3), kListArea, &chosen));   // above the list
  TEST_ASSERT_FALSE(chosen);
  TEST_ASSERT_FALSE(list.handle(Input::touch(238, kListArea.y + 3), kListArea, &chosen));  // on the scroll bar
  TEST_ASSERT_EQUAL(4, list.selected());
}

void test_a_tap_below_the_last_row_of_a_short_list_hits_nothing() {
  ListView list;
  list.setItems(numbered(2));
  bool chosen = true;
  const Rect third = list.rowRect(kListArea, 2);
  TEST_ASSERT_FALSE(list.handle(Input::touch(third.x + 10, third.y + 3), kListArea, &chosen));
  TEST_ASSERT_FALSE(chosen);
}

void test_a_tap_in_a_scrolled_list_hits_the_row_on_show() {
  ListView list;
  list.setItems(numbered(30));
  press(list, Key::Down, 29);
  bool chosen = false;
  const size_t first = list.firstVisible();
  const Rect row = list.rowRect(kListArea, 1);
  list.handle(Input::touch(row.x + 10, row.y + 3), kListArea, &chosen);
  TEST_ASSERT_EQUAL(first + 1, list.selected());
}

void test_replacing_the_items_keeps_the_selection_inside_the_list() {
  ListView list;
  list.setItems(numbered(10));
  press(list, Key::Down, 8);
  list.setItems(numbered(4));
  TEST_ASSERT_EQUAL(3, list.selected());
  list.setItems({});
  TEST_ASSERT_EQUAL(0, list.selected());
  bool chosen = true;
  TEST_ASSERT_FALSE(list.handle(Input::pressed(Key::Down), kListArea, &chosen));
  TEST_ASSERT_FALSE(list.handle(Input::pressed(Key::Select), kListArea, &chosen));
  TEST_ASSERT_FALSE(chosen);
  FrameBuffer fb;
  list.draw(fb, kListArea);
  TEST_ASSERT_EQUAL(0, fb.blackPixels());
}

void test_the_selected_row_is_inverted_and_a_long_list_shows_a_scroll_bar() {
  ListView list;
  list.setItems(numbered(30));
  FrameBuffer fb;
  list.draw(fb, kListArea);
  const Rect first = list.rowRect(kListArea, 0), second = list.rowRect(kListArea, 1);
  TEST_ASSERT_TRUE(fb.pixel(first.x + first.w - 3, first.y + 1));      // filled across the row
  TEST_ASSERT_FALSE(fb.pixel(second.x + second.w - 3, second.y + 1));
  TEST_ASSERT_TRUE(fb.pixel(kListArea.right() - 4, kListArea.y));      // the thumb at the top

  ListView shortList;
  shortList.setItems(numbered(3));
  FrameBuffer none;
  shortList.draw(none, kListArea);
  TEST_ASSERT_EQUAL(0, none.blackPixels({kListArea.right() - 4, kListArea.y, 4, kListArea.h}));
}

void test_a_list_that_shrinks_fills_its_window_again() {
  ListView list;
  list.setItems(numbered(30));
  const size_t rows = list.visibleRows(kListArea);
  press(list, Key::Down, 100);                         // at the end, scrolled to the bottom
  list.setItems(numbered(rows + 2));
  press(list, Key::Up);
  TEST_ASSERT_EQUAL(rows + 1 - 1, list.selected());
  TEST_ASSERT_EQUAL(2, list.firstVisible());           // not 8 or so, which would leave the window mostly empty
}

void test_user_text_is_shortened_and_fixed_text_is_not() {
  const Font& f = font(FontSize::Text);
  const std::string name = "A very long name of a contact that cannot fit";
  ListView list;
  list.setItems({{name, true}, {name, false}});
  FrameBuffer fb;
  fb.beginRegion(kListArea);
  list.draw(fb, kListArea);
  fb.endRegion();
  // The second row is a fixed label that does not fit: that is exactly what the frame buffer reports.
  TEST_ASSERT_TRUE(fb.clipped());
  for (const auto& e : fb.clipEvents()) TEST_ASSERT_TRUE(e.firstY >= list.rowRect(kListArea, 1).y);

  // The first row, alone, is shortened with dots and fits.
  ListView only;
  only.setItems({{name, true}});
  FrameBuffer one;
  one.beginRegion(only.rowRect(kListArea, 0));
  only.draw(one, kListArea);
  one.endRegion();
  TEST_ASSERT_FALSE_MESSAGE(one.clipped(), one.describeClips().c_str());
  FrameBuffer expected;
  const Rect row = only.rowRect(kListArea, 0);
  expected.fillRect(row, true);
  text::draw(expected, f, row.x + 4, row.y + 4, text::ellipsize(f, name, row.w - 8), false);
  TEST_ASSERT_TRUE(one.toAscii(row) == expected.toAscii(row));   // exactly the ellipsized text on the selected row
}

void test_rows_stay_inside_their_row_in_every_language() {
  languages::forEach([](const std::string& language, Translator& t) {
    ListView list;
    std::vector<ListItem> items = {{t.get("messages.none"), false},
                                   {t.get("language.title"), false},
                                   {t.get("home.sos"), false},
                                   {"A contact with an extraordinarily long name indeed", true},
                                   {"Parent One", true}};
    list.setItems(items);
    for (size_t i = 0; i < items.size(); ++i) {
      FrameBuffer one;
      ListView solo;
      solo.setItems({items[i]});
      one.beginRegion(solo.rowRect(kListArea, 0));
      solo.draw(one, kListArea);
      one.endRegion();
      TEST_ASSERT_FALSE_MESSAGE(one.clipped(), clips(language + " " + items[i].text, one).c_str());
    }
    press(list, Key::Down, 1);
    FrameBuffer fb;
    list.draw(fb, kListArea);
    languages::expectPicture(fb, "list_view", language, kListArea);
  });
}

// ---- text block ------------------------------------------------------------------------------

namespace {

const Rect kBlockArea{0, 44, 240, 120};
const char* kLongMessage =
    "Please call me back when you can. The key is under the flower pot, and there is some soup on the stove "
    "for dinner. Do not forget the homework, and be home before it gets dark. See you soon! Parent One";

}  // namespace

void test_a_long_text_wraps_and_scrolls_by_line() {
  TextBlock block;
  block.setText(kLongMessage);
  const size_t total = block.lineCount(kBlockArea), visible = block.visibleLines(kBlockArea);
  TEST_ASSERT_TRUE(total > visible);
  bool used = block.handle(Input::pressed(Key::Down), kBlockArea);
  TEST_ASSERT_TRUE(used);
  TEST_ASSERT_EQUAL(1, block.firstLine());
  for (int i = 0; i < 50; ++i) block.handle(Input::pressed(Key::Down), kBlockArea);
  TEST_ASSERT_EQUAL(total - visible, block.firstLine());    // the last line is the last line of the window
  for (int i = 0; i < 50; ++i) block.handle(Input::pressed(Key::Up), kBlockArea);
  TEST_ASSERT_EQUAL(0, block.firstLine());
  TEST_ASSERT_FALSE(block.handle(Input::pressed(Key::Select), kBlockArea));
}

void test_a_text_scrolled_to_the_end_fills_a_taller_area_again() {
  TextBlock block;
  block.setText(kLongMessage);
  for (int i = 0; i < 50; ++i) block.handle(Input::pressed(Key::Down), kBlockArea);
  const Rect taller{kBlockArea.x, kBlockArea.y, kBlockArea.w, 220};
  FrameBuffer fb;
  block.draw(fb, taller);
  TEST_ASSERT_EQUAL(block.lineCount(taller) - block.visibleLines(taller), block.firstLine());
}

void test_a_short_text_does_not_scroll_and_a_new_text_starts_at_the_top() {
  TextBlock block;
  block.setText("Short text");
  block.handle(Input::pressed(Key::Down), kBlockArea);
  TEST_ASSERT_EQUAL(0, block.firstLine());
  TEST_ASSERT_EQUAL(1, block.lineCount(kBlockArea));
  block.setText(kLongMessage);
  for (int i = 0; i < 3; ++i) block.handle(Input::pressed(Key::Down), kBlockArea);
  TEST_ASSERT_EQUAL(3, block.firstLine());
  block.setText(kLongMessage);
  TEST_ASSERT_EQUAL(0, block.firstLine());
}

void test_a_text_block_draws_the_lines_it_shows_and_nothing_else() {
  TextBlock block;
  block.setText(kLongMessage);
  FrameBuffer top;
  top.beginRegion(kBlockArea);
  block.draw(top, kBlockArea);
  top.endRegion();
  TEST_ASSERT_FALSE_MESSAGE(top.clipped(), top.describeClips().c_str());
  block.handle(Input::pressed(Key::Down), kBlockArea);
  FrameBuffer scrolled;
  block.draw(scrolled, kBlockArea);
  TEST_ASSERT_FALSE(top == scrolled);
  // The scroll bar is drawn for a long text and not for a short one.
  const Rect bar{kBlockArea.right() - 4, kBlockArea.y, 4, kBlockArea.h};
  TEST_ASSERT_TRUE(top.blackPixels(bar) > 0);
  TextBlock shortBlock;
  shortBlock.setText("Short text");
  FrameBuffer s;
  shortBlock.draw(s, kBlockArea);
  TEST_ASSERT_EQUAL(0, s.blackPixels(bar));
}

void test_catalog_text_in_a_text_block_stays_inside_in_every_language() {
  languages::forEach([](const std::string& language, Translator& t) {
    TextBlock block;
    block.setText(t.get("sos.emergency") + "\n" + t.get("school.until", {{"time", "14:30"}}) + "\n" + t.get("call.failed", {{"name", "Parent One"}}) +
                  "\n" + t.plural("messages.unread", 3));
    FrameBuffer fb;
    fb.beginRegion(kBlockArea);
    block.draw(fb, kBlockArea);
    fb.endRegion();
    TEST_ASSERT_FALSE_MESSAGE(fb.clipped(), clips(language, fb).c_str());
    languages::expectPicture(fb, "text_block", language, kBlockArea);
  });
}

// ---- button ----------------------------------------------------------------------------------

void test_a_focused_button_is_inverted() {
  Button button{"OK", false};
  const Rect area{10, 10, 100, 36};
  FrameBuffer idle, focused;
  button.draw(idle, area);
  button.focused = true;
  button.draw(focused, area);
  TEST_ASSERT_FALSE(idle.pixel(60, 14));      // inside an unfocused button: white
  TEST_ASSERT_TRUE(focused.pixel(60, 14));    // inside a focused one: black
  TEST_ASSERT_TRUE(idle.pixel(60, 10));       // the frame
  TEST_ASSERT_TRUE(idle.blackPixels() < focused.blackPixels());
}

void test_button_labels_fit_in_every_language() {
  languages::forEach([](const std::string& language, Translator& t) {
    const Rect left{10, 10, 112, 36}, right{128, 10, 112, 36};
    Button cancel{t.get("sos.cancel"), false};
    Button send{t.get("key.send"), true};
    FrameBuffer first, second, both;
    first.beginRegion(left);
    cancel.draw(first, left);
    first.endRegion();
    second.beginRegion(right);
    send.draw(second, right);
    second.endRegion();
    TEST_ASSERT_FALSE_MESSAGE(first.clipped(), clips(language, first).c_str());
    TEST_ASSERT_FALSE_MESSAGE(second.clipped(), clips(language, second).c_str());
    cancel.draw(both, left);
    send.draw(both, right);
    languages::expectPicture(both, "buttons", language, {0, 0, 240, 56});
  });
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_the_standard_layout_tiles_the_screen_from_top_to_bottom);
  RUN_TEST(test_the_bars_are_high_enough_for_their_font);
  RUN_TEST(test_the_symbol_constants_are_the_symbols_of_the_character_set);
  RUN_TEST(test_a_round_rectangle_with_radius_zero_is_a_rectangle);
  RUN_TEST(test_rounded_corners_cut_the_corner_pixels_symmetrically);
  RUN_TEST(test_an_outline_is_the_fill_without_its_inside_and_leaves_the_background);
  RUN_TEST(test_shapes_stay_inside_their_rectangle_even_when_it_is_tiny);
  RUN_TEST(test_a_radius_beyond_half_the_size_is_the_largest_that_fits);
  RUN_TEST(test_the_scroll_bar_shows_the_window_and_hides_when_all_is_visible);
  RUN_TEST(test_soft_key_slots_share_the_width);
  RUN_TEST(test_a_single_button_gets_the_whole_width);
  RUN_TEST(test_a_tap_acts_like_the_key_of_its_button);
  RUN_TEST(test_a_button_label_gives_up_the_space_and_then_the_symbol_when_it_does_not_fit);
  RUN_TEST(test_the_soft_keys_stay_inside_their_buttons_in_every_language);
  RUN_TEST(test_every_standard_soft_key_word_fits_either_button_with_and_without_the_arrows);
  RUN_TEST(test_the_battery_is_five_bars_rounded_up);
  RUN_TEST(test_the_left_side_names_the_most_important_thing);
  RUN_TEST(test_the_left_side_without_an_operator);
  RUN_TEST(test_the_date_is_written_the_way_the_language_writes_it);
  RUN_TEST(test_every_weekday_has_its_own_name_and_the_index_wraps);
  RUN_TEST(test_an_unknown_date_is_shown_as_dashes);
  RUN_TEST(test_the_status_bar_fits_its_area_in_every_language);
  RUN_TEST(test_the_gps_note_is_drawn_exactly_when_the_position_is_being_determined);
  RUN_TEST(test_a_long_operator_name_is_shortened_and_keeps_the_gps_note);
  RUN_TEST(test_the_weekday_is_dropped_when_the_sides_meet);
  RUN_TEST(test_the_battery_frame_shows_one_bar_per_fifth);
  RUN_TEST(test_an_empty_alert_draws_nothing_and_a_text_is_inverted);
  RUN_TEST(test_an_alert_that_is_too_long_is_shortened_inside_the_bar);
  RUN_TEST(test_alerts_with_catalog_texts_fit_in_every_language);
  RUN_TEST(test_the_selection_moves_in_reading_order_and_wraps);
  RUN_TEST(test_the_page_follows_the_selection);
  RUN_TEST(test_select_chooses_the_selected_tile_and_other_input_is_not_used);
  RUN_TEST(test_a_tap_on_a_tile_selects_and_chooses_it);
  RUN_TEST(test_only_the_tiles_of_the_page_on_show_can_be_tapped);
  RUN_TEST(test_tiles_fill_the_grid_row_by_row_without_overlap);
  RUN_TEST(test_a_page_holds_at_most_one_grid_and_empty_pages_are_dropped);
  RUN_TEST(test_setting_the_tiles_again_keeps_the_selection_while_it_exists);
  RUN_TEST(test_select_moves_the_selection_to_a_tile_or_to_the_last_one);
  RUN_TEST(test_an_empty_grid_uses_no_input_and_draws_nothing);
  RUN_TEST(test_the_selected_tile_is_inverted_and_the_others_are_raised);
  RUN_TEST(test_page_dots_show_how_many_pages_there_are_and_where_we_are);
  RUN_TEST(test_a_tap_on_a_page_dot_goes_to_that_page_without_choosing_anything);
  RUN_TEST(test_the_tiles_stay_inside_their_cells_in_every_language);
  RUN_TEST(test_the_list_selection_stops_at_both_ends);
  RUN_TEST(test_the_window_follows_the_selection_in_a_long_list);
  RUN_TEST(test_select_and_taps_choose_a_row);
  RUN_TEST(test_a_tap_below_the_last_row_of_a_short_list_hits_nothing);
  RUN_TEST(test_a_tap_in_a_scrolled_list_hits_the_row_on_show);
  RUN_TEST(test_replacing_the_items_keeps_the_selection_inside_the_list);
  RUN_TEST(test_the_selected_row_is_inverted_and_a_long_list_shows_a_scroll_bar);
  RUN_TEST(test_a_list_that_shrinks_fills_its_window_again);
  RUN_TEST(test_user_text_is_shortened_and_fixed_text_is_not);
  RUN_TEST(test_rows_stay_inside_their_row_in_every_language);
  RUN_TEST(test_a_long_text_wraps_and_scrolls_by_line);
  RUN_TEST(test_a_text_scrolled_to_the_end_fills_a_taller_area_again);
  RUN_TEST(test_a_short_text_does_not_scroll_and_a_new_text_starts_at_the_top);
  RUN_TEST(test_a_text_block_draws_the_lines_it_shows_and_nothing_else);
  RUN_TEST(test_catalog_text_in_a_text_block_stays_inside_in_every_language);
  RUN_TEST(test_a_focused_button_is_inverted);
  RUN_TEST(test_button_labels_fit_in_every_language);
  return UNITY_END();
}
