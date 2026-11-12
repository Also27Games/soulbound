// Tests de los destellos (S-029, R-VIS-12): cómo se convierten los eventos nuevos del registro en la lista de destellos que ejecuta la vista.
// Función pura sobre un FMatchState armado a mano: sin mundo y sin actor. Filtro: "Soulbound.Game.Presentation.Destellos".

#include "../../Presentation/FlashCues.h"
#include "Match/MatchState.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags FlashTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter;
	constexpr float FcSeconds = 0.6f;

	using Soulbound::Presentation::EFlashTarget;
	using Soulbound::Presentation::FFlashCue;

	void FcAdd(FMatchState& State, EMatchEventType Type, int32 Seat, int32 Value = INDEX_NONE, int32 OtherSeat = INDEX_NONE)
	{
		FMatchEvent Event;
		Event.Type = Type;
		Event.Seat = Seat;
		Event.Value = Value;
		Event.OtherSeat = OtherSeat;
		State.EventLog.Add(Event);
	}

	FFlashCue FcCue(EFlashTarget Target, int32 Id, bool bGain, float Delay = 0.f)
	{
		FFlashCue Cue;
		Cue.Target = Target;
		Cue.Id = Id;
		Cue.bGain = bGain;
		Cue.DurationSeconds = FcSeconds;
		Cue.DelaySeconds = Delay;
		return Cue;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFlashObtainedTest, "Soulbound.Game.Presentation.Destellos.GanarRunaDaClaroEnFichaYFuente", FlashTestFlags)

