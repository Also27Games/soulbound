// Tests del texto del acta de partida (S-030, R-ACT-1 a R-ACT-5 y R-ACT-10): una línea por evento, la frase del Warden con su texto, la
// cabecera, el cierre, el nombre del archivo y las líneas del anillo. Sin mundo, sin actor y sin disco: funciones puras de
// Presentation/MatchRecord.h. La escritura del archivo y los subsistemas solo tienen criterio manual. Filtro: "Soulbound.Game.Presentation.Acta".

#include "../../Presentation/EventText.h"
#include "../../Presentation/MatchRecord.h"
#include "Match/MatchState.h"
#include "Misc/AutomationTest.h"
#include "Warden/WardenLines.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags ActaTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter;

	FMatchEvent MakeActaEvent(EMatchEventType Type, double Clock, int32 Seat, int32 Other = INDEX_NONE, int32 Value = INDEX_NONE, int32 Detail = INDEX_NONE)
	{
		FMatchEvent Event;
		Event.Type = Type;
		Event.ClockSeconds = Clock;
		Event.Seat = Seat;
		Event.OtherSeat = Other;
		Event.Value = Value;
		Event.Detail = Detail;
		return Event;
	}

	/** La partida de seis casillas del spike (cuatro asientos) con los seis Elegidos nombrados. */
	FMatchState ActaState()
	{
		FMatchState State = FMatchState::MakeSpikeMatch();
		for (int32 Domain = 0; Domain < 6; ++Domain)
		{
			FElectDef& Elect = State.Elects.Elects.AddDefaulted_GetRef();
			Elect.Name = FString::Printf(TEXT("Elegido%d"), Domain);
		}
		State.WardenParams.MatchEndSeconds = 2400.0;
		return State;
	}

	/** Las líneas unidas con saltos, para buscar texto sin fijarse en en cuál va. */
	FString Joined(const TArray<FString>& Lines)
	{
		return FString::Join(Lines, TEXT("\n"));
	}
}

// CA-ACT-1, R-ACT-1: un Rolled del asiento 2 con valor 7 a los 83,4 s; también TileEntered y WardenTriggered producen línea.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FActaEventLineTest, "Soulbound.Game.Presentation.Acta.LineaDeEvento", ActaTestFlags)
bool FActaEventLineTest::RunTest(const FString& Parameters)
{
	FMatchState State = ActaState();
	State.EventLog.Add(MakeActaEvent(EMatchEventType::Rolled, 83.4, 2, INDEX_NONE, 7));
	State.EventLog.Add(MakeActaEvent(EMatchEventType::TileEntered, 84.0, 2, INDEX_NONE, 0));
	State.EventLog.Add(MakeActaEvent(EMatchEventType::WardenTriggered, 85.0, 1, INDEX_NONE, 0));

	const FString Rolled = Soulbound::Presentation::MatchRecordLine(State, 0, 0, INDEX_NONE);
	TestTrue(TEXT("Marca de tiempo del reloj de partida"), Rolled.StartsWith(TEXT("[01:23.4]")));
	TestTrue(TEXT("El índice del evento"), Rolled.Contains(TEXT("#0 ")));
	TestTrue(TEXT("El nombre del tipo, tal cual y sin el prefijo del enum"), Rolled.Contains(TEXT(" Rolled ")) && !Rolled.Contains(TEXT("EMatchEventType")));
	TestTrue(TEXT("Los cuatro campos, con - en los vacíos"), Rolled.Contains(TEXT("asiento=2 otro=- valor=7 detalle=-")));
	TestTrue(TEXT("Y la frase de DescribeEvent tras |"), Rolled.Contains(TEXT("| ")) && Rolled.Contains(TEXT("7")));
	TestEqual(TEXT("La frase es la de DescribeEvent"), Rolled.RightChop(Rolled.Find(TEXT("| ")) + 2),
		Soulbound::Presentation::DescribeEvent(State.EventLog[0], State.Board, &State.WardenEvents, &State.WardenLines, INDEX_NONE));

	// El panel de crónica se salta estos dos; el acta no.
	const FString Entered = Soulbound::Presentation::MatchRecordLine(State, 1, 0, INDEX_NONE);
	const FString Triggered = Soulbound::Presentation::MatchRecordLine(State, 2, 0, INDEX_NONE);
	TestTrue(TEXT("TileEntered tiene línea"), Entered.Contains(TEXT("TileEntered")));
	TestTrue(TEXT("WardenTriggered tiene línea"), Triggered.Contains(TEXT("WardenTriggered")));
	// Con texto propio, no "(sin frase)": el del acta, sin tocar DescribeEvent ni la crónica.
	TestTrue(TEXT("TileEntered dice quién entra y dónde"),
		Entered.EndsWith(FString::Printf(TEXT("| el asiento 2 entra en %s"), *Soulbound::Presentation::TileShortName(State.Board, 0))));
	TestTrue(TEXT("WardenTriggered dice el disparador y su asiento"),
		Triggered.EndsWith(FString::Printf(TEXT("| disparador %s (asiento 1)"), *Soulbound::Presentation::TriggerName(0))));
	TestFalse(TEXT("Ninguno de los dos queda sin frase"), Entered.Contains(TEXT("sin frase")) || Triggered.Contains(TEXT("sin frase")));
	// La crónica y DescribeEvent siguen como estaban.
	TestTrue(TEXT("DescribeEvent de TileEntered no cambia"), Soulbound::Presentation::DescribeEvent(State.EventLog[1], State.Board).IsEmpty());
	TestTrue(TEXT("Un índice fuera del registro no da línea"), Soulbound::Presentation::MatchRecordLine(State, 3, 0, INDEX_NONE).IsEmpty());
	return true;
}

