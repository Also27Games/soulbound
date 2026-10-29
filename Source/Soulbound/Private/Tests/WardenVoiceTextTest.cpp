// Tests del texto de la voz del Warden en el juego (S-021, GDD 06 y 07): la frase que enseña la línea del Warden (R-VOZ-1 sin audio,
// R-VOZ-3b), la crónica con la sentencia del Oráculo en una sola línea y la cola de la línea en pantalla; desde S-022, la duración del
// prólogo (R-PRO-1); desde S-023, la espera del Warden (qué frases detienen la mesa) y las frases de la cola ya enseñadas. Sin mundo ni
// actor: las funciones de texto de la vista. Filtros: "Soulbound.Game.Presentation.Voz" y "…Prologo".

#include "../../Presentation/EventText.h"
#include "../../Presentation/WardenLineQueue.h"
#include "Match/MatchState.h"
#include "Misc/AutomationTest.h"
#include "Warden/WardenLines.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags VoiceTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter;

	FMatchEvent MakeVoiceEvent(EMatchEventType Type, int32 Seat, int32 Value, int32 Detail = INDEX_NONE)
	{
		FMatchEvent Event;
		Event.Type = Type;
		Event.Seat = Seat;
		Event.Value = Value;
		Event.Detail = Detail;
		return Event;
	}

	/** Una partida con un evento ("prisa") y su frase, con una acotación de audio y dos variantes. */
	FMatchState StateWithLines()
	{
		FMatchState State;
		FWardenEventDef& Event = State.WardenEvents.Events.AddDefaulted_GetRef();
		Event.Id = TEXT("prisa");
		Event.Text = TEXT("Prisa");
		FWardenLineEntry& Entry = State.WardenLines.Entries.AddDefaulted_GetRef();
		Entry.Id = TEXT("evento:prisa");
		Entry.Lines = {TEXT("(pendiente) evento:prisa"), TEXT("[risa] Prisa, prisa. [pausa]")};
		return State;
	}

	/** Añade a State la entrada Key con una variante y devuelve su posición. */
	int32 AddLine(FMatchState& State, const FString& Key)
	{
		FWardenLineEntry& Entry = State.WardenLines.Entries.AddDefaulted_GetRef();
		Entry.Id = Key;
		Entry.Lines = {TEXT("(pendiente) ") + Key};
		return State.WardenLines.Entries.Num() - 1;
	}

	/** Añade a State un WardenSpoke de la clave Key (que se crea si no está) en el instante Clock. */
	void Say(FMatchState& State, const FString& Key, double Clock)
	{
		int32 Entry = State.WardenLines.IndexOf(Key);
		if (Entry == INDEX_NONE)
		{
			Entry = AddLine(State, Key);
		}
		FMatchEvent Spoke = MakeVoiceEvent(EMatchEventType::WardenSpoke, INDEX_NONE, Entry, 0);
		Spoke.ClockSeconds = Clock;
		State.EventLog.Add(Spoke);
	}

	/** Las líneas de la crónica de la partida con texto, en el orden del registro. */
	TArray<FString> ChronicleTexts(const FMatchState& State)
	{
		TArray<FString> Texts;
		for (int32 Index = 0; Index < State.EventLog.Num(); ++Index)
		{
			const FString Text = Soulbound::Presentation::DescribeChronicleEvent(State, Index);
			if (!Text.IsEmpty())
			{
				Texts.Add(Text);
			}
		}
		return Texts;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoiceSpokenTextTest, "Soulbound.Game.Presentation.Voz.TextoDeLaFrase", VoiceTestFlags)

