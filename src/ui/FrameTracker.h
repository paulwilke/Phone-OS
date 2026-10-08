#pragma once

// Remembers the checksum of the picture on the panel and says whether the next frame is a
// change. The first frame always is one: the panel's content is not known.
//
// The checksum is checksum::crc32 of the packed picture; two different pictures with the same
// checksum would be taken for one, and the panel would miss an update. For a 240 x 320 picture
// that is a one-in-four-billion chance per change, and the next change corrects it.

#include <cstdint>

namespace ui {

class FrameTracker {
public:
  // True when `crc` differs from the picture on the panel (or nothing is known about it).
  bool update(uint32_t crc) {
    ++frames_;
    const bool changed = !known_ || crc != last_;
    if (changed) ++changes_;
    known_ = true;
    last_ = crc;
    return changed;
  }

  uint32_t frames() const { return frames_; }    // frames drawn
  uint32_t changes() const { return changes_; }  // of which different from the one before

private:
  bool known_ = false;
  uint32_t last_ = 0;
  uint32_t frames_ = 0, changes_ = 0;
};

}  // namespace ui
