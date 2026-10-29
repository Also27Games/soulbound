// Tests del texto de la crónica del juego (S-017, decisión del autor del 23/10/2026): los disparadores no tienen línea propia,
// van entre paréntesis en la decisión del Warden, que solo tiene línea si interviene con efecto. Sin mundo ni actor: las funciones
// de texto de la vista. Desde S-022 (R-HUD-4, R-VOZ-3b), cada golpe, escena, castigo, Furia, comentario y aviso con su frase es una sola
// línea. Filtro: "Soulbound.Game.Presentation.Cronica".

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

	/** Una partida con un golpe ("Impuesto"), una escena ("Cosecha"), un evento de prueba fuera de los sacos y una frase. */
	FMatchState ChronicleStateWithLines()
	{
		FMatchState State;
		FWardenEventDef& Tax = State.WardenEvents.Events.AddDefaulted_GetRef();
		Tax.Id = TEXT("impuesto");
		Tax.Name = TEXT("Impuesto");
		Tax.bInBag = true;
		Tax.Channel = EWardenChannel::Strike;
		FWardenEventDef& Harvest = State.WardenEvents.Events.AddDefaulted_GetRef();
		Harvest.Id = TEXT("cosecha");
		Harvest.Name = TEXT("Cosecha");
		Harvest.bInBag = true;
		Harvest.Channel = EWardenChannel::Scene;
		FWardenEventDef& Probe = State.WardenEvents.Events.AddDefaulted_GetRef();
		Probe.Id = TEXT("prueba-golpe");
		Probe.Name = TEXT("Prueba");
		FWardenLineEntry& Entry = State.WardenLines.Entries.AddDefaulted_GetRef();
		Entry.Id = TEXT("evento:impuesto");
		Entry.Lines = {TEXT("[risa] Pagarás.")};
		return State;
	}

	/** La frase de la tabla de ChronicleStateWithLines, dicha a la mesa. */
	FMatchEvent ChronicleSpoke()
	{
		return MakeEvent(EMatchEventType::WardenSpoke, INDEX_NONE, 0, 0);
	}

	/** La línea de la crónica de Event seguido de su frase; comprueba además que la línea de la frase queda vacía. */
	FString MergedLine(FAutomationTestBase& Test, FMatchState State, const FMatchEvent& Event)
	{
		State.EventLog = {Event, ChronicleSpoke()};
		Test.TestTrue(FString::Printf(TEXT("La línea de la frase tras el evento %d queda vacía"), static_cast<int32>(Event.Type)),
			Soulbound::Presentation::DescribeChronicleEvent(State, 1).IsEmpty());
		return Soulbound::Presentation::DescribeChronicleEvent(State, 0);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FChronicleMergedEventTest, "Soulbound.Game.Presentation.Cronica.EventoYFraseEnUnaLinea", ChronicleTestFlags)

