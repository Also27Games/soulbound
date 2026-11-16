#include "RingPanelModel.h"

#include "Match/MatchState.h"

double Soulbound::Presentation::RingInnerRadius(double CloseFraction, double RuneRadius, double MaxRadius)
{
	return RuneRadius + (MaxRadius - RuneRadius) * (1.0 - FMath::Clamp(CloseFraction, 0.0, 1.0));
}

FRingProgress Soulbound::Presentation::RingProgress(TConstArrayView<double> Precisions, int32 NumPulses, ERingCriterion Criterion, double PassThreshold)
{
	FRingProgress Progress;
	if (NumPulses > 0)
	{
		double Sum = 0.0;
		for (const double Precision : Precisions)
		{
			Sum += Precision;
		}
		Progress.Fill = FMath::Clamp(Sum / NumPulses, 0.0, 1.0);
	}
	if (Criterion == ERingCriterion::MeanAtLeastThreshold)
	{
		Progress.Mark = PassThreshold;
	}
	return Progress;
}

int32 Soulbound::Presentation::RingRuneDomain(const FMatchState& State, int32 Seat, ESkillTrial Trial)
{
	if (Trial != ESkillTrial::ForgottenSource || !State.Seats.IsValidIndex(Seat))
	{
		return INDEX_NONE;
	}
	const int32 TileId = State.Seats[Seat].TileId;
	return State.Board.Tiles.IsValidIndex(TileId) ? State.Board.Tiles[TileId].DomainId : INDEX_NONE;
}

FString Soulbound::Presentation::RingTitleText(ESkillTrial Trial, bool bDuel)
{
	return bDuel ? TEXT("DUELO")
		: Trial == ESkillTrial::ForgottenSource ? TEXT("FUENTE DE UN DOMINIO OLVIDADO")
		: Trial == ESkillTrial::WardenTrial ? TEXT("PRUEBA DEL WARDEN")
		: Trial == ESkillTrial::VoidEscape ? TEXT("ESCAPE DEL VACÍO")
		: Trial == ESkillTrial::FinalChallenge ? TEXT("DESAFÍO FINAL")
		: TEXT("PRUEBA");
}

FString Soulbound::Presentation::RingStakeTitle(ESkillTrial Trial, bool bDuel)
{
	return RingTitleText(Trial, bDuel); // TODO(R-VIS-30): stub para ver el test en rojo
}

FString Soulbound::Presentation::RingStartCountdownText(double RemainingSeconds)
{
	return FString(); // TODO(R-MIN-19): stub
}

void Soulbound::Presentation::AddRingStartPrompt(FRingPanelModel& Model, const FRingTrialRun& Run, double Now, const FString& KeyDisplayName)
{
	// TODO(R-MIN-19): stub
}

TArray<FString> Soulbound::Presentation::RingInstructionLines()
{
	return {TEXT("Haz clic cuando el aro toque la runa."), TEXT("En la franja verde cuenta; cuanto más cerca, mejor.")};
}
