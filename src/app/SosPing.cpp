#include "SosPing.h"

#include <cmath>
#include <cstdio>

#include "../i18n/Format.h"
#include "MapLink.h"

namespace {

// A position the receiver reports as valid but with NaN or impossible numbers must
// not become "the best": comparisons with NaN are all false, so it would stay best
// for ever and no better fix could replace it.
bool usable(const GnssFix& fix) {
  return fix.valid && std::isfinite(fix.hdop) && fix.hdop >= 0.0f && std::isfinite(fix.lat) && std::isfinite(fix.lon) &&
         std::fabs(fix.lat) <= 90.0 && std::fabs(fix.lon) <= 180.0;
}

std::string fill(const std::string& pattern, const i18n::Arg* args, size_t count) {
  char buffer[512];
  i18n::format(pattern.c_str(), args, count, buffer, sizeof(buffer));
  return buffer;
}

}  // namespace

unsigned SosPing::accuracyMeters(float hdop) {
  float m = hdop * 5.0f + 0.5f;
  if (!(m >= 1.0f)) m = 1.0f;  // also catches NaN
  if (m > 9999.0f) m = 9999.0f;
  return (unsigned)m;
}

std::string SosPing::buildText(const Wording& w, const std::string& who, const std::string& mapTemplate,
                               const GnssFix& fix, bool haveFix, unsigned waited, Kind kind) {
  // The name tells in three words who is meant; the receiver of the message often
  // sees only an unknown number.
  const bool named = !who.empty();
  const std::string* head;
  switch (kind) {
    case Kind::Pickup: head = named ? &w.pickup : &w.pickupAnonymous; break;
    case Kind::Query: head = named ? &w.location : &w.locationAnonymous; break;
    default: head = named ? &w.emergency : &w.emergencyAnonymous; break;
  }
  const i18n::Arg nameArg[] = {{"name", who.c_str()}};
  const std::string headText = fill(*head, nameArg, 1);

  if (!haveFix) {
    const i18n::Arg args[] = {{"head", headText.c_str()}, {"seconds", waited}};
    return fill(w.noGps, args, 2);
  }

  char map[96];
  const bool haveMap = maplink::build(mapTemplate.c_str(), fix.lat, fix.lon, map, sizeof(map)) == maplink::Result::Ok;
  char lat[24], lon[24];
  maplink::formatDegrees(fix.lat, lat, sizeof(lat));
  maplink::formatDegrees(fix.lon, lon, sizeof(lon));
  const std::string coordinates = std::string(lat) + "," + lon;
  const i18n::Arg args[] = {{"head", headText.c_str()},
                            {"map", map},
                            {"coordinates", coordinates.c_str()},
                            {"accuracy", accuracyMeters(fix.hdop)}};
  return fill(haveMap ? w.position : w.positionNoMap, args, 4);
}

bool SosPing::start(const char* peer, uint32_t now, Kind kind) {
  if (!peer || !*peer || !wording_.complete()) return false;
  if (active()) {
    if (!(kind_ == Kind::Query && kind != Kind::Query)) return false;
    // The reply to a parent's question gives way; its asker will not get an answer,
    // which the log records as a cancelled search.
  }
  peer_ = peer;
  kind_ = kind;
  stage_ = Stage::Search;
  start_ = now;
  best_ = GnssFix();
  text_.clear();
  return true;
}

void SosPing::cancel() {
  if (!active()) return;
  stage_ = Stage::Idle;
  text_.clear();
}

void SosPing::noteSendFailed(const char* peer) {
  if (stage_ != Stage::Done || !peer || peer_ != peer) return;
  stage_ = Stage::Failed;
}

int SosPing::progress() const {
  switch (stage_) {
    case Stage::Search: return 1;
    case Stage::Refine: return 2;
    case Stage::Done:
    case Stage::Failed: return 3;
    default: return 0;
  }
}

void SosPing::poll(uint32_t now) {
  if (!active()) return;

  GnssFix fix;
  // Of two positions the more accurate wins, not the newer: while searching, the
  // HDOP fluctuates, and whoever takes the last one sends a random, possibly the
  // worst, position. A tie keeps the earlier one.
  if (host_.readFix(fix) && usable(fix) && (!best_.valid || fix.hdop < best_.hdop)) best_ = fix;
  if (best_.valid && stage_ == Stage::Search) stage_ = Stage::Refine;

  const uint32_t waited = now >= start_ ? now - start_ : 0;
  const bool accurate = best_.valid && best_.hdop < cfg_.goodHdop && waited >= cfg_.minSeconds;
  const bool timedOut = waited >= cfg_.timeoutSeconds;
  if (accurate || timedOut) finish(waited);
}

void SosPing::finish(uint32_t waited) {
  text_ = buildText(wording_, who_, mapTemplate_, best_, best_.valid, waited, kind_);
  stage_ = host_.sendText(peer_.c_str(), text_) ? Stage::Done : Stage::Failed;
}
