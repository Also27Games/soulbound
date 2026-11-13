// Tests de la carta del Elegido (S-029, R-VIS-13): cuándo se enseña. Seguidor puro sobre un FMatchState armado a mano y un reloj que da el
// test: sin mundo y sin actor. Filtro: "Soulbound.Game.Presentation.Carta".

#include "../../Presentation/ElectCard.h"
#include "Match/MatchState.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags CardTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter;
	constexpr int32 CdHuman = 0;
	constexpr int32 CdDummy = 2;
	constexpr double CdSeconds = 6.0;

	void CdBond(FMatchState& State, int32 Seat, int32 Domain)
	{
		FMatchEvent Event;
		Event.Type = EMatchEventType::Bonded;
		Event.Seat = Seat;
		Event.Value = Domain;
		State.EventLog.Add(Event);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FElectCardVisibilityTest, "Soulbound.Game.Presentation.Carta.VisibleHastaTiempoClicOGrimorio", CardTestFlags)

bool FElectCardVisibilityTest::RunTest(const FString& Parameters)
{
	// CA-VIS-13 (R-VIS-13): visible desde el Bonded del humano hasta ElectCardSeconds después.
	FMatchState State;
	Soulbound::Presentation::FElectCardTracker Tracker;
	Tracker.Observe(State, CdHuman, 9.0);
	TestFalse(TEXT("Antes del vínculo, no se ve"), Tracker.IsVisible(9.0, CdSeconds));
	CdBond(State, CdHuman, 2);
	Tracker.Observe(State, CdHuman, 10.0);
	TestTrue(TEXT("En el instante del vínculo, se ve"), Tracker.IsVisible(10.0, CdSeconds));
	TestEqual(TEXT("La carta es del Dominio 2"), Tracker.GetDomainId(), 2);
	TestTrue(TEXT("A los 5,9 s, se ve"), Tracker.IsVisible(15.9, CdSeconds));
	TestFalse(TEXT("A los 6 s, ya no"), Tracker.IsVisible(16.0, CdSeconds));

	// Hasta un clic.
	FMatchState Clicked;
	Soulbound::Presentation::FElectCardTracker ClickTracker;
	CdBond(Clicked, CdHuman, 1);
	ClickTracker.Observe(Clicked, CdHuman, 20.0);
	TestTrue(TEXT("Con clic: antes, se ve"), ClickTracker.IsVisible(21.0, CdSeconds));
	ClickTracker.Dismiss();
	TestFalse(TEXT("Con clic: después, no"), ClickTracker.IsVisible(21.0, CdSeconds));

	// Hasta abrir el Grimorio, y mientras siga abierto no vuelve.
	FMatchState Grimoire;
	Soulbound::Presentation::FElectCardTracker GrimoireTracker;
	CdBond(Grimoire, CdHuman, 4);
	GrimoireTracker.Observe(Grimoire, CdHuman, 30.0);
	GrimoireTracker.SetGrimoireOpen(true);
	TestFalse(TEXT("Con el Grimorio abierto, no"), GrimoireTracker.IsVisible(31.0, CdSeconds));
	GrimoireTracker.SetGrimoireOpen(false);
	TestFalse(TEXT("Cerrado el Grimorio, la carta no vuelve"), GrimoireTracker.IsVisible(31.0, CdSeconds));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FElectCardDummyTest, "Soulbound.Game.Presentation.Carta.NoParaDummiesNiParaLoYaOcurrido", CardTestFlags)

bool FElectCardDummyTest::RunTest(const FString& Parameters)
{
	// CA-VIS-13 (R-VIS-13): el Bonded de un dummy no enseña la carta.
	FMatchState State;
	Soulbound::Presentation::FElectCardTracker Tracker;
	CdBond(State, CdDummy, 3);
	Tracker.Observe(State, CdHuman, 5.0);
	TestFalse(TEXT("Vínculo de un dummy: nada"), Tracker.IsVisible(5.0, CdSeconds));
	// Los eventos se leen una vez: observar otra vez sin eventos nuevos no reabre nada.
	CdBond(State, CdHuman, 0);
	Tracker.Observe(State, CdHuman, 8.0);
	TestTrue(TEXT("Luego el del humano, sí"), Tracker.IsVisible(8.0, CdSeconds));
	Tracker.Dismiss();
	Tracker.Observe(State, CdHuman, 9.0);
	TestFalse(TEXT("Sin eventos nuevos, el clic se respeta"), Tracker.IsVisible(9.0, CdSeconds));
	// Una vista que arranca con la partida en marcha no enseña un vínculo antiguo.
	Soulbound::Presentation::FElectCardTracker Late;
	Late.SkipExisting(State);
	Late.Observe(State, CdHuman, 50.0);
	TestFalse(TEXT("Vínculo anterior al arranque: nada"), Late.IsVisible(50.0, CdSeconds));
	// Sin asiento humano, nada.
	Soulbound::Presentation::FElectCardTracker NoHuman;
	NoHuman.Observe(State, INDEX_NONE, 1.0);
	TestFalse(TEXT("Sin humano, nada"), NoHuman.IsVisible(1.0, CdSeconds));
	return true;
}

#endif
