#include "ElectCard.h"

#include "Match/MatchState.h"

void Soulbound::Presentation::FElectCardTracker::Observe(const FMatchState&, int32, double) {}
void Soulbound::Presentation::FElectCardTracker::SkipExisting(const FMatchState&) {}
void Soulbound::Presentation::FElectCardTracker::Dismiss() {}
void Soulbound::Presentation::FElectCardTracker::SetGrimoireOpen(bool) {}
bool Soulbound::Presentation::FElectCardTracker::IsVisible(double, double) const { return true; }
