// Host tests for the frame buffer (drawing, clip detection, ASCII art, CRC) and the
// golden-file helper.

#include <unity.h>

#include <filesystem>
#include <fstream>

#include <Golden.h>

using ui::Rect;

void setUp() {}
void tearDown() {}

namespace {

std::string scratchDirectory(const char* name) {
  const std::string dir = std::string("/tmp/phoneplus-golden-test-") + name;
  std::filesystem::remove_all(dir);
  return dir;
}

}  // namespace

// ---- drawing ----------------------------------------------------------------------------

void test_a_new_buffer_is_white_and_screen_sized() {
  FrameBuffer fb;
  TEST_ASSERT_EQUAL(240, fb.width());
  TEST_ASSERT_EQUAL(320, fb.height());
  TEST_ASSERT_EQUAL(0, fb.blackPixels());
  TEST_ASSERT_FALSE(fb.clipped());
}

void test_set_pixel_and_white_over_black() {
  FrameBuffer fb(8, 8);
  fb.setPixel(3, 4, true);
  TEST_ASSERT_TRUE(fb.pixel(3, 4));
  fb.setPixel(3, 4, false);
  TEST_ASSERT_FALSE(fb.pixel(3, 4));
}

void test_fill_and_outline() {
  FrameBuffer fb(10, 10);
  fb.fillRect({2, 2, 4, 3}, true);
  TEST_ASSERT_EQUAL(12, fb.blackPixels());
  fb.clear();
  fb.outlineRect({1, 1, 5, 4}, true);
  TEST_ASSERT_EQUAL(14, fb.blackPixels());  // 5+5 top/bottom, 2+2 sides
  TEST_ASSERT_FALSE(fb.pixel(2, 2));        // the inside stays white
  fb.clear();
  fb.outlineRect({1, 1, 1, 1}, true);       // one pixel
  TEST_ASSERT_EQUAL(1, fb.blackPixels());
  fb.clear();
  fb.outlineRect({1, 1, 5, 1}, true);       // one row
  TEST_ASSERT_EQUAL(5, fb.blackPixels());
  fb.outlineRect({0, 0, 0, 0}, true);       // nothing
  TEST_ASSERT_EQUAL(5, fb.blackPixels());
}

// Rows start at a byte boundary and the leftmost pixel is the top bit; clear bits are transparent.
void test_bitmap_packing_and_transparency() {
  FrameBuffer fb(12, 4);
  fb.fillRect({0, 0, 12, 4}, true);
  const uint8_t bits[] = {0b10100000, 0b10000000,   // 9 wide: two bytes per row
                          0b01000000, 0b01000000};
  fb.drawBitmap(0, 0, bits, 9, 2, false);  // white where the bits are set
  TEST_ASSERT_EQUAL_STRING(
      ".#.#####.###\n"
      "#.##########\n"
      "############\n"
      "############\n",
      fb.toAscii().c_str());
  // Nine pixels wide: the first row has pixels 0, 2 and 8 set; the second row has pixel 1 set
  // (its bit 9 would be column 9, which does not exist).
}

// ---- clipping ----------------------------------------------------------------------------

void test_drawing_inside_the_declared_region_is_not_clipped() {
  FrameBuffer fb;
  fb.beginRegion({10, 10, 20, 20});
  fb.fillRect({10, 10, 20, 20}, true);
  fb.setPixel(29, 29, true);
  fb.outlineRect({10, 10, 20, 20}, false);
  fb.endRegion();
  TEST_ASSERT_FALSE_MESSAGE(fb.clipped(), fb.describeClips().c_str());
}

void test_a_fill_that_is_too_large_is_reported_with_its_first_offender() {
  FrameBuffer fb;
  fb.beginRegion({10, 10, 20, 20});
  fb.fillRect({10, 10, 21, 20}, true);  // one column too many
  fb.endRegion();
  TEST_ASSERT_TRUE(fb.clipped());
  TEST_ASSERT_EQUAL(1, fb.clipEvents().size());
  const auto& e = fb.clipEvents()[0];
  TEST_ASSERT_EQUAL_STRING("fillRect", e.call.c_str());
  TEST_ASSERT_EQUAL(20, e.outside);
  TEST_ASSERT_EQUAL(0, e.offScreen);
  TEST_ASSERT_EQUAL(30, e.firstX);
  TEST_ASSERT_EQUAL(10, e.firstY);
  TEST_ASSERT_TRUE(fb.describeClips().find("fillRect") != std::string::npos);
}

