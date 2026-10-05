#pragma once

// The simulator's window, built only with -DPHONEPLUS_SIM_SDL=1 (environment `sim-sdl`). It shows
// what the panel shows, with the refresh made visible: a partial refresh keeps the old picture on
// show until it is done, a full refresh flashes. The Mac keyboard is the device's keyboard (arrows,
// Enter, Backspace, F1 / F2 / F3 for the three keys under the screen, typed characters as text) and
// a click is a touch. Nothing in here decides anything: it hands keys to the Simulator.

#if PHONEPLUS_SIM_SDL

#include <SDL.h>

#include <cstdint>
#include <string>

#include "SimPanel.h"
#include "Simulator.h"

namespace sim {

class SdlWindow {
public:
  explicit SdlWindow(int scale) : scale_(scale) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) return;
    window_ = SDL_CreateWindow("phoneplus simulator", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                               ui::kScreenWidth * scale_, ui::kScreenHeight * scale_, 0);
    if (!window_) return;
    renderer_ = SDL_CreateRenderer(window_, -1, 0);
    if (!renderer_) return;
    texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, ui::kScreenWidth,
                                 ui::kScreenHeight);
    SDL_StartTextInput();
  }
  ~SdlWindow() {
    if (texture_) SDL_DestroyTexture(texture_);
    if (renderer_) SDL_DestroyRenderer(renderer_);
    if (window_) SDL_DestroyWindow(window_);
    SDL_Quit();
  }
  SdlWindow(const SdlWindow&) = delete;
  SdlWindow& operator=(const SdlWindow&) = delete;

  bool ok() const { return texture_ != nullptr; }

  // Hands the events to the simulator. False when the window was closed.
  bool pump(Simulator& sim) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      switch (e.type) {
        case SDL_QUIT: return false;
        case SDL_MOUSEBUTTONDOWN:
          if (e.button.button == SDL_BUTTON_LEFT) sim.tap(e.button.x / scale_, e.button.y / scale_);
          break;
        case SDL_KEYDOWN: key(sim, e.key.keysym.sym); break;
        case SDL_TEXTINPUT:
          for (const char* p = e.text.text; *p;) p += codePoint(p, sim);
          break;
        default: break;
      }
    }
    return true;
  }

  // Draws the panel as it looks at `nowMs`.
  void present(const SimPanel& panel, uint32_t nowMs) {
    const bool busy = panel.busy(nowMs);
    const bool flash = busy && !panel.refreshes().empty() && panel.refreshes().back().full &&
                       nowMs - panel.refreshes().back().atMs < SimPanel::kFullMs / 2;
    const FrameBuffer& picture = busy ? panel.previous() : panel.shown();
    uint32_t pixels[ui::kScreenWidth * ui::kScreenHeight];
    for (int y = 0; y < ui::kScreenHeight; ++y)
      for (int x = 0; x < ui::kScreenWidth; ++x) {
        const bool black = picture.pixel(x, y) != flash;  // the flash is the picture inverted
        pixels[y * ui::kScreenWidth + x] = black ? 0xFF1A1A18u : 0xFFE6E6DCu;
      }
    SDL_UpdateTexture(texture_, nullptr, pixels, ui::kScreenWidth * static_cast<int>(sizeof(uint32_t)));
    SDL_RenderClear(renderer_);
    SDL_RenderCopy(renderer_, texture_, nullptr, nullptr);
    SDL_RenderPresent(renderer_);
  }

private:
  void key(Simulator& sim, SDL_Keycode code) {
    switch (code) {
      case SDLK_UP: sim.press(ui::Key::Up); break;
      case SDLK_DOWN: sim.press(ui::Key::Down); break;
      case SDLK_RETURN: case SDLK_KP_ENTER: sim.press(ui::Key::Select); break;
      case SDLK_BACKSPACE: sim.press(ui::Key::Back); break;
      case SDLK_F1: sim.press(ui::Key::Home); break;
      case SDLK_F2: sim.press(ui::Key::Inbox); break;
      case SDLK_F3: sim.press(ui::Key::Compose); break;
      default: break;
    }
  }
  // One UTF-8 code point of typed text to the simulator; returns how many bytes it took.
  static int codePoint(const char* p, Simulator& sim) {
    const unsigned char c = static_cast<unsigned char>(*p);
    const int len = c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4;
    uint32_t cp = len == 1 ? c : c & (0xFF >> (len + 1));
    for (int k = 1; k < len && p[k]; ++k) cp = (cp << 6) | (static_cast<unsigned char>(p[k]) & 0x3F);
    sim.typeCharacter(cp);
    return len;
  }

  int scale_;
  SDL_Window* window_ = nullptr;
  SDL_Renderer* renderer_ = nullptr;
  SDL_Texture* texture_ = nullptr;
};

}  // namespace sim

#endif  // PHONEPLUS_SIM_SDL
