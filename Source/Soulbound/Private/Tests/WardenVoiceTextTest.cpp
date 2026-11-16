// Tests del texto de la voz del Warden en el juego (S-021, GDD 06 y 07): la frase que enseña la línea del Warden (R-VOZ-1 sin audio,
// R-VOZ-3b), la crónica con la sentencia del Oráculo en una sola línea y la cola de la línea en pantalla; desde S-022, la duración del
// prólogo (R-PRO-1); desde S-023, la espera del Warden (qué frases detienen la mesa) y las frases de la cola ya enseñadas. Sin mundo ni
// actor: las funciones de texto de la vista. Desde S-041, la duración de cada frase, la burla que no tapa y la señal de corte (CA-VOZ-5
// a CA-VOZ-10). Filtros: "Soulbound.Game.Presentation.Voz" y "…Prologo".

#include "../../Presentation/EventText.h"
#include "../../Presentation/WardenLineQueue.h"
#include "Board/BoardLoader.h"
#include "Elects/ElectsLoader.h"
#include "Match/DebugEdits.h"
#include "Match/MatchParams.h"
#include "Match/MatchStart.h"
#include "Match/MatchState.h"
#include "Match/WardenEvents.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Warden/WardenLines.h"
#include "Warden/WardenParams.h"

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

	/** Una frase de la cola, de un instante de partida, que ni detiene la mesa ni es un comentario (una instrucción, un aviso). */
	Soulbound::Presentation::FWardenLineItem VoiceItem(const FString& Text, double Seconds, double GameClock)
	{
		Soulbound::Presentation::FWardenLineItem Item;
		Item.Text = Text;
		Item.Seconds = Seconds;
		Item.GameClock = GameClock;
		return Item;
	}

	/** Una frase que detiene la mesa: protegida de las burlas (R-VOZ-7). */
	Soulbound::Presentation::FWardenLineItem HoldingItem(const FString& Text, double Seconds, double GameClock)
	{
		Soulbound::Presentation::FWardenLineItem Item = VoiceItem(Text, Seconds, GameClock);
		Item.bHoldsTable = true;
		Item.bShielded = true;
		return Item;
	}

	/** Un comentario (`comentario:*`). */
	Soulbound::Presentation::FWardenLineItem RemarkItem(const FString& Text, double Seconds, double GameClock)
	{
		Soulbound::Presentation::FWardenLineItem Item = VoiceItem(Text, Seconds, GameClock);
		Item.bRemark = true;
		return Item;
	}

	/** Añade a State la entrada Key con la variante Text y un WardenSpoke suyo en el instante Clock, dicho a Seat. */
	void SayText(FMatchState& State, const FString& Key, const FString& Text, double Clock, int32 Seat = INDEX_NONE)
	{
		FWardenLineEntry& Entry = State.WardenLines.Entries.AddDefaulted_GetRef();
		Entry.Id = Key;
		Entry.Lines = {Text};
		FMatchEvent Spoke = MakeVoiceEvent(EMatchEventType::WardenSpoke, Seat, State.WardenLines.Entries.Num() - 1, 0);
		Spoke.ClockSeconds = Clock;
		State.EventLog.Add(Spoke);
	}

	/** Qué corta el turno en CutState. */
	enum class CutKind : uint8 { None, Warden, Oracle, RaceEnd, MatchEnd };

	/** Añade al registro de State un evento en el instante Clock. */
	void LogAt(FMatchState& State, EMatchEventType Type, double Clock, int32 Seat, int32 Value = INDEX_NONE, int32 Detail = INDEX_NONE)
	{
		FMatchEvent Event = MakeVoiceEvent(Type, Seat, Value, Detail);
		Event.ClockSeconds = Clock;
		State.EventLog.Add(Event);
	}

	/**
	 * Un registro con un corte del turno de Seat a los 50 s como lo deja el motor (S-041, R-VOZ-8): un golpe (TurnInterrupted con Detail
	 * INDEX_NONE, TurnEnded, un castigo de insolencia dicho al pasar el turno, WardenEventStarted y la frase del evento, dicha a la mesa), el
	 * Oráculo como golpe (TurnInterrupted, TurnEnded y OracleOpened por golpe, sin frase), el fin de la carrera (Detail 1, TurnEnded,
	 * BondRaceEnded y su frase) o el fin de la partida (Detail 2 y la frase del fin por tiempo); sin corte, solo la frase del evento. El castigo
	 * dicho entre medias comprueba que el emparejado no toma la primera frase que encuentra. La tabla trae la palabra de corte "¡Quietos!".
	 */
	FMatchState CutState(CutKind Kind, int32 Seat)
	{
		FMatchState State;
		State.Seats.SetNum(4);
		FWardenEventDef& Event = State.WardenEvents.Events.AddDefaulted_GetRef();
		Event.Id = TEXT("prueba-golpe");
		Event.Name = TEXT("El tropiezo");
		Event.Text = TEXT("En tu próximo turno, la Prueba del Warden");
		FWardenLineEntry& Word = State.WardenLines.Entries.AddDefaulted_GetRef();
		Word.Id = Soulbound::Presentation::CutWordKey;
		Word.Lines = {TEXT("¡Quietos!")};
		const int32 EventLine = AddLine(State, TEXT("evento:prueba-golpe"));
		State.WardenLines.Entries[EventLine].Lines = {TEXT("Quieto ahí.")};
		const int32 Insolence = AddLine(State, TEXT("insolencia:leve"));
		State.WardenLines.Entries[Insolence].Lines = {TEXT("Eso no se hace.")};
		const int32 RaceLine = AddLine(State, Soulbound::Warden::LineRaceEnd);
		State.WardenLines.Entries[RaceLine].Lines = {TEXT("Se acabó la carrera.")};
		const int32 TimeLine = AddLine(State, Soulbound::Warden::LineTimeOut);
		State.WardenLines.Entries[TimeLine].Lines = {TEXT("Se acabó el tiempo.")};

		constexpr double Clock = 50.0;
		const int32 Phase = static_cast<int32>(ETurnPhase::RollAndChoose);
		switch (Kind)
		{
		case CutKind::Warden:
			LogAt(State, EMatchEventType::WardenDecided, Clock, INDEX_NONE, 0);
			LogAt(State, EMatchEventType::TurnInterrupted, Clock, Seat, Phase);
			LogAt(State, EMatchEventType::TurnEnded, Clock, Seat);
			LogAt(State, EMatchEventType::WardenSpoke, Clock, 1, Insolence, 0);
			LogAt(State, EMatchEventType::WardenEventStarted, Clock, Seat, 0, 1);
			LogAt(State, EMatchEventType::WardenSpoke, Clock, INDEX_NONE, EventLine, 0);
			break;
		case CutKind::Oracle:
			LogAt(State, EMatchEventType::WardenDecided, Clock, INDEX_NONE, 0);
			LogAt(State, EMatchEventType::TurnInterrupted, Clock, Seat, Phase);
			LogAt(State, EMatchEventType::TurnEnded, Clock, Seat);
			LogAt(State, EMatchEventType::OracleOpened, Clock, Seat, static_cast<int32>(EOracleOrigin::Strike));
			break;
		case CutKind::RaceEnd:
			LogAt(State, EMatchEventType::TurnInterrupted, Clock, Seat, Phase, Soulbound::Match::RaceEndCut);
			LogAt(State, EMatchEventType::TurnEnded, Clock, Seat);
			LogAt(State, EMatchEventType::BondRaceEnded, Clock, INDEX_NONE, 2);
			LogAt(State, EMatchEventType::WardenSpoke, Clock, INDEX_NONE, RaceLine, 0);
			break;
		case CutKind::MatchEnd:
			LogAt(State, EMatchEventType::TurnInterrupted, Clock, Seat, Phase, Soulbound::Match::MatchEndCut);
			LogAt(State, EMatchEventType::TurnEnded, Clock, Seat);
			LogAt(State, EMatchEventType::WardenSpoke, Clock, INDEX_NONE, TimeLine, 0);
			break;
		case CutKind::None:
			LogAt(State, EMatchEventType::WardenEventStarted, Clock, Seat, 0, 1);
			LogAt(State, EMatchEventType::WardenSpoke, Clock, INDEX_NONE, EventLine, 0);
			break;
		}
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
	// R-VOZ-1 sin audio (S-021): cada frase se enseña su tiempo (6 s aquí) o hasta la siguiente línea. Las que se dicen en el mismo
	// instante de la partida (la apertura: inicio, corrupción, orden) se enseñan una detrás de otra; una dicha más tarde sustituye a las
	// que quedaban. Desde S-041 cada frase lleva su duración y Push recibe el instante real (R-VOZ-6, R-VOZ-7).
	Soulbound::Presentation::FWardenLineQueue Queue;
	TestTrue(TEXT("Sin nada, no hay línea"), Queue.Current(0.0).IsEmpty());

	Queue.Push(VoiceItem(TEXT("uno"), 6.0, 0.0), 0.0);
	Queue.Push(VoiceItem(TEXT("dos"), 6.0, 0.0), 0.0);
	Queue.Push(VoiceItem(TEXT("tres"), 6.0, 0.0), 0.0);
	TestEqual(TEXT("Primero, la primera"), Queue.Current(10.0).Text, FString(TEXT("uno")));
	TestEqual(TEXT("Casi a los 6 s, la misma"), Queue.Current(15.9).Text, FString(TEXT("uno")));
	TestEqual(TEXT("A los 6 s, la siguiente"), Queue.Current(16.0).Text, FString(TEXT("dos")));
	TestEqual(TEXT("A los 12 s, la tercera"), Queue.Current(22.0).Text, FString(TEXT("tres")));
	TestTrue(TEXT("A los 18 s, ninguna"), Queue.Current(28.0).IsEmpty());
	TestTrue(TEXT("Y sigue sin haberla"), Queue.Current(40.0).IsEmpty());

	// Una dicha más tarde sustituye a las que quedaban.
	Queue.Push(VoiceItem(TEXT("a"), 6.0, 100.0), 49.0);
	Queue.Push(VoiceItem(TEXT("b"), 6.0, 100.0), 49.0);
	TestEqual(TEXT("La primera de la apertura nueva"), Queue.Current(50.0).Text, FString(TEXT("a")));
	Queue.Push(VoiceItem(TEXT("c"), 6.0, 130.0), 51.0);
	TestEqual(TEXT("La nueva sustituye a la que quedaba"), Queue.Current(52.0).Text, FString(TEXT("c")));
	TestEqual(TEXT("Y se enseña sus 6 s enteros desde que se pide"), Queue.Current(57.9).Text, FString(TEXT("c")));
	TestTrue(TEXT("Luego, ninguna: a y b se descartaron"), Queue.Current(58.0).IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoiceLineDurationTest, "Soulbound.Game.Presentation.Voz.DuracionPorLongitud", VoiceTestFlags)

bool FVoiceLineDurationTest::RunTest(const FString& Parameters)
{
	// CA-VOZ-5 (R-VOZ-6, S-041): con 6 s de mínimo y 0,065 s por carácter, una frase de 40 caracteres dura 6 s y una de 150, 9,75 s; con el
	// tiempo por carácter a cero, todas duran el mínimo. En un corte cuentan también los caracteres de la línea pequeña (respuesta del autor
	// al plan): 100 de frase y 50 de línea pequeña duran lo de 150.
	using Soulbound::Presentation::WardenLineDuration;
	const FString Forty = FString::ChrN(40, TEXT('a'));
	const FString OneHundredFifty = FString::ChrN(150, TEXT('a'));
	TestEqual(TEXT("40 caracteres, el mínimo"), WardenLineDuration(Forty, FString(), 6.0, 0.065), 6.0, 1e-9);
	TestEqual(TEXT("150 caracteres, 9,75 s"), WardenLineDuration(OneHundredFifty, FString(), 6.0, 0.065), 9.75, 1e-9);
	TestEqual(TEXT("Sin tiempo por carácter, el mínimo"), WardenLineDuration(OneHundredFifty, FString(), 6.0, 0.0), 6.0, 1e-9);
	TestEqual(TEXT("Con un tiempo negativo, el mínimo"), WardenLineDuration(OneHundredFifty, FString(), 6.0, -1.0), 6.0, 1e-9);
	TestEqual(TEXT("En un corte, la frase y la línea pequeña: 100 y 50 son 9,75 s"),
		WardenLineDuration(FString::ChrN(100, TEXT('a')), FString::ChrN(50, TEXT('b')), 6.0, 0.065), 9.75, 1e-9);
	TestEqual(TEXT("Sin texto, el mínimo"), WardenLineDuration(FString(), FString(), 6.0, 0.065), 6.0, 1e-9);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoiceQueueDurationsTest, "Soulbound.Game.Presentation.Voz.ColaConDuraciones", VoiceTestFlags)

bool FVoiceQueueDurationsTest::RunTest(const FString& Parameters)
{
	// CA-VOZ-6 (R-VOZ-6, S-041): dos frases del mismo instante, una corta (6 s) y una larga (9,75 s), se enseñan seguidas, cada una su
	// duración; ShownCount sigue contando las que han empezado a enseñarse.
	Soulbound::Presentation::FWardenLineQueue Queue;
	Queue.Push(VoiceItem(TEXT("corta"), 6.0, 30.0), 100.0);
	Queue.Push(VoiceItem(TEXT("larga"), 9.75, 30.0), 100.0);
	TestEqual(TEXT("Primero, la corta"), Queue.Current(100.0).Text, FString(TEXT("corta")));
	TestEqual(TEXT("Una enseñada"), Queue.ShownCount(), 1);
	TestEqual(TEXT("Casi a los 6 s, la corta"), Queue.Current(105.9).Text, FString(TEXT("corta")));
	TestEqual(TEXT("A los 6 s, la larga"), Queue.Current(106.0).Text, FString(TEXT("larga")));
	TestEqual(TEXT("Dos enseñadas"), Queue.ShownCount(), 2);
	TestEqual(TEXT("La larga sigue a los 9,7 s de empezar"), Queue.Current(115.7).Text, FString(TEXT("larga")));
	TestTrue(TEXT("Y a los 9,75 s se va"), Queue.Current(115.75).IsEmpty());
	TestEqual(TEXT("Siguen siendo dos"), Queue.ShownCount(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoiceRemarkShieldTest, "Soulbound.Game.Presentation.Voz.LaBurlaNoTapa", VoiceTestFlags)

bool FVoiceRemarkShieldTest::RunTest(const FString& Parameters)
{
	// CA-VOZ-8 (R-VOZ-7, S-041): con una frase que detiene la mesa enseñándose, un comentario dicho después se descarta y la frase sigue
	// hasta cumplir su tiempo. Dicho cuando la frase ya ha terminado, el comentario se enseña. Un comentario sustituye a otro comentario. Una
	// frase que detiene la mesa sustituye a un comentario. Tampoco tapa a una que espera en la cola ni a la línea del Oráculo como golpe.
	Soulbound::Presentation::FWardenLineQueue Queue;
	Queue.Push(HoldingItem(TEXT("frase"), 6.0, 10.0), 0.0);
	TestEqual(TEXT("Se enseña la frase"), Queue.Current(0.0).Text, FString(TEXT("frase")));
	Queue.Push(RemarkItem(TEXT("burla"), 6.0, 12.0), 2.0);
	TestEqual(TEXT("La burla no la sustituye"), Queue.Current(3.0).Text, FString(TEXT("frase")));
	TestEqual(TEXT("La frase cumple su tiempo"), Queue.Current(5.9).Text, FString(TEXT("frase")));
	TestTrue(TEXT("Y la burla no sale después"), Queue.Current(6.0).IsEmpty());

	Queue.Push(HoldingItem(TEXT("otra frase"), 6.0, 20.0), 10.0);
	TestEqual(TEXT("Otra frase"), Queue.Current(10.0).Text, FString(TEXT("otra frase")));
	Queue.Push(RemarkItem(TEXT("burla tardía"), 6.0, 25.0), 16.0);
	TestEqual(TEXT("Dicha cuando la frase ya ha terminado, la burla se enseña"), Queue.Current(16.0).Text, FString(TEXT("burla tardía")));
	Queue.Push(RemarkItem(TEXT("otra burla"), 6.0, 26.0), 17.0);
	TestEqual(TEXT("Un comentario sustituye a otro"), Queue.Current(17.0).Text, FString(TEXT("otra burla")));
	Queue.Push(HoldingItem(TEXT("golpe"), 6.0, 27.0), 18.0);
	TestEqual(TEXT("Una frase que detiene sustituye a un comentario"), Queue.Current(18.0).Text, FString(TEXT("golpe")));

	// Tampoco tapa a una frase protegida que espera en la cola detrás de otra del mismo instante.
	Soulbound::Presentation::FWardenLineQueue Waiting;
	Waiting.Push(VoiceItem(TEXT("instrucción"), 6.0, 40.0), 50.0);
	Waiting.Push(HoldingItem(TEXT("detrás"), 6.0, 40.0), 50.0);
	TestEqual(TEXT("Primero la instrucción"), Waiting.Current(50.0).Text, FString(TEXT("instrucción")));
	Waiting.Push(RemarkItem(TEXT("burla"), 6.0, 41.0), 51.0);
	TestEqual(TEXT("La burla no tapa lo que espera"), Waiting.Current(51.0).Text, FString(TEXT("instrucción")));
	TestEqual(TEXT("Y la que esperaba llega"), Waiting.Current(56.0).Text, FString(TEXT("detrás")));

	// La línea del Oráculo como golpe no detiene la mesa, pero está protegida.
	Soulbound::Presentation::FWardenLineQueue Oracle;
	Soulbound::Presentation::FWardenLineItem CutOnly = VoiceItem(TEXT("¡Quietos!"), 6.0, 60.0);
	CutOnly.bShielded = true;
	Oracle.Push(CutOnly, 70.0);
	TestEqual(TEXT("La línea del corte"), Oracle.Current(70.0).Text, FString(TEXT("¡Quietos!")));
	Oracle.Push(RemarkItem(TEXT("risa"), 6.0, 61.0), 71.0);
	TestEqual(TEXT("La burla no la tapa"), Oracle.Current(71.0).Text, FString(TEXT("¡Quietos!")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoiceOpeningSevenTest, "Soulbound.Game.Presentation.Prologo.SieteFrasesEnCero", VoiceTestFlags)

bool FVoiceOpeningSevenTest::RunTest(const FString& Parameters)
{
	// CA-PRL-1, R-PRO-1 (S-022, decisión del autor del 29/10/2026): el prólogo dura lo que tarda en enseñarse la apertura. Siete frases
	// (inicio, dos corrupciones, cuatro del orden) a 6 s, sin tiempo por carácter: 42 s.
	FMatchState State = StateWithLines();
	for (int32 Line = 0; Line < 7; ++Line)
	{
		State.EventLog.Add(MakeVoiceEvent(EMatchEventType::WardenSpoke, INDEX_NONE, 0, 0));
	}
	TestEqual(TEXT("Siete frases a 6 s"), Soulbound::Presentation::OpeningSeconds(State, 0, 6.0, 0.0), 42.0);
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
	TestEqual(TEXT("Dos frases en el cero, a 6 s"), Soulbound::Presentation::OpeningSeconds(State, 0, 6.0, 0.0), 12.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoiceOpeningSilentTest, "Soulbound.Game.Presentation.Prologo.SinFrases", VoiceTestFlags)

bool FVoiceOpeningSilentTest::RunTest(const FString& Parameters)
{
	// CA-PRL-1, R-PRO-1: con la tabla de frases vacía el motor no deja ningún WardenSpoke: cero segundos y ningún prólogo, así que los tests
	// de reglas y la simulación no cambian.
	FMatchState State;
	State.EventLog.Add(MakeVoiceEvent(EMatchEventType::HomeAssigned, 0, 0));
	TestEqual(TEXT("Sin frases, cero"), Soulbound::Presentation::OpeningSeconds(State, 0, 6.0, 0.065), 0.0);
	TestEqual(TEXT("Con el registro vacío, cero"), Soulbound::Presentation::OpeningSeconds(FMatchState(), 0, 6.0, 0.065), 0.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoiceQueueShownCountTest, "Soulbound.Game.Presentation.Voz.FrasesEnsenadas", VoiceTestFlags)

bool FVoiceQueueShownCountTest::RunTest(const FString& Parameters)
{
	// CA-DOM-1 (S-023, R-PRO-1): la cola cuenta las frases que han empezado a enseñarse, para que la vista sepa por qué frase de la
	// apertura va. Una frase cuenta la primera vez que se pide; las que se descartan sin enseñarse no cuentan.
	Soulbound::Presentation::FWardenLineQueue Queue;
	TestEqual(TEXT("Al principio, ninguna"), Queue.ShownCount(), 0);
	Queue.Push(VoiceItem(TEXT("uno"), 6.0, 0.0), 0.0);
	Queue.Push(VoiceItem(TEXT("dos"), 6.0, 0.0), 0.0);
	Queue.Push(VoiceItem(TEXT("tres"), 6.0, 0.0), 0.0);
	TestEqual(TEXT("Añadidas, aún ninguna enseñada"), Queue.ShownCount(), 0);
	Queue.Current(10.0);
	TestEqual(TEXT("Pedida, la primera"), Queue.ShownCount(), 1);
	Queue.Current(12.0);
	TestEqual(TEXT("Pedir otra vez la misma no suma"), Queue.ShownCount(), 1);
	Queue.Current(16.0);
	TestEqual(TEXT("A los 6 s, la segunda"), Queue.ShownCount(), 2);
	Queue.Push(VoiceItem(TEXT("más tarde"), 6.0, 50.0), 16.5);
	Queue.Current(17.0);
	TestEqual(TEXT("La que sustituye a la tercera cuenta; la tercera, descartada, no"), Queue.ShownCount(), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardenWaitTest, "Soulbound.Game.Presentation.Voz.EsperaDelWarden", VoiceTestFlags)

bool FWardenWaitTest::RunTest(const FString& Parameters)
{
	// CA-ESP-2 (S-023, espera del Warden): con 6 s de línea y sin tiempo por carácter, una presentación, un golpe, una escena, un castigo, la
	// Furia, el fin de carrera y el aspirante dan 6 cada uno; un comentario, un aviso del final, la victoria y el fin por tiempo (y nadie
	// vinculado, y una instrucción, decisión del autor del 29/10/2026) dan 0; las frases del instante cero dan 0; dos frases que detienen
	// seguidas dan 12; FromIndex deja fuera las ya contadas. Qué detiene sale de la clave: una clave nueva detiene por defecto.
	constexpr double Seconds = 6.0;
	const TArray<FString> Stopping = {Soulbound::Rules::IntroductionKey(2), TEXT("evento:prisa"), TEXT("evento:banquete"), TEXT("insolencia:leve"),
		Soulbound::Warden::LineFury, Soulbound::Warden::LineRaceEnd, Soulbound::Warden::LineRaceEndAll, Soulbound::Warden::LineAspirant, TEXT("clave:nueva")};
	for (const FString& Key : Stopping)
	{
		FMatchState State;
		Say(State, Key, 30.0);
		TestEqual(FString::Printf(TEXT("%s detiene la mesa 6 s"), *Key), Soulbound::Presentation::WardenWaitSeconds(State, 0, 0, Seconds, 0.0), 6.0);
	}
	const TArray<FString> NotStopping = {Soulbound::Rules::WardenRemarkKey(EWardenRemarkKind::Hurry), Soulbound::Rules::WardenRemarkKey(EWardenRemarkKind::MockLast),
		Soulbound::Warden::LineLastMinute, Soulbound::Warden::LineTenSeconds, Soulbound::Warden::LineVictory, Soulbound::Warden::LineTimeOut,
		Soulbound::Warden::LineNobodyBonded, Soulbound::Rules::InstructionKey(EInstructionMoment::Roll)};
	for (const FString& Key : NotStopping)
	{
		FMatchState State;
		Say(State, Key, 30.0);
		TestEqual(FString::Printf(TEXT("%s no detiene nada"), *Key), Soulbound::Presentation::WardenWaitSeconds(State, 0, 0, Seconds, 0.0), 0.0);
	}

	// Las del instante cero son del prólogo, aunque su clave detenga.
	FMatchState Opening;
	Say(Opening, Soulbound::Warden::LineStart, 0.0);
	Say(Opening, Soulbound::Warden::LineCorruption, 0.0);
	Say(Opening, Soulbound::Warden::LineBegin, 0.0);
	Say(Opening, TEXT("evento:prisa"), 0.0);
	TestEqual(TEXT("Las del instante cero, 0"), Soulbound::Presentation::WardenWaitSeconds(Opening, 0, 0, Seconds, 0.0), 0.0);

	// Dos que detienen seguidas suman; un comentario entre ellas, y los demás eventos, no.
	FMatchState Two;
	Two.EventLog.Add(MakeVoiceEvent(EMatchEventType::Bonded, 1, 2));
	Say(Two, Soulbound::Rules::IntroductionKey(2), 40.0);
	Say(Two, Soulbound::Rules::WardenRemarkKey(EWardenRemarkKind::Hurry), 40.0);
	Say(Two, Soulbound::Warden::LineRaceEnd, 40.0);
	TestEqual(TEXT("Dos que detienen, 12"), Soulbound::Presentation::WardenWaitSeconds(Two, 0, 0, Seconds, 0.0), 12.0);
	TestEqual(TEXT("Desde la tercera, solo el fin de carrera"), Soulbound::Presentation::WardenWaitSeconds(Two, 3, 0, Seconds, 0.0), 6.0);
	TestEqual(TEXT("Desde el final, nada"), Soulbound::Presentation::WardenWaitSeconds(Two, Two.EventLog.Num(), 0, Seconds, 0.0), 0.0);
	TestEqual(TEXT("Sin registro, nada"), Soulbound::Presentation::WardenWaitSeconds(FMatchState(), 0, 0, Seconds, 0.0), 0.0);

	// S-041 (R-VOZ-8): la línea del Oráculo como golpe no detiene la mesa (el plazo de 7 s no se mueve).
	TestEqual(TEXT("El Oráculo como golpe no suma espera"), Soulbound::Presentation::WardenWaitSeconds(CutState(CutKind::Oracle, 0), 0, 0, Seconds, 0.0), 0.0);
	TestEqual(TEXT("Un golpe suma su frase (y el castigo dicho al pasar el turno)"), Soulbound::Presentation::WardenWaitSeconds(CutState(CutKind::Warden, 0), 0, 0, Seconds, 0.0), 12.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoiceWaitDurationsTest, "Soulbound.Game.Presentation.Voz.EsperaYPrologoSumanDuraciones", VoiceTestFlags)

bool FVoiceWaitDurationsTest::RunTest(const FString& Parameters)
{
	// CA-VOZ-7 (R-VOZ-6, S-041): con 6 s de mínimo y 0,065 s por carácter, la espera de la mesa por dos frases que la detienen es la suma de
	// sus dos duraciones; un comentario entre ellas no suma. El prólogo dura la suma de las duraciones de las frases del instante cero. Cada
	// duración cuenta el texto tal como se enseña, con su prefijo: "Warden: " son 8 caracteres y "Warden, al asiento 2: ", 22.
	constexpr double Min = 6.0;
	constexpr double PerChar = 0.065;
	FMatchState Wait;
	SayText(Wait, Soulbound::Rules::IntroductionKey(2), FString::ChrN(142, TEXT('a')), 40.0);
	SayText(Wait, Soulbound::Rules::WardenRemarkKey(EWardenRemarkKind::Hurry), FString::ChrN(300, TEXT('b')), 40.0);
	SayText(Wait, Soulbound::Warden::LineRaceEnd, TEXT("Corto."), 40.0);
	TestEqual(TEXT("Una de 150 con el prefijo (9,75) y una corta (6): 15,75; el comentario largo no suma"),
		Soulbound::Presentation::WardenWaitSeconds(Wait, 0, 0, Min, PerChar), 15.75, 1e-9);

	FMatchState Opening;
	SayText(Opening, Soulbound::Warden::LineStart, FString::ChrN(142, TEXT('a')), 0.0);
	SayText(Opening, Soulbound::Rules::ScheduledOrderKey(1), FString::ChrN(128, TEXT('c')), 0.0, 2);
	SayText(Opening, Soulbound::Warden::LineBegin, TEXT("Empezad."), 0.0);
	SayText(Opening, TEXT("evento:prisa"), FString::ChrN(200, TEXT('d')), 30.0);
	TestEqual(TEXT("Prólogo: 9,75 + 9,75 (128 con el prefijo de asiento) + 6; la de más tarde no cuenta"),
		Soulbound::Presentation::OpeningSeconds(Opening, 0, Min, PerChar), 25.5, 1e-9);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoiceCutTextsTest, "Soulbound.Game.Presentation.Voz.TextosDelCorte", VoiceTestFlags)

bool FVoiceCutTextsTest::RunTest(const FString& Parameters)
{
	// CA-VOZ-9 (R-VOZ-8, S-041): la línea y la línea pequeña de un corte, letra por letra. Golpe al humano con frase y texto de evento; golpe a
	// otro asiento, solo con el asiento (corrección del autor del 17/11/2026); el Oráculo como golpe, sin frase (la línea es la palabra de corte sola, que no detiene la mesa
	// pero está protegida); fin de la carrera (sin palabra de corte ni oscurecido); fin de la partida (nada); sin corte, la frase tal cual. La
	// palabra de corte sale de la tabla de frases: con otra palabra, otra línea; sin la clave, no hay palabra.
	using Soulbound::Presentation::FWardenLineItem;
	using Soulbound::Presentation::WardenLineItems;
	constexpr int32 Human = 0;

	// En el golpe, el castigo de insolencia dicho al pasar el turno sale tal cual y la señal va con la frase del evento, la segunda.
	const FMatchState Strike = CutState(CutKind::Warden, Human);
	const TArray<FWardenLineItem> StrikeItems = WardenLineItems(Strike, 0, Human, 6.0, 0.0);
	if (TestEqual(TEXT("Golpe al humano: el castigo y la frase del evento"), StrikeItems.Num(), 2))
	{
		TestEqual(TEXT("El castigo, tal cual"), StrikeItems[0].Text, FString(TEXT("Warden, al asiento 1: Eso no se hace.")));
		TestTrue(TEXT("Sin línea pequeña ni oscurecido"), StrikeItems[0].CutNote.IsEmpty() && !StrikeItems[0].bWardenCut);
		TestEqual(TEXT("La palabra de corte antes de la frase, tras el prefijo"), StrikeItems[1].Text, FString(TEXT("Warden: ¡Quietos! Quieto ahí.")));
		TestEqual(TEXT("Debajo, qué ha pasado, con el texto del evento"), StrikeItems[1].CutNote, FString(TEXT("Tu turno se corta. En tu próximo turno, la Prueba del Warden")));
		TestTrue(TEXT("Oscurece"), StrikeItems[1].bWardenCut);
		TestTrue(TEXT("Detiene la mesa y está protegida"), StrikeItems[1].bHoldsTable && StrikeItems[1].bShielded);
	}
	// La duración cuenta la frase y la línea pequeña (R-VOZ-6): con 0,2 s por carácter, la frase sola daría el mínimo.
	const TArray<FWardenLineItem> Slow = WardenLineItems(Strike, 0, Human, 6.0, 0.2);
	if (TestEqual(TEXT("Golpe al humano, despacio: dos frases"), Slow.Num(), 2))
	{
		TestEqual(TEXT("Dura lo de la frase y la línea pequeña"), Slow[1].Seconds, (Slow[1].Text.Len() + Slow[1].CutNote.Len()) * 0.2, 1e-9);
	}

	const TArray<FWardenLineItem> Other = WardenLineItems(CutState(CutKind::Warden, 2), 0, Human, 6.0, 0.0);
	if (TestEqual(TEXT("Golpe a otro asiento: dos frases"), Other.Num(), 2))
	{
		TestEqual(TEXT("La misma línea"), Other[1].Text, FString(TEXT("Warden: ¡Quietos! Quieto ahí.")));
		// Corrección del autor del 17/11/2026 tras PIE: solo el asiento; el nombre del evento, suelto, no explicaba nada.
		TestEqual(TEXT("Solo con el asiento, sin el nombre del evento"), Other[1].CutNote, FString(TEXT("Se corta el turno del asiento 2.")));
		TestTrue(TEXT("También oscurece"), Other[1].bWardenCut);
	}

	const TArray<FWardenLineItem> Oracle = WardenLineItems(CutState(CutKind::Oracle, Human), 0, Human, 6.0, 0.0);
	if (TestEqual(TEXT("El Oráculo como golpe: una línea"), Oracle.Num(), 1))
	{
		TestEqual(TEXT("La palabra de corte sola, a quien corta"), Oracle[0].Text, FString(TEXT("Warden, a ti: ¡Quietos!")));
		TestEqual(TEXT("El Oráculo te llama"), Oracle[0].CutNote, FString(TEXT("Tu turno se corta. El Oráculo te llama.")));
		TestFalse(TEXT("No detiene la mesa: el plazo de 7 s no se mueve"), Oracle[0].bHoldsTable);
		TestTrue(TEXT("Pero está protegida de las burlas y oscurece"), Oracle[0].bShielded && Oracle[0].bWardenCut);
	}
	const TArray<FWardenLineItem> OracleOther = WardenLineItems(CutState(CutKind::Oracle, 2), 0, Human, 6.0, 0.0);
	if (TestEqual(TEXT("El Oráculo como golpe a otro: una línea"), OracleOther.Num(), 1))
	{
		TestEqual(TEXT("Al asiento"), OracleOther[0].Text, FString(TEXT("Warden, al asiento 2: ¡Quietos!")));
		TestEqual(TEXT("El Oráculo le llama"), OracleOther[0].CutNote, FString(TEXT("Se corta el turno del asiento 2. El Oráculo le llama.")));
	}

	const TArray<FWardenLineItem> Race = WardenLineItems(CutState(CutKind::RaceEnd, Human), 0, Human, 6.0, 0.0);
	if (TestEqual(TEXT("Fin de la carrera: una frase"), Race.Num(), 1))
	{
		TestEqual(TEXT("Sin palabra de corte"), Race[0].Text, FString(TEXT("Warden: Se acabó la carrera.")));
		TestEqual(TEXT("La línea pequeña"), Race[0].CutNote, FString(TEXT("Tu turno se corta: la carrera ha terminado.")));
		TestFalse(TEXT("No oscurece: no es un golpe"), Race[0].bWardenCut);
	}
	const TArray<FWardenLineItem> RaceOther = WardenLineItems(CutState(CutKind::RaceEnd, 3), 0, Human, 6.0, 0.0);
	if (TestEqual(TEXT("Fin de la carrera en otro turno: una frase"), RaceOther.Num(), 1))
	{
		TestEqual(TEXT("Fin de la carrera en otro turno"), RaceOther[0].CutNote, FString(TEXT("Se corta el turno del asiento 3: la carrera ha terminado.")));
	}

	const TArray<FWardenLineItem> MatchEnd = WardenLineItems(CutState(CutKind::MatchEnd, Human), 0, Human, 6.0, 0.0);
	if (TestEqual(TEXT("Fin de la partida: una frase"), MatchEnd.Num(), 1))
	{
		TestEqual(TEXT("Tal cual"), MatchEnd[0].Text, FString(TEXT("Warden: Se acabó el tiempo.")));
		TestTrue(TEXT("Sin línea pequeña"), MatchEnd[0].CutNote.IsEmpty());
		TestFalse(TEXT("Ni oscurecido"), MatchEnd[0].bWardenCut);
	}

	const TArray<FWardenLineItem> Plain = WardenLineItems(CutState(CutKind::None, Human), 0, Human, 6.0, 0.0);
	if (TestEqual(TEXT("Sin corte: una frase"), Plain.Num(), 1))
	{
		TestEqual(TEXT("La frase tal cual"), Plain[0].Text, FString(TEXT("Warden: Quieto ahí.")));
		TestTrue(TEXT("Sin línea pequeña"), Plain[0].CutNote.IsEmpty());
		TestFalse(TEXT("Sin oscurecido"), Plain[0].bWardenCut);
	}

	// La palabra sale de la tabla, no del código.
	FMatchState OtherWord = CutState(CutKind::Warden, Human);
	OtherWord.WardenLines.Entries[OtherWord.WardenLines.IndexOf(Soulbound::Presentation::CutWordKey)].Lines = {TEXT("¡Alto ahí! [grito]")};
	const TArray<FWardenLineItem> Word = WardenLineItems(OtherWord, 0, Human, 6.0, 0.0);
	if (TestEqual(TEXT("Con otra palabra: dos frases"), Word.Num(), 2))
	{
		TestEqual(TEXT("Otra palabra en la tabla, otra línea (sin acotaciones)"), Word[1].Text, FString(TEXT("Warden: ¡Alto ahí! Quieto ahí.")));
	}
	// Sin la clave: se vacía la entrada de la palabra (quitarla movería las posiciones a las que apuntan los WardenSpoke).
	FMatchState NoWord = CutState(CutKind::Warden, Human);
	NoWord.WardenLines.Entries[NoWord.WardenLines.IndexOf(Soulbound::Presentation::CutWordKey)].Id = TEXT("otra:clave");
	const TArray<FWardenLineItem> Without = WardenLineItems(NoWord, 0, Human, 6.0, 0.0);
	if (TestEqual(TEXT("Sin la clave: dos frases"), Without.Num(), 2))
	{
		TestEqual(TEXT("Sin la clave, la frase sin palabra de corte"), Without[1].Text, FString(TEXT("Warden: Quieto ahí.")));
		TestEqual(TEXT("Y la línea pequeña igual"), Without[1].CutNote, FString(TEXT("Tu turno se corta. En tu próximo turno, la Prueba del Warden")));
	}
	FMatchState NoWordOracle = CutState(CutKind::Oracle, Human);
	NoWordOracle.WardenLines.Entries[NoWordOracle.WardenLines.IndexOf(Soulbound::Presentation::CutWordKey)].Id = TEXT("otra:clave");
	const TArray<FWardenLineItem> WithoutOracle = WardenLineItems(NoWordOracle, 0, Human, 6.0, 0.0);
	if (TestEqual(TEXT("Sin la clave, el Oráculo como golpe sigue dando una línea"), WithoutOracle.Num(), 1))
	{
		TestTrue(TEXT("Sin texto"), WithoutOracle[0].Text.IsEmpty());
		TestEqual(TEXT("Con la línea pequeña"), WithoutOracle[0].CutNote, FString(TEXT("Tu turno se corta. El Oráculo te llama.")));
	}

	// Desde FromIndex: lo ya mirado no se repite.
	TestEqual(TEXT("Desde el final, nada"), WardenLineItems(Strike, Strike.EventLog.Num(), Human, 6.0, 0.0).Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoiceCutDimTest, "Soulbound.Game.Presentation.Voz.OscurecidoDelCorte", VoiceTestFlags)

bool FVoiceCutDimTest::RunTest(const FString& Parameters)
{
	// CA-VOZ-10 (R-VOZ-8, S-041; corrección del autor del 17/11/2026 tras PIE: un solo pulso de 0,5 era demasiado sutil): la capa da varios pulsos
	// seguidos. Cada uno vale el máximo al empezar y baja en línea recta hasta cero al cumplir su duración, y el siguiente empieza ahí; después del
	// último, y antes del corte, cero. Un corte nuevo mientras dura la reinicia. Con tres pulsos de 0,25 s (números exactos en binario; los valores
	// iniciales son 0,85 y tres de un tercio de segundo).
	using Soulbound::Presentation::CutDimOpacity;
	TestEqual(TEXT("En el corte, el máximo"), CutDimOpacity(10.0, 10.0, 0.8f, 3, 0.25), 0.8f, 1e-5f);
	TestEqual(TEXT("A mitad del primer pulso, la mitad"), CutDimOpacity(10.125, 10.0, 0.8f, 3, 0.25), 0.4f, 1e-5f);
	TestEqual(TEXT("El segundo pulso vuelve al máximo"), CutDimOpacity(10.25, 10.0, 0.8f, 3, 0.25), 0.8f, 1e-5f);
	TestEqual(TEXT("Y baja igual"), CutDimOpacity(10.375, 10.0, 0.8f, 3, 0.25), 0.4f, 1e-5f);
	TestEqual(TEXT("El tercero, otra vez el máximo"), CutDimOpacity(10.5, 10.0, 0.8f, 3, 0.25), 0.8f, 1e-5f);
	TestEqual(TEXT("A mitad del tercero, la mitad"), CutDimOpacity(10.625, 10.0, 0.8f, 3, 0.25), 0.4f, 1e-5f);
	TestEqual(TEXT("Al cumplirse los tres, cero"), CutDimOpacity(10.75, 10.0, 0.8f, 3, 0.25), 0.f, 1e-5f);
	TestEqual(TEXT("Antes del corte, cero"), CutDimOpacity(9.9, 10.0, 0.8f, 3, 0.25), 0.f, 1e-5f);
	TestEqual(TEXT("Después, cero"), CutDimOpacity(12.0, 10.0, 0.8f, 3, 0.25), 0.f, 1e-5f);
	TestEqual(TEXT("Sin corte, cero"), CutDimOpacity(10.0, -1.0, 0.8f, 3, 0.25), 0.f, 1e-5f);
	TestEqual(TEXT("Sin duración de pulso, cero"), CutDimOpacity(10.0, 10.0, 0.8f, 3, 0.0), 0.f, 1e-5f);
	TestEqual(TEXT("Sin pulsos, cero"), CutDimOpacity(10.0, 10.0, 0.8f, 0, 0.25), 0.f, 1e-5f);
	TestEqual(TEXT("Con un pulso, al cumplirlo ya es cero"), CutDimOpacity(10.25, 10.0, 0.8f, 1, 0.25), 0.f, 1e-5f);
	// Un corte nuevo a los 10,6 (el anterior iba por la mitad de su tercer pulso): vuelve al máximo y cuenta tres pulsos desde ahí.
	TestEqual(TEXT("Un corte nuevo la reinicia"), CutDimOpacity(10.6, 10.6, 0.8f, 3, 0.25), 0.8f, 1e-5f);
	TestEqual(TEXT("Y pulsa desde el nuevo"), CutDimOpacity(11.1, 10.6, 0.8f, 3, 0.25), 0.8f, 1e-5f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoiceEndInPrologueTest, "Soulbound.Game.Presentation.Voz.FinalEnElPrologoNoSePierde", VoiceTestFlags)

bool FVoiceEndInPrologueTest::RunTest(const FString& Parameters)
{
	// Punto 4 de las correcciones del autor del 17/11/2026 (investigación; no es regla nueva): SbEndRace en el turno 0, con el prólogo en marcha y
	// nadie vinculado. Con los datos reales, el fin de la carrera durante el prólogo se registra en el instante cero (el reloj de partida está
	// parado hasta que acaba la apertura), así que su frase (`programado:nadie-vinculado`) no sustituye a la apertura: hace cola detrás de ella.
	// Se comprueba que la cola de S-041 no la pierde: sale cuando termina la última frase de la apertura. Test de regresión.
	constexpr int32 Human = 0;
	constexpr double Min = 6.0;
	constexpr double PerChar = 0.065;
	const FString Data = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Data"));
	const FBoardLoadResult Board = Soulbound::Rules::LoadBoardFromFile(FPaths::Combine(Data, TEXT("Boards/tablero_35.json")));
	const FMatchParamsLoadResult Params = Soulbound::Rules::LoadMatchParamsFromFile(FPaths::Combine(Data, TEXT("Match/partida.json")));
	const FElectsLoadResult Elects = Soulbound::Rules::LoadElectsFromFile(FPaths::Combine(Data, TEXT("Elects/elegidos.json")));
	const FWardenEventsLoadResult Events = Soulbound::Rules::LoadWardenEventsFromFile(FPaths::Combine(Data, TEXT("Events/eventos.json")));
	const FWardenParamsLoadResult Warden = Soulbound::Rules::LoadWardenParamsFromFile(FPaths::Combine(Data, TEXT("Warden/warden.json")));
	if (!TestTrue(TEXT("Los datos cargan"), Board.IsSuccess() && Params.IsSuccess() && Elects.IsSuccess() && Events.IsSuccess() && Warden.IsSuccess()))
	{
		return false;
	}
	const FWardenLinesLoadResult Lines = Soulbound::Rules::LoadWardenLinesFromFile(FPaths::Combine(Data, TEXT("Warden/frases.json")),
		Soulbound::Rules::RequiredWardenLineKeys(Events.Table, Elects.Table));
	if (!TestTrue(TEXT("Las frases cargan"), Lines.IsSuccess()))
	{
		return false;
	}
	FRandomStream Random(17);
	FMatchStartResult Start = Soulbound::Rules::StartMatch(Board.Board, Params.Params, Elects.Table, Soulbound::Match::MaxPlayers, Random, Events.Table, Warden.Params,
		Lines.Table);
	if (!TestTrue(TEXT("La partida empieza"), Start.IsSuccess()))
	{
		return false;
	}
	FMatchState State = MoveTemp(Start.State);
	const TArray<Soulbound::Presentation::FWardenLineItem> Opening = Soulbound::Presentation::WardenLineItems(State, 0, Human, Min, PerChar);
	const double OpeningSeconds = Soulbound::Presentation::OpeningSeconds(State, Human, Min, PerChar);
	const int32 Scanned = State.EventLog.Num();

	// SbEndRace durante el prólogo: el anfitrión pone el reloj de la partida, que sigue en cero.
	TestEqual(TEXT("El fin de la carrera se aplica"), static_cast<int32>(Soulbound::Debug::EndRace(State, 0.0, Random)), static_cast<int32>(EDebugEditResult::Applied));
	TestTrue(TEXT("Gana el Warden: nadie se vinculó"), State.Outcome == EMatchOutcome::WardenVictory && State.EndReason == EMatchEndReason::NobodyBonded);
	const TArray<Soulbound::Presentation::FWardenLineItem> End = Soulbound::Presentation::WardenLineItems(State, Scanned, Human, Min, PerChar);
	if (!TestEqual(TEXT("El final dice una frase"), End.Num(), 1))
	{
		return false;
	}
	TestEqual(TEXT("En el instante cero, como la apertura"), End[0].GameClock, 0.0);

	// La vista: la apertura entra al subir el telón (a los 100 s reales) y el final llega un segundo después.
	Soulbound::Presentation::FWardenLineQueue Queue;
	for (const Soulbound::Presentation::FWardenLineItem& Item : Opening)
	{
		Queue.Push(Item, 100.0);
	}
	TestEqual(TEXT("Se enseña la apertura"), Queue.Current(100.0).Text, Opening[0].Text);
	Queue.Push(End[0], 101.0);
	TestEqual(TEXT("El final no corta la apertura"), Queue.Current(101.0).Text, Opening[0].Text);
	TestNotEqual(TEXT("Justo antes de acabar la apertura, aún no sale"), Queue.Current(100.0 + OpeningSeconds - 0.01).Text, End[0].Text);
	TestEqual(TEXT("Al acabar la apertura, sale la frase del final"), Queue.Current(100.0 + OpeningSeconds + 0.01).Text, End[0].Text);
	AddInfo(FString::Printf(TEXT("Apertura: %d frases, %.1f s; la frase del final sale tras ella."), Opening.Num(), OpeningSeconds));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
