#pragma once

// A scrolling list of one-line rows: conversations, contacts, settings. The selected row is
// drawn inverted, a scroll bar on the right shows where in a longer list the window is.
//
// Up and Down move the selection by one row and stop at the ends (a list has a start and an
// end, unlike the tiles of a home screen). Select, or a tap on a row, chooses it.
//
// Text from the user (a contact's name, a message) is shortened with "..." to the row's width;
// a row whose text is a fixed catalog word says so with `userText = false` and is never
// shortened, so a word that does not fit shows up as a clip in the layout tests.

#include <string>
#include <vector>

#include "ui/Display.h"
#include "ui/Input.h"

namespace ui {

struct ListItem {
  std::string text;
  bool userText = true;
};

class ListView {
public:
  void setItems(std::vector<ListItem> items);
  void resetSelection() { selected_ = 0; first_ = 0; }
  // Puts the selection on `index` (ignored when there is no such row); the window follows when it is drawn.
  void select(size_t index) {
    if (index < items_.size()) selected_ = index;
  }

  size_t count() const { return items_.size(); }
  size_t selected() const { return selected_; }
  // The first row on show, for the area last used to draw or handle.
  size_t firstVisible() const { return first_; }

  int rowHeight() const;
  size_t visibleRows(const Rect& area) const;
  Rect rowRect(const Rect& area, size_t index) const;  // index relative to the first row on show

  // `chosen` is set to true when a row was chosen (Select, or a tap, which also selects it).
  bool handle(const Input& input, const Rect& area, bool* chosen);
  // What a tap hits: the rows on show, from the first one the last draw or input put in the window.
  std::vector<Rect> targets(const Rect& area) const;

  void draw(Display& display, const Rect& area);

private:
  void scrollToSelection(const Rect& area);

  std::vector<ListItem> items_;
  size_t selected_ = 0;
  size_t first_ = 0;
};

}  // namespace ui
