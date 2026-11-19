// Tests del texto de la voz del Warden en el juego (S-021, GDD 06 y 07): la frase que enseña la línea del Warden (R-VOZ-1 sin audio,
// R-VOZ-3b), la crónica con la sentencia del Oráculo en una sola línea y la cola de la línea en pantalla; desde S-022, la duración del
// prólogo (R-PRO-1); desde S-023, la espera del Warden (qué frases detienen la mesa) y las frases de la cola ya enseñadas. Sin mundo ni
// actor: las funciones de texto de la vista. Desde S-041, la duración de cada frase, la burla que no tapa y la señal de corte (CA-VOZ-5
// a CA-VOZ-10). Desde S-050, cuándo suena el golpe del corte (CA-VOZ-15 a CA-VOZ-17, R-VOZ-12). Filtros: "Soulbound.Game.Presentation.Voz" y
// "…Prologo".

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
	// "Warden (al asiento 2):" a un asiento y "Warden (a ti):" al asiento humano de esta máquina. CA-VOZ-14 (R-VOZ-11, S-043): entre paréntesis y
	// sin coma, como en un guion; sustituye a "Warden, a ti:" y "Warden, al asiento 2:".
	TestEqual(TEXT("A la mesa"), Soulbound::Presentation::WardenLinePrefix(INDEX_NONE, 0), FString(TEXT("Warden:")));
	TestEqual(TEXT("A un asiento"), Soulbound::Presentation::WardenLinePrefix(2, 0), FString(TEXT("Warden (al asiento 2):")));
	TestEqual(TEXT("Al humano de esta máquina"), Soulbound::Presentation::WardenLinePrefix(0, 0), FString(TEXT("Warden (a ti):")));
	TestEqual(TEXT("Sin humano, a un asiento"), Soulbound::Presentation::WardenLinePrefix(0, INDEX_NONE), FString(TEXT("Warden (al asiento 0):")));
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
	// duración cuenta el texto tal como se enseña, con su prefijo: "Warden: " son 8 caracteres y "Warden (al asiento 2): ", 23 (desde S-043, R-VOZ-11).
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
	SayText(Opening, Soulbound::Rules::ScheduledOrderKey(1), FString::ChrN(127, TEXT('c')), 0.0, 2);
	SayText(Opening, Soulbound::Warden::LineBegin, TEXT("Empezad."), 0.0);
	SayText(Opening, TEXT("evento:prisa"), FString::ChrN(200, TEXT('d')), 30.0);
	TestEqual(TEXT("Prólogo: 9,75 + 9,75 (127 con el prefijo de asiento) + 6; la de más tarde no cuenta"),
		Soulbound::Presentation::OpeningSeconds(Opening, 0, Min, PerChar), 25.5, 1e-9);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoiceCutTextsTest, "Soulbound.Game.Presentation.Voz.TextosDelCorte", VoiceTestFlags)

