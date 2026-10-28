#include "Warden/WardenRemarks.h"

#include "Match/MatchState.h"

// Esqueleto del corte A2 de S-021: la API existe y no hace nada, para que los tests compilen y se vean fallar antes de implementar.

namespace Soulbound::Rules
{
	TArray<FRemarkCandidate> ApplicableRemarks(const FMatchState& State, double NowSeconds) { return TArray<FRemarkCandidate>(); }
	void SpeakRemark(FMatchState& State, EWardenRemarkKind Kind, int32 Seat, double NowSeconds) {}
	FRemarkCandidate SpeakEvaluatedRemark(FMatchState& State, double NowSeconds) { return FRemarkCandidate(); }
}
