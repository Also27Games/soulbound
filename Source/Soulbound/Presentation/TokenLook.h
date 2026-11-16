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

	/**
	 * La escala del orbe del Oráculo en el instante Seconds (S-041, R-VIS-29, nombre propuesto): late de MinScale a su tamaño entero (1) y vuelta,
	 * con periodo PeriodSeconds, empezando en el mínimo; es el pulso de la vista (RingPulse). Sin periodo válido (cero o menos), 1 fijo.
	 * @param Seconds Reloj de la vista.
	 * @param PeriodSeconds Periodo del latido (ABoardViewActor::OracleOrbPulseSeconds, 0,9 s).
	 * @param MinScale Escala mínima, de 0 a 1 (ABoardViewActor::OracleOrbPulseMin, 0,8).
	 * @return Escala de MinScale a 1, que multiplica el diámetro del orbe.
	 */
	SOULBOUND_API float OracleOrbScale(double Seconds, float PeriodSeconds, float MinScale);

	/**
	 * El color con que se pinta el halo de una casilla alcanzable en el instante Seconds (S-039, R-VIS-26): el color base por la intensidad del
	 * pulso (RingPulse, la misma función que el aro del turno), sin tocar el alfa. Sin periodo válido, el color base entero.
	 * @param BaseColor El color del halo (ABoardViewActor::ReachableColor).
	 * @param Seconds Reloj de la vista.
	 * @param PeriodSeconds Periodo del latido (ABoardViewActor::ReachablePulseSeconds).
	 * @param MinIntensity Intensidad mínima, de 0 a 1 (ReachablePulseMin).
	 * @return El color a pintar.
	 */
	SOULBOUND_API FLinearColor ReachableHaloColor(const FLinearColor& BaseColor, double Seconds, float PeriodSeconds, float MinIntensity);

	/** Margen entre el borde del disco de rol y el borde interior del aro del turno, en cm (S-029, corrección del 13/11/2026). */
	inline constexpr float RoleDiscRingMargin = 0.1f;

	/** Cuánto sube el disco de rol por número de asiento, en cm (S-029): dos discos solapados en una casilla no comparten cota y no parpadean. */
	inline constexpr float RoleDiscSeatStep = 0.01f;

	/**
	 * Diámetro máximo del disco de rol (S-029): el borde interior del aro del turno menos un margen, para que el borde del disco no quede bajo el
	 * aro. El borde interior es su diámetro menos dos veces su grosor (cuenta conservadora: los segmentos del aro se centran en su radio).
	 * @param TurnRingDiameter Diámetro del aro del turno (ABoardViewActor::RingDiameter), en cm.
	 * @param TurnRingWidth Grosor del aro del turno (RingWidth), en cm.
	 * @return El diámetro máximo, al menos 0.
	 */
	SOULBOUND_API float RoleDiscMaxDiameter(float TurnRingDiameter, float TurnRingWidth);

	/**
	 * Diámetro del disco de rol, la peana de color bajo la ficha (S-029, R-VIS-10): la huella de la carta por Scale, sin pasar de MaxDiameter, y
	 * nunca menos de 1,1 veces la huella para que sea siempre más ancho que la peana de la carta.
	 * @param Footprint Huella de la carta (o diámetro de la ficha sin carta), en cm.
	 * @param Scale RoleDiscScale (Config).
	 * @param MaxDiameter Tope, de RoleDiscMaxDiameter.
	 * @return El diámetro del disco, en cm.
	 */
	SOULBOUND_API float RoleDiscDiameter(float Footprint, float Scale, float MaxDiameter);

	/**
	 * Cota del disco de rol sobre la base de la ficha (S-029): BaseLift más RoleDiscSeatStep por número de asiento.
	 * @param Seat Asiento de la ficha.
	 * @param BaseLift RoleDiscLift (Config), en cm.
	 * @return La cota, en cm.
	 */
	SOULBOUND_API float RoleDiscLift(int32 Seat, float BaseLift);

	/**
	 * Escala del aro del destello y del halo de la Fuente según el avance del destello (S-029, R-VIS-12, corrección del autor del 13/11/2026: el destello
	 * era demasiado sutil; y punto 8, el sentido): hacia fuera (el defecto) crece de 1 a 1 + Growth en lo que dura, en línea recta; hacia dentro hace el camino
	 * contrario, de 1 + Growth a 1 (se cierra).
	 * @param Progress Avance del destello, de 0 (empieza) a 1 (acaba); fuera de rango se toma el extremo.
	 * @param Growth Cuánto crece (o encoge) como fracción del tamaño base (FlashGrowth, Config); negativo, 0.
	 * @param bInward Si el destello va hacia dentro (se cierra).
	 * @return La escala, entre 1 y 1 + Growth.
	 */
	SOULBOUND_API float FlashScale(float Progress, float Growth, bool bInward = false);

	/**
	 * La rotación con que el número de una ficha queda paralelo a la pantalla (S-037, R-VIS-23; nombre propuesto `LabelFacingRotation`): su frente mira a la
	 * cámara y su "arriba" y su "derecha" son los de la cámara. Los ejes de UTextRenderComponent: frente +X, arriba +Z y se lee hacia -Y (como el texto
	 * tumbado del tablero, FRotator(90, 180, 0)).
	 * @param CameraRotation Rotación de la cámara activa.
	 * @return La rotación del texto, en el mundo.
	 */
	SOULBOUND_API FRotator LabelFacingRotation(const FRotator& CameraRotation);
}
