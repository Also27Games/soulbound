#include "FlashCues.h"

#include "Match/MatchState.h"

namespace
{
	using Soulbound::Presentation::EFlashTarget;
	using Soulbound::Presentation::FFlashCue;

	/** Un destello nuevo; el retardo lo pone Chain al final, por objetivo. */
	void AddCue(TArray<FFlashCue>& Cues, TMap<TPair<EFlashTarget, int32>, int32>& PerTarget, EFlashTarget Target, int32 Id, bool bGain, float Seconds)
	{
		if (Id == INDEX_NONE)
		{
			return;
		}
		FFlashCue Cue;
		Cue.Target = Target;
		Cue.Id = Id;
		Cue.bGain = bGain;
		Cue.DurationSeconds = Seconds;
		// Los de un mismo objetivo van uno tras otro: el k-ésimo espera k duraciones.
		int32& Before = PerTarget.FindOrAdd(TPair<EFlashTarget, int32>(Target, Id), 0);
		Cue.DelaySeconds = Seconds * Before;
		++Before;
		Cues.Add(Cue);
	}
}

TArray<FFlashCue> Soulbound::Presentation::FlashCuesFromEvents(const FMatchState& State, int32 FromIndex, float FlashSeconds)
{
	TArray<FFlashCue> Cues;
	TMap<TPair<EFlashTarget, int32>, int32> PerTarget;
	for (int32 Index = FMath::Max(FromIndex, 0); Index < State.EventLog.Num(); ++Index)
	{
		const FMatchEvent& Event = State.EventLog[Index];
		switch (Event.Type)
		{
		case EMatchEventType::RuneObtained:
			AddCue(Cues, PerTarget, EFlashTarget::Token, Event.Seat, true, FlashSeconds);
			AddCue(Cues, PerTarget, EFlashTarget::Source, Event.Value, true, FlashSeconds);
			break;
		case EMatchEventType::RuneReturned:
		{
			// S-018: tras un desafío final fallado el registro deja su RuneReturned; la ficha ya destelló con aquel, aquí solo la Fuente.
			const bool bAfterFailedChallenge = Index > 0 && State.EventLog[Index - 1].Type == EMatchEventType::FinalChallengeFailed
				&& State.EventLog[Index - 1].Seat == Event.Seat && State.EventLog[Index - 1].Value == Event.Value;
			if (!bAfterFailedChallenge)
			{
				AddCue(Cues, PerTarget, EFlashTarget::Token, Event.Seat, false, FlashSeconds);
			}
			AddCue(Cues, PerTarget, EFlashTarget::Source, Event.Value, false, FlashSeconds);
			break;
		}
		case EMatchEventType::RuneStolen:
		case EMatchEventType::RunesSeized:
			// Seat se queda la runa, OtherSeat la pierde. La runa no pasa por ninguna Fuente.
			AddCue(Cues, PerTarget, EFlashTarget::Token, Event.Seat, true, FlashSeconds);
			AddCue(Cues, PerTarget, EFlashTarget::Token, Event.OtherSeat, false, FlashSeconds);
			break;
		case EMatchEventType::FinalChallengeFailed:
			AddCue(Cues, PerTarget, EFlashTarget::Token, Event.Seat, false, FlashSeconds);
			break;
		default:
			break;
		}
	}
	return Cues;
}
