#pragma once

// Rows that each set one thing: a box with what it sets in the small font and, under it, how it is set
// now in the text font. The one in focus is drawn inverted. It is for the options and the Wi-Fi page,
// where a one-line list row has no room for both the question and the answer.
//
// A row without a value is a button: its label is drawn in the text font in a lower box. All rows of one
// list are as high as the highest needs, so the list does not change its rhythm when a value appears.
//
// Up and Down move the focus and stop at the ends, like every list; Select, or a tap on a row, chooses
// it. Longer lists scroll with the focus and show a scroll bar. The words of a row are fixed catalog
// words and values and are never shortened: one that does not fit is a layout bug that the tests find.

#include <string>
#include <vector>

#include "ui/Display.h"
#include "ui/Input.h"

namespace ui {

struct OptionRow {
  std::string label;
  std::string value;  // empty: the row is a button
  // What is in force now, for buttons that choose between ways (Wi-Fi: home network, access point,
  // off): drawn with a second outline inside the first, which shows in the focus colours too.
  bool current = false;
};

class OptionRows {
public:
  void setRows(std::vector<OptionRow> rows);
  void resetSelection() { selected_ = 0; first_ = 0; }

  size_t count() const { return rows_.size(); }
  size_t selected() const { return selected_; }

  int rowHeight() const;
  size_t visibleRows(const Rect& area) const;
  // Where row `index` (counted from the first one on show) is drawn.
  Rect rowRect(const Rect& area, size_t index) const;

  // `chosen` is set to true when a row was chosen (Select, or a tap, which also moves the focus there).
  bool handle(const Input& input, const Rect& area, bool* chosen);

  void draw(Display& display, const Rect& area);

private:
  void scrollToSelection(const Rect& area);

  std::vector<OptionRow> rows_;
  size_t selected_ = 0;
  size_t first_ = 0;
};

}  // namespace ui
