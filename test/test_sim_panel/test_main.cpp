// Host tests for the simulated panel and its PNG writer: the PNG is checked by decoding it again
// with the rules of the format (chunk CRCs, zlib header, stored blocks, Adler-32), and the panel
// by what the refresh policy of docs/ui-refresh.md says it must do.

#include <unity.h>

#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include <Png.h>
#include <SimPanel.h>

using namespace sim;

void setUp() {}
void tearDown() {}

namespace {

struct Decoded {
  bool ok = false;
  std::string why;
  int width = 0, height = 0;
  std::vector<uint8_t> packed;  // as the encoder was given it: a set bit is black
};

uint32_t be32(const uint8_t* p) { return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3]; }

Decoded fail(const std::string& why) {
  Decoded d;
  d.why = why;
  return d;
}

// Everything a viewer would insist on, and nothing more.
Decoded decode(const std::vector<uint8_t>& png) {
  static const uint8_t signature[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
  if (png.size() < 8 || !std::equal(signature, signature + 8, png.begin())) return fail("signature");
  Decoded d;
  std::vector<uint8_t> zlib;
  bool sawHeader = false, sawEnd = false;
  size_t at = 8;
  while (at < png.size() && !sawEnd) {
    if (at + 12 > png.size()) return fail("truncated chunk");
    const uint32_t length = be32(&png[at]);
    if (at + 12 + length > png.size()) return fail("chunk longer than the file");
    const std::string type(reinterpret_cast<const char*>(&png[at + 4]), 4);
    if (checksum::crc32(&png[at + 4], 4 + length) != be32(&png[at + 8 + length])) return fail("crc of " + type);
    const uint8_t* data = &png[at + 8];
    if (type == "IHDR") {
      if (length != 13 || data[8] != 1 || data[9] != 0 || data[10] != 0 || data[11] != 0 || data[12] != 0) return fail("IHDR");
      d.width = static_cast<int>(be32(data));
      d.height = static_cast<int>(be32(data + 4));
      sawHeader = true;
    } else if (type == "IDAT") {
      zlib.insert(zlib.end(), data, data + length);
    } else if (type == "IEND") {
      sawEnd = length == 0;
    } else {
      return fail("unexpected chunk " + type);
    }
    at += 12 + length;
  }
  if (!sawHeader || !sawEnd || at != png.size()) return fail("structure");

  if (zlib.size() < 6 || ((zlib[0] << 8) | zlib[1]) % 31 != 0 || (zlib[0] & 0x0F) != 8) return fail("zlib header");
  std::vector<uint8_t> raw;
  size_t z = 2;
  for (bool last = false; !last;) {
    if (z + 5 > zlib.size()) return fail("block header");
    last = zlib[z] & 1;
    if ((zlib[z] & 6) != 0) return fail("block is not stored");
    const size_t len = zlib[z + 1] | (zlib[z + 2] << 8);
    const size_t nlen = zlib[z + 3] | (zlib[z + 4] << 8);
    if ((len ^ nlen) != 0xFFFF) return fail("block length check");
    if (z + 5 + len > zlib.size()) return fail("block data");
    raw.insert(raw.end(), zlib.begin() + static_cast<long>(z + 5), zlib.begin() + static_cast<long>(z + 5 + len));
    z += 5 + len;
  }
  if (z + 4 != zlib.size() || be32(&zlib[z]) != png_detail::adler32(raw)) return fail("adler");

  const size_t stride = (static_cast<size_t>(d.width) + 7) / 8;
  if (raw.size() != (stride + 1) * static_cast<size_t>(d.height)) return fail("size of the pixel data");
  for (int y = 0; y < d.height; ++y) {
    const uint8_t* row = &raw[static_cast<size_t>(y) * (stride + 1)];
    if (row[0] != 0) return fail("filter");
    for (size_t i = 0; i < stride; ++i) d.packed.push_back(static_cast<uint8_t>(~row[1 + i]));
  }
  d.ok = true;
  return d;
}

bool blackAt(const Decoded& d, int x, int y) {
  const size_t stride = (static_cast<size_t>(d.width) + 7) / 8;
  return d.packed[static_cast<size_t>(y) * stride + static_cast<size_t>(x) / 8] & (0x80 >> (x % 8));
}

// A picture to tell apart from a blank one and from its mirror image.
void drawSomething(ui::Display& d, int variant = 0) {
  d.beginFrame();
  d.fillRect({0, 0, d.width(), d.height()}, false);
  d.fillRect({10 + variant, 20, 30, 5}, true);
  d.setPixel(239, 319, true);
  d.endFrame();
}

}  // namespace

// ---- the PNG writer -------------------------------------------------------------------------------