// CA-ACT-2, R-ACT-2: la frase del Warden con su texto sin corchetes; una entrada que no existe lo dice.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FActaSpokenLineTest, "Soulbound.Game.Presentation.Acta.FraseDelWarden", ActaTestFlags)
bool FActaSpokenLineTest::RunTest(const FString& Parameters)
{
	FMatchState State = ActaState();
	FWardenLineEntry& Entry = State.WardenLines.Entries.AddDefaulted_GetRef();
	Entry.Id = TEXT("evento:prisa");
	Entry.Lines = {TEXT("[risa] Prisa, prisa. [pausa]")};
	State.EventLog.Add(MakeActaEvent(EMatchEventType::WardenSpoke, 10.0, INDEX_NONE, INDEX_NONE, 0, 0));
	State.EventLog.Add(MakeActaEvent(EMatchEventType::WardenSpoke, 11.0, INDEX_NONE, INDEX_NONE, 9, 0));
	State.EventLog.Add(MakeActaEvent(EMatchEventType::WardenSpoke, 12.0, 2, INDEX_NONE, 0, 0));

	const FString Known = Soulbound::Presentation::MatchRecordLine(State, 0, 0, INDEX_NONE);
	TestTrue(TEXT("Lleva el prefijo del Warden a la mesa"), Known.Contains(TEXT("| Warden: ")));
	TestTrue(TEXT("Y el texto de la variante sin corchetes"), Known.EndsWith(TEXT("Prisa, prisa.")));
	TestFalse(TEXT("Sin acotaciones de audio"), Known.Contains(TEXT("[risa]")));
	TestTrue(TEXT("Una entrada inexistente dice frase desconocida"), Soulbound::Presentation::MatchRecordLine(State, 1, 0, INDEX_NONE).Contains(TEXT("frase desconocida")));
	TestTrue(TEXT("A otro asiento, el prefijo lo nombra"), Soulbound::Presentation::MatchRecordLine(State, 2, 0, INDEX_NONE).Contains(TEXT("Warden (al asiento 2):")));
	TestTrue(TEXT("Al asiento humano, a ti"), Soulbound::Presentation::MatchRecordLine(State, 2, 2, INDEX_NONE).Contains(TEXT("Warden (a ti):")));
	return true;
}

