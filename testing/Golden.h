#pragma once

// Golden files for widget tests: a small text file with the ASCII art of what a widget
// drew (`#` black, `.` white), stored under test/golden/ and reviewed like code. A
// change in how something looks shows up as a change of the file in the diff of the
// pull request, which is the point.
//
//   golden::expect(fb, "tile_grid_en", region);   // in a test: compares, or fails
//
// With the environment variable UPDATE_GOLDEN set (to anything), expect() writes the
// file instead of comparing, and passes. A missing file fails with a message that says
// how to create it; it is never created silently.
//
// A mismatch reports the rows that differ, expected above actual, with a marker, instead
// of dumping two screens. Tests of the helper itself call compare() with their own
// directory and look at the result.

#include <unity.h>

#include "GoldenCompare.h"

namespace golden {

inline void expect(const FrameBuffer& fb, const std::string& name, const ui::Rect& region) {
  const Result r = compare(fb, name, region, defaultDirectory(), updateRequested());
  TEST_ASSERT_TRUE_MESSAGE(r.ok, r.message.c_str());
}
inline void expect(const FrameBuffer& fb, const std::string& name) {
  expect(fb, name, {0, 0, fb.width(), fb.height()});
}

}  // namespace golden
