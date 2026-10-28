#pragma once

#include "CoreMinimal.h"
#include "Warden/WardenLines.h"

struct FMatchState;

/** Un comentario posible (S-021, R-COM-1, nombre propuesto): su tipo y el asiento del que se habla. */
struct SOULBOUNDCORE_API FRemarkCandidate
{
	EWardenRemarkKind Kind = EWardenRemarkKind::MockLast;
	/** El destinatario: el asiento del que se habla (decisión del autor del 28/10/2026). */
	int32 Seat = INDEX_NONE;

	bool operator==(const FRemarkCandidate& Other) const { return Kind == Other.Kind && Seat == Other.Seat; }
};

/**
 * Los comentarios del Warden (S-021, GDD 06: R-COM-1, R-COM-2). Un comentario no tiene efecto, no corta nada y no gasta la garantía: deja
 * `WardenRemark` (Seat el destinatario, Value el tipo) y, si hay frase, `WardenSpoke` con `comentario:<tipo>` justo después. La elige la
 * evaluación (cinco tipos) o la dispara el suceso (cinco más); LostBet es de la Fase 2.
 */
namespace Soulbound::Rules
{
	/**
	 * Los comentarios que la evaluación puede elegir en NowSeconds (R-COM-1), cada uno con su asiento, en el orden de EWardenRemarkKind:
	 *  - MockLast: siempre; el asiento con menos runas en el Brazal (los duplicados cuentan) y, si empatan, el último del orden de turno.
	 *  - FiveRunes: quien tiene cinco tipos de runa distintos (el primero del orden de turno si son varios).
	 *  - Hurry: el activo, con un turno en curso en RollAndChoose o con una elección suya abierta, si lleva más de HurryFraction de su reloj
	 *    de turno (el de los datos) sin entregar intención.
	 *  - Hoarder: quien causa el disparador Acaparador (el primero del orden de turno).
	 *  - CutLaugh: el cortado por un TurnInterrupted en los últimos CutLaughWindowSeconds (el más reciente).
	 * Sin Warden en la partida, ninguno.
	 */
	SOULBOUNDCORE_API TArray<FRemarkCandidate> ApplicableRemarks(const FMatchState& State, double NowSeconds);

	/**
	 * Hace que el Warden comente (R-COM-1, R-COM-2): deja `WardenRemark` (Seat, Value = Kind) y, si la tabla tiene frase, justo después
	 * `WardenSpoke` con `comentario:<tipo>` para ese asiento. Se registra siempre, con o sin frases. No corta nada ni toca la garantía.
	 */
	SOULBOUNDCORE_API void SpeakRemark(FMatchState& State, EWardenRemarkKind Kind, int32 Seat, double NowSeconds);

	/**
	 * La evaluación comenta (R-COM-1): elige un tipo de ApplicableRemarks con la misma probabilidad cada uno, con el sorteo de la voz
	 * (VoiceDraw), y lo dice (SpeakRemark). Devuelve el elegido, o un candidato con Seat INDEX_NONE si no había ninguno.
	 */
	SOULBOUNDCORE_API FRemarkCandidate SpeakEvaluatedRemark(FMatchState& State, double NowSeconds);
}
