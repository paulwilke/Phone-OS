#pragma once

// A paragraph of wrapped text in a rectangle, scrollable by line: a message being read, the
// text of a warning. Wrapped at word boundaries (see ui::text::wrap), never shortened - the
// end of a message is often the point, so what does not fit is reached by scrolling.
//
// Up and Down scroll one line; the scroll bar on the right shows the position in a longer
// text.

#include <string>

#include "ui/Display.h"
#include "ui/Fonts.h"
#include "ui/Input.h"

namespace ui {

class TextBlock {
public:
  explicit TextBlock(FontSize size = FontSize::Text) : size_(size) {}

  // Replaces the text and scrolls back to the start.
  void setText(std::string text);

  // How many lines the text takes in `area`, and how many of them fit.
  size_t lineCount(const Rect& area) const;
  size_t visibleLines(const Rect& area) const;
  size_t firstLine() const { return first_; }

  bool handle(const Input& input, const Rect& area);
  void draw(Display& display, const Rect& area);

private:
  int textWidth(const Rect& area) const;

  FontSize size_;
  std::string text_;
  size_t first_ = 0;
};

}  // namespace ui