bool FVoiceSpokenTextTest::RunTest(const FString& Parameters)
{
	// Texto (S-021): DescribeEvent de WardenSpoke devuelve la frase de la tabla sin corchetes, tal cual (sin prefijo); los textos
	// provisionales de hoy desaparecen.
	const FMatchState State = StateWithLines();
	const FBoardState Board;
	const FMatchEvent Provisional = MakeVoiceEvent(EMatchEventType::WardenSpoke, INDEX_NONE, 0, 0);
	const FMatchEvent WithCues = MakeVoiceEvent(EMatchEventType::WardenSpoke, INDEX_NONE, 0, 1);
	TestEqual(TEXT("La frase provisional, tal cual"), Soulbound::Presentation::DescribeEvent(Provisional, Board, &State.WardenEvents, &State.WardenLines),
		FString(TEXT("(pendiente) evento:prisa")));
	TestEqual(TEXT("Sin corchetes ni los espacios que dejan"), Soulbound::Presentation::DescribeEvent(WithCues, Board, &State.WardenEvents, &State.WardenLines),
		FString(TEXT("Prisa, prisa.")));
	TestEqual(TEXT("SpokenText es lo mismo"), Soulbound::Presentation::SpokenText(State.WardenLines, WithCues), FString(TEXT("Prisa, prisa.")));
	TestTrue(TEXT("Una entrada que no existe no tiene texto"), Soulbound::Presentation::SpokenText(State.WardenLines, MakeVoiceEvent(EMatchEventType::WardenSpoke, INDEX_NONE, 7, 0)).IsEmpty());
	TestTrue(TEXT("Ni una variante que no existe"), Soulbound::Presentation::SpokenText(State.WardenLines, MakeVoiceEvent(EMatchEventType::WardenSpoke, INDEX_NONE, 0, 5)).IsEmpty());

	const FString Remark = Soulbound::Presentation::DescribeEvent(MakeVoiceEvent(EMatchEventType::WardenRemark, INDEX_NONE, INDEX_NONE), Board, &State.WardenEvents, &State.WardenLines);
	TestFalse(TEXT("El comentario ya no dice que la frase llega con otra story"), Remark.Contains(TEXT("story")) || Remark.Contains(TEXT("llega")));
	// A2: el comentario dice su tipo y de qué asiento habla (el Warden no dice nombres: lo dice la interfaz).
	const FString HurryRemark = Soulbound::Presentation::DescribeEvent(MakeVoiceEvent(EMatchEventType::WardenRemark, 2, static_cast<int32>(EWardenRemarkKind::Hurry)), Board, &State.WardenEvents, &State.WardenLines);
	TestEqual(TEXT("El comentario dice su tipo y el asiento"), HurryRemark, FString(TEXT("el Warden comenta (prisa) sobre el asiento 2")));
	TestEqual(TEXT("Un comentario sin asiento"), Soulbound::Presentation::DescribeEvent(MakeVoiceEvent(EMatchEventType::WardenRemark, INDEX_NONE, static_cast<int32>(EWardenRemarkKind::LostBet)), Board),
		FString(TEXT("el Warden comenta (apuesta fallida)")));
	TestEqual(TEXT("El aviso del final, en la crónica"), Soulbound::Presentation::DescribeEvent(MakeVoiceEvent(EMatchEventType::MatchEndWarning, INDEX_NONE, 10), Board),
		FString(TEXT("el Warden avisa: quedan 10 s para el final de la partida")));
	TestEqual(TEXT("La frase y el aviso son del Warden (su color)"), static_cast<int32>(Soulbound::Presentation::EventSource(EMatchEventType::WardenSpoke)),
		static_cast<int32>(Soulbound::Presentation::EEventSource::Warden));
	TestEqual(TEXT("El aviso también"), static_cast<int32>(Soulbound::Presentation::EventSource(EMatchEventType::MatchEndWarning)),
		static_cast<int32>(Soulbound::Presentation::EEventSource::Warden));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoiceLinePrefixTest, "Soulbound.Game.Presentation.Voz.AQuienHabla", VoiceTestFlags)

