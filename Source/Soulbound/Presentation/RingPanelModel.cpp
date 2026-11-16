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
	// R-VIC-3 y R-RUN-6 (ApplyFinalChallengeResult); duelo: R-DUE-5, R-DUE-6, R-EFE-13 y el duelo contra el Warden de S-015. La del duelo, la exacta
	// (corrección del autor del 18/11/2026): quien pierde sin runas vuelve a su hogar, y las runas no siempre van al ganador.
	return bDuel ? TEXT("Duelo: si pierdes, te quitan runas o vuelves a tu hogar")
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
	return 420.f * FMath::Max(FontSize, 8) / 18.f;
}

FRingPanelLayout Soulbound::Presentation::RingPanelLayout(float Width, float Height, float TextBottom, float Scale)
{
	// Márgenes a 18 puntos, escalados con la letra. Desde abajo: marcas de los pulsos (radio 9), barra (12 de alto, su marca sobresale 4), franja de
	// "Empezar" y el aro, con 8 de aire por arriba y por abajo.
	constexpr float DotsFromBottom = 22.f;
	constexpr float DotRadius = 9.f;
	constexpr float BarToDots = 16.f;
	constexpr float BarHeight = 12.f;
	constexpr float BandToBar = 10.f;
	constexpr float BandHeight = 60.f;
	constexpr float RingGap = 8.f;

	FRingPanelLayout Layout;
	Layout.DotsY = Height - DotsFromBottom * Scale;
	Layout.BarTop = Layout.DotsY - (DotRadius + BarToDots + BarHeight) * Scale;
	Layout.StartBandBottom = Layout.BarTop - BandToBar * Scale;
	Layout.StartBandTop = Layout.StartBandBottom - BandHeight * Scale;
	// R-MIN-19 (corrección del autor del 18/11/2026): el aro va entre los textos y la franja, centrado en ese hueco, sin pisar ninguno de los dos. El
	// centro (la runa y la franja verde) queda libre durante la espera.
	const float RingTop = TextBottom + RingGap * Scale;
	const float RingBottom = Layout.StartBandTop - RingGap * Scale;
	Layout.MaxRadius = FMath::Max(FMath::Min(0.36f * FMath::Min(Width, Height), (RingBottom - RingTop) * 0.5f), 0.f);
	Layout.CenterY = (RingTop + RingBottom) * 0.5f;
	return Layout;
}

TArray<FString> Soulbound::Presentation::RingInstructionLines()
{
	return {TEXT("Haz clic cuando el aro toque la runa."), TEXT("En la franja verde cuenta; cuanto más cerca, mejor.")};
}