bool FVoiceCutTextsTest::RunTest(const FString& Parameters)
{
	// CA-VOZ-9 (R-VOZ-8, S-041): la línea y la línea pequeña de un corte, letra por letra. Golpe al humano con frase y texto de evento; golpe a
	// otro asiento, solo con el asiento (corrección del autor del 16/11/2026); el Oráculo como golpe, sin frase (la línea es la palabra de corte sola, que no detiene la mesa
	// pero está protegida); fin de la carrera (sin palabra de corte ni oscurecido); fin de la partida (nada); sin corte, la frase tal cual. La
	// palabra de corte sale de la tabla de frases: con otra palabra, otra línea; sin la clave, no hay palabra. Corrección de un defecto de S-041
	// (autor, 16/11/2026, en S-042): en un corte del Warden el prefijo es el del asiento cortado ("Warden (a ti):" o "Warden (al asiento N):"), no el
	// de a quién va la frase del evento, que el motor dice a la mesa ("Warden:"); con o sin palabra de corte. Las demás frases, como antes.
	using Soulbound::Presentation::FWardenLineItem;
	using Soulbound::Presentation::WardenLineItems;
	constexpr int32 Human = 0;

	// En el golpe, el castigo de insolencia dicho al pasar el turno sale tal cual y la señal va con la frase del evento, la segunda.
	const FMatchState Strike = CutState(CutKind::Warden, Human);
	const TArray<FWardenLineItem> StrikeItems = WardenLineItems(Strike, 0, Human, 6.0, 0.0);
	if (TestEqual(TEXT("Golpe al humano: el castigo y la frase del evento"), StrikeItems.Num(), 2))
	{
		TestEqual(TEXT("El castigo, tal cual"), StrikeItems[0].Text, FString(TEXT("Warden (al asiento 1): Eso no se hace.")));
		TestTrue(TEXT("Sin línea pequeña ni oscurecido"), StrikeItems[0].CutNote.IsEmpty() && !StrikeItems[0].bWardenCut);
		TestEqual(TEXT("La palabra de corte antes de la frase, tras el prefijo del cortado"), StrikeItems[1].Text, FString(TEXT("Warden (a ti): ¡Quietos! Quieto ahí.")));
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
		TestEqual(TEXT("La misma línea, al asiento cortado"), Other[1].Text, FString(TEXT("Warden (al asiento 2): ¡Quietos! Quieto ahí.")));
		// Corrección del autor del 16/11/2026 tras PIE: solo el asiento; el nombre del evento, suelto, no explicaba nada.
		TestEqual(TEXT("Solo con el asiento, sin el nombre del evento"), Other[1].CutNote, FString(TEXT("Se corta el turno del asiento 2.")));
		TestTrue(TEXT("También oscurece"), Other[1].bWardenCut);
	}

	const TArray<FWardenLineItem> Oracle = WardenLineItems(CutState(CutKind::Oracle, Human), 0, Human, 6.0, 0.0);
	if (TestEqual(TEXT("El Oráculo como golpe: una línea"), Oracle.Num(), 1))
	{
		TestEqual(TEXT("La palabra de corte sola, a quien corta"), Oracle[0].Text, FString(TEXT("Warden (a ti): ¡Quietos!")));
		TestEqual(TEXT("El Oráculo te llama"), Oracle[0].CutNote, FString(TEXT("Tu turno se corta. El Oráculo te llama.")));
		TestFalse(TEXT("No detiene la mesa: el plazo de 7 s no se mueve"), Oracle[0].bHoldsTable);
		TestTrue(TEXT("Pero está protegida de las burlas y oscurece"), Oracle[0].bShielded && Oracle[0].bWardenCut);
	}
	const TArray<FWardenLineItem> OracleOther = WardenLineItems(CutState(CutKind::Oracle, 2), 0, Human, 6.0, 0.0);
	if (TestEqual(TEXT("El Oráculo como golpe a otro: una línea"), OracleOther.Num(), 1))
	{
		TestEqual(TEXT("Al asiento"), OracleOther[0].Text, FString(TEXT("Warden (al asiento 2): ¡Quietos!")));
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
		TestEqual(TEXT("Otra palabra en la tabla, otra línea (sin acotaciones)"), Word[1].Text, FString(TEXT("Warden (a ti): ¡Alto ahí! Quieto ahí.")));
	}
	// Sin la clave: se vacía la entrada de la palabra (quitarla movería las posiciones a las que apuntan los WardenSpoke).
	FMatchState NoWord = CutState(CutKind::Warden, Human);
	NoWord.WardenLines.Entries[NoWord.WardenLines.IndexOf(Soulbound::Presentation::CutWordKey)].Id = TEXT("otra:clave");
	const TArray<FWardenLineItem> Without = WardenLineItems(NoWord, 0, Human, 6.0, 0.0);
	if (TestEqual(TEXT("Sin la clave: dos frases"), Without.Num(), 2))
	{
		TestEqual(TEXT("Sin la clave, la frase sin palabra de corte, al cortado"), Without[1].Text, FString(TEXT("Warden (a ti): Quieto ahí.")));
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
	// CA-VOZ-10 (R-VOZ-8, S-041; corrección del autor del 16/11/2026 tras PIE: un solo pulso de 0,5 era demasiado sutil): la capa da varios pulsos
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
	// Punto 4 de las correcciones del autor del 16/11/2026 (investigación; no es regla nueva): SbEndRace en el turno 0, con el prólogo en marcha y
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoiceEchoNightNoteTest, "Soulbound.Game.Presentation.Voz.LineaDeLaNoche", VoiceTestFlags)

namespace
{
	/** Una partida con los eventos que abren y cierran la Noche, sus frases, la palabra de corte y la frase del fin por el tope de rondas (S-043). */
	struct FEchoNightLog
	{
		FMatchState State;
		int32 NightEvent = INDEX_NONE;
		int32 ShadowEvent = INDEX_NONE;
		int32 MutedNightEvent = INDEX_NONE;
		int32 NightLine = INDEX_NONE;
		int32 ShadowLine = INDEX_NONE;
		int32 EndLine = INDEX_NONE;

		FEchoNightLog()
		{
			State.Seats.SetNum(4);
			NightEvent = AddEvent(TEXT("noche-de-los-ecos"), TEXT("Los Ecos caminan por cualquier Dominio"));
			ShadowEvent = AddEvent(TEXT("la-ley-de-sombras"), TEXT("Los Ecos, a casa"));
			// Como "noche" de S-014: fuera de los sacos y sin frase en la tabla.
			MutedNightEvent = AddEvent(TEXT("noche"), TEXT("Ha llegado la noche"));
			FWardenLineEntry& Word = State.WardenLines.Entries.AddDefaulted_GetRef();
			Word.Id = Soulbound::Presentation::CutWordKey;
			Word.Lines = {TEXT("¡Quietos!")};
			NightLine = AddLine(State, TEXT("evento:noche-de-los-ecos"));
			State.WardenLines.Entries[NightLine].Lines = {TEXT("Que llegue la oscuridad…")};
			ShadowLine = AddLine(State, TEXT("evento:la-ley-de-sombras"));
			State.WardenLines.Entries[ShadowLine].Lines = {TEXT("Se acabó la noche…")};
			EndLine = AddLine(State, Soulbound::Warden::LineEchoNightEnd);
			State.WardenLines.Entries[EndLine].Lines = {TEXT("Amanece.")};
		}

		int32 AddEvent(const TCHAR* Id, const TCHAR* Text)
		{
			FWardenEventDef& Event = State.WardenEvents.Events.AddDefaulted_GetRef();
			Event.Id = Id;
			Event.Text = Text;
			return State.WardenEvents.Events.Num() - 1;
		}

		/** Como lo deja el motor: el evento, su frase a la mesa (si la tiene) y después su efecto, que abre la Noche. */
		void Begin(double Clock, int32 Event, bool bSpoken)
		{
			LogAt(State, EMatchEventType::WardenEventStarted, Clock, 1, Event, 1);
			if (bSpoken)
			{
				LogAt(State, EMatchEventType::WardenSpoke, Clock, INDEX_NONE, NightLine, 0);
			}
			LogAt(State, EMatchEventType::EchoNightBegan, Clock, INDEX_NONE, 9);
		}
	};
}

bool FVoiceEchoNightNoteTest::RunTest(const FString& Parameters)
{
	// CA-VOZ-13 (R-VOZ-10, S-043): la Noche empieza y termina con su línea pequeña, letra por letra, sin palabra de corte ni oscurecido: bajo la
	// frase del evento que la abre ("Empieza…"), bajo la frase nueva al acabar por el tope de rondas y bajo la de la Ley de Sombras ("Termina…").
	// La duración cuenta los caracteres de la línea pequeña. Decisiones del autor del 17/11/2026: si el evento además corta un turno, dos líneas
	// pequeñas, primero la del corte como hoy y debajo la de la Noche; una Noche sin frase deja la línea pequeña sola, sin prefijo, sin detener la
	// mesa y sin protección frente a las burlas. CA-VOZ-12 (R-VOZ-9): la frase del fin por el tope detiene la mesa.
	using Soulbound::Presentation::FWardenLineItem;
	using Soulbound::Presentation::WardenLineItems;
	constexpr int32 Human = 0;
	const FString BeginNote = TEXT("Empieza la Noche de los Ecos: los Ecos caminan por cualquier Dominio.");
	const FString EndNote = TEXT("Termina la Noche de los Ecos: los Ecos vuelven a su hogar.");

	// Empieza: WardenEventStarted, la frase del evento y EchoNightBegan, en el mismo instante.
	FEchoNightLog Begin;
	Begin.Begin(60.0, Begin.NightEvent, true);
	const TArray<FWardenLineItem> BeginItems = WardenLineItems(Begin.State, 0, Human, 6.0, 0.0);
	if (TestEqual(TEXT("Empieza: una frase"), BeginItems.Num(), 1))
	{
		TestEqual(TEXT("La frase del evento, tal cual y sin palabra de corte"), BeginItems[0].Text, FString(TEXT("Warden: Que llegue la oscuridad…")));
		TestEqual(TEXT("Debajo, Empieza…"), BeginItems[0].CutNote, BeginNote);
		TestFalse(TEXT("Sin oscurecido"), BeginItems[0].bWardenCut);
		TestTrue(TEXT("Detiene la mesa como antes"), BeginItems[0].bHoldsTable);
	}

	// Termina por el tope de rondas: EchoNightEnded y justo después la frase nueva.
	FEchoNightLog Limit;
	LogAt(Limit.State, EMatchEventType::EchoNightEnded, 80.0, INDEX_NONE, INDEX_NONE, static_cast<int32>(EEchoNightEnd::RoundLimit));
	LogAt(Limit.State, EMatchEventType::WardenSpoke, 80.0, INDEX_NONE, Limit.EndLine, 0);
	const TArray<FWardenLineItem> LimitItems = WardenLineItems(Limit.State, 0, Human, 6.0, 0.0);
	if (TestEqual(TEXT("Tope de rondas: una frase"), LimitItems.Num(), 1))
	{
		TestEqual(TEXT("La frase nueva"), LimitItems[0].Text, FString(TEXT("Warden: Amanece.")));
		TestEqual(TEXT("Debajo, Termina…"), LimitItems[0].CutNote, EndNote);
		TestFalse(TEXT("Sin oscurecido"), LimitItems[0].bWardenCut);
		TestTrue(TEXT("CA-VOZ-12: detiene la mesa y está protegida"), LimitItems[0].bHoldsTable && LimitItems[0].bShielded);
	}
	TestTrue(TEXT("CA-VOZ-12: la clave detiene la mesa"), Soulbound::Presentation::LineHoldsTheTable(Soulbound::Warden::LineEchoNightEnd));

	// Termina por la Ley de Sombras: el evento, su frase, los Ecos a casa y EchoNightEnded.
	FEchoNightLog Shadow;
	LogAt(Shadow.State, EMatchEventType::WardenEventStarted, 90.0, 1, Shadow.ShadowEvent, 1);
	LogAt(Shadow.State, EMatchEventType::WardenSpoke, 90.0, INDEX_NONE, Shadow.ShadowLine, 0);
	LogAt(Shadow.State, EMatchEventType::SentHome, 90.0, 2);
	LogAt(Shadow.State, EMatchEventType::EchoNightEnded, 90.0, INDEX_NONE, INDEX_NONE, static_cast<int32>(EEchoNightEnd::ShadowLaw));
	const TArray<FWardenLineItem> ShadowItems = WardenLineItems(Shadow.State, 0, Human, 6.0, 0.0);
	if (TestEqual(TEXT("Ley de Sombras: una frase"), ShadowItems.Num(), 1))
	{
		TestEqual(TEXT("La frase de la Ley"), ShadowItems[0].Text, FString(TEXT("Warden: Se acabó la noche…")));
		TestEqual(TEXT("Debajo, Termina…"), ShadowItems[0].CutNote, EndNote);
		TestFalse(TEXT("Sin oscurecido"), ShadowItems[0].bWardenCut);
	}

	// La duración cuenta la línea pequeña: con 0,2 s por carácter, la frase sola daría el mínimo.
	const TArray<FWardenLineItem> Slow = WardenLineItems(Begin.State, 0, Human, 6.0, 0.2);
	if (TestEqual(TEXT("Despacio: una frase"), Slow.Num(), 1))
	{
		TestTrue(TEXT("Con su línea pequeña"), !Slow[0].CutNote.IsEmpty());
		TestEqual(TEXT("Dura lo de la frase y la línea pequeña"), Slow[0].Seconds, (Slow[0].Text.Len() + Slow[0].CutNote.Len()) * 0.2, 1e-9);
	}

	// Un golpe que corta el turno del humano y abre la Noche: la palabra de corte y el oscurecido del corte, y dos líneas pequeñas.
	FEchoNightLog Cut;
	LogAt(Cut.State, EMatchEventType::TurnInterrupted, 100.0, Human, static_cast<int32>(ETurnPhase::RollAndChoose));
	LogAt(Cut.State, EMatchEventType::TurnEnded, 100.0, Human);
	Cut.Begin(100.0, Cut.NightEvent, true);
	const TArray<FWardenLineItem> CutItems = WardenLineItems(Cut.State, 0, Human, 6.0, 0.0);
	if (TestEqual(TEXT("Corte y Noche: una frase"), CutItems.Num(), 1))
	{
		TestEqual(TEXT("Con la palabra de corte, como un corte"), CutItems[0].Text, FString(TEXT("Warden (a ti): ¡Quietos! Que llegue la oscuridad…")));
		TestEqual(TEXT("Primero lo del corte y debajo lo de la Noche"), CutItems[0].CutNote,
			FString(TEXT("Tu turno se corta. Los Ecos caminan por cualquier Dominio")) + TEXT("\n") + BeginNote);
		TestTrue(TEXT("Oscurece: es un corte"), CutItems[0].bWardenCut);
	}

	// Una Noche sin frase (la de S-014, fuera de los sacos): la línea pequeña sola.
	FEchoNightLog Muted;
	Muted.Begin(110.0, Muted.MutedNightEvent, false);
	const TArray<FWardenLineItem> MutedItems = WardenLineItems(Muted.State, 0, Human, 6.0, 0.0);
	if (TestEqual(TEXT("Sin frase: una línea"), MutedItems.Num(), 1))
	{
		TestTrue(TEXT("Sin texto ni prefijo"), MutedItems[0].Text.IsEmpty());
		TestEqual(TEXT("Solo la línea pequeña"), MutedItems[0].CutNote, BeginNote);
		TestFalse(TEXT("No detiene la mesa"), MutedItems[0].bHoldsTable);
		TestFalse(TEXT("Ni está protegida de las burlas"), MutedItems[0].bShielded);
		TestFalse(TEXT("Ni oscurece"), MutedItems[0].bWardenCut);
	}
	// Sin tabla de frases, el fin por el tope tampoco trae frase: la línea pequeña sola.
	FEchoNightLog Silent;
	Silent.State.WardenLines = FWardenLineTable();
	LogAt(Silent.State, EMatchEventType::EchoNightEnded, 120.0, INDEX_NONE, INDEX_NONE, static_cast<int32>(EEchoNightEnd::RoundLimit));
	const TArray<FWardenLineItem> SilentItems = WardenLineItems(Silent.State, 0, Human, 6.0, 0.0);
	if (TestEqual(TEXT("Sin tabla: una línea"), SilentItems.Num(), 1))
	{
		TestTrue(TEXT("Sin texto"), SilentItems[0].Text.IsEmpty());
		TestEqual(TEXT("Termina…"), SilentItems[0].CutNote, EndNote);
	}
	return true;
}

namespace
{
	/**
	 * Lo que hace la vista con las frases nuevas del registro en cada vuelta (ABoardViewActor::RefreshWardenLine, S-050): las lee desde Scanned, pregunta
	 * CutSoundPlays por cada una y deja Scanned al final del registro. Devuelve cuántas veces pediría el sonido.
	 */
	int32 CutSoundRequests(const FMatchState& State, int32& Scanned, int32 HumanSeat, bool bSoundLoaded = true, float Volume = 1.f, bool bCurtainDown = false)
	{
		int32 Requests = 0;
		for (const Soulbound::Presentation::FWardenLineItem& Item : Soulbound::Presentation::WardenLineItems(State, Scanned, HumanSeat, 6.0, 0.0))
		{
			if (Soulbound::Presentation::CutSoundPlays(Item, bSoundLoaded, Volume, bCurtainDown))
			{
				++Requests;
			}
		}
		Scanned = State.EventLog.Num();
		return Requests;
	}

	/** Añade a un registro de CutState(CutKind::Warden, …) otro golpe al turno de Seat en el instante Clock, como lo deja el motor. */
	void AddStrike(FMatchState& State, int32 Seat, double Clock)
	{
		const int32 EventLine = State.WardenLines.IndexOf(TEXT("evento:prueba-golpe"));
		LogAt(State, EMatchEventType::WardenDecided, Clock, INDEX_NONE, 0);
		LogAt(State, EMatchEventType::TurnInterrupted, Clock, Seat, static_cast<int32>(ETurnPhase::RollAndChoose));
		LogAt(State, EMatchEventType::TurnEnded, Clock, Seat);
		LogAt(State, EMatchEventType::WardenEventStarted, Clock, Seat, 0, 1);
		LogAt(State, EMatchEventType::WardenSpoke, Clock, INDEX_NONE, EventLine, 0);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoiceCutSoundWhenTest, "Soulbound.Game.Presentation.Voz.SonidoDelCorteCuando", VoiceTestFlags)

bool FVoiceCutSoundWhenTest::RunTest(const FString& Parameters)
{
	// CA-VOZ-15 (R-VOZ-12, cuándo; S-050): suena un corte del Warden (golpe, o el Oráculo como golpe), del humano o de otro asiento; no suenan el corte por
	// el fin de la carrera, el del fin de la partida ni ninguna otra frase (el castigo de insolencia dicho al pasar el turno, una frase sin corte, una burla).
	// Es la misma condición que arranca el oscurecido (bWardenCut, R-VOZ-8): para toda frase, con sonido, volumen y telón alzado, CutSoundPlays coincide con
	// bWardenCut. Con el telón bajado no suena (decisión del autor del 19/11/2026: tampoco se ve el oscurecido).
	using Soulbound::Presentation::CutSoundPlays;
	using Soulbound::Presentation::FWardenLineItem;
	using Soulbound::Presentation::WardenLineItems;
	constexpr int32 Human = 0;

	struct FCase
	{
		CutKind Kind;
		int32 Seat;
		const TCHAR* Name;
		int32 Expected;
	};
	const FCase Cases[] = {
		{CutKind::Warden, Human, TEXT("Golpe al humano"), 1},
		{CutKind::Warden, 2, TEXT("Golpe a otro asiento"), 1},
		{CutKind::Oracle, Human, TEXT("El Oráculo como golpe al humano"), 1},
		{CutKind::Oracle, 2, TEXT("El Oráculo como golpe a otro asiento"), 1},
		{CutKind::RaceEnd, Human, TEXT("Fin de la carrera"), 0},
		{CutKind::RaceEnd, 3, TEXT("Fin de la carrera en otro turno"), 0},
		{CutKind::MatchEnd, Human, TEXT("Fin de la partida"), 0},
		{CutKind::None, Human, TEXT("Sin corte"), 0},
	};
	for (const FCase& Case : Cases)
	{
		const TArray<FWardenLineItem> Items = WardenLineItems(CutState(Case.Kind, Case.Seat), 0, Human, 6.0, 0.0);
		int32 Sounds = 0;
		for (const FWardenLineItem& Item : Items)
		{
			const bool bPlays = CutSoundPlays(Item, true, 1.f, false);
			TestEqual(FString::Printf(TEXT("%s: la misma condición que el oscurecido"), Case.Name), bPlays, Item.bWardenCut);
			Sounds += bPlays ? 1 : 0;
		}
		TestEqual(FString::Printf(TEXT("%s: sonidos"), Case.Name), Sounds, Case.Expected);
	}

	// En el golpe, la señal va con la frase del evento: el castigo de insolencia, dicho antes en el mismo instante, no suena.
	const TArray<FWardenLineItem> Strike = WardenLineItems(CutState(CutKind::Warden, Human), 0, Human, 6.0, 0.0);
	if (TestEqual(TEXT("Golpe: el castigo y la frase del evento"), Strike.Num(), 2))
	{
		TestFalse(TEXT("El castigo de insolencia no suena"), CutSoundPlays(Strike[0], true, 1.f, false));
		TestTrue(TEXT("La frase del corte suena"), CutSoundPlays(Strike[1], true, 1.f, false));
		TestFalse(TEXT("Con el telón bajado, no suena"), CutSoundPlays(Strike[1], true, 1.f, true));
	}
	// Una burla y una frase cualquiera de la cola, sin corte.
	TestFalse(TEXT("Una burla no suena"), CutSoundPlays(RemarkItem(TEXT("Warden: Ja."), 6.0, 10.0), true, 1.f, false));
	TestFalse(TEXT("Una frase que detiene la mesa, sin corte, no suena"), CutSoundPlays(HoldingItem(TEXT("Warden: Quieto."), 6.0, 10.0), true, 1.f, false));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoiceCutSoundOnceTest, "Soulbound.Game.Presentation.Voz.SonidoDelCorteUnaVez", VoiceTestFlags)

bool FVoiceCutSoundOnceTest::RunTest(const FString& Parameters)
{
	// CA-VOZ-16 (R-VOZ-12, una vez; S-050): un corte con varios pulsos de oscurecido pide el sonido una sola vez; dos cortes seguidos, dos veces. Se repiten
	// las vueltas de la vista: la primera lectura del registro con el golpe pide el sonido; mientras duran los pulsos (CutDimOpacity por encima de cero, con
	// los valores iniciales de Config: dos pulsos de 0,08 s), las vueltas siguientes no leen frases nuevas y no piden nada. Un segundo golpe, más tarde,
	// lo pide otra vez. Leídos de una vez, dos golpes piden dos sonidos (lectura literal de la story, decisión del autor del 19/11/2026).
	using Soulbound::Presentation::CutDimOpacity;
	constexpr int32 Human = 0;
	constexpr float MaxOpacity = 0.85f;
	constexpr int32 PulseCount = 2;
	constexpr double PulseSeconds = 0.08;

	FMatchState State = CutState(CutKind::Warden, Human);
	int32 Scanned = 0;
	int32 Total = CutSoundRequests(State, Scanned, Human);
	TestEqual(TEXT("El golpe pide el sonido una vez"), Total, 1);

	constexpr double CutStart = 10.0;
	int32 PulsesSeen = 0;
	for (double Now = CutStart; Now < CutStart + PulseCount * PulseSeconds; Now += 0.01)
	{
		PulsesSeen += CutDimOpacity(Now, CutStart, MaxOpacity, PulseCount, PulseSeconds) > 0.f ? 1 : 0;
		Total += CutSoundRequests(State, Scanned, Human);
	}
	TestTrue(TEXT("El oscurecido ha pulsado"), PulsesSeen > 0);
	TestTrue(TEXT("Y ha empezado su segundo pulso"), CutDimOpacity(CutStart + PulseSeconds, CutStart, MaxOpacity, PulseCount, PulseSeconds) > 0.f);
	TestEqual(TEXT("Con varios pulsos, una sola vez"), Total, 1);

	AddStrike(State, 2, 120.0);
	Total += CutSoundRequests(State, Scanned, Human);
	TestEqual(TEXT("Un segundo corte, más tarde: dos veces"), Total, 2);
	TestEqual(TEXT("Y las vueltas siguientes no repiten"), CutSoundRequests(State, Scanned, Human), 0);

	int32 FromStart = 0;
	TestEqual(TEXT("Los dos cortes leídos de una vez: dos veces"), CutSoundRequests(State, FromStart, Human), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoiceCutSoundOptionalTest, "Soulbound.Game.Presentation.Voz.SonidoDelCorteOpcional", VoiceTestFlags)

bool FVoiceCutSoundOptionalTest::RunTest(const FString& Parameters)
{
	// CA-VOZ-17 (R-VOZ-12, opcional y volumen; S-050): sin sonido cargado, o con el volumen a 0 (o por debajo), no se pide ningún sonido y nada falla; la
	// señal de corte sigue igual (bWardenCut, que arranca el oscurecido, no cambia). Con un volumen bajo pero por encima de cero, suena.
	using Soulbound::Presentation::CutSoundPlays;
	using Soulbound::Presentation::FWardenLineItem;
	constexpr int32 Human = 0;

	const FMatchState State = CutState(CutKind::Warden, Human);
	const TArray<FWardenLineItem> Items = Soulbound::Presentation::WardenLineItems(State, 0, Human, 6.0, 0.0);
	if (!TestEqual(TEXT("Golpe: dos frases"), Items.Num(), 2))
	{
		return false;
	}
	const FWardenLineItem& Cut = Items[1];
	TestTrue(TEXT("La frase es un corte: oscurece igual"), Cut.bWardenCut);
	TestFalse(TEXT("Sin sonido cargado, no suena"), CutSoundPlays(Cut, false, 1.f, false));
	TestFalse(TEXT("Con el volumen a 0, no suena"), CutSoundPlays(Cut, true, 0.f, false));
	TestFalse(TEXT("Con un volumen negativo, tampoco"), CutSoundPlays(Cut, true, -1.f, false));
	TestTrue(TEXT("Con un volumen bajo, suena"), CutSoundPlays(Cut, true, 0.05f, false));

	int32 Scanned = 0;
	TestEqual(TEXT("Sin sonido, ninguna petición en la vuelta de la vista"), CutSoundRequests(State, Scanned, Human, false, 1.f), 0);
	Scanned = 0;
	TestEqual(TEXT("Con el volumen a 0, ninguna"), CutSoundRequests(State, Scanned, Human, true, 0.f), 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
