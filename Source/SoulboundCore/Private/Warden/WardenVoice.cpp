#include "Warden/WardenVoice.h"

#include "Match/MatchEvents.h"

namespace Soulbound::Rules
{
	FString InsolenceLineKey(EInsolencePunishment Punishment)
	{
		switch (Punishment)
		{
		case EInsolencePunishment::Light:
			return TEXT("insolencia:leve");
		case EInsolencePunishment::Medium:
			return TEXT("insolencia:media");
		case EInsolencePunishment::Severe:
			return TEXT("insolencia:grave");
		case EInsolencePunishment::Fury:
			return Soulbound::Warden::LineFury;
		}
		return FString();
	}

	FWardenLineChoice DrawWardenLine(FMatchState& State, const FString& Key)
	{
		FWardenLineChoice Choice;
		Choice.EntryIndex = State.WardenLines.IndexOf(Key);
		if (Choice.EntryIndex == INDEX_NONE || State.WardenLines.Entries[Choice.EntryIndex].Lines.IsEmpty())
		{
			Choice.EntryIndex = INDEX_NONE;
			return Choice;
		}
		const int32 Count = State.WardenLines.Entries[Choice.EntryIndex].Lines.Num();
		if (Count == 1)
		{
			// Con una variante no hay sorteo: la voz no gasta nada.
			Choice.Variant = 0;
			return Choice;
		}
		// Como WardenDraw, con la semilla de la voz y su propia cuenta: ni el sorteo del Warden ni el de los dados (R-WAR-9).
		FWardenState& Warden = State.Warden;
		FRandomStream Draw(static_cast<int32>(HashCombineFast(GetTypeHash(Warden.VoiceSeed), GetTypeHash(Warden.VoiceDraws++))));
		const int32* Last = Warden.LastLineVariant.Find(Key);
		if (Last && *Last >= 0 && *Last < Count)
		{
			// Una al azar entre las demás: se sortea entre Count - 1 y se salta la última.
			Choice.Variant = Draw.RandRange(0, Count - 2);
			Choice.Variant += Choice.Variant >= *Last ? 1 : 0;
		}
		else
		{
			Choice.Variant = Draw.RandRange(0, Count - 1);
		}
		Warden.LastLineVariant.Add(Key, Choice.Variant);
		return Choice;
	}

	bool SpeakWardenLine(FMatchState& State, const FString& Key, double NowSeconds, int32 Seat)
	{
		const FWardenLineChoice Choice = DrawWardenLine(State, Key);
		if (!Choice.IsValid())
		{
			return false;
		}
		LogEvent(State, EMatchEventType::WardenSpoke, NowSeconds, Seat, Choice.EntryIndex, INDEX_NONE, Choice.Variant);
		return true;
	}
}