// CA-ACT-3, R-ACT-3: la cabecera.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FActaHeaderTest, "Soulbound.Game.Presentation.Acta.Cabecera", ActaTestFlags)
bool FActaHeaderTest::RunTest(const FString& Parameters)
{
	const FMatchState State = ActaState();
	const FString Header = Joined(Soulbound::Presentation::MatchRecordHeader(State, 742711968, 1, TEXT("0.1.0 Development"), TEXT("Data/Boards/tablero_15.json"),
		FDateTime(2026, 11, 8, 19, 37, 6)));
	TestTrue(TEXT("La semilla"), Header.Contains(TEXT("742711968")));
	TestTrue(TEXT("La hora de inicio"), Header.Contains(TEXT("2026-11-08 19:37:06")));
	TestTrue(TEXT("El texto de la build"), Header.Contains(TEXT("0.1.0 Development")));
	TestTrue(TEXT("El asiento humano y el número de asientos"), Header.Contains(TEXT("asiento 1 de 4")));
	TestTrue(TEXT("La ruta del tablero y el número de casillas"), Header.Contains(TEXT("Data/Boards/tablero_15.json")) && Header.Contains(FString::Printf(TEXT("%d casillas"), State.Board.Tiles.Num())));
	TestTrue(TEXT("La duración prevista"), Header.Contains(TEXT("2400")) && Header.Contains(TEXT("40:00")));

	// Una línea por Dominio con el nombre de su Elegido, en orden de Dominio.
	int32 Last = -1;
	for (int32 Domain = 0; Domain < 6; ++Domain)
	{
		const int32 At = Header.Find(FString::Printf(TEXT("Elegido%d"), Domain));
		TestTrue(*FString::Printf(TEXT("El Elegido del Dominio %d, tras el anterior"), Domain), At > Last);
		Last = At;
	}
	return true;
}

// CA-ACT-4, R-ACT-4: el cierre, con partida de jugador, del Warden por tiempo y abandonada.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FActaSummaryTest, "Soulbound.Game.Presentation.Acta.Cierre", ActaTestFlags)
bool FActaSummaryTest::RunTest(const FString& Parameters)
{
	auto CheckSeatLines = [this](const TArray<FString>& Lines, const TCHAR* Label)
	{
		for (int32 Seat = 0; Seat < 4; ++Seat)
		{
			const FString Prefix = FString::Printf(TEXT("asiento %d:"), Seat);
			const FString* Found = Lines.FindByPredicate([&Prefix](const FString& Line) { return Line.StartsWith(Prefix); });
			if (TestNotNull(*FString::Printf(TEXT("%s: línea del asiento %d"), Label, Seat), Found))
			{
				TestTrue(*FString::Printf(TEXT("%s: rol, runas, total e insolencia del asiento %d"), Label, Seat),
					Found->Contains(TEXT("rol")) && Found->Contains(TEXT("runas")) && Found->Contains(TEXT("total")) && Found->Contains(TEXT("insolencia")));
			}
		}
	};

	FMatchState Player = ActaState();
	Player.MatchPhase = EMatchPhase::MatchOver;
	Player.Outcome = EMatchOutcome::PlayerVictory;
	Player.WinnerSeat = 1;
	Player.Seats[1].Role = ESeatRole::Elect;
	Player.Seats[1].DomainId = 3;
	Player.Seats[1].Runes[3] = 2;
	Player.Seats[1].Insolence = 2;
	const TArray<FString> PlayerLines = Soulbound::Presentation::MatchRecordSummary(Player);
	TestTrue(TEXT("Gana un jugador"), PlayerLines.Num() > 0 && PlayerLines[0].StartsWith(TEXT("un jugador gana: asiento 1")));
	CheckSeatLines(PlayerLines, TEXT("PlayerVictory"));
	TestTrue(TEXT("El Elegido del ganador y su total"), Joined(PlayerLines).Contains(TEXT("Elegido3")) && Joined(PlayerLines).Contains(TEXT("total 2")));

	FMatchState Warden = ActaState();
	Warden.MatchPhase = EMatchPhase::MatchOver;
	Warden.Outcome = EMatchOutcome::WardenVictory;
	Warden.EndReason = EMatchEndReason::TimeOut;
	const TArray<FString> WardenLines = Soulbound::Presentation::MatchRecordSummary(Warden);
	TestTrue(TEXT("Gana el Warden por tiempo"), WardenLines.Num() > 0 && WardenLines[0].StartsWith(TEXT("el Warden gana: tiempo")));
	CheckSeatLines(WardenLines, TEXT("WardenVictory"));

	FMatchState Nobody = ActaState();
	Nobody.MatchPhase = EMatchPhase::MatchOver;
	Nobody.Outcome = EMatchOutcome::WardenVictory;
	Nobody.EndReason = EMatchEndReason::NobodyBonded;
	const TArray<FString> NobodyLines = Soulbound::Presentation::MatchRecordSummary(Nobody);
	TestTrue(TEXT("Gana el Warden porque nadie se vinculó"), NobodyLines.Num() > 0 && NobodyLines[0].StartsWith(TEXT("el Warden gana: nadie se vinculó")));

	FMatchState Abandoned = ActaState();
	Abandoned.ClockSeconds = 83.4;
	Abandoned.Round = 5;
	Abandoned.ActiveSeat = 2;
	Abandoned.TurnPhase = ETurnPhase::RollAndChoose;
	const TArray<FString> AbandonedLines = Soulbound::Presentation::MatchRecordSummary(Abandoned);
	TestTrue(TEXT("Sin terminar: partida abandonada, con el reloj, la ronda, la fase y el asiento activo"),
		AbandonedLines.Num() > 0 && AbandonedLines[0].StartsWith(TEXT("partida abandonada")) && AbandonedLines[0].Contains(TEXT("01:23.4")) && AbandonedLines[0].Contains(TEXT("ronda 5"))
		&& AbandonedLines[0].Contains(TEXT("RollAndChoose")) && AbandonedLines[0].Contains(TEXT("asiento activo 2")));
	CheckSeatLines(AbandonedLines, TEXT("Abandonada"));
	return true;
}

