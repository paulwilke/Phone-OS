// The simulator in a browser page (WebAssembly, built by scripts/build_wasm.sh; docs/simulator.md, D-58): the entry
// point beside main.cpp, which it replaces. No thread, no standard input, no window and no socket: a non-blocking loop
// that the page's animation frames drive, the panel handed to the page as pixels to draw on a canvas, and the keys,
// the touch and the console commands that make a scene come in through the functions below. src/ knows nothing of it.
#include <emscripten.h>

#include <iostream>
#include <memory>
#include <string>

#include "Simulator.h"

namespace {
std::unique_ptr<sim::Simulator> g_sim;
double g_last = 0;
uint32_t g_pixels[ui::kScreenWidth * ui::kScreenHeight];

void draw() {
  const sim::SimPanel& panel = g_sim->panel();
  const uint32_t nowMs = g_sim->nowMs();
  const bool busy = panel.busy(nowMs);
  const bool flash = busy && !panel.refreshes().empty() && panel.refreshes().back().full &&
                     nowMs - panel.refreshes().back().atMs < sim::SimPanel::kFullMs / 2;
  const FrameBuffer& picture = busy ? panel.previous() : panel.shown();
  for (int y = 0; y < ui::kScreenHeight; ++y)
    for (int x = 0; x < ui::kScreenWidth; ++x) {
      const bool black = picture.pixel(x, y) != flash;
      g_pixels[y * ui::kScreenWidth + x] = black ? 0xFF181A1Au : 0xFFDCE6E6u;  // ABGR for canvas
    }
  EM_ASM({ if (Module.onFrame) Module.onFrame($0, $1, $2); }, g_pixels, ui::kScreenWidth, ui::kScreenHeight);
}

void loop() {
  const double now = emscripten_get_now();
  const double elapsed = now - g_last;
  g_last = now;
  g_sim->advance(static_cast<uint64_t>(elapsed) * g_sim->lapse());
  draw();
}
}  // namespace

extern "C" {
EMSCRIPTEN_KEEPALIVE void phone_key(int k) {
  if (!g_sim) return;
  switch (k) {
    case 0: g_sim->press(ui::Key::Up); break;
    case 1: g_sim->press(ui::Key::Down); break;
    case 2: g_sim->press(ui::Key::Select); break;
    case 3: g_sim->press(ui::Key::Back); break;
    case 4: g_sim->press(ui::Key::Home); break;
    case 5: g_sim->press(ui::Key::Inbox); break;
    case 6: g_sim->press(ui::Key::Compose); break;
  }
}
EMSCRIPTEN_KEEPALIVE void phone_text(unsigned cp) { if (g_sim) g_sim->typeCharacter(cp); }
EMSCRIPTEN_KEEPALIVE void phone_tap(int x, int y) { if (g_sim) g_sim->tap(x, y); }
EMSCRIPTEN_KEEPALIVE void phone_cmd(const char* line) { if (g_sim) g_sim->execute(line); }
}

int main(int argc, char** argv) {
  sim::Options options;
  options.dataDir = "/data";
  options.langDir = "/lang";
  // The words only host tools show are not shipped: the page has its own.
  options.hostLangDir = "/nonexistent";
  options.workDir = "/work";
  options.goldenDir = "/golden";
  options.webPort = 0;
  // The languages of the device, as the web page's demo has them.
  options.pseudoLanguage = false;
  g_sim = std::make_unique<sim::Simulator>(options, std::cout);
  if (!g_sim->ok()) {
    std::cerr << "error: " << g_sim->error() << "\n";
    return 2;
  }
  // The page's language, as the argument of the page's script (a code of the page's catalogs).
  if (argc > 1) {
    const std::string language = argv[1];
    if (!g_sim->execute("language " + language)) std::cerr << "error: no language " << language << "\n";
  }
  g_last = emscripten_get_now();
  emscripten_set_main_loop(loop, 0, 1);
  return 0;
}
