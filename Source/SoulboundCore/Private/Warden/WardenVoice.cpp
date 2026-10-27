#include "Warden/WardenVoice.h"

// Esqueleto del corte A1 de S-021: la API existe y no hace nada, para que los tests compilen y se vean fallar antes de implementar.

namespace Soulbound::Rules
{
	FString InsolenceLineKey(EInsolencePunishment Punishment) { return FString(); }
	FWardenLineChoice DrawWardenLine(FMatchState& State, const FString& Key) { return FWardenLineChoice(); }
	bool SpeakWardenLine(FMatchState& State, const FString& Key, double NowSeconds, int32 Seat) { return false; }
}
