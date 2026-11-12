#pragma once

#include "CoreMinimal.h"

#include "Match/MatchState.h"

namespace Soulbound::Presentation
{
	/**
	 * Color de la ficha según el rol de su asiento (S-029, R-VIS-10): un Eco, el gris de datos; un Elegido, el color de su Dominio.
	 * @param Role Rol del asiento (FSeatState::Role).
	 * @param DomainId Dominio del asiento; solo cuenta si es Elegido (el de un Eco es su hogar, no un color).
	 * @param EchoColor El gris del Eco (ABoardViewActor::EchoTokenColor).
	 * @param DomainColors El color de cada Dominio, el de la leyenda.
	 * @return El color; el del Eco si es Elegido de un Dominio sin color.
	 */
	SOULBOUND_API FLinearColor TokenColor(ESeatRole Role, int32 DomainId, const FLinearColor& EchoColor, const TArray<FLinearColor>& DomainColors);

	/**
	 * Los asientos cuya ficha lleva el aro del turno (S-029, R-VIS-11), en orden de asiento. Durante un turno, el activo (fases de elección
	 * incluidas); entre turnos, ninguno; en un duelo o un trilero que juega un evento del Warden entre turnos, sus dos participantes (el
	 * ActiveSeat entonces es el del último turno, no el que juega). Con la partida terminada, ninguno.
	 * @param State Estado de la partida, en solo lectura.
	 * @return Los asientos con aro.
	 */
	SOULBOUND_API TArray<int32> RingedSeats(const FMatchState& State);

	/**
	 * Intensidad del pulso lento del aro del turno (S-029, R-VIS-11): de MinIntensity a 1 y vuelta, con periodo PeriodSeconds, empezando en el
	 * mínimo. Sin periodo válido (cero o menos), 1 fijo.
	 * @param Seconds Reloj de la vista.
	 * @param PeriodSeconds Periodo del pulso (ABoardViewActor::ActiveRingPulseSeconds).
	 * @param MinIntensity Intensidad mínima, de 0 a 1.
	 * @return Intensidad de MinIntensity a 1.
	 */
	SOULBOUND_API float RingPulse(double Seconds, float PeriodSeconds, float MinIntensity = 0.35f);
}