// Only the pixels a bitmap would set count, not its empty margin.
void test_a_bitmap_is_clipped_only_by_its_set_bits() {
  FrameBuffer fb;
  fb.beginRegion({0, 0, 8, 8});
  const uint8_t bits[] = {0b11000000, 0b00000011};  // 8 wide, 2 rows... first row left, second row right
  fb.drawBitmap(0, 0, bits, 8, 2, true);
  TEST_ASSERT_FALSE(fb.clipped());
  fb.drawBitmap(4, 0, bits, 8, 2, true);  // the second row's set bits land at x 10, 11
  TEST_ASSERT_TRUE(fb.clipped());
  TEST_ASSERT_EQUAL(2, fb.clipEvents()[0].outside);
  fb.endRegion();
}

void test_white_drawing_outside_the_region_is_reported_too() {
  FrameBuffer fb;
  fb.beginRegion({0, 0, 5, 5});
  fb.setPixel(5, 0, false);
  fb.endRegion();
  TEST_ASSERT_TRUE(fb.clipped());
}

void test_pixels_off_the_screen_are_dropped_and_reported_even_without_a_region() {
  FrameBuffer fb(10, 10);
  fb.setPixel(-1, 0, true);
  fb.setPixel(10, 0, true);
  fb.fillRect({8, 8, 4, 4}, true);  // 12 of 16 pixels are off the screen
  TEST_ASSERT_EQUAL(3, fb.clipEvents().size());
  TEST_ASSERT_EQUAL(12, fb.clipEvents()[2].offScreen);
  TEST_ASSERT_EQUAL(4, fb.blackPixels());  // the ones that were on the screen
}

// A region that reaches off the screen does not make the off-screen part acceptable.
void test_a_region_beyond_the_screen_edge_still_reports_off_screen_pixels() {
  FrameBuffer fb(10, 10);
  fb.beginRegion({5, 5, 20, 20});
  fb.setPixel(12, 6, true);
  fb.endRegion();
  TEST_ASSERT_TRUE(fb.clipped());
  TEST_ASSERT_EQUAL(1, fb.clipEvents()[0].offScreen);
  TEST_ASSERT_EQUAL(1, fb.clipEvents()[0].outside);  // counted as outside as well
}

void test_regions_nest_and_the_innermost_one_is_the_rule() {
  FrameBuffer fb;
  fb.beginRegion({0, 0, 100, 100});
  fb.beginRegion({10, 10, 10, 10});
  fb.setPixel(50, 50, true);  // inside the outer, outside the inner
  fb.endRegion();
  TEST_ASSERT_TRUE(fb.clipped());
  fb.clearClipEvents();
  fb.setPixel(50, 50, true);  // back in the outer region
  fb.endRegion();
  TEST_ASSERT_FALSE(fb.clipped());
  fb.setPixel(300, 0, true);  // no region left: only the screen edge counts
  TEST_ASSERT_EQUAL(1, fb.clipEvents().size());
}

void test_an_empty_region_clips_everything() {
  FrameBuffer fb;
  fb.beginRegion({10, 10, 0, 0});
  fb.setPixel(10, 10, true);
  fb.endRegion();
  TEST_ASSERT_TRUE(fb.clipped());
}

void test_end_region_without_a_region_is_harmless() {
  FrameBuffer fb;
  fb.endRegion();
  fb.setPixel(1, 1, true);
  TEST_ASSERT_FALSE(fb.clipped());
}

// ---- frames ----------------------------------------------------------------------------

void test_frames_are_counted() {
  FrameBuffer fb;
  fb.beginFrame();
  fb.endFrame();
  fb.beginFrame();
  fb.endFrame();
  TEST_ASSERT_EQUAL(2, fb.frames());
  TEST_ASSERT_EQUAL(2, fb.endedFrames());
}

// ---- ASCII art -------------------------------------------------------------------------

void test_ascii_art_round_trips() {
  FrameBuffer fb(6, 3);
  fb.setPixel(0, 0, true);
  fb.setPixel(5, 2, true);
  fb.setPixel(2, 1, true);
  const std::string art = fb.toAscii();
  TEST_ASSERT_EQUAL_STRING("#.....\n..#...\n.....#\n", art.c_str());
  FrameBuffer back(1, 1);
  TEST_ASSERT_TRUE(FrameBuffer::fromAscii(art, back));
  TEST_ASSERT_TRUE(back == fb);
}