// CA-ACT-5, R-ACT-5: el nombre del archivo.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FActaFileNameTest, "Soulbound.Game.Presentation.Acta.NombreDeArchivo", ActaTestFlags)
bool FActaFileNameTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("El nombre lleva la fecha, la hora y la semilla"), Soulbound::Presentation::MatchRecordFileName(FDateTime(2026, 11, 8, 19, 37, 6), 742711968),
		FString(TEXT("partida-20261108-193706-semilla-742711968.log")));
	return true;
}

// CA-ACT-6, R-ACT-1: nada se salta. Doce eventos de tipos distintos, doce líneas, en orden, con índices consecutivos.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FActaNothingSkippedTest, "Soulbound.Game.Presentation.Acta.NadaSeSalta", ActaTestFlags)
bool FActaNothingSkippedTest::RunTest(const FString& Parameters)
{
	const EMatchEventType Types[] = {EMatchEventType::TurnStarted, EMatchEventType::Rolled, EMatchEventType::TurnClockExpired, EMatchEventType::TurnSkipped,
		EMatchEventType::TurnEnded, EMatchEventType::SealPlaced, EMatchEventType::SealReleased, EMatchEventType::Bonded, EMatchEventType::BoardAwakening,
		EMatchEventType::HomeAssigned, EMatchEventType::ReturnedHome, EMatchEventType::Moved};
	FMatchState State = ActaState();
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Types); ++Index)
	{
		State.EventLog.Add(MakeActaEvent(Types[Index], Index * 1.5, Index % 4, INDEX_NONE, Index));
	}
	TestEqual(TEXT("Doce eventos"), State.EventLog.Num(), 12);
	for (int32 Index = 0; Index < State.EventLog.Num(); ++Index)
	{
		const FString Line = Soulbound::Presentation::MatchRecordLine(State, Index, 0, INDEX_NONE);
		TestTrue(*FString::Printf(TEXT("La línea %d lleva su índice y su tipo"), Index),
			Line.Contains(FString::Printf(TEXT("#%d %s "), Index, *StaticEnum<EMatchEventType>()->GetNameStringByValue(static_cast<int64>(Types[Index])))));
	}
	return true;
}