bool FVoiceLinePrefixTest::RunTest(const FString& Parameters)
{
	// R-VOZ-3b (S-021): el Warden no pronuncia nombres; a quién habla lo dice la interfaz con el prefijo de la línea: "Warden:" a la mesa,
	// "Warden, al asiento 2:" a un asiento y "Warden, a ti:" al asiento humano de esta máquina.
	TestEqual(TEXT("A la mesa"), Soulbound::Presentation::WardenLinePrefix(INDEX_NONE, 0), FString(TEXT("Warden:")));
	TestEqual(TEXT("A un asiento"), Soulbound::Presentation::WardenLinePrefix(2, 0), FString(TEXT("Warden, al asiento 2:")));
	TestEqual(TEXT("Al humano de esta máquina"), Soulbound::Presentation::WardenLinePrefix(0, 0), FString(TEXT("Warden, a ti:")));
	TestEqual(TEXT("Sin humano, a un asiento"), Soulbound::Presentation::WardenLinePrefix(0, INDEX_NONE), FString(TEXT("Warden, al asiento 0:")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoiceOracleOneLineTest, "Soulbound.Game.Presentation.Voz.SentenciaEnUnaLinea", VoiceTestFlags)

bool FVoiceOracleOneLineTest::RunTest(const FString& Parameters)
{
	// Texto (S-021): la sentencia del Oráculo es una sola línea de la crónica, "sentencia del Oráculo: <frase>", y no tres (la sentencia, la
	// frase y el evento lanzado). Desde S-022 (CA-CRO-1), un golpe por Warden directo también es una sola línea, la del evento con la frase.
	FMatchState Oracle = StateWithLines();
	Oracle.EventLog = {
		MakeVoiceEvent(EMatchEventType::OracleSentence, 2, 0, 0),
		MakeVoiceEvent(EMatchEventType::WardenTriggered, 2, 0),
		MakeVoiceEvent(EMatchEventType::WardenSpoke, INDEX_NONE, 0, 1),
		MakeVoiceEvent(EMatchEventType::WardenEventStarted, 2, 0, 1),
	};
	const TArray<FString> OracleTexts = ChronicleTexts(Oracle);
	if (TestEqual(TEXT("Una sola línea para la sentencia"), OracleTexts.Num(), 1))
	{
		TestEqual(TEXT("Con la frase"), OracleTexts[0], FString(TEXT("sentencia del Oráculo: Prisa, prisa.")));
	}

	FMatchState Direct = StateWithLines();
	Direct.EventLog = {MakeVoiceEvent(EMatchEventType::WardenEventStarted, 2, 0, 1), MakeVoiceEvent(EMatchEventType::WardenSpoke, INDEX_NONE, 0, 1)};
	const TArray<FString> DirectTexts = ChronicleTexts(Direct);
	if (TestEqual(TEXT("Directo: una sola línea, la del evento con la frase"), DirectTexts.Num(), 1))
	{
		TestEqual(TEXT("El evento de prueba, por su identificador"), DirectTexts[0], FString(TEXT("evento \"prisa\": Prisa, prisa.")));
	}

	// Sin frases en la partida (la tabla vacía), la sentencia conserva sus dos líneas de siempre.
	FMatchState Silent = StateWithLines();
	Silent.WardenLines = FWardenLineTable();
	Silent.EventLog = {MakeVoiceEvent(EMatchEventType::OracleSentence, 2, 0, 0), MakeVoiceEvent(EMatchEventType::WardenEventStarted, 2, 0, 1)};
	TestEqual(TEXT("Sin frases, las dos líneas de siempre"), ChronicleTexts(Silent).Num(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoiceLineQueueTest, "Soulbound.Game.Presentation.Voz.ColaDeLaLinea", VoiceTestFlags)

bool FVoiceLineQueueTest::RunTest(const FString& Parameters)
{
	// R-VOZ-1 sin audio (S-021): cada frase se enseña WardenLineSeconds (6 s) o hasta la siguiente línea. Las que se dicen en el mismo
	// instante de la partida (la apertura: inicio, corrupción, orden) se enseñan una detrás de otra; una dicha más tarde sustituye a las
	// que quedaban.
	constexpr double Seconds = 6.0;
	Soulbound::Presentation::FWardenLineQueue Queue;
	TestTrue(TEXT("Sin nada, no hay línea"), Queue.Current(0.0, Seconds).IsEmpty());

	Queue.Push(TEXT("uno"), 0.0);
	Queue.Push(TEXT("dos"), 0.0);
	Queue.Push(TEXT("tres"), 0.0);
	TestEqual(TEXT("Primero, la primera"), Queue.Current(10.0, Seconds), FString(TEXT("uno")));
	TestEqual(TEXT("Casi a los 6 s, la misma"), Queue.Current(15.9, Seconds), FString(TEXT("uno")));
	TestEqual(TEXT("A los 6 s, la siguiente"), Queue.Current(16.0, Seconds), FString(TEXT("dos")));
	TestEqual(TEXT("A los 12 s, la tercera"), Queue.Current(22.0, Seconds), FString(TEXT("tres")));
	TestTrue(TEXT("A los 18 s, ninguna"), Queue.Current(28.0, Seconds).IsEmpty());
	TestTrue(TEXT("Y sigue sin haberla"), Queue.Current(40.0, Seconds).IsEmpty());

	// Una dicha más tarde sustituye a las que quedaban.
	Queue.Push(TEXT("a"), 100.0);
	Queue.Push(TEXT("b"), 100.0);
	TestEqual(TEXT("La primera de la apertura nueva"), Queue.Current(50.0, Seconds), FString(TEXT("a")));
	Queue.Push(TEXT("c"), 130.0);
	TestEqual(TEXT("La nueva sustituye a la que quedaba"), Queue.Current(52.0, Seconds), FString(TEXT("c")));
	TestEqual(TEXT("Y se enseña sus 6 s enteros desde que se pide"), Queue.Current(57.9, Seconds), FString(TEXT("c")));
	TestTrue(TEXT("Luego, ninguna: a y b se descartaron"), Queue.Current(58.0, Seconds).IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoiceOpeningSevenTest, "Soulbound.Game.Presentation.Prologo.SieteFrasesEnCero", VoiceTestFlags)

bool FVoiceOpeningSevenTest::RunTest(const FString& Parameters)
{
	// CA-PRL-1, R-PRO-1 (S-022, decisión del autor del 29/10/2026): el prólogo dura lo que tarda en enseñarse la apertura, el número de
	// WardenSpoke del instante cero por los segundos de línea. Siete frases (inicio, dos corrupciones, cuatro del orden) a 6 s: 42 s.
	FMatchState State = StateWithLines();
	for (int32 Line = 0; Line < 7; ++Line)
	{
		State.EventLog.Add(MakeVoiceEvent(EMatchEventType::WardenSpoke, INDEX_NONE, 0, 0));
	}
	TestEqual(TEXT("Siete frases a 6 s"), Soulbound::Presentation::OpeningSeconds(State, 6.0), 42.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoiceOpeningOnlyZeroTest, "Soulbound.Game.Presentation.Prologo.SoloCuentaElCero", VoiceTestFlags)

bool FVoiceOpeningOnlyZeroTest::RunTest(const FString& Parameters)
{
	// CA-PRL-1, R-PRO-1: solo cuentan las frases del instante cero; las de más tarde y los demás eventos del instante cero, no.
	FMatchState State = StateWithLines();
	State.EventLog.Add(MakeVoiceEvent(EMatchEventType::WardenSpoke, INDEX_NONE, 0, 0));
	State.EventLog.Add(MakeVoiceEvent(EMatchEventType::WardenSpoke, 1, 0, 1));
	State.EventLog.Add(MakeVoiceEvent(EMatchEventType::HomeAssigned, 1, 0));
	FMatchEvent Later = MakeVoiceEvent(EMatchEventType::WardenSpoke, INDEX_NONE, 0, 0);
	Later.ClockSeconds = 12.5;
	State.EventLog.Add(Later);
	TestEqual(TEXT("Dos frases en el cero, a 6 s"), Soulbound::Presentation::OpeningSeconds(State, 6.0), 12.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoiceOpeningSilentTest, "Soulbound.Game.Presentation.Prologo.SinFrases", VoiceTestFlags)

bool FVoiceOpeningSilentTest::RunTest(const FString& Parameters)
{
	// CA-PRL-1, R-PRO-1: con la tabla de frases vacía el motor no deja ningún WardenSpoke: cero segundos y ningún prólogo, así que los tests
	// de reglas y la simulación no cambian.
	FMatchState State;
	State.EventLog.Add(MakeVoiceEvent(EMatchEventType::HomeAssigned, 0, 0));
	TestEqual(TEXT("Sin frases, cero"), Soulbound::Presentation::OpeningSeconds(State, 6.0), 0.0);
	TestEqual(TEXT("Con el registro vacío, cero"), Soulbound::Presentation::OpeningSeconds(FMatchState(), 6.0), 0.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoiceQueueShownCountTest, "Soulbound.Game.Presentation.Voz.FrasesEnsenadas", VoiceTestFlags)

bool FVoiceQueueShownCountTest::RunTest(const FString& Parameters)
{
	// CA-DOM-1 (S-023, R-PRO-1): la cola cuenta las frases que han empezado a enseñarse, para que la vista sepa por qué frase de la
	// apertura va. Una frase cuenta la primera vez que se pide; las que se descartan sin enseñarse no cuentan.
	constexpr double Seconds = 6.0;
	Soulbound::Presentation::FWardenLineQueue Queue;
	TestEqual(TEXT("Al principio, ninguna"), Queue.ShownCount(), 0);
	Queue.Push(TEXT("uno"), 0.0);
	Queue.Push(TEXT("dos"), 0.0);
	Queue.Push(TEXT("tres"), 0.0);
	TestEqual(TEXT("Añadidas, aún ninguna enseñada"), Queue.ShownCount(), 0);
	Queue.Current(10.0, Seconds);
	TestEqual(TEXT("Pedida, la primera"), Queue.ShownCount(), 1);
	Queue.Current(12.0, Seconds);
	TestEqual(TEXT("Pedir otra vez la misma no suma"), Queue.ShownCount(), 1);
	Queue.Current(16.0, Seconds);
	TestEqual(TEXT("A los 6 s, la segunda"), Queue.ShownCount(), 2);
	Queue.Push(TEXT("más tarde"), 50.0);
	Queue.Current(17.0, Seconds);
	TestEqual(TEXT("La que sustituye a la tercera cuenta; la tercera, descartada, no"), Queue.ShownCount(), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardenWaitTest, "Soulbound.Game.Presentation.Voz.EsperaDelWarden", VoiceTestFlags)

bool FWardenWaitTest::RunTest(const FString& Parameters)
{
	// CA-ESP-2 (S-023, espera del Warden): con 6 s de línea, una presentación, un golpe, una escena, un castigo, la Furia, el fin de carrera
	// y el aspirante dan 6 cada uno; un comentario, un aviso del final, la victoria y el fin por tiempo (y nadie vinculado, y una
	// instrucción, decisión del autor del 29/10/2026) dan 0; las frases del instante cero dan 0; dos frases que detienen seguidas dan 12;
	// FromIndex deja fuera las ya contadas. Qué detiene sale de la clave: una clave nueva detiene por defecto.
	constexpr double Seconds = 6.0;
	const TArray<FString> Stopping = {Soulbound::Rules::IntroductionKey(2), TEXT("evento:prisa"), TEXT("evento:banquete"), TEXT("insolencia:leve"),
		Soulbound::Warden::LineFury, Soulbound::Warden::LineRaceEnd, Soulbound::Warden::LineRaceEndAll, Soulbound::Warden::LineAspirant, TEXT("clave:nueva")};
	for (const FString& Key : Stopping)
	{
		FMatchState State;
		Say(State, Key, 30.0);
		TestEqual(FString::Printf(TEXT("%s detiene la mesa 6 s"), *Key), Soulbound::Presentation::WardenWaitSeconds(State, 0, Seconds), 6.0);
	}
	const TArray<FString> NotStopping = {Soulbound::Rules::WardenRemarkKey(EWardenRemarkKind::Hurry), Soulbound::Rules::WardenRemarkKey(EWardenRemarkKind::MockLast),
		Soulbound::Warden::LineLastMinute, Soulbound::Warden::LineTenSeconds, Soulbound::Warden::LineVictory, Soulbound::Warden::LineTimeOut,
		Soulbound::Warden::LineNobodyBonded, Soulbound::Rules::InstructionKey(EInstructionMoment::Roll)};
	for (const FString& Key : NotStopping)
	{
		FMatchState State;
		Say(State, Key, 30.0);
		TestEqual(FString::Printf(TEXT("%s no detiene nada"), *Key), Soulbound::Presentation::WardenWaitSeconds(State, 0, Seconds), 0.0);
	}

	// Las del instante cero son del prólogo, aunque su clave detenga.
	FMatchState Opening;
	Say(Opening, Soulbound::Warden::LineStart, 0.0);
	Say(Opening, Soulbound::Warden::LineCorruption, 0.0);
	Say(Opening, Soulbound::Warden::LineBegin, 0.0);
	Say(Opening, TEXT("evento:prisa"), 0.0);
	TestEqual(TEXT("Las del instante cero, 0"), Soulbound::Presentation::WardenWaitSeconds(Opening, 0, Seconds), 0.0);

	// Dos que detienen seguidas suman; un comentario entre ellas, y los demás eventos, no.
	FMatchState Two;
	Two.EventLog.Add(MakeVoiceEvent(EMatchEventType::Bonded, 1, 2));
	Say(Two, Soulbound::Rules::IntroductionKey(2), 40.0);
	Say(Two, Soulbound::Rules::WardenRemarkKey(EWardenRemarkKind::Hurry), 40.0);
	Say(Two, Soulbound::Warden::LineRaceEnd, 40.0);
	TestEqual(TEXT("Dos que detienen, 12"), Soulbound::Presentation::WardenWaitSeconds(Two, 0, Seconds), 12.0);
	TestEqual(TEXT("Desde la tercera, solo el fin de carrera"), Soulbound::Presentation::WardenWaitSeconds(Two, 3, Seconds), 6.0);
	TestEqual(TEXT("Desde el final, nada"), Soulbound::Presentation::WardenWaitSeconds(Two, Two.EventLog.Num(), Seconds), 0.0);
	TestEqual(TEXT("Sin registro, nada"), Soulbound::Presentation::WardenWaitSeconds(FMatchState(), 0, Seconds), 0.0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