void test_ascii_art_of_a_rectangle_and_outside_it() {
  FrameBuffer fb(6, 3);
  fb.setPixel(1, 1, true);
  TEST_ASSERT_EQUAL_STRING(".#\n..\n", fb.toAscii({0, 1, 2, 2}).c_str());
  TEST_ASSERT_EQUAL_STRING("..\n..\n", fb.toAscii({5, 2, 2, 2}).c_str());  // partly off the screen reads white
}

void test_malformed_ascii_art_is_refused() {
  FrameBuffer out;
  TEST_ASSERT_FALSE(FrameBuffer::fromAscii("", out));
  TEST_ASSERT_FALSE(FrameBuffer::fromAscii("\n\n", out));
  TEST_ASSERT_FALSE(FrameBuffer::fromAscii("##\n#\n", out));    // uneven lines
  TEST_ASSERT_FALSE(FrameBuffer::fromAscii("#x\n..\n", out));   // a foreign character
  TEST_ASSERT_FALSE(FrameBuffer::fromAscii("# #\n", out));
  TEST_ASSERT_EQUAL(240, out.width());                        // unchanged on failure
  TEST_ASSERT_TRUE(FrameBuffer::fromAscii("#.\r\n.#\r\n", out));  // windows line breaks are fine
  TEST_ASSERT_TRUE(out.pixel(1, 1));
  TEST_ASSERT_TRUE(FrameBuffer::fromAscii("#.\n.#", out));        // no final line break
}

// ---- CRC -------------------------------------------------------------------------------

void test_the_crc_follows_the_content() {
  FrameBuffer a, b;
  TEST_ASSERT_EQUAL_UINT32(a.crc(), b.crc());
  a.setPixel(100, 100, true);
  TEST_ASSERT_TRUE(a.crc() != b.crc());
  b.setPixel(100, 100, true);
  TEST_ASSERT_EQUAL_UINT32(a.crc(), b.crc());
  a.setPixel(100, 100, true);  // drawing the same again changes nothing
  TEST_ASSERT_EQUAL_UINT32(b.crc(), a.crc());
}

void test_the_crc_sees_a_single_pixel_anywhere_and_the_size() {
  FrameBuffer base;
  const uint32_t white = base.crc();
  for (const auto& p : {std::pair<int, int>{0, 0}, {239, 0}, {0, 319}, {239, 319}, {7, 8}, {8, 7}}) {
    FrameBuffer fb;
    fb.setPixel(p.first, p.second, true);
    TEST_ASSERT_TRUE(fb.crc() != white);
  }
  FrameBuffer wide(320, 240), tall(240, 320);
  TEST_ASSERT_TRUE(wide.crc() != tall.crc());
}