bool FFlashObtainedTest::RunTest(const FString& Parameters)
{
	// CA-VIS-12 (R-VIS-12): un RuneObtained del asiento 0 con la runa del Dominio 4 produce dos destellos claros, la ficha 0 y la Fuente del Dominio 4.
	FMatchState State;
	FcAdd(State, EMatchEventType::RuneObtained, 0, 4);
	const TArray<FFlashCue> Cues = Soulbound::Presentation::FlashCuesFromEvents(State, 0, FcSeconds);
	TestTrue(TEXT("Ficha 0 y Fuente del Dominio 4, claros"), Cues == TArray<FFlashCue>({FcCue(EFlashTarget::Token, 0, true), FcCue(EFlashTarget::Source, 4, true)}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFlashStolenTest, "Soulbound.Game.Presentation.Destellos.RoboClaroAlLadronYRojoAlRobado", FlashTestFlags)

bool FFlashStolenTest::RunTest(const FString& Parameters)
{
	// CA-VIS-12 (R-VIS-12): un RuneStolen con ladrón 1 y víctima 0 produce claro en el 1 y rojo en el 0, y ninguna Fuente (decisión del autor
	// del 12/11/2026: la runa no pasa por ella). Lo mismo para RunesSeized: ganador claro, perdedor rojo.
	FMatchState State;
	FcAdd(State, EMatchEventType::RuneStolen, 1, 2, 0);
	TestTrue(TEXT("Robo: claro en el ladrón, rojo en la víctima"),
		Soulbound::Presentation::FlashCuesFromEvents(State, 0, FcSeconds) == TArray<FFlashCue>({FcCue(EFlashTarget::Token, 1, true), FcCue(EFlashTarget::Token, 0, false)}));
	FMatchState Seized;
	FcAdd(Seized, EMatchEventType::RunesSeized, 3, 5, 2);
	TestTrue(TEXT("Confiscación: claro en el ganador, rojo en el perdedor"),
		Soulbound::Presentation::FlashCuesFromEvents(Seized, 0, FcSeconds) == TArray<FFlashCue>({FcCue(EFlashTarget::Token, 3, true), FcCue(EFlashTarget::Token, 2, false)}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFlashLossesTest, "Soulbound.Game.Presentation.Destellos.PerdidasEnRojo", FlashTestFlags)

bool FFlashLossesTest::RunTest(const FString& Parameters)
{
	// R-VIS-12: devolver una runa (Desvío, robo anulado...) da rojo en la ficha y en la Fuente de su tipo.
	FMatchState Returned;
	FcAdd(Returned, EMatchEventType::RuneReturned, 2, 1);
	TestTrue(TEXT("RuneReturned: ficha y Fuente en rojo"),
		Soulbound::Presentation::FlashCuesFromEvents(Returned, 0, FcSeconds) == TArray<FFlashCue>({FcCue(EFlashTarget::Token, 2, false), FcCue(EFlashTarget::Source, 1, false)}));
	// Desafío final fallado: el registro deja FinalChallengeFailed y, justo después, el RuneReturned de la misma runa (S-018). La ficha
	// destella una sola vez en rojo; el RuneReturned solo aporta la Fuente (decisión del autor del 12/11/2026).
	FMatchState Failed;
	FcAdd(Failed, EMatchEventType::FinalChallengeFailed, 2, 1);
	FcAdd(Failed, EMatchEventType::RuneReturned, 2, 1);
	TestTrue(TEXT("Desafío fallado: la ficha una vez y la Fuente"),
		Soulbound::Presentation::FlashCuesFromEvents(Failed, 0, FcSeconds) == TArray<FFlashCue>({FcCue(EFlashTarget::Token, 2, false), FcCue(EFlashTarget::Source, 1, false)}));
	// Un RuneReturned que no sigue a un desafío fallado sí lleva su ficha.
	FMatchState Other;
	FcAdd(Other, EMatchEventType::FinalChallengeFailed, 2, 1);
	FcAdd(Other, EMatchEventType::RuneReturned, 3, 1);
	TestEqual(TEXT("RuneReturned de otro asiento: ficha y Fuente además del desafío"), Soulbound::Presentation::FlashCuesFromEvents(Other, 0, FcSeconds).Num(), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFlashChainTest, "Soulbound.Game.Presentation.Destellos.EventosSeguidosSeEncadenan", FlashTestFlags)

bool FFlashChainTest::RunTest(const FString& Parameters)
{
	// CA-VIS-12 (R-VIS-12): dos eventos en el mismo instante que tocan la misma ficha no la pisan: el segundo espera a que acabe el primero.
	FMatchState State;
	FcAdd(State, EMatchEventType::RuneObtained, 0, 4);
	FcAdd(State, EMatchEventType::RuneStolen, 1, 2, 0);
	const TArray<FFlashCue> Cues = Soulbound::Presentation::FlashCuesFromEvents(State, 0, FcSeconds);
	TestTrue(TEXT("Cuatro destellos: ficha 0, Fuente 4, ficha 1 y ficha 0 otra vez, con retardo"),
		Cues == TArray<FFlashCue>({FcCue(EFlashTarget::Token, 0, true), FcCue(EFlashTarget::Source, 4, true), FcCue(EFlashTarget::Token, 1, true),
			FcCue(EFlashTarget::Token, 0, false, FcSeconds)}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFlashIncrementalTest, "Soulbound.Game.Presentation.Destellos.SoloLosEventosNuevos", FlashTestFlags)

bool FFlashIncrementalTest::RunTest(const FString& Parameters)
{
	// R-VIS-12, nota de la story y añadido del autor del 12/11/2026: se lee desde FromIndex; lo anterior (y lo que no es de runas) no destella.
	FMatchState State;
	FcAdd(State, EMatchEventType::RuneObtained, 0, 4);
	FcAdd(State, EMatchEventType::Moved, 1, 3);
	FcAdd(State, EMatchEventType::RuneObtained, 2, 1);
	TestTrue(TEXT("Desde el evento 1, solo el del asiento 2"),
		Soulbound::Presentation::FlashCuesFromEvents(State, 1, FcSeconds) == TArray<FFlashCue>({FcCue(EFlashTarget::Token, 2, true), FcCue(EFlashTarget::Source, 1, true)}));
	TestTrue(TEXT("Desde el final, nada"), Soulbound::Presentation::FlashCuesFromEvents(State, State.EventLog.Num(), FcSeconds).IsEmpty());
	TestTrue(TEXT("Un índice pasado del final, nada"), Soulbound::Presentation::FlashCuesFromEvents(State, 99, FcSeconds).IsEmpty());
	return true;
}

#endif