void test_a_panel_sized_picture_comes_back_pixel_for_pixel() {
  FrameBuffer fb;
  drawSomething(fb);
  const Decoded d = decode(encodePng(fb.packed(), fb.width(), fb.height()));
  TEST_ASSERT_TRUE_MESSAGE(d.ok, d.why.c_str());
  TEST_ASSERT_EQUAL(240, d.width);
  TEST_ASSERT_EQUAL(320, d.height);
  for (int y = 0; y < 320; ++y)
    for (int x = 0; x < 240; ++x)
      if (blackAt(d, x, y) != fb.pixel(x, y)) TEST_FAIL_MESSAGE(("pixel differs at " + std::to_string(x) + "," + std::to_string(y)).c_str());
}

void test_a_width_that_is_not_a_multiple_of_eight_keeps_every_column() {
  for (int width : {1, 7, 9, 15, 17}) {
    FrameBuffer fb(width, 3);
    fb.setPixel(width - 1, 2, true);  // the last column of the last row
    fb.setPixel(0, 0, true);
    const Decoded d = decode(encodePng(fb.packed(), width, 3));
    TEST_ASSERT_TRUE_MESSAGE(d.ok, d.why.c_str());
    TEST_ASSERT_TRUE(blackAt(d, width - 1, 2));
    TEST_ASSERT_TRUE(blackAt(d, 0, 0));
    if (width > 1) TEST_ASSERT_FALSE(blackAt(d, width - 1, 0));
  }
}

void test_a_picture_larger_than_one_stored_block_is_split_and_still_decodes() {
  // 1000 x 700 is 88,200 bytes of rows: two blocks, the first exactly 65,535 bytes.
  FrameBuffer fb(1000, 700);
  fb.fillRect({0, 0, 1000, 700}, false);
  fb.setPixel(999, 699, true);
  fb.setPixel(0, 0, true);
  const std::vector<uint8_t> png = encodePng(fb.packed(), 1000, 700);
  const Decoded d = decode(png);
  TEST_ASSERT_TRUE_MESSAGE(d.ok, d.why.c_str());
  TEST_ASSERT_TRUE(blackAt(d, 999, 699));
  TEST_ASSERT_FALSE(blackAt(d, 998, 699));
}

void test_a_single_pixel_is_a_valid_picture() {
  FrameBuffer fb(1, 1);
  fb.setPixel(0, 0, true);
  const Decoded d = decode(encodePng(fb.packed(), 1, 1));
  TEST_ASSERT_TRUE_MESSAGE(d.ok, d.why.c_str());
  TEST_ASSERT_TRUE(blackAt(d, 0, 0));
}

void test_a_damaged_png_is_refused_by_the_decoder_this_test_relies_on() {
  FrameBuffer fb;
  drawSomething(fb);
  std::vector<uint8_t> png = encodePng(fb.packed(), fb.width(), fb.height());
  png[png.size() / 2] ^= 0x01;  // one bit in the pixel data
  TEST_ASSERT_FALSE(decode(png).ok);
}

void test_the_png_of_the_known_picture_has_the_known_size() {
  // 8 signature + IHDR 25 + IDAT (12 + 2 zlib + 5 block + 320 * 31 rows + 4 adler) + IEND 12.
  FrameBuffer fb;
  TEST_ASSERT_EQUAL(8 + 25 + (12 + 2 + 5 + 320 * 31 + 4) + 12, static_cast<int>(encodePng(fb.packed(), 240, 320).size()));
}

// ---- the panel --------------------------------------------------------------------------------------

namespace {
struct Panel {
  uint32_t ms = 0;
  SimPanel panel{[this] { return ms; }};
  void frame(int variant = 0) { drawSomething(panel, variant); }
};
}  // namespace

void test_the_first_frame_is_a_full_refresh_and_blank_until_then() {
  Panel p;
  TEST_ASSERT_EQUAL(0, p.panel.shown().blackPixels());
  TEST_ASSERT_EQUAL(0, static_cast<int>(p.panel.refreshes().size()));
  p.frame();
  TEST_ASSERT_EQUAL(1, static_cast<int>(p.panel.refreshes().size()));
  TEST_ASSERT_TRUE(p.panel.refreshes()[0].full);
  TEST_ASSERT_TRUE(p.panel.shown().blackPixels() > 0);
}

void test_an_unchanged_picture_costs_no_refresh() {
  Panel p;
  p.frame();
  p.ms = 1000;
  p.frame();
  p.ms = 2000;
  p.frame();
  TEST_ASSERT_EQUAL(1, static_cast<int>(p.panel.refreshes().size()));
  TEST_ASSERT_EQUAL(3, static_cast<int>(p.panel.stats().frames));
  TEST_ASSERT_EQUAL(2, static_cast<int>(p.panel.stats().skipped));
}

