#pragma once

// Montaje de los tests de la voz del Warden (S-021): las frases de Content/Data/Warden/frases.json cargadas contra los eventos y los
// Elegidos de los archivos, y la partida del Warden (WardenFixture.h) con esas frases puestas en el estado. Solo para tests.

#include "Warden/WardenLines.h"
#include "WardenFixture.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace Soulbound::Tests
{
	/** Ruta de Content/Data/Warden/frases.json. */
	inline FString WardenLinesPath()
	{
		return FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Data/Warden/frases.json"));
	}

	/** Las claves que exige frases.json, construidas de los eventos y los Elegidos de los archivos. */
	inline bool RequiredTestLineKeys(FAutomationTestBase& Test, TArray<FString>& OutKeys)
	{
		FWardenEventTable Events;
		FElectsTable Elects;
		if (!LoadTestWardenEvents(Test, Events) || !LoadTestElects(Test, Elects))
		{
			return false;
		}
		OutKeys = Soulbound::Rules::RequiredWardenLineKeys(Events, Elects);
		return true;
	}

	/** Carga Content/Data/Warden/frases.json; si falla, lo apunta como error del test y devuelve false. */
	inline bool LoadTestWardenLines(FAutomationTestBase& Test, FWardenLineTable& OutTable)
	{
		TArray<FString> Required;
		if (!RequiredTestLineKeys(Test, Required))
		{
			return false;
		}
		const FWardenLinesLoadResult Result = Soulbound::Rules::LoadWardenLinesFromFile(WardenLinesPath(), Required);
		OutTable = Result.Table;
		return Test.TestTrue(FString::Printf(TEXT("Las frases del Warden cargan (%s)"), *Result.Message), Result.IsSuccess());
	}

	/** Clave de la frase que dijo un WardenSpoke: la de la entrada que apunta su Value. Vacía si el evento no es una frase. */
	inline FString SpokenKey(const FMatchState& State, const FMatchEvent& Event)
	{
		return Event.Type == EMatchEventType::WardenSpoke && State.WardenLines.Entries.IsValidIndex(Event.Value) ? State.WardenLines.Entries[Event.Value].Id : FString();
	}

	/** Posición del primer evento de Type desde From, o INDEX_NONE. */
	inline int32 IndexOfEvent(const FMatchState& State, EMatchEventType Type, int32 From = 0)
	{
		for (int32 Index = From; Index < State.EventLog.Num(); ++Index)
		{
			if (State.EventLog[Index].Type == Type)
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}

	/** Si el evento que sigue al de la posición Index es un WardenSpoke de la clave Key. */
	inline bool IsFollowedBySpoke(const FMatchState& State, int32 Index, const FString& Key)
	{
		return State.EventLog.IsValidIndex(Index) && State.EventLog.IsValidIndex(Index + 1) && SpokenKey(State, State.EventLog[Index + 1]) == Key;
	}

	/** La partida del Warden (acabada la carrera) con las frases del archivo en el estado. */
	struct FVoiceFixture : FWardenFixture
	{
		FWardenLineTable Lines;

		bool StartWithVoice(FAutomationTestBase& Test)
		{
			if (!StartWithWarden(Test) || !LoadTestWardenLines(Test, Lines))
			{
				return false;
			}
			State.WardenLines = Lines;
			State.Warden.VoiceSeed = 5;
			return true;
		}
	};
}

#endif // WITH_DEV_AUTOMATION_TESTS
