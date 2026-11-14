#include "FlashCues.h"

#include "Match/MatchState.h"

namespace
{
	using Soulbound::Presentation::EFlashColorSource;
	using Soulbound::Presentation::EFlashDirection;
	using Soulbound::Presentation::EFlashTarget;
	using Soulbound::Presentation::FFlashCue;

	/** Un destello nuevo; el retardo lo pone el encadenado, por objetivo. */
	void AddCue(TArray<FFlashCue>& Cues, TMap<TPair<EFlashTarget, int32>, int32>& PerTarget, EFlashTarget Target, int32 Id, EFlashDirection Direction,
		EFlashColorSource ColorSource, int32 RuneType, float Seconds)
	{
		if (Id == INDEX_NONE)
		{
			return;
		}
		FFlashCue Cue;
		Cue.Target = Target;
		Cue.Id = Id;
		Cue.Direction = Direction;
		Cue.ColorSource = ColorSource;
		Cue.RuneType = RuneType;
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
	// Hacia dentro es recibir y hacia fuera es perder, igual para la ficha que para la Fuente (punto 8 de la story).
	for (int32 Index = FMath::Max(FromIndex, 0); Index < State.EventLog.Num(); ++Index)
	{
		const FMatchEvent& Event = State.EventLog[Index];
		switch (Event.Type)
		{
		case EMatchEventType::RuneObtained:
			AddCue(Cues, PerTarget, EFlashTarget::Token, Event.Seat, EFlashDirection::Inward, EFlashColorSource::Rune, Event.Value, FlashSeconds);
			AddCue(Cues, PerTarget, EFlashTarget::Source, Event.Value, EFlashDirection::Outward, EFlashColorSource::Rune, Event.Value, FlashSeconds);
			break;
		case EMatchEventType::RuneReturned:
		{
			// S-018: tras un desafío final fallado el registro deja su RuneReturned; la ficha ya destelló con aquel, aquí solo la Fuente.
			const bool bAfterFailedChallenge = Index > 0 && State.EventLog[Index - 1].Type == EMatchEventType::FinalChallengeFailed
				&& State.EventLog[Index - 1].Seat == Event.Seat && State.EventLog[Index - 1].Value == Event.Value;
			if (!bAfterFailedChallenge)
			{
				AddCue(Cues, PerTarget, EFlashTarget::Token, Event.Seat, EFlashDirection::Outward, EFlashColorSource::Loss, Event.Value, FlashSeconds);
			}
			AddCue(Cues, PerTarget, EFlashTarget::Source, Event.Value, EFlashDirection::Inward, EFlashColorSource::Rune, Event.Value, FlashSeconds);
			break;
		}
		case EMatchEventType::RuneStolen:
		case EMatchEventType::RunesSeized:
			// Seat se queda la runa (hacia dentro, con su color) y OtherSeat la pierde (hacia fuera, con el de perder). La runa no pasa por ninguna Fuente.
			AddCue(Cues, PerTarget, EFlashTarget::Token, Event.Seat, EFlashDirection::Inward, EFlashColorSource::Rune, Event.Value, FlashSeconds);
			AddCue(Cues, PerTarget, EFlashTarget::Token, Event.OtherSeat, EFlashDirection::Outward, EFlashColorSource::Loss, Event.Value, FlashSeconds);
			break;
		case EMatchEventType::SealPlaced:
			// S-037 (R-VIS-25): la casilla sellada recibe el Sello (hacia dentro); Value es la casilla.
			AddCue(Cues, PerTarget, EFlashTarget::Tile, Event.Value, EFlashDirection::Inward, EFlashColorSource::Seal, INDEX_NONE, FlashSeconds);
			break;
		case EMatchEventType::SealReleased:
			// Al liberarse, el inverso: hacia fuera, del mismo color.
			AddCue(Cues, PerTarget, EFlashTarget::Tile, Event.Value, EFlashDirection::Outward, EFlashColorSource::Seal, INDEX_NONE, FlashSeconds);
			break;
		case EMatchEventType::FinalChallengeFailed:
			AddCue(Cues, PerTarget, EFlashTarget::Token, Event.Seat, EFlashDirection::Outward, EFlashColorSource::Loss, Event.Value, FlashSeconds);
			break;
		default:
			break;
		}
	}
	return Cues;
}

FLinearColor Soulbound::Presentation::FlashColorOf(const FFlashCue& Cue, const TArray<FLinearColor>& DomainColors, const FLinearColor& ClearColor, const FLinearColor& LossColor, const FLinearColor& SealColor)
{
	if (Cue.ColorSource == EFlashColorSource::Loss)
	{
		return LossColor;
	}
	if (Cue.ColorSource == EFlashColorSource::Seal)
	{
		return SealColor;
	}
	// Sin tipo de runa conocido (o sin color), el claro de siempre.
	return DomainColors.IsValidIndex(Cue.RuneType) ? DomainColors[Cue.RuneType] : ClearColor;
}
