#pragma once

// The grid of big tiles of a home screen: a symbol above a short label, in rows and columns,
// on one or more pages. The selected tile is drawn inverted; the others as raised buttons
// (an outline with a one-pixel shadow below and to the right, the only way to look pressable
// on a panel without grey).
//
// Up and Down move the selection one tile back or forward in reading order and wrap around;
// the page follows the selection, there are no keys for pages. Select, or a tap on a tile,
// chooses it. A tap on a page dot goes to the first tile of that page: without it the second page
// would be reachable only by stepping through the first, on a device the child mostly touches. Labels are fixed catalog words and are never shortened (a tile is about 74 pixels
// wide; the catalog words are chosen to fit). A tile without a symbol wraps its label over
// several lines.

#include <string>
#include <vector>

#include "ui/Display.h"
#include "ui/Input.h"

namespace ui {

struct Tile {
  std::string label;
  const char* symbol = nullptr;  // UTF-8, one of ui::symbol; nullptr = text only
  std::string badge;             // a short text in the top right corner ("3"); empty = none
};

class TileGrid {
public:
  // The grid size is the screen's decision: with three columns a tile is 74 pixels wide, too
  // narrow for a long word in the pseudo-language of the layout tests; with two it is 114.
  TileGrid(int columns, int rows) : columns_(columns), rows_(rows) {}

  // The tiles by page. The selection stays on its index if there still is one.
  void setPages(std::vector<std::vector<Tile>> pages);
  void resetSelection() { selected_ = 0; }
  // Moves the selection to the tile with the global index `index`, or to the last tile when there is
  // no such one: a screen that rebuilds its tiles keeps the user on the same tile.
  void select(size_t index);

  size_t count() const;
  size_t selected() const { return selected_; }
  const Tile& selectedTile() const;
  size_t page() const;
  size_t pageCount() const { return pages_.size(); }

  // Where the tile with the global index `index` is drawn in `area`.
  Rect tileRect(const Rect& area, size_t index) const;
  // The touch target of the dot of `page`: its part of the strip under the tiles, the whole width cut
  // into one part per page, because a finger does not hit a dot. Empty when there is only one page,
  // where no dots are drawn.
  Rect dotTarget(const Rect& area, size_t page) const;

  // Used when the input moved the selection or chose a tile; `chosen` is set to the index of
  // a tile that was chosen (Select, or a tap, which also selects it).
  bool handle(const Input& input, const Rect& area, bool* chosen);
  // What a tap hits, as handle() sees it: the dots' parts of the strip (with more than one page) and the tiles
  // of the page on show.
  std::vector<Rect> targets(const Rect& area) const;

  void draw(Display& display, const Rect& area) const;

  // One tile in `cell`; its shadow takes the pixel row below and the pixel column to the right
  // of the cell. Public so that a test can draw a tile alone and declare the cell, shadow
  // included, as the region it may paint in.
  static void drawTile(Display& display, const Rect& cell, const Tile& tile, bool selected);

private:
  size_t perPage() const { return static_cast<size_t>(columns_ * rows_); }
  size_t firstOfPage(size_t page) const;

  int columns_, rows_;
  std::vector<std::vector<Tile>> pages_;
  size_t selected_ = 0;
};

}  // namespace ui