// CA-ACT-7, R-ACT-10: el anillo medido. Pulsación con su pulso y su desfase con signo, o el motivo; y los fotogramas por segundo del cierre.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FActaRingTest, "Soulbound.Game.Presentation.Acta.Anillo", ActaTestFlags)
bool FActaRingTest::RunTest(const FString& Parameters)
{
	FRingPressQuery Counts;
	Counts.Kind = ERingPressKind::Counts;
	Counts.Pulse = 2;
	Counts.OffsetSeconds = -0.031;
	const FString CountsLine = Soulbound::Presentation::RingPressLine(Counts);
	TestTrue(TEXT("La pulsación que cuenta dice su pulso (de 1 en adelante) y el desfase con signo"), CountsLine.Contains(TEXT("pulso 3")) && CountsLine.Contains(TEXT("-0.031")));

	FRingPressQuery Announcement;
	Announcement.Kind = ERingPressKind::Announcement;
	const FString AnnouncementLine = Soulbound::Presentation::RingPressLine(Announcement);
	TestTrue(TEXT("En el anuncio, lo dice"), AnnouncementLine.Contains(TEXT("anuncio")));
	TestFalse(TEXT("Y no lleva desfase"), AnnouncementLine.Contains(TEXT("desfase")));

	FRingPressQuery Between;
	Between.Kind = ERingPressKind::BetweenPulses;
	TestTrue(TEXT("Entre dos pulsos, lo dice"), Soulbound::Presentation::RingPressLine(Between).Contains(TEXT("entre pulsos")));

	FRingPressQuery Finished;
	Finished.Kind = ERingPressKind::Finished;
	TestTrue(TEXT("Con la prueba terminada, lo dice"), Soulbound::Presentation::RingPressLine(Finished).Contains(TEXT("prueba terminada")));

	TestTrue(TEXT("180 fotogramas en 3,0 s son 60.0 por segundo"), Soulbound::Presentation::RingClosedRecordText(TEXT("prueba superada"), 180, 3.0).Contains(TEXT("60.0 fotogramas por segundo")));
	TestTrue(TEXT("El motivo del cierre se conserva"), Soulbound::Presentation::RingClosedRecordText(TEXT("prueba superada"), 180, 3.0).Contains(TEXT("prueba superada")));
	TestTrue(TEXT("Sin tiempo no se divide"), Soulbound::Presentation::RingClosedRecordText(TEXT("la fase ha cambiado"), 0, 0.0).Contains(TEXT("0.0 fotogramas por segundo")));
	return true;
}

// CA-MIN-18, R-MIN-19: la línea del acta al empezar una prueba del anillo dice "por el botón" o "sola" y los segundos esperados. Y una pulsación
// del anillo durante la espera, si llegara al acta, dice que es la espera, no el anuncio.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FActaRingStartTest, "Soulbound.Game.Presentation.Acta.LineaDeInicioDelAnillo", ActaTestFlags)
bool FActaRingStartTest::RunTest(const FString& Parameters)
{
	const FString Button = Soulbound::Presentation::RingStartLine(ERingStartCause::Button, 2.04);
	TestTrue(TEXT("Por el botón: lo dice"), Button.Contains(TEXT("por el botón")));
	TestFalse(TEXT("Y no dice \"sola\""), Button.Contains(TEXT("sola")));
	TestTrue(TEXT("Con los segundos esperados"), Button.Contains(TEXT("2.0 s")));

	const FString Alone = Soulbound::Presentation::RingStartLine(ERingStartCause::Countdown, 8.0);
	TestTrue(TEXT("Sola: lo dice"), Alone.Contains(TEXT("sola")));
	TestFalse(TEXT("Y no dice \"por el botón\""), Alone.Contains(TEXT("por el botón")));
	TestTrue(TEXT("Con los segundos esperados"), Alone.Contains(TEXT("8.0 s")));

	FRingPressQuery Waiting;
	Waiting.Kind = ERingPressKind::Waiting;
	const FString WaitingLine = Soulbound::Presentation::RingPressLine(Waiting);
	TestTrue(TEXT("En la espera, no cuenta y lo dice"), WaitingLine.Contains(TEXT("no cuenta")) && WaitingLine.Contains(TEXT("espera")));
	return true;
}

