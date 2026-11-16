#include "RingPanelModel.h"

#include "BoardViewState.h"
#include "../Match/RingTrialRun.h"
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
	// R-VIS-30: lo que hace el motor al fallar o perder, con las frases aprobadas por el autor el 16/11/2026. Fuente: R-TAB-7 y R-RUN-3
	// (ApplySkillTrialResult); Prueba del Warden: R-EFE-10 (ApplyWardenTrialResult); escape: R-VAC-2 (ApplyVoidEscapeResult); desafío final:
	// R-VIC-3 y R-RUN-6 (ApplyFinalChallengeResult); duelo: R-DUE-5, R-EFE-13 y el duelo contra el Warden de S-015.
	return bDuel ? TEXT("Duelo: si pierdes, te roban runas")
		: Trial == ESkillTrial::ForgottenSource ? TEXT("Fuente de un Dominio Olvidado: si fallas, no consigues la runa")
		: Trial == ESkillTrial::WardenTrial ? TEXT("Prueba del Warden: si fallas, pierdes el turno")
		: Trial == ESkillTrial::VoidEscape ? TEXT("Escape del Vacío: si fallas, sigues en el Vacío")
		: Trial == ESkillTrial::FinalChallenge ? TEXT("Desafío final: si fallas, pierdes una runa y vuelves a tu hogar")
		: TEXT("Prueba");
}

FString Soulbound::Presentation::RingStartCountdownText(double RemainingSeconds)
{
	// R-MIN-19: hacia arriba, para que el número no llegue a 0 mientras aún queda tiempo.
	return FString::Printf(TEXT("Empieza sola en %d s"), FMath::CeilToInt(FMath::Max(RemainingSeconds, 0.0)));
}

void Soulbound::Presentation::AddRingStartPrompt(FRingPanelModel& Model, const FRingTrialRun& Run, double Now, const FString& KeyDisplayName)
{
	if (!Run.IsWaiting())
	{
		Model.StartLabel.Reset();
		Model.StartKeyHint.Reset();
		Model.StartCountdown.Reset();
		return;
	}
	Model.StartLabel = TEXT("Empezar");
	Model.StartKeyHint = KeyHintText(KeyDisplayName);
	Model.StartCountdown = RingStartCountdownText(Run.CountdownSeconds(Now));
}

float Soulbound::Presentation::RingPanelWidth(int32 FontSize)
{
	return 0.f; // TODO(R-MIN-19): stub para ver el test en rojo
}

FRingPanelLayout Soulbound::Presentation::RingPanelLayout(float Width, float Height, float TextBottom, float Scale)
{
	return FRingPanelLayout(); // TODO(R-MIN-19): stub para ver el test en rojo
}

TArray<FString> Soulbound::Presentation::RingInstructionLines()
{
	return {TEXT("Haz clic cuando el aro toque la runa."), TEXT("En la franja verde cuenta; cuanto más cerca, mejor.")};
}
