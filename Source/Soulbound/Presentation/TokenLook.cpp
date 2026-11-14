#include "TokenLook.h"

#include "Match/MatchState.h"

FLinearColor Soulbound::Presentation::TokenColor(ESeatRole Role, int32 DomainId, const FLinearColor& EchoColor, const TArray<FLinearColor>& DomainColors)
{
	// R-VIS-10: solo un Elegido toma el color de su Dominio. El DomainId de un Eco es su hogar y no cuenta (glosario, "Rol del asiento").
	return Role == ESeatRole::Elect && DomainColors.IsValidIndex(DomainId) ? DomainColors[DomainId] : EchoColor;
}

TArray<int32> Soulbound::Presentation::RingedSeats(const FMatchState& State)
{
	TArray<int32> Seats;
	if (State.IsMatchOver())
	{
		return Seats;
	}
	// Un duelo o un trilero que juega un evento del Warden entre turnos: la fase es Duel o ShellGame, pero ActiveSeat guarda el del último
	// turno. Los que juegan son los participantes (R-VIS-11).
	const bool bEventAction = State.CurrentEvent.IsActive() && State.CurrentEvent.bWaitsForAction;
	if (bEventAction && State.TurnPhase == ETurnPhase::Duel && State.Duel.IsActive())
	{
		Seats = {State.Duel.Challenger, State.Duel.Challenged};
	}
	else if (bEventAction && State.TurnPhase == ETurnPhase::ShellGame && State.ShellGame.IsActive())
	{
		Seats = {State.ShellGame.Thief, State.ShellGame.Victim};
	}
	else if (State.TurnPhase != ETurnPhase::BetweenTurns)
	{
		Seats = {State.ActiveSeat};
	}
	Seats.RemoveAll([&State](int32 Seat) { return !State.Seats.IsValidIndex(Seat); });
	Seats.Sort();
	return Seats;
}

float Soulbound::Presentation::RingPulse(double Seconds, float PeriodSeconds, float MinIntensity)
{
	if (PeriodSeconds <= 0.f)
	{
		return 1.f;
	}
	const float Min = FMath::Clamp(MinIntensity, 0.f, 1.f);
	const double Phase = FMath::Fmod(Seconds, static_cast<double>(PeriodSeconds)) / PeriodSeconds;
	// Coseno: parte del mínimo, sube hasta 1 a medio periodo y vuelve, sin saltos.
	const float Wave = static_cast<float>(0.5 * (1.0 - FMath::Cos(2.0 * UE_DOUBLE_PI * Phase)));
	return Min + (1.f - Min) * Wave;
}

float Soulbound::Presentation::RoleDiscMaxDiameter(float TurnRingDiameter, float TurnRingWidth)
{
	return FMath::Max(TurnRingDiameter - 2.f * TurnRingWidth - RoleDiscRingMargin, 0.f);
}

float Soulbound::Presentation::RoleDiscDiameter(float Footprint, float Scale, float MaxDiameter)
{
	// El tope manda sobre la escala, y el suelo (siempre más ancho que la peana de la carta) sobre el tope.
	return FMath::Max(FMath::Min(Footprint * Scale, MaxDiameter), Footprint * 1.1f);
}

float Soulbound::Presentation::RoleDiscLift(int32 Seat, float BaseLift)
{
	return BaseLift + RoleDiscSeatStep * static_cast<float>(FMath::Max(Seat, 0));
}

float Soulbound::Presentation::FlashScale(float Progress, float Growth, bool bInward)
{
	// Hacia dentro es el mismo camino al revés: empieza grande y se cierra hasta su tamaño base.
	const float Walked = FMath::Clamp(Progress, 0.f, 1.f);
	return 1.f + FMath::Max(Growth, 0.f) * (bInward ? 1.f - Walked : Walked);
}

FRotator Soulbound::Presentation::LabelFacingRotation(const FRotator& CameraRotation)
{
	// El texto tiene el frente en +X, el arriba en +Z y se lee hacia -Y. Para que su frente mire a la cámara (-adelante de la cámara) con el mismo arriba y la misma
	// derecha, es la rotación de la cámara girada media vuelta sobre su eje vertical: adelante y derecha se invierten y el arriba se queda.
	return (CameraRotation.Quaternion() * FRotator(0.f, 180.f, 0.f).Quaternion()).Rotator();
}
