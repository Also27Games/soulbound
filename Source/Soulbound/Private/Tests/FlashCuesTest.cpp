// Tests de los destellos (S-029, R-VIS-12, punto 8 de la story): cómo se convierten los eventos nuevos del registro en la lista de destellos que ejecuta la
// vista, con su sentido (hacia dentro es recibir, hacia fuera es perder) y su color. Funciones puras sobre un FMatchState armado a mano: sin mundo y sin
// actor. Filtro: "Soulbound.Game.Presentation.Destellos".

#include "../../Presentation/FlashCues.h"
#include "Match/MatchState.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags FlashTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter;
	constexpr float FcSeconds = 1.f;

	using Soulbound::Presentation::EFlashColorSource;
	using Soulbound::Presentation::EFlashDirection;
	using Soulbound::Presentation::EFlashTarget;
	using Soulbound::Presentation::FFlashCue;

	constexpr EFlashDirection FcIn = EFlashDirection::Inward;
	constexpr EFlashDirection FcOut = EFlashDirection::Outward;
	constexpr EFlashColorSource ByRune = EFlashColorSource::Rune;
	constexpr EFlashColorSource ByLoss = EFlashColorSource::Loss;

	void FcAdd(FMatchState& State, EMatchEventType Type, int32 Seat, int32 Value = INDEX_NONE, int32 OtherSeat = INDEX_NONE)
	{
		FMatchEvent Event;
		Event.Type = Type;
		Event.Seat = Seat;
		Event.Value = Value;
		Event.OtherSeat = OtherSeat;
		State.EventLog.Add(Event);
	}

	FFlashCue FcCue(EFlashTarget Target, int32 Id, EFlashDirection Direction, EFlashColorSource Color, int32 Rune, float Delay = 0.f)
	{
		FFlashCue Cue;
		Cue.Target = Target;
		Cue.Id = Id;
		Cue.Direction = Direction;
		Cue.ColorSource = Color;
		Cue.RuneType = Rune;
		Cue.DurationSeconds = FcSeconds;
		Cue.DelaySeconds = Delay;
		return Cue;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFlashObtainedTest, "Soulbound.Game.Presentation.Destellos.GanarRunaCierraLaFichaYAbreLaFuente", FlashTestFlags)