// CA-MIN-18 (corrección del autor del 16/11/2026 tras PIE: salió "empieza por el botón tras 8.0 s de espera"), R-MIN-19. Como lo escribe el anfitrión
// (StartRing: FRingTrialRun::Start y luego RingStartLine con su causa y su espera): si "Empezar" llega con la cuenta ya a cero y sin que el tick lo
// haya notado, la prueba empezó sola y el acta dice "sola", con la espera máxima. Si llega justo antes de cero, es por el botón y el acta nunca dice la
// espera máxima: los segundos se cortan a la décima, no se redondean (7,96 s dicen 7.9). Montaje: se abre en el 1000,3 con espera de 8 s (vence
// en el 1008,3; una suma que en coma flotante no da 8 exactos) y respiro de 0,5 s.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FActaRingLateStartTest, "Soulbound.Game.Presentation.Acta.InicioConLaCuentaVencida", ActaTestFlags)
bool FActaRingLateStartTest::RunTest(const FString& Parameters)
{
	constexpr double Open = 1000.3;
	FRingSequence Sequence;
	Sequence.PulseStarts = {0.0, 2.0};
	Sequence.CloseSeconds = 1.0;
	Sequence.SweetZoneSeconds = 0.2;
	const auto StartAt = [&](double Now)
	{
		FRingTrialRun Run;
		Run.Open(Sequence, Open, 8.0, 0.5);
		Run.Start(Now);
		return Soulbound::Presentation::RingStartLine(Run.StartCause, Run.WaitedSeconds());
	};

	for (const double Late : {Open + 8.0, Open + 8.02, Open + 9.5})
	{
		const FString Line = StartAt(Late);
		const FString Case = FString::Printf(TEXT("\"Empezar\" a los %.2f s, con la cuenta a cero sin notar"), Late - Open);
		TestTrue(Case + TEXT(": dice \"sola\""), Line.Contains(TEXT("sola")) && !Line.Contains(TEXT("por el botón")));
		TestTrue(Case + TEXT(": con la espera máxima"), Line.Contains(TEXT("tras 8.0 s")));
	}

	const FString JustBefore = StartAt(Open + 7.96);
	TestTrue(TEXT("A los 7,96 s: por el botón"), JustBefore.Contains(TEXT("por el botón")));
	TestTrue(TEXT("Y la espera cortada a la décima: 7.9, nunca la máxima"), JustBefore.Contains(TEXT("tras 7.9 s")));
	TestTrue(TEXT("A los 2,04 s: 2.0"), StartAt(Open + 2.04).Contains(TEXT("tras 2.0 s")));
	TestTrue(TEXT("A los 2,99 s: 2.9"), StartAt(Open + 2.99).Contains(TEXT("tras 2.9 s")));
	return true;
}

// R-ACT-7, R-ACT-8: las líneas que no son del registro llevan marca de tiempo y prefijo; el resultado del motor, en texto.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FActaHostLineTest, "Soulbound.Game.Presentation.Acta.LineaDelAnfitrion", ActaTestFlags)
bool FActaHostLineTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Marca, prefijo y texto"), Soulbound::Presentation::MatchRecordHostLine(83.4, TEXT("host"), TEXT("Partida iniciada")), FString(TEXT("[01:23.4] host | Partida iniciada")));
	TestEqual(TEXT("El reloj pasa de la hora sin romperse"), Soulbound::Presentation::MatchRecordClock(3725.0), FString(TEXT("[62:05.0]")));
	TestEqual(TEXT("Y un reloj negativo se queda en cero"), Soulbound::Presentation::MatchRecordClock(-1.0), FString(TEXT("[00:00.0]")));
	TestEqual(TEXT("El resultado del motor, en texto"), Soulbound::Presentation::IntentResultText(EIntentResult::Unreachable), FString(TEXT("Unreachable")));
	return true;
}

// Corrección del autor del 17/11/2026 (S-043): la crónica y el panel de estado dejan de decir la ronda tope de la Noche de los Ecos, pero el acta
// la sigue diciendo, porque sirve para leer partidas.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FActaEchoNightRoundTest, "Soulbound.Game.Presentation.Acta.NocheConRonda", ActaTestFlags)
bool FActaEchoNightRoundTest::RunTest(const FString& Parameters)
{
	FMatchState State = ActaState();
	State.EventLog.Add(MakeActaEvent(EMatchEventType::EchoNightBegan, 300.0, INDEX_NONE, INDEX_NONE, 9));
	const FString Line = Soulbound::Presentation::MatchRecordLine(State, 0, 0, INDEX_NONE);
	TestTrue(TEXT("El acta dice la ronda tope"), Line.EndsWith(TEXT("| NOCHE DE LOS ECOS: los Ecos caminan por cualquier Dominio (como mucho hasta la ronda 9)")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FActaExtraTurnTest, "Soulbound.Game.Presentation.Acta.TurnoExtra", ActaTestFlags)
bool FActaExtraTurnTest::RunTest(const FString& Parameters)
{
	// CA-ACT-9 (R-DEC-19, S-046): el acta escribe el turno extra con su tipo y el mismo texto que la crónica.
	FMatchState State = ActaState();
	State.EventLog.Add(MakeActaEvent(EMatchEventType::ExtraTurnTaken, 120.0, 2, INDEX_NONE, 0));
	const FString Line = Soulbound::Presentation::MatchRecordLine(State, 0, 0, INDEX_NONE);
	TestTrue(TEXT("Lleva su tipo"), Line.Contains(TEXT("#0 ExtraTurnTaken ")));
	TestTrue(TEXT("Y el texto"), Line.EndsWith(TEXT("| el asiento 2 juega un turno extra")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
