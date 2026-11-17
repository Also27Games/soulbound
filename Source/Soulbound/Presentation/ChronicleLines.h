#pragma once

#include "CoreMinimal.h"
#include "ChroniclePanel.h"

struct FMatchState;

namespace Soulbound::Presentation
{
	/**
	 * Una instrucción del Warden ya enseñada, para la crónica (S-021, A3; sacada de ABoardViewActor en S-044): su texto, su marca de tiempo y el
	 * índice del último evento del registro cuando se enseñó; la nota va justo después de él.
	 */
	struct FInstructionNote
	{
		int32 After = INDEX_NONE;
		double ClockSeconds = 0.0;
		FString Text;
	};

	/** Los colores y límites de la crónica (S-044, nombre propuesto): los de la vista. */
	struct FChronicleLook
	{
		/** El asiento cuyos resultados dicen "con el anillo" (S-026); INDEX_NONE si nadie lo juega. */
		int32 RingSeat = INDEX_NONE;
		/** Cuántos eventos del final del registro se miran (ChronicleMaxLines); al menos uno. */
		int32 MaxEvents = 200;
		/** El color de cada asiento; un asiento sin color, blanco. */
		TArray<FLinearColor> SeatColors;
		FLinearColor WardenColor = FLinearColor::White;
		FLinearColor RaceColor = FLinearColor::White;
		FLinearColor DebugColor = FLinearColor::White;
	};

	/**
	 * El encabezado del panel de crónica (S-048, corrección 6 del autor del 17/11/2026, nombre propuesto), en el Grimorio y en la columna.
	 * TODO(R-HUD-20): stub del commit de los tests en rojo, con el texto de antes; el corregido va en el commit siguiente.
	 */
	inline const TCHAR* const ChronicleHeadingText = TEXT("CRONICA (la ultima arriba; rueda para ir al inicio)");

	/** Marca de tiempo de la crónica: minutos y segundos del reloj de partida ("3:07"); los negativos, 0:00. */
	FString ChronicleStamp(double Seconds);

	/**
	 * Las líneas de la crónica, la más reciente primero (S-044: sacadas de ABoardViewActor::RefreshChronicle sin cambiar nada). De los últimos
	 * Look.MaxEvents eventos del registro, una línea con su marca de tiempo por cada uno que tiene texto (DescribeChronicleEvent; la decisión del
	 * Warden con sus disparadores, DescribeWardenDecision) y, encima de cada evento, las instrucciones enseñadas justo después de él. La usan la
	 * columna y la pestaña "Crónica" del Grimorio (R-HUD-10): las dos dicen lo mismo y en el mismo orden.
	 * @param State El estado de la partida.
	 * @param Notes Las instrucciones enseñadas, en el orden en que se enseñaron.
	 * @param Look Colores, asiento del anillo y límite.
	 * @return Las líneas.
	 */
	TArray<FChronicleLine> ChronicleLines(const FMatchState& State, const TArray<FInstructionNote>& Notes, const FChronicleLook& Look);
}
