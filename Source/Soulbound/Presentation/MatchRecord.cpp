#include "MatchRecord.h"

#include "EventText.h"
#include "Misc/DateTime.h"
#include "UObject/Class.h"

namespace Soulbound::Presentation
{
	namespace
	{
		/** Un campo del evento: el número, o "-" si es INDEX_NONE (R-ACT-1). */
		FString Field(int32 Value)
		{
			return Value == INDEX_NONE ? FString(TEXT("-")) : FString::FromInt(Value);
		}

		/** El nombre de un UENUM tal cual, sin el prefijo del enum. */
		template <typename TEnum>
		FString EnumName(TEnum Value)
		{
			return StaticEnum<TEnum>()->GetNameStringByValue(static_cast<int64>(Value));
		}

		/** "mm:ss" de unos segundos, sin corchetes ni décimas: la duración prevista. */
		FString MinutesAndSeconds(double Seconds)
		{
			const int32 Total = FMath::Max(0, FMath::FloorToInt32(Seconds));
			return FString::Printf(TEXT("%02d:%02d"), Total / 60, Total % 60);
		}

		/** Una línea por asiento del cierre (R-ACT-4): rol, Elegido o Dominio Olvidado, runas por tipo y en total, insolencia y Vacío. */
		FString SeatSummaryLine(const FMatchState& State, int32 Seat)
		{
			const FSeatState& Seated = State.Seats[Seat];
			FString Role;
			if (Seated.Role == ESeatRole::Elect)
			{
				Role = State.Elects.IsValidDomain(Seated.DomainId) ? FString::Printf(TEXT("Elegido, Elegido %s (Dominio %d)"), *State.Elects.Elects[Seated.DomainId].Name, Seated.DomainId)
					: FString::Printf(TEXT("Elegido, Dominio %d"), Seated.DomainId);
			}
			else
			{
				Role = Seated.DomainId == INDEX_NONE ? FString(TEXT("Eco Perdido, sin Dominio")) : FString::Printf(TEXT("Eco Perdido, Dominio Olvidado %d"), Seated.DomainId);
			}
			TArray<FString> ByType;
			int32 Total = 0;
			for (const int32 Count : Seated.Runes)
			{
				ByType.Add(FString::FromInt(Count));
				Total += Count;
			}
			return FString::Printf(TEXT("asiento %d: rol %s | runas %s, total %d | insolencia %d | Vacío %s"), Seat, *Role, *FString::Join(ByType, TEXT("/")), Total,
				Seated.Insolence, Seated.IsInVoid() ? TEXT("sí") : TEXT("no"));
		}
	}

	FString MatchRecordClock(double Seconds)
	{
		// Décimas truncadas; el épsilon evita que 83,4 s (que en binario es 83,3999...) se lea como 83,3.
		const int64 Tenths = FMath::Max<int64>(0, FMath::FloorToInt64(Seconds * 10.0 + 1e-6));
		return FString::Printf(TEXT("[%02lld:%02lld.%lld]"), Tenths / 600, (Tenths / 10) % 60, Tenths % 10);
	}

	FString MatchRecordLine(const FMatchState& State, int32 Index, int32 HumanSeat, int32 RingSeat)
	{
		if (!State.EventLog.IsValidIndex(Index))
		{
			return FString();
		}
		const FMatchEvent& Event = State.EventLog[Index];
		FString Phrase;
		if (Event.Type == EMatchEventType::WardenSpoke)
		{
			// R-ACT-2: la frase completa, con a quién habla; si la entrada no existe, lo dice.
			const bool bKnown = State.WardenLines.Entries.IsValidIndex(Event.Value) && State.WardenLines.Entries[Event.Value].Lines.IsValidIndex(Event.Detail);
			Phrase = bKnown ? WardenLinePrefix(Event.Seat, HumanSeat) + TEXT(" ") + SpokenText(State.WardenLines, Event)
				: FString::Printf(TEXT("frase desconocida (entrada %d, variante %d)"), Event.Value, Event.Detail);
		}
		else if (Event.Type == EMatchEventType::TileEntered)
		{
			// La crónica no los dice (DescribeEvent no tiene texto para ellos); el acta, que lleva todos los eventos, sí.
			Phrase = FString::Printf(TEXT("el asiento %d entra en %s"), Event.Seat, *TileShortName(State.Board, Event.Value));
		}
		else if (Event.Type == EMatchEventType::WardenTriggered)
		{
			Phrase = Event.Seat == INDEX_NONE ? FString::Printf(TEXT("disparador %s"), *TriggerName(Event.Value))
				: FString::Printf(TEXT("disparador %s (asiento %d)"), *TriggerName(Event.Value), Event.Seat);
		}
		else
		{
			Phrase = DescribeEvent(Event, State.Board, &State.WardenEvents, &State.WardenLines, RingSeat);
			if (Phrase.IsEmpty())
			{
				Phrase = TEXT("(sin frase)"); // el evento Moved no tiene texto en la crónica; en el acta, la línea existe igual
			}
		}
		return FString::Printf(TEXT("%s #%d %s asiento=%s otro=%s valor=%s detalle=%s | %s"), *MatchRecordClock(Event.ClockSeconds), Index, *EnumName(Event.Type),
			*Field(Event.Seat), *Field(Event.OtherSeat), *Field(Event.Value), *Field(Event.Detail), *Phrase);
	}

