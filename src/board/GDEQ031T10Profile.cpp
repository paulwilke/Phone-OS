#include "GDEQ031T10Profile.h"

#include "Uc8253.h"

// Sources, by short name (document names and versions: docs/device-display.md):
//   DS   the panel specification GDEQ031T10, revision 1.0, section 3.6 (operation flow, power sequence)
//   UC   the UC8253 datasheet (command descriptions)
//   DEMO what the vendor's published demo for this panel sends to it (read for the values, not copied)
// The panel runs on the controller's own waveform (OTP). There is no waveform table, no power setting and
// no booster setting in this profile, because neither the demo nor the datasheet's flow sends one; a wrong
// value of those can damage the panel, so none is added.

namespace board {
namespace {

// Partial window, UC command 0x90: x start, x end, y start (high, low), y end (high, low), scan flag.
// The x values are bit positions 7..3 of the byte, so x starts on a multiple of 8 and x end is the last pixel
// of a byte (UC "PARTIAL WINDOW"; DEMO sends the same, with x end = x start + width - 1). The y values have
// nine bits. The last byte, 1, lets the gates scan inside and outside the window: it is the UC default and
// what DEMO sends. Anything the window cannot hold is refused, and the driver sends a full refresh instead.
std::vector<uint8_t> encodeWindow(const ui::Rect& r) {
  if (r.empty() || r.x < 0 || r.y < 0 || r.right() > 240 || r.bottom() > 320) return {};
  if (r.x % 8 != 0 || r.w % 8 != 0) return {};
  const int xEnd = r.x + r.w - 1;
  const int yEnd = r.bottom() - 1;
  return {(uint8_t)r.x, (uint8_t)xEnd, (uint8_t)(r.y >> 8), (uint8_t)(r.y & 0xFF), (uint8_t)(yEnd >> 8),
          (uint8_t)(yEnd & 0xFF), 0x01};
}

PanelProfile build() {
  using namespace uc8253;
  PanelProfile p;
  p.name = "GDEQ031T10 on UC8253, OTP waveform";
  p.width = 240;   // DS 1.3 (240 H x 320 V); DEMO: 240 x 320, 9600 bytes of picture
  p.height = 320;

  // Start, after the reset. DEMO and DS 3.6.1 agree on the order reset, panel setting, power on, wait for
  // BUSY. The panel setting byte is **0x1F** in DEMO and **0x0F** in the flow chart of DS. They differ in
  // the KW/R bit (UC 0x00, bit 4): 1 is "black/white", 0 is "black/white/red" with a second data plane. The
  // panel has no red, DEMO is the code the vendor ships for this panel, so 0x1F is used:
  //   RES=00 (the resolution is not taken from this field: DEMO sends no resolution command, the OTP
  //   values stay), REG=0 (waveform from OTP), KW/R=1, UD=1 and SHL=1 (UC defaults: the scan directions
  //   that give an upright picture, checked against DEMO's picture), SHD_N=1 (booster on), RST_N=1.
  // Power on (0x04) is followed by the wait for BUSY: low is busy (DS 1.5 pin 9; UC "BUSY_N"). DS 3.6.2 says
  // commands sent within 1 ms after reset are ignored; DEMO waits 10 ms.
  p.start = {{kPanelSetting, {0x1F}}, {kPowerOn, {}, /*waitBusy=*/true}};
  p.resetLowMs = 10;   // DEMO "at least 10 ms"
  p.resetHighMs = 10;  // DEMO; DS 3.6.2 asks for more than 1 ms

  // Full refresh: nothing more than the start. DEMO's full refresh (EPD_Init) sends no temperature override
  // and no data interval setting, so the controller measures the temperature itself (UC 0x40, "temperature
  // sensor is activated automatically for one-time sensing before enabling booster") and takes the waveform
  // for it. The UI's "full" is this mode, the one that flashes several times (DS 3.3 note 2).
  p.beforeFull = {};

  // Partial refresh, as DEMO's partial mode (EPD_Init_Part) after the same start:
  //  0xE0 = 0x02: UC "CASCADE SETTING", bit TSFIX: "the temperature value is defined by the TS_SET register"
  //        instead of the sensor. (The command is described for cascaded chips; DEMO uses it on one.)
  //  0xE5 = 0x79: UC "FORCE TEMPERATURE", TS_SET[7:0]: the temperature the waveform is chosen for, coded as
  //        in the sensor table (UC 0x40: 0x19 = 25 degrees), so 0x79 is 121 degrees. The OTP holds one
  //        waveform per temperature range; by claiming a temperature far above the panel's own range
  //        (DS 3.1: 0 to 50 degrees) DEMO selects the short, non-flashing one, which is how the vendor gets
  //        its partial refresh without a waveform table. The UC datasheet does not say which range holds
  //        the partial waveform: that 0x79 gives it is DEMO's statement only.
  //  0x50 = 0xD7: UC "VCOM AND DATA INTERVAL": VBD=11, DDX=01, CDI=0111 is the **reset default**, so DEMO
  //        writes what is already there. DDX=01 with KW/R=1 is "new and old data" (data 0 = black, 1 = white).
  p.beforePartial = {{kCascadeSetting, {0x02}}, {kForceTemperature, {0x79}}, {kDataIntervalSetting, {0xD7}}};
  p.encodeWindow = encodeWindow;
  // UC: "HRED must be greater than HRST", "VRED must be greater than VRST", so a window is at least two
  // banks wide and two lines high. DEMO's smallest is 32 x 64 and never tests less. A smaller change is sent
  // as the window around it.
  p.minWindowWidth = 16;
  p.minWindowHeight = 2;

  // Polarity: 1 is white, 0 is black. DEMO clears to 0xFF and fills black with 0x00; UC with DDX=01 and
  // KW/R=1 maps new/old data 0 to the black-keeping waveform. DEMO's picture, read as 240 pixels per row,
  // most significant bit first, is upright and readable, so there is no rotation either.
  p.blackIsZero = true;

  // After the refresh: power off, wait for BUSY, 100 ms, deep sleep with its check code 0xA5 (DS 3.6.1;
  // DEMO's EPD_DeepSleep, "at least 100 ms"). Deep sleep is left only by a hardware reset, which is why
  // the start commands run before every refresh. DEMO sends power off twice (after the refresh and again in
  // its sleep function); once is enough, the second finds the panel already off.
  p.sleep = {{kPowerOff, {}, /*waitBusy=*/true, /*delayAfterMs=*/100}, {kDeepSleep, {0xA5}}};

  // After 0x12 DEMO waits 1 ms before it looks at BUSY ("200 us at least"), because the line is still
  // "free" for a moment after the command.
  p.refreshSettleMs = 1;
  // DS 3.4: a full update takes about 3 s at 25 degrees. A panel that is busy for three times that is
  // hung (and colder panels are slower).
  p.busyTimeoutMs = 10000;
  p.busyIsHigh = false;  // DEMO: "0: BUSY, 1: FREE"; DS flow chart: BUSY=Low
  return p;
}

}  // namespace

const PanelProfile& gdeq031t10Profile() {
  static const PanelProfile profile = build();
  return profile;
}

}  // namespace board