bool FFlashObtainedTest::RunTest(const FString& Parameters)
{
	// CA-VIS-12 (R-VIS-12, punto 8): un RuneObtained del asiento 0 con la runa del Dominio 4 produce dos destellos, la ficha 0 hacia dentro (recibe) y la Fuente
	// del Dominio 4 hacia fuera, los dos con el color de la runa 4.
	FMatchState State;
	FcAdd(State, EMatchEventType::RuneObtained, 0, 4);
	TestTrue(TEXT("Ficha 0 hacia dentro y Fuente 4 hacia fuera, con el color de la runa 4"),
		Soulbound::Presentation::FlashCuesFromEvents(State, 0, FcSeconds)
			== TArray<FFlashCue>({FcCue(EFlashTarget::Token, 0, FcIn, ByRune, 4), FcCue(EFlashTarget::Source, 4, FcOut, ByRune, 4)}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFlashStolenTest, "Soulbound.Game.Presentation.Destellos.RoboHaciaDentroAlLadronYHaciaFueraAlRobado", FlashTestFlags)

bool FFlashStolenTest::RunTest(const FString& Parameters)
{
	// CA-VIS-12 (R-VIS-12, punto 8): un RuneStolen con ladrón 1 y víctima 0 (runa del Dominio 2) produce el ladrón hacia dentro con el color de la runa y la
	// víctima hacia fuera con el de perder, y ninguna Fuente (la runa no pasa por ella). Lo mismo para RunesSeized: ganador y perdedor.
	FMatchState State;
	FcAdd(State, EMatchEventType::RuneStolen, 1, 2, 0);
	TestTrue(TEXT("Robo: ladrón hacia dentro con el color de la runa, víctima hacia fuera con el de perder"),
		Soulbound::Presentation::FlashCuesFromEvents(State, 0, FcSeconds)
			== TArray<FFlashCue>({FcCue(EFlashTarget::Token, 1, FcIn, ByRune, 2), FcCue(EFlashTarget::Token, 0, FcOut, ByLoss, 2)}));
	FMatchState Seized;
	FcAdd(Seized, EMatchEventType::RunesSeized, 3, 5, 2);
	TestTrue(TEXT("Confiscación: ganador hacia dentro con el color de la runa, perdedor hacia fuera con el de perder"),
		Soulbound::Presentation::FlashCuesFromEvents(Seized, 0, FcSeconds)
			== TArray<FFlashCue>({FcCue(EFlashTarget::Token, 3, FcIn, ByRune, 5), FcCue(EFlashTarget::Token, 2, FcOut, ByLoss, 5)}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFlashLossesTest, "Soulbound.Game.Presentation.Destellos.PerderRunaAbreLaFichaYCierraLaFuente", FlashTestFlags)

bool FFlashLossesTest::RunTest(const FString& Parameters)
{
	// R-VIS-12, punto 8: devolver una runa (Desvío, robo anulado...) abre el aro de la ficha hacia fuera con el color de perder y cierra el halo de la Fuente
	// de su tipo hacia dentro con el color de su runa.
	FMatchState Returned;
	FcAdd(Returned, EMatchEventType::RuneReturned, 2, 1);
	TestTrue(TEXT("RuneReturned: ficha hacia fuera con el de perder, Fuente hacia dentro con el de su runa"),
		Soulbound::Presentation::FlashCuesFromEvents(Returned, 0, FcSeconds)
			== TArray<FFlashCue>({FcCue(EFlashTarget::Token, 2, FcOut, ByLoss, 1), FcCue(EFlashTarget::Source, 1, FcIn, ByRune, 1)}));
	// Desafío final fallado: el registro deja FinalChallengeFailed y, justo después, el RuneReturned de la misma runa (S-018). La ficha destella una sola vez, hacia
	// fuera con el de perder; el RuneReturned solo aporta la Fuente (decisión del autor del 12/11/2026).
	FMatchState Failed;
	FcAdd(Failed, EMatchEventType::FinalChallengeFailed, 2, 1);
	FcAdd(Failed, EMatchEventType::RuneReturned, 2, 1);
	TestTrue(TEXT("Desafío fallado: la ficha una vez hacia fuera y la Fuente hacia dentro"),
		Soulbound::Presentation::FlashCuesFromEvents(Failed, 0, FcSeconds)
			== TArray<FFlashCue>({FcCue(EFlashTarget::Token, 2, FcOut, ByLoss, 1), FcCue(EFlashTarget::Source, 1, FcIn, ByRune, 1)}));
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
	TestTrue(TEXT("Cuatro destellos: ficha 0, Fuente 4, ficha 1 y ficha 0 otra vez, con retardo"),
		Soulbound::Presentation::FlashCuesFromEvents(State, 0, FcSeconds)
			== TArray<FFlashCue>({FcCue(EFlashTarget::Token, 0, FcIn, ByRune, 4), FcCue(EFlashTarget::Source, 4, FcOut, ByRune, 4), FcCue(EFlashTarget::Token, 1, FcIn, ByRune, 2),
				FcCue(EFlashTarget::Token, 0, FcOut, ByLoss, 2, FcSeconds)}));
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
		Soulbound::Presentation::FlashCuesFromEvents(State, 1, FcSeconds)
			== TArray<FFlashCue>({FcCue(EFlashTarget::Token, 2, FcIn, ByRune, 1), FcCue(EFlashTarget::Source, 1, FcOut, ByRune, 1)}));
	TestTrue(TEXT("Desde el final, nada"), Soulbound::Presentation::FlashCuesFromEvents(State, State.EventLog.Num(), FcSeconds).IsEmpty());
	TestTrue(TEXT("Un índice pasado del final, nada"), Soulbound::Presentation::FlashCuesFromEvents(State, 99, FcSeconds).IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFlashSealPlacedTest, "Soulbound.Game.Presentation.Destellos.SellarDestellaLaCasillaHaciaDentro", FlashTestFlags)

bool FFlashSealPlacedTest::RunTest(const FString& Parameters)
{
	// CA-VIS-23 (R-VIS-25): un SealPlaced de la casilla 12 da un destello de casilla, Id 12, hacia dentro y con el color del Sello, de la duración pedida;
	// FlashColorOf de ese destello es el color del Sello que se le pasa.
	FMatchState State;
	FcAdd(State, EMatchEventType::SealPlaced, INDEX_NONE, 12);
	const TArray<FFlashCue> Cues = Soulbound::Presentation::FlashCuesFromEvents(State, 0, 0.7f);
	TestEqual(TEXT("Un destello"), Cues.Num(), 1);
	if (Cues.Num() == 1)
	{
		TestTrue(TEXT("Casilla"), Cues[0].Target == EFlashTarget::Tile);
		TestEqual(TEXT("Id 12"), Cues[0].Id, 12);
		TestTrue(TEXT("Hacia dentro"), Cues[0].Direction == FcIn);
		TestTrue(TEXT("Color del Sello"), Cues[0].ColorSource == EFlashColorSource::Seal);
		TestEqual(TEXT("La duración pedida"), Cues[0].DurationSeconds, 0.7f);
		TestEqual(TEXT("Sin espera"), Cues[0].DelaySeconds, 0.f);
		const FLinearColor Seal(1.f, 0.05f, 0.05f);
		TestEqual(TEXT("FlashColorOf es el color del Sello"), Soulbound::Presentation::FlashColorOf(Cues[0], {}, FLinearColor::White, FLinearColor(0.55f, 0.f, 1.f), Seal), Seal);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFlashSealReleasedTest, "Soulbound.Game.Presentation.Destellos.LiberarDestellaLaCasillaHaciaFuera", FlashTestFlags)

bool FFlashSealReleasedTest::RunTest(const FString& Parameters)
{
	// CA-VIS-24 (R-VIS-25): un SealReleased de la casilla 12 da un destello de casilla hacia fuera con el color del Sello; un SealPlaced y un SealReleased de la
	// misma casilla en la misma lectura salen encadenados (el segundo espera una duración), y los de otra casilla no esperan.
	FMatchState State;
	FcAdd(State, EMatchEventType::SealReleased, INDEX_NONE, 12);
	TestTrue(TEXT("Casilla 12 hacia fuera, color del Sello"),
		Soulbound::Presentation::FlashCuesFromEvents(State, 0, FcSeconds) == TArray<FFlashCue>({FcCue(EFlashTarget::Tile, 12, FcOut, EFlashColorSource::Seal, INDEX_NONE)}));

	FMatchState Chained;
	FcAdd(Chained, EMatchEventType::SealPlaced, INDEX_NONE, 12);
	FcAdd(Chained, EMatchEventType::SealReleased, INDEX_NONE, 12);
	FcAdd(Chained, EMatchEventType::SealPlaced, INDEX_NONE, 7);
	TestTrue(TEXT("Misma casilla: encadenados; otra casilla: sin espera"),
		Soulbound::Presentation::FlashCuesFromEvents(Chained, 0, FcSeconds)
			== TArray<FFlashCue>({FcCue(EFlashTarget::Tile, 12, FcIn, EFlashColorSource::Seal, INDEX_NONE),
				FcCue(EFlashTarget::Tile, 12, FcOut, EFlashColorSource::Seal, INDEX_NONE, FcSeconds), FcCue(EFlashTarget::Tile, 7, FcIn, EFlashColorSource::Seal, INDEX_NONE)}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFlashSealLeavesRunesTest, "Soulbound.Game.Presentation.Destellos.LosDeRunasNoCambianConSellos", FlashTestFlags)

bool FFlashSealLeavesRunesTest::RunTest(const FString& Parameters)
{
	// CA-VIS-25 (R-VIS-25): los destellos de RuneObtained, RuneReturned y RuneStolen son los mismos aunque haya Sellos entre medias en la misma lectura.
	FMatchState State;
	FcAdd(State, EMatchEventType::RuneObtained, 0, 4);
	FcAdd(State, EMatchEventType::SealPlaced, INDEX_NONE, 12);
	FcAdd(State, EMatchEventType::RuneReturned, 1, 2);
	FcAdd(State, EMatchEventType::SealReleased, INDEX_NONE, 12);
	FcAdd(State, EMatchEventType::RuneStolen, 2, 3, 1);
	TArray<FFlashCue> OnlyRunes;
	const TArray<FFlashCue> All = Soulbound::Presentation::FlashCuesFromEvents(State, 0, FcSeconds);
	for (const FFlashCue& Cue : All)
	{
		if (Cue.Target != EFlashTarget::Tile)
		{
			OnlyRunes.Add(Cue);
		}
	}
	FMatchState Reference;
	FcAdd(Reference, EMatchEventType::RuneObtained, 0, 4);
	FcAdd(Reference, EMatchEventType::RuneReturned, 1, 2);
	FcAdd(Reference, EMatchEventType::RuneStolen, 2, 3, 1);
	TestTrue(TEXT("Mismos destellos de runas, con el mismo encadenado"), OnlyRunes == Soulbound::Presentation::FlashCuesFromEvents(Reference, 0, FcSeconds));
	TestEqual(TEXT("Y los dos de casilla se suman"), All.Num(), OnlyRunes.Num() + 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFlashColorTest,"Soulbound.Game.Presentation.Destellos.ColorDeCadaCaso", FlashTestFlags)

bool FFlashColorTest::RunTest(const FString& Parameters)
{
	// Punto 8: ganar o recibir usa el color del Dominio de la runa; perder en la ficha, el de perder (configuración), sea cual sea la runa; y un destello sin tipo
	// de runa conocido (o sin color), el claro de siempre.
	using Soulbound::Presentation::FlashColorOf;
	TArray<FLinearColor> Domains;
	for (int32 Index = 0; Index < 6; ++Index)
	{
		Domains.Add(FLinearColor(0.1f * (Index + 1), 0.2f, 0.3f));
	}
	const FLinearColor Clear(1.f, 1.f, 1.f);
	const FLinearColor Loss(0.55f, 0.f, 1.f);
	const FLinearColor Seal(1.f, 0.05f, 0.05f);
	TestEqual(TEXT("Recibir: el color del Dominio de la runa"), FlashColorOf(FcCue(EFlashTarget::Token, 0, FcIn, ByRune, 4), Domains, Clear, Loss, Seal), Domains[4]);
	TestEqual(TEXT("La Fuente al abrirse: el color de su runa"), FlashColorOf(FcCue(EFlashTarget::Source, 4, FcOut, ByRune, 4), Domains, Clear, Loss, Seal), Domains[4]);
	TestEqual(TEXT("La Fuente al cerrarse: el color de su runa"), FlashColorOf(FcCue(EFlashTarget::Source, 1, FcIn, ByRune, 1), Domains, Clear, Loss, Seal), Domains[1]);
	TestEqual(TEXT("Perder en la ficha: el de perder, no el de la runa"), FlashColorOf(FcCue(EFlashTarget::Token, 0, FcOut, ByLoss, 2), Domains, Clear, Loss, Seal), Loss);
	TestEqual(TEXT("Perder sin tipo de runa: el de perder"), FlashColorOf(FcCue(EFlashTarget::Token, 0, FcOut, ByLoss, INDEX_NONE), Domains, Clear, Loss, Seal), Loss);
	TestEqual(TEXT("Sin tipo de runa conocido: el claro de siempre"), FlashColorOf(FcCue(EFlashTarget::Token, 0, FcIn, ByRune, INDEX_NONE), Domains, Clear, Loss, Seal), Clear);
	TestEqual(TEXT("Un tipo sin color: el claro de siempre"), FlashColorOf(FcCue(EFlashTarget::Token, 0, FcIn, ByRune, 9), Domains, Clear, Loss, Seal), Clear);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFlashUnknownRuneTest, "Soulbound.Game.Presentation.Destellos.SinTipoDeRunaConocido", FlashTestFlags)

bool FFlashUnknownRuneTest::RunTest(const FString& Parameters)
{
	// Punto 8: un RuneObtained sin tipo de runa en el registro destella la ficha hacia dentro (con el claro de siempre, FlashColorOf) y ninguna Fuente.
	FMatchState State;
	FcAdd(State, EMatchEventType::RuneObtained, 0, INDEX_NONE);
	TestTrue(TEXT("Solo la ficha, hacia dentro, sin tipo de runa"),
		Soulbound::Presentation::FlashCuesFromEvents(State, 0, FcSeconds) == TArray<FFlashCue>({FcCue(EFlashTarget::Token, 0, FcIn, ByRune, INDEX_NONE)}));
	return true;
}

#endif
