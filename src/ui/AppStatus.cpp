#include "ui/AppStatus.h"

namespace ui {

StatusModel statusModelOf(const AppCore::State& state) {
  StatusModel model;
  // "Unknown" is a modem that was not asked yet; it tries anyway, so the bar says nothing of the SIM.
  switch (state.sim) {
    case SimStatus::Unknown:
    case SimStatus::Ready: model.sim = StatusModel::Sim::Ready; break;
    case SimStatus::Missing: model.sim = StatusModel::Sim::Missing; break;
    case SimStatus::PinRequired:
    case SimStatus::PinWrong: model.sim = StatusModel::Sim::PinRequired; break;
    case SimStatus::PukRequired: model.sim = StatusModel::Sim::Locked; break;
  }
  switch (state.network.kind) {
    case Connectivity::NetworkLabel::Kind::None: model.link = StatusModel::Link::None; break;
    case Connectivity::NetworkLabel::Kind::Operator: model.link = StatusModel::Link::Operator; break;
    case Connectivity::NetworkLabel::Kind::Radio: model.link = StatusModel::Link::Radio; break;
    case Connectivity::NetworkLabel::Kind::RadioOnly: model.link = StatusModel::Link::RadioOnly; break;
  }
  model.operatorName = state.network.name;
  model.quietHours = state.schoolTime;
  // No verdict yet is no complaint.
  switch (state.voice) {
    case VoiceStatus::Unknown:
    case VoiceStatus::Ready: model.voice = StatusModel::Voice::Ready; break;
    case VoiceStatus::SmsOnly: model.voice = StatusModel::Voice::SmsOnly; break;
    case VoiceStatus::ImsMissing: model.voice = StatusModel::Voice::NotReady; break;
  }
  model.locating = state.positioningActive;
  model.batteryPercent = state.batteryPercent;
  model.dateKnown = state.wall.known;
  model.month = state.wall.month;
  model.day = state.wall.day;
  model.weekday = state.wall.weekday;
  return model;
}

}  // namespace ui