bool FChronicleMergedEventTest::RunTest(const FString& Parameters)
{
	// CA-CRO-1 (S-022, R-HUD-4, R-VOZ-3b): WardenEventStarted seguido de su WardenSpoke es una sola línea, la etiqueta del canal con el
	// nombre del evento y la frase; la del WardenSpoke queda vacía. Un dilema, con la etiqueta de su canal. Un evento de prueba, por su
	// identificador (decisión del autor del 29/10/2026).
	const FMatchState State = ChronicleStateWithLines();
	TestEqual(TEXT("Golpe"), MergedLine(*this, State, MakeEvent(EMatchEventType::WardenEventStarted, 2, 0, 1)), FString(TEXT("golpe \"Impuesto\": Pagarás.")));
	TestEqual(TEXT("Escena"), MergedLine(*this, State, MakeEvent(EMatchEventType::WardenEventStarted, 1, 1, 1)), FString(TEXT("escena \"Cosecha\": Pagarás.")));
	TestEqual(TEXT("Dilema: la etiqueta de su canal"), MergedLine(*this, State, MakeEvent(EMatchEventType::WardenEventStarted, 2, 0, 3)), FString(TEXT("golpe \"Impuesto\": Pagarás.")));
	TestEqual(TEXT("Evento de prueba"), MergedLine(*this, State, MakeEvent(EMatchEventType::WardenEventStarted, 2, 2, 1)), FString(TEXT("evento \"prueba-golpe\": Pagarás.")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FChronicleMergedWardenTest, "Soulbound.Game.Presentation.Cronica.CastigoFuriaComentarioYAvisoEnUnaLinea", ChronicleTestFlags)

bool FChronicleMergedWardenTest::RunTest(const FString& Parameters)
{
	// CA-CRO-1 (S-022): lo mismo para el castigo de insolencia (con su gravedad), la Furia, el comentario y el aviso del final.
	const FMatchState State = ChronicleStateWithLines();
	TestEqual(TEXT("Castigo leve"), MergedLine(*this, State, MakeEvent(EMatchEventType::InsolencePunished, 1, 1, static_cast<int32>(EInsolencePunishment::Light))),
		FString(TEXT("castigo leve: Pagarás.")));
	TestEqual(TEXT("Castigo grave"), MergedLine(*this, State, MakeEvent(EMatchEventType::InsolencePunished, 1, 3, static_cast<int32>(EInsolencePunishment::Severe))),
		FString(TEXT("castigo grave: Pagarás.")));
	TestEqual(TEXT("Furia"), MergedLine(*this, State, MakeEvent(EMatchEventType::FuryUnleashed, 1, INDEX_NONE)), FString(TEXT("Furia: Pagarás.")));
	TestEqual(TEXT("Comentario"), MergedLine(*this, State, MakeEvent(EMatchEventType::WardenRemark, 1, static_cast<int32>(EWardenRemarkKind::Hurry))),
		FString(TEXT("comentario: Pagarás.")));
	TestEqual(TEXT("Aviso del final"), MergedLine(*this, State, MakeEvent(EMatchEventType::MatchEndWarning, INDEX_NONE, 60)), FString(TEXT("aviso: Pagarás.")));

	// La Furia por insolencia: el castigo no dice frase (la dice la Furia), así que conserva su línea y la Furia lleva la frase.
	FMatchState Fury = State;
	Fury.EventLog = {MakeEvent(EMatchEventType::InsolencePunished, 1, 4, static_cast<int32>(EInsolencePunishment::Fury)),
		MakeEvent(EMatchEventType::FuryUnleashed, 1, INDEX_NONE), ChronicleSpoke()};
	TestTrue(TEXT("El castigo de la Furia, su línea de siempre"), Soulbound::Presentation::DescribeChronicleEvent(Fury, 0).Contains(TEXT("CASTIGO")));
	TestEqual(TEXT("La Furia, con la frase"), Soulbound::Presentation::DescribeChronicleEvent(Fury, 1), FString(TEXT("Furia: Pagarás.")));
	TestTrue(TEXT("Y la frase, vacía"), Soulbound::Presentation::DescribeChronicleEvent(Fury, 2).IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FChronicleUnmergedTest, "Soulbound.Game.Presentation.Cronica.SinFraseDetrasLasDeSiempre", ChronicleTestFlags)

bool FChronicleUnmergedTest::RunTest(const FString& Parameters)
{
	// CA-CRO-1 (S-022): un WardenEventStarted sin WardenSpoke detrás conserva la línea descriptiva; con la tabla de frases vacía, todo como
	// hoy (cada evento con su línea); y lo que la story no nombra (la presentación de un Elegido tras Bonded) sigue en dos líneas.
	const FMatchState State = ChronicleStateWithLines();
	FMatchState Alone = State;
	Alone.EventLog = {MakeEvent(EMatchEventType::WardenEventStarted, 2, 0, 1), MakeEvent(EMatchEventType::EffectApplied, 2, 0)};
	const FString AloneText = Soulbound::Presentation::DescribeChronicleEvent(Alone, 0);
	TestTrue(FString::Printf(TEXT("Sin frase detrás, la descriptiva (%s)"), *AloneText), AloneText.Contains(TEXT("GOLPE")) && AloneText.Contains(TEXT("asiento 2")));

	FMatchState Silent = State;
	Silent.WardenLines = FWardenLineTable();
	Silent.EventLog = {MakeEvent(EMatchEventType::WardenEventStarted, 2, 0, 1), ChronicleSpoke(), MakeEvent(EMatchEventType::WardenRemark, 1, 0), ChronicleSpoke()};
	TestTrue(TEXT("Sin tabla, el evento con su línea"), Soulbound::Presentation::DescribeChronicleEvent(Silent, 0).Contains(TEXT("GOLPE")));
	TestFalse(TEXT("Sin tabla, la frase con la suya"), Soulbound::Presentation::DescribeChronicleEvent(Silent, 1).IsEmpty());
	TestTrue(TEXT("Sin tabla, el comentario con la suya"), Soulbound::Presentation::DescribeChronicleEvent(Silent, 2).Contains(TEXT("comenta")));
	TestFalse(TEXT("Sin tabla, su frase con la suya"), Soulbound::Presentation::DescribeChronicleEvent(Silent, 3).IsEmpty());

	FMatchState Bonded = State;
	Bonded.EventLog = {MakeEvent(EMatchEventType::Bonded, 1, 0), ChronicleSpoke()};
	TestFalse(TEXT("Bonded, su línea"), Soulbound::Presentation::DescribeChronicleEvent(Bonded, 0).IsEmpty());
	TestEqual(TEXT("Y la frase aparte"), Soulbound::Presentation::DescribeChronicleEvent(Bonded, 1), FString(TEXT("Pagarás.")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FChronicleOracleStillOneLineTest, "Soulbound.Game.Presentation.Cronica.SentenciaSigueEnUnaLinea", ChronicleTestFlags)

bool FChronicleOracleStillOneLineTest::RunTest(const FString& Parameters)
{
	// CA-CRO-1 (S-022): la sentencia del Oráculo sigue siendo una sola línea (S-021); el evento que lanza no se junta con nada.
	FMatchState State = ChronicleStateWithLines();
	State.EventLog = {MakeEvent(EMatchEventType::OracleSentence, 2, 0, 0), MakeEvent(EMatchEventType::WardenTriggered, 2, 0), ChronicleSpoke(),
		MakeEvent(EMatchEventType::WardenEventStarted, 2, 0, 1)};
	TArray<FString> Texts;
	for (int32 Index = 0; Index < State.EventLog.Num(); ++Index)
	{
		const FString Text = Soulbound::Presentation::DescribeChronicleEvent(State, Index);
		if (!Text.IsEmpty())
		{
			Texts.Add(Text);
		}
	}
	if (TestEqual(TEXT("Una sola línea"), Texts.Num(), 1))
	{
		TestEqual(TEXT("La sentencia con la frase"), Texts[0], FString(TEXT("sentencia del Oráculo: Pagarás.")));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