void test_a_changed_picture_is_a_partial_refresh_and_the_tenth_change_is_full() {
  Panel p;
  p.frame(0);
  for (int change = 1; change <= 10; ++change) {
    p.ms = static_cast<uint32_t>(change) * 1000;
    p.frame(change);
    const SimPanel::Refresh& last = p.panel.refreshes().back();
    TEST_ASSERT_EQUAL_MESSAGE(change == 10 ? 1 : 0, last.full ? 1 : 0, std::to_string(change).c_str());
  }
  TEST_ASSERT_EQUAL(11, static_cast<int>(p.panel.refreshes().size()));
}

void test_a_static_picture_is_refreshed_in_full_after_four_hours() {
  Panel p;
  p.frame();
  p.ms = 4u * 3600 * 1000 - 1;
  p.frame();
  TEST_ASSERT_EQUAL(1, static_cast<int>(p.panel.refreshes().size()));
  p.ms = 4u * 3600 * 1000;
  p.frame();
  TEST_ASSERT_EQUAL(2, static_cast<int>(p.panel.refreshes().size()));
  TEST_ASSERT_TRUE(p.panel.refreshes()[1].full);
}

void test_a_refresh_is_busy_for_its_duration() {
  Panel p;
  TEST_ASSERT_FALSE(p.panel.busy(0));
  p.frame();  // full at 0
  TEST_ASSERT_TRUE(p.panel.busy(SimPanel::kFullMs - 1));
  TEST_ASSERT_FALSE(p.panel.busy(SimPanel::kFullMs));
  p.ms = 5000;
  p.frame(1);  // partial at 5000
  TEST_ASSERT_TRUE(p.panel.busy(5000 + SimPanel::kPartialMs - 1));
  TEST_ASSERT_FALSE(p.panel.busy(5000 + SimPanel::kPartialMs));
}

void test_the_panel_shows_the_picture_of_the_last_refresh_not_of_the_last_frame() {
  Panel p;
  p.frame(0);
  const FrameBuffer first = p.panel.shown();
  p.panel.beginFrame();  // a frame that is drawn but never ended: nothing reaches the panel
  p.panel.fillRect({0, 0, 240, 320}, true);
  TEST_ASSERT_TRUE(first == p.panel.shown());
}

void test_the_previous_picture_is_kept_while_the_last_refresh_runs() {
  Panel p;
  p.frame(0);
  const FrameBuffer first = p.panel.shown();
  p.ms = 1000;
  p.frame(5);
  TEST_ASSERT_TRUE(first == p.panel.previous());
  TEST_ASSERT_FALSE(first == p.panel.shown());
}

void test_the_png_is_the_picture_on_the_panel() {
  Panel p;
  p.frame();
  const Decoded d = decode(p.panel.png());
  TEST_ASSERT_TRUE_MESSAGE(d.ok, d.why.c_str());
  TEST_ASSERT_TRUE(d.packed == p.panel.shown().packed());
}

void test_a_png_is_written_to_a_file_and_an_unwritable_path_is_reported() {
  Panel p;
  p.frame();
  const std::string path = (std::filesystem::temp_directory_path() / "phoneplus-sim-panel-test.png").string();
  TEST_ASSERT_TRUE(p.panel.writePng(path));
  FILE* file = std::fopen(path.c_str(), "rb");
  TEST_ASSERT_NOT_NULL(file);
  std::vector<uint8_t> bytes(100000);
  const size_t n = std::fread(bytes.data(), 1, bytes.size(), file);
  std::fclose(file);
  std::remove(path.c_str());
  bytes.resize(n);
  TEST_ASSERT_TRUE(bytes == p.panel.png());
  TEST_ASSERT_FALSE(p.panel.writePng("/nonexistent-directory/panel.png"));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_a_panel_sized_picture_comes_back_pixel_for_pixel);
  RUN_TEST(test_a_width_that_is_not_a_multiple_of_eight_keeps_every_column);
  RUN_TEST(test_a_picture_larger_than_one_stored_block_is_split_and_still_decodes);
  RUN_TEST(test_a_single_pixel_is_a_valid_picture);
  RUN_TEST(test_a_damaged_png_is_refused_by_the_decoder_this_test_relies_on);
  RUN_TEST(test_the_png_of_the_known_picture_has_the_known_size);
  RUN_TEST(test_the_first_frame_is_a_full_refresh_and_blank_until_then);
  RUN_TEST(test_an_unchanged_picture_costs_no_refresh);
  RUN_TEST(test_a_changed_picture_is_a_partial_refresh_and_the_tenth_change_is_full);
  RUN_TEST(test_a_static_picture_is_refreshed_in_full_after_four_hours);
  RUN_TEST(test_a_refresh_is_busy_for_its_duration);
  RUN_TEST(test_the_panel_shows_the_picture_of_the_last_refresh_not_of_the_last_frame);
  RUN_TEST(test_the_previous_picture_is_kept_while_the_last_refresh_runs);
  RUN_TEST(test_the_png_is_the_picture_on_the_panel);
  RUN_TEST(test_a_png_is_written_to_a_file_and_an_unwritable_path_is_reported);
  return UNITY_END();
}
