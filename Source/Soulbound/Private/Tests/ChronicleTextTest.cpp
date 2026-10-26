// Tests del texto de la crónica del juego (S-017, decisión del autor del 23/10/2026): los disparadores no tienen línea propia,
// van entre paréntesis en la decisión del Warden, que solo tiene línea si interviene con efecto. Sin mundo ni actor: las funciones
// de texto de la vista. Filtro: "Soulbound.Game.Presentation.Cronica".

#include "../../Presentation/EventText.h"
#include "Match/MatchState.h"
#include "Misc/AutomationTest.h"
#include "Warden/WardenBrain.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags ChronicleTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter;

	FMatchEvent MakeEvent(EMatchEventType Type, int32 Seat, int32 Value, int32 Detail = INDEX_NONE)
	{
		FMatchEvent Event;
		Event.Type = Type;
		Event.Seat = Seat;
		Event.Value = Value;
		Event.Detail = Detail;
		return Event;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FChronicleTriggersTest, "Soulbound.Game.Presentation.Cronica.DecisionConDisparadores", ChronicleTestFlags)

bool FChronicleTriggersTest::RunTest(const FString& Parameters)
{
	// R-WAR-10, R-DIS-1: en la crónica del juego, WardenTriggered no tiene línea; la decisión con efecto lleva sus disparadores
	// entre paréntesis ("golpe "Impuesto" (Acaparador: asiento 2)"), y la que no interviene no tiene línea.
	FWardenEventTable Events;
	FWardenEventDef& Tax = Events.Events.AddDefaulted_GetRef();
	Tax.Id = TEXT("impuesto");
	Tax.Name = TEXT("Impuesto");
	const FBoardState Board;
	const FMatchEvent Hoarder = MakeEvent(EMatchEventType::WardenTriggered, 2, static_cast<int32>(EWardenTrigger::Hoarder));
	const FMatchEvent Stalled = MakeEvent(EMatchEventType::WardenTriggered, INDEX_NONE, static_cast<int32>(EWardenTrigger::StalledMatch));
	TestTrue(TEXT("WardenTriggered no tiene línea"), Soulbound::Presentation::DescribeEvent(Hoarder, Board, &Events).IsEmpty());

	const FMatchEvent Strike = MakeEvent(EMatchEventType::WardenDecided, INDEX_NONE, 0, static_cast<int32>(EWardenDecisionKind::Strike));
	TestEqual(TEXT("Golpe con dos disparadores"), Soulbound::Presentation::DescribeWardenDecision(Strike, {Hoarder, Stalled}, false, &Events),
		FString(TEXT("el Warden decide: golpe \"Impuesto\" (Acaparador: asiento 2; Partida estancada)")));
	TestEqual(TEXT("Por el Oráculo, sin disparadores"), Soulbound::Presentation::DescribeWardenDecision(Strike, {}, true, &Events),
		FString(TEXT("el Warden decide: golpe \"Impuesto\" por el Oráculo")));
	const FMatchEvent Remark = MakeEvent(EMatchEventType::WardenDecided, INDEX_NONE, INDEX_NONE, static_cast<int32>(EWardenDecisionKind::Remark));
	TestTrue(TEXT("Un comentario no tiene línea de decisión"), Soulbound::Presentation::DescribeWardenDecision(Remark, {Hoarder}, false, &Events).IsEmpty());
	const FMatchEvent Nothing = MakeEvent(EMatchEventType::WardenDecided, INDEX_NONE, INDEX_NONE, static_cast<int32>(EWardenDecisionKind::None));
	TestTrue(TEXT("Ni una evaluación que no hace nada"), Soulbound::Presentation::DescribeWardenDecision(Nothing, {Hoarder}, false, &Events).IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FChronicleVoteTest, "Soulbound.Game.Presentation.Cronica.Votacion", ChronicleTestFlags)

bool FChronicleVoteTest::RunTest(const FString& Parameters)
{
	// R-VOT-3 (decisión del autor del 26/10/2026): la crónica lista al resolverse quién votó a quién (los VoteCast llegan entonces)
	// y el más votado, con el desempate si lo hubo; el voto al azar de quien no votó se distingue.
	FWardenEventTable Events;
	FWardenEventDef& Trial = Events.Events.AddDefaulted_GetRef();
	Trial.Id = TEXT("juicio");
	Trial.Name = TEXT("Juicio");
	const FBoardState Board;
	FMatchEvent Started = MakeEvent(EMatchEventType::VoteStarted, INDEX_NONE, 0, 4);
	const FString StartedText = Soulbound::Presentation::DescribeEvent(Started, Board, &Events);
	TestTrue(FString::Printf(TEXT("El inicio, con el nombre (%s)"), *StartedText), StartedText.Contains(TEXT("VOTACIÓN")) && StartedText.Contains(TEXT("Juicio")));
	FMatchEvent Cast = MakeEvent(EMatchEventType::VoteCast, 1, INDEX_NONE, 0);
	Cast.OtherSeat = 2;
	TestEqual(TEXT("Un voto: quién a quién"), Soulbound::Presentation::DescribeEvent(Cast, Board, &Events), FString(TEXT("el asiento 1 votó al asiento 2")));
	Cast.Detail = 1;
	TestEqual(TEXT("Un voto al azar"), Soulbound::Presentation::DescribeEvent(Cast, Board, &Events), FString(TEXT("el asiento 1 votó al asiento 2 (al azar: no votó a tiempo)")));
	const FMatchEvent Resolved = MakeEvent(EMatchEventType::VoteResolved, 2, 3, 1);
	TestEqual(TEXT("El más votado, con desempate"), Soulbound::Presentation::DescribeEvent(Resolved, Board, &Events),
		FString(TEXT("el más votado: asiento 2, con 3 votos (desempate del Warden)")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
