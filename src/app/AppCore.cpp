#include "AppCore.h"

#include "Calendar.h"

AppCore::AppCore(Clock& clock, Power& power, Modem& modem, GnssReceiver& gnss, Haptic& haptic, EventLog& eventLog,
                 i18n::Translator& translator, Profile profile)
    : clock_(clock),
      modem_(modem),
      haptic_(haptic),
      translator_(translator),
      profile_(std::move(profile)),
      logger_(eventLog, profile_),
      notices_(clock),
      school_(clock, profile_, logger_),
      positioning_(clock, power, gnss, logger_),
      messaging_(clock, modem, power, store_, profile_, school_, notices_, haptic, logger_),
      calls_(clock, modem, power, store_, profile_, school_, notices_, haptic, logger_),
      emergency_(clock, positioning_, messaging_, school_, store_, profile_, translator, haptic, logger_),
      powerCoordination_(clock, power, modem, profile_, notices_, logger_),
      connectivity_(clock, power, modem, profile_, notices_, powerCoordination_, logger_) {
  messaging_.setHooks(this);
}

void AppCore::setRadio(RadioLink* radio) {
  radio_ = radio;
  messaging_.setRadio(radio);
  connectivity_.setRadio(radio);
}

void AppCore::setWifi(Wifi& wifi, WifiController::Random random) {
  wifi_ = std::make_unique<WifiController>(wifi, random);
  wifi_->configure(profile_.wifi);
  connectivity_.setWifi(wifi_.get());
}

// The trigger word of every language of the catalogs, so that a parent may write it in any of
// them, and the profile's extra words.
void AppCore::configureTriggers() {
  std::vector<std::string> triggers = translator_.textsInAllLanguages("sms.command.where");
  for (const std::string& word : profile_.extraTriggers) triggers.push_back(word);
  messaging_.configure(triggers);
}

void AppCore::setMessageVibration(bool on) {
  profile_.messageVibration = on;
  settingsNeedWrite_ = true;
}

void AppCore::setCommandNotice(CommandNotice notice) {
  profile_.commandNotice = notice;
  configureTriggers();
  settingsNeedWrite_ = true;
}

bool AppCore::setInterfaceLanguage(const std::string& code) {
  if (!translator_.select(code)) return false;
  profile_.interfaceLanguage = translator_.language();
  settingsNeedWrite_ = true;
  return true;
}

void AppCore::applyTimeZone() {
  const uint32_t epoch = clock_.epochUtc();
  if (!profile_.timeZone || epoch == 0) return;
  const int offset = tz::offsetMinutes(*profile_.timeZone, epoch);
  clock_.setTzOffsetMinutes(offset);
}

void AppCore::start() {
  if (!profile_.interfaceLanguage.empty()) translator_.select(profile_.interfaceLanguage);
  store_.restore();
  configureTriggers();
  applyTimeZone();
  positioning_.startStartupFix();
}

PowerDecision AppCore::poll(bool displayOn, uint32_t nextRenderInMs) {
  applyTimeZone();

  // Anything the modem reports proves that it is awake.
  bool modemActive = messaging_.poll();
  modemActive = calls_.poll() || modemActive;
  if (modemActive) powerCoordination_.noteModemAwake();

  positioning_.poll();
  emergency_.poll();
  connectivity_.poll();

  PowerCoordination::Needs needs;
  needs.callActive = calls_.modemBusy();
  needs.pendingWork = messaging_.hasWork() || emergency_.ping().active() || emergency_.phase() != SosFlow::Phase::Idle;
  needs.radioUsable = messaging_.radioUsable();
  needs.wifiActive = connectivity_.wifiActive();
  return powerCoordination_.apply(needs, displayOn, nextRenderInMs);
}

size_t AppCore::radioPeerCount() { return radio_ ? radio_->peerCount() : 0; }

bool AppCore::radioPeer(size_t index, RadioPeer& out) { return radio_ && radio_->peerAt(index, out); }

AppCore::State::WallClock AppCore::wallClockOf(uint32_t epochUtc) {
  State::WallClock wall;
  if (epochUtc == 0) return wall;
  const calendar::LocalTime local = calendar::toLocal(epochUtc, clock_.tzOffsetMinutes());
  const calendar::Civil date = calendar::civilFromDays(local.days);
  wall.known = true;
  wall.year = date.year;
  wall.month = date.month;
  wall.day = date.day;
  wall.weekday = (local.weekday + 1) % 7;  // the calendar counts from Monday, the weekday words from Sunday
  wall.minuteOfDay = local.minuteOfDay;
  return wall;
}

AppCore::State AppCore::state() {
  State s;
  s.wall = wallClockOf(clock_.epochUtc());
  s.epochUtc = clock_.epochUtc();
  s.uptimeSeconds = clock_.uptimeSeconds();
  s.network = connectivity_.networkLabel();
  s.registered = connectivity_.registered();
  s.signalBars = modem_.signalBars();
  s.sim = modem_.simStatus();
  s.voiceAvailable = modem_.voiceAvailable();
  s.cellularEnabled = connectivity_.cellularEnabled();
  s.saveMode = connectivity_.saveMode();
  s.probeSecondsLeft = connectivity_.probeSecondsLeft();
  s.radioAttached = connectivity_.radioAttached();
  if (radio_) {
    s.radioReady = radio_->ready();
    s.radioRssi = radio_->lastRssi();
  }
  if (wifi_) {
    s.wifiPhase = wifi_->phase();
    s.wifiReason = wifi_->reason();
    s.wifiSsid = wifi_->ssid();
    s.wifiPassword = wifi_->phase() == WifiPhase::AccessPoint ? wifi_->apPassword() : std::string();
    s.wifiAddress = wifi_->address();
    s.wifiSecondsLeft = wifi_->secondsLeft(clock_.uptimeSeconds());
  }
  s.unread = store_.unread();
  s.queued = messaging_.pending();
  s.notice = notices_.current();
  s.callState = calls_.state();
  s.callActive = calls_.active();
  s.callPeer = calls_.peer();
  s.volume = calls_.volume();
  s.volumeAdjustable = calls_.volumeAdjustable();
  s.sosPhase = emergency_.phase();
  s.sosSecondsLeft = emergency_.autoLeft();
  s.sosTapsLeft = emergency_.tapsLeft();
  s.emergencyRunning = emergency_.emergencyRunning();
  s.pingStage = emergency_.ping().stage();
  s.pingKind = emergency_.ping().kind();
  s.pingProgress = emergency_.ping().progress();
  s.pingElapsedSeconds = emergency_.ping().elapsed(clock_.uptimeSeconds());
  s.pingHasFix = emergency_.ping().best().valid;
  s.pingAccuracyMeters = s.pingHasFix ? SosPing::accuracyMeters(emergency_.ping().best().hdop) : 0;
  s.pingPeer = emergency_.ping().peer();
  s.schoolTime = school_.active(&s.schoolTimeEndMinute);
  s.positioningActive = positioning_.active();
  s.batteryPercent = powerCoordination_.batteryPercent();
  s.batteryMinutes = powerCoordination_.batteryMinutesRemaining();
  s.charge = powerCoordination_.chargeState();
  s.modemAsleep = powerCoordination_.modemAsleep();
  s.frontlight = powerCoordination_.frontlightOn();
  s.powerOffRequested = powerCoordination_.powerOffRequested();
  return s;
}