	TArray<FString> MatchRecordHeader(const FMatchState& State, int32 Seed, int32 HumanSeat, const FString& BuildText, const FString& BoardPath, const FDateTime& StartedAt)
	{
		TArray<FString> Lines;
		Lines.Add(TEXT("# Soulbound: acta de partida"));
		Lines.Add(FString::Printf(TEXT("# inicio: %s (hora local)"), *StartedAt.ToString(TEXT("%Y-%m-%d %H:%M:%S"))));
		Lines.Add(FString::Printf(TEXT("# build: %s"), *BuildText));
		Lines.Add(FString::Printf(TEXT("# semilla: %d"), Seed));
		Lines.Add(FString::Printf(TEXT("# tablero: %s, %d casillas"), *BoardPath, State.Board.Tiles.Num()));
		Lines.Add(FString::Printf(TEXT("# humano: asiento %d de %d asientos"), HumanSeat, State.Seats.Num()));
		Lines.Add(FString::Printf(TEXT("# duración prevista: %.0f s (%s)"), State.WardenParams.MatchEndSeconds, *MinutesAndSeconds(State.WardenParams.MatchEndSeconds)));
		for (int32 Domain = 0; Domain < State.Elects.Elects.Num(); ++Domain)
		{
			Lines.Add(FString::Printf(TEXT("# Dominio %d: Elegido %s"), Domain, *State.Elects.Elects[Domain].Name));
		}
		return Lines;
	}

	TArray<FString> MatchRecordSummary(const FMatchState& State)
	{
		TArray<FString> Lines;
		if (!State.IsMatchOver())
		{
			Lines.Add(FString::Printf(TEXT("partida abandonada: reloj %s, ronda %d, fase del turno %s, asiento activo %d"), *MatchRecordClock(State.ClockSeconds), State.Round,
				*EnumName(State.TurnPhase), State.ActiveSeat));
		}
		else
		{
			if (State.Outcome == EMatchOutcome::PlayerVictory)
			{
				Lines.Add(FString::Printf(TEXT("un jugador gana: asiento %d"), State.WinnerSeat));
			}
			else
			{
				Lines.Add(State.EndReason == EMatchEndReason::TimeOut ? TEXT("el Warden gana: tiempo") : TEXT("el Warden gana: nadie se vinculó"));
			}
			Lines.Add(FString::Printf(TEXT("fin de la partida: reloj %s, ronda %d"), *MatchRecordClock(State.ClockSeconds), State.Round));
		}
		for (int32 Seat = 0; Seat < State.Seats.Num(); ++Seat)
		{
			Lines.Add(SeatSummaryLine(State, Seat));
		}
		return Lines;
	}

	FString MatchRecordFileName(const FDateTime& StartedAt, int32 Seed)
	{
		return FString::Printf(TEXT("partida-%s-semilla-%d.log"), *StartedAt.ToString(TEXT("%Y%m%d-%H%M%S")), Seed);
	}

	FString MatchRecordHostLine(double Seconds, const TCHAR* Prefix, const FString& Text)
	{
		return FString::Printf(TEXT("%s %s | %s"), *MatchRecordClock(Seconds), Prefix, *Text);
	}

	FString IntentResultText(EIntentResult Result)
	{
		switch (Result)
		{
		case EIntentResult::Accepted: return TEXT("Accepted");
		case EIntentResult::WrongSeat: return TEXT("WrongSeat");
		case EIntentResult::WrongPhase: return TEXT("WrongPhase");
		case EIntentResult::Unreachable: return TEXT("Unreachable");
		case EIntentResult::ClockExpired: return TEXT("ClockExpired");
		case EIntentResult::InvalidOption: return TEXT("InvalidOption");
		case EIntentResult::NotNextTile: return TEXT("NotNextTile");
		}
		return FString::Printf(TEXT("resultado %d"), static_cast<int32>(Result));
	}

	FString RingPressLine(const FRingPressQuery& Query)
	{
		switch (Query.Kind)
		{
		case ERingPressKind::Counts:
			return FString::Printf(TEXT("pulsación del anillo: cuenta, pulso %d, desfase %+.3f s"), Query.Pulse + 1, Query.OffsetSeconds);
		case ERingPressKind::Announcement:
			return TEXT("pulsación del anillo: no cuenta, anuncio");
		case ERingPressKind::BetweenPulses:
			return TEXT("pulsación del anillo: no cuenta, entre pulsos");
		default:
			return TEXT("pulsación del anillo: no cuenta, prueba terminada");
		}
	}

	FString RingClosedRecordText(const FString& Why, int64 Frames, double Seconds)
	{
		const double FramesPerSecond = Seconds > 0.0 ? static_cast<double>(Frames) / Seconds : 0.0;
		return FString::Printf(TEXT("Anillo cerrado: %s; %.1f fotogramas por segundo (%lld fotogramas en %.1f s)."), *Why, FramesPerSecond, Frames, FMath::Max(Seconds, 0.0));
	}
}
