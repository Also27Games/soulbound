// Tests del texto de la voz del Warden en el juego (S-021, GDD 06 y 07): la frase que enseña la línea del Warden (R-VOZ-1 sin audio,
// R-VOZ-3b), la crónica con la sentencia del Oráculo en una sola línea y la cola de la línea en pantalla. Sin mundo ni actor: las
// funciones de texto de la vista. Filtro: "Soulbound.Game.Presentation.Voz".

#include "../../Presentation/EventText.h"
#include "../../Presentation/WardenLineQueue.h"
#include "Match/MatchState.h"
#include "Misc/AutomationTest.h"

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
	// frase y el evento lanzado). Un golpe por Warden directo conserva su línea del evento, y la frase aparte.
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
	if (TestEqual(TEXT("Directo: la línea del evento y la frase"), DirectTexts.Num(), 2))
	{
		TestTrue(TEXT("La primera, la del evento"), DirectTexts[0].Contains(TEXT("DECRETA")));
		TestEqual(TEXT("La segunda, la frase sin prefijo"), DirectTexts[1], FString(TEXT("Prisa, prisa.")));
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

#endif // WITH_DEV_AUTOMATION_TESTS