void test_crc32_of_a_known_text() {
  const uint8_t text[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
  TEST_ASSERT_EQUAL_UINT32(0xCBF43926u, checksum::crc32(text, sizeof(text)));  // the standard check value
}

// ---- golden files --------------------------------------------------------------------------

void test_golden_update_writes_and_then_compare_passes() {
  const std::string dir = scratchDirectory("write");
  FrameBuffer fb(6, 3);
  fb.setPixel(2, 1, true);
  golden::Result w = golden::compare(fb, "picture", {0, 0, 6, 3}, dir, true);
  TEST_ASSERT_TRUE(w.ok && w.written);
  golden::Result c = golden::compare(fb, "picture", {0, 0, 6, 3}, dir, false);
  TEST_ASSERT_TRUE_MESSAGE(c.ok, c.message.c_str());
  bool found = false;
  TEST_ASSERT_EQUAL_STRING("......\n..#...\n......\n", golden::readFile(dir + "/picture.txt", &found).c_str());
  TEST_ASSERT_TRUE(found);
}

void test_a_missing_golden_file_fails_and_says_how_to_create_it() {
  const std::string dir = scratchDirectory("missing");
  FrameBuffer fb(4, 2);
  golden::Result r = golden::compare(fb, "nothing", {0, 0, 4, 2}, dir, false);
  TEST_ASSERT_FALSE(r.ok);
  TEST_ASSERT_TRUE(r.message.find("does not exist") != std::string::npos);
  TEST_ASSERT_TRUE(r.message.find("UPDATE_GOLDEN=1") != std::string::npos);
  TEST_ASSERT_FALSE(std::filesystem::exists(dir + "/nothing.txt"));  // never created silently
}

void test_a_mismatch_reports_the_differing_rows_only() {
  const std::string dir = scratchDirectory("diff");
  FrameBuffer fb(6, 4);
  fb.setPixel(1, 1, true);
  golden::compare(fb, "p", {0, 0, 6, 4}, dir, true);
  fb.setPixel(1, 1, false);
  fb.setPixel(4, 2, true);
  golden::Result r = golden::compare(fb, "p", {0, 0, 6, 4}, dir, false);
  TEST_ASSERT_FALSE(r.ok);
  TEST_ASSERT_TRUE(r.message.find("row 1") != std::string::npos);
  TEST_ASSERT_TRUE(r.message.find("row 2") != std::string::npos);
  TEST_ASSERT_TRUE(r.message.find("row 0") == std::string::npos);
  TEST_ASSERT_TRUE(r.message.find("expected .#....") != std::string::npos);
  TEST_ASSERT_TRUE(r.message.find("actual   ......") != std::string::npos);
}

void test_a_difference_of_size_is_reported() {
  const std::string dir = scratchDirectory("size");
  FrameBuffer fb(6, 4);
  golden::compare(fb, "p", {0, 0, 6, 4}, dir, true);
  golden::Result r = golden::compare(fb, "p", {0, 0, 6, 5}, dir, false);
  TEST_ASSERT_FALSE(r.ok);
  TEST_ASSERT_TRUE(r.message.find("size differs") != std::string::npos);
}

void test_a_long_diff_is_cut_with_a_count() {
  const std::string dir = scratchDirectory("long");
  FrameBuffer fb(4, 20);
  golden::compare(fb, "p", {0, 0, 4, 20}, dir, true);
  fb.fillRect({0, 0, 4, 20}, true);
  golden::Result r = golden::compare(fb, "p", {0, 0, 4, 20}, dir, false);
  TEST_ASSERT_TRUE(r.message.find("14 more differing row(s)") != std::string::npos);
}

void test_the_comparison_is_exact_not_approximate() {
  const std::string dir = scratchDirectory("exact");
  FrameBuffer fb(40, 40);
  fb.fillRect({5, 5, 30, 30}, true);
  golden::compare(fb, "p", {0, 0, 40, 40}, dir, true);
  fb.setPixel(20, 20, false);  // a single pixel
  TEST_ASSERT_FALSE(golden::compare(fb, "p", {0, 0, 40, 40}, dir, false).ok);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_a_new_buffer_is_white_and_screen_sized);
  RUN_TEST(test_set_pixel_and_white_over_black);
  RUN_TEST(test_fill_and_outline);
  RUN_TEST(test_bitmap_packing_and_transparency);
  RUN_TEST(test_drawing_inside_the_declared_region_is_not_clipped);
  RUN_TEST(test_a_fill_that_is_too_large_is_reported_with_its_first_offender);
  RUN_TEST(test_a_bitmap_is_clipped_only_by_its_set_bits);
  RUN_TEST(test_white_drawing_outside_the_region_is_reported_too);
  RUN_TEST(test_pixels_off_the_screen_are_dropped_and_reported_even_without_a_region);
  RUN_TEST(test_a_region_beyond_the_screen_edge_still_reports_off_screen_pixels);
  RUN_TEST(test_regions_nest_and_the_innermost_one_is_the_rule);
  RUN_TEST(test_an_empty_region_clips_everything);
  RUN_TEST(test_end_region_without_a_region_is_harmless);
  RUN_TEST(test_frames_are_counted);
  RUN_TEST(test_ascii_art_round_trips);
  RUN_TEST(test_ascii_art_of_a_rectangle_and_outside_it);
  RUN_TEST(test_malformed_ascii_art_is_refused);
  RUN_TEST(test_the_crc_follows_the_content);
  RUN_TEST(test_the_crc_sees_a_single_pixel_anywhere_and_the_size);
  RUN_TEST(test_crc32_of_a_known_text);
  RUN_TEST(test_golden_update_writes_and_then_compare_passes);
  RUN_TEST(test_a_missing_golden_file_fails_and_says_how_to_create_it);
  RUN_TEST(test_a_mismatch_reports_the_differing_rows_only);
  RUN_TEST(test_a_difference_of_size_is_reported);
  RUN_TEST(test_a_long_diff_is_cut_with_a_count);
  RUN_TEST(test_the_comparison_is_exact_not_approximate);
  return UNITY_END();
}
