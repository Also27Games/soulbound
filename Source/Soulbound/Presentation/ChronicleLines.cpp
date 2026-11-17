#include "ChronicleLines.h"

#include "EventText.h"
#include "Match/MatchState.h"

namespace Soulbound::Presentation
{
	FString ChronicleStamp(double Seconds)
	{
		const int32 Total = FMath::Max(0, FMath::FloorToInt(Seconds));
		return FString::Printf(TEXT("%d:%02d"), Total / 60, Total % 60);
	}

	TArray<FChronicleLine> ChronicleLines(const FMatchState& State, const TArray<FInstructionNote>& Notes, const FChronicleLook& Look)
	{
		// Sacado de ABoardViewActor::RefreshChronicle (S-044) sin cambiar nada: la más reciente primero, solo los últimos MaxEvents eventos.
		TArray<FChronicleLine> Lines;
		const int32 First = FMath::Max(0, State.EventLog.Num() - FMath::Max(Look.MaxEvents, 1));
		for (int32 Index = State.EventLog.Num() - 1; Index >= First; --Index)
		{
			// S-021 (A3): las instrucciones enseñadas justo después de este evento van encima de él (la crónica es de la más reciente a la más vieja).
			for (int32 Note = Notes.Num() - 1; Note >= 0; --Note)
			{
				if (Notes[Note].After == Index)
				{
					Lines.Add({FString::Printf(TEXT("[%s] %s"), *ChronicleStamp(Notes[Note].ClockSeconds), *Notes[Note].Text), Look.WardenColor});
				}
			}
			const FMatchEvent& Event = State.EventLog[Index];
			FLinearColor Color = Look.SeatColors.IsValidIndex(Event.Seat) ? Look.SeatColors[Event.Seat] : FLinearColor::White;
			switch (EventSource(Event.Type))
			{
			case EEventSource::Warden: Color = Look.WardenColor; break;
			case EEventSource::Race: Color = Look.RaceColor; break;
			case EEventSource::Debug: Color = Look.DebugColor; break;
			case EEventSource::Seat: break;
			}
			// S-016: las evaluaciones del Warden que no hacen nada no tienen línea (una cada 15 s taparía la crónica). S-017: la decisión
			// con efecto sí, con sus disparadores entre paréntesis (los WardenTriggered que la siguen, que no tienen línea propia).
			FString Text;
			if (Event.Type == EMatchEventType::WardenDecided)
			{
				TArray<FMatchEvent> Triggers;
				bool bViaOracle = false;
				for (int32 Next = Index + 1; Next < State.EventLog.Num(); ++Next)
				{
					const FMatchEvent& After = State.EventLog[Next];
					if (After.Type == EMatchEventType::WardenTriggered)
					{
						Triggers.Add(After);
						continue;
					}
					// Lo que aplica la decisión, en el mismo instante: el corte, el fin del turno y, si va por el Oráculo, su consulta.
					if (After.ClockSeconds != Event.ClockSeconds || After.Type == EMatchEventType::WardenEventStarted || After.Type == EMatchEventType::WardenEvaluated)
					{
						break;
					}
					bViaOracle |= After.Type == EMatchEventType::OracleOpened && After.Value == static_cast<int32>(EOracleOrigin::Strike);
				}
				Text = DescribeWardenDecision(Event, Triggers, bViaOracle, &State.WardenEvents);
			}
			else
			{
				Text = DescribeChronicleEvent(State, Index, Look.RingSeat);
			}
			if (!Text.IsEmpty())
			{
				Lines.Add({FString::Printf(TEXT("[%s] %s"), *ChronicleStamp(Event.ClockSeconds), *Text), Color});
			}
		}
		return Lines;
	}
}
