// Tests de las instrucciones del Warden (S-021, A3; decisión del autor del 22/10/2026: el Warden enseña a jugar): qué momento se da y para
// quién (CA-INS-1) y cuándo se enseña según el ajuste de ayudas y el perfil (CA-INS-2). Funciones puras sobre un FMatchState armado a mano:
// sin mundo, sin actor y sin disco. Filtro: "Soulbound.Game.Presentation.Instrucciones".

#include "../../Presentation/InstructionTracker.h"
#include "Match/MatchState.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags InstructionTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter;

	using Soulbound::Presentation::FInstructionCue;
	using Soulbound::Presentation::FInstructionTracker;

	constexpr int32 InsHuman = 0;
	constexpr int32 InsDummy = 1;
	constexpr int32 InsSecondHuman = 2;
	constexpr int32 InsSourceTile = 1;
	constexpr int32 InsNicheTile = 2;

	/** Una partida de tres asientos sobre un tablero mínimo: casilla 0 normal, 1 Fuente del Dominio 0 y 2 Nicho libre del Dominio 1. */
	FMatchState InsMakeState()
	{
		FMatchState State;
		State.Seats.SetNum(3);
		State.Board.Tiles.SetNum(3);
		State.Board.Tiles[0].TileId = 0;
		State.Board.Tiles[1].TileId = InsSourceTile;
		State.Board.Tiles[1].Type = ETileType::PowerSource;
		State.Board.Tiles[1].DomainId = 0;
		State.Board.Tiles[2].TileId = InsNicheTile;
		State.Board.Tiles[2].Type = ETileType::Niche;
		State.Board.Tiles[2].DomainId = 1;
		State.Board.Domains.SetNum(2);
		State.Board.Domains[0].DomainId = 0;
		State.Board.Domains[1].DomainId = 1;
		return State;
	}

	void InsAdd(FMatchState& State, EMatchEventType Type, int32 Seat, int32 Value = 0, int32 OtherSeat = INDEX_NONE)
	{
		FMatchEvent Event;
		Event.Type = Type;
		Event.Seat = Seat;
		Event.Value = Value;
		Event.OtherSeat = OtherSeat;
		State.EventLog.Add(Event);
	}

	/** Los momentos de las órdenes devueltas, para comparar. */
	TArray<EInstructionMoment> InsMoments(const TArray<FInstructionCue>& Cues)
	{
		TArray<EInstructionMoment> Moments;
		for (const FInstructionCue& Cue : Cues)
		{
			Moments.Add(Cue.Moment);
		}
		return Moments;
	}

	TArray<int32> InsHumans(bool bTwo = false)
	{
		return bTwo ? TArray<int32>{InsHuman, InsSecondHuman} : TArray<int32>{InsHuman};
	}

	TArray<FString> InsAllKeys()
	{
		TArray<FString> Keys;
		for (int32 Moment = 0; Moment <= static_cast<int32>(EInstructionMoment::Insolence); ++Moment)
		{
			Keys.Add(Soulbound::Rules::InstructionKey(static_cast<EInstructionMoment>(Moment)));
		}
		return Keys;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInstructionRollTest, "Soulbound.Game.Presentation.Instrucciones.Tirar", InstructionTestFlags)

bool FInstructionRollTest::RunTest(const FString& Parameters)
{
	// CA-INS-1 (instrucciones, una vez por partida): el primer TurnStarted de un asiento humano da `tirar` para ese asiento; el segundo turno,
	// nada; con dos humanos, el segundo no vuelve a recibir `tirar`; el turno de un dummy no da nada.
	{
		FMatchState State = InsMakeState();
		FInstructionTracker Tracker;
		InsAdd(State, EMatchEventType::TurnStarted, InsDummy);
		TestEqual(TEXT("El turno de un dummy no da nada"), Tracker.Observe(State, InsHumans()).Num(), 0);
		InsAdd(State, EMatchEventType::TurnStarted, InsHuman);
		const TArray<FInstructionCue> First = Tracker.Observe(State, InsHumans());
		TestEqual(TEXT("El primer turno del humano da una orden"), First.Num(), 1);
		if (First.Num() == 1)
		{
			TestEqual(TEXT("Es tirar"), static_cast<int32>(First[0].Moment), static_cast<int32>(EInstructionMoment::Roll));
			TestEqual(TEXT("Para su asiento"), First[0].Seat, InsHuman);
		}
		InsAdd(State, EMatchEventType::TurnStarted, InsHuman);
		TestEqual(TEXT("El segundo turno no da nada"), Tracker.Observe(State, InsHumans()).Num(), 0);
		TestEqual(TEXT("Y mirar otra vez sin eventos nuevos, tampoco"), Tracker.Observe(State, InsHumans()).Num(), 0);
	}
	{
		FMatchState State = InsMakeState();
		FInstructionTracker Tracker;
		InsAdd(State, EMatchEventType::TurnStarted, InsHuman);
		TestEqual(TEXT("Con dos humanos, el primero recibe tirar"), Tracker.Observe(State, InsHumans(true)).Num(), 1);
		InsAdd(State, EMatchEventType::TurnStarted, InsSecondHuman);
		TestEqual(TEXT("El segundo no vuelve a recibirlo: queda dado para la mesa"), Tracker.Observe(State, InsHumans(true)).Num(), 0);
	}
	{
		// Una partida nueva (registro más corto que lo recorrido) empieza de cero.
		FMatchState State = InsMakeState();
		FInstructionTracker Tracker;
		InsAdd(State, EMatchEventType::TurnStarted, InsHuman);
		InsAdd(State, EMatchEventType::TurnStarted, InsHuman);
		Tracker.Observe(State, InsHumans());
		FMatchState Next = InsMakeState();
		InsAdd(Next, EMatchEventType::TurnStarted, InsHuman);
		TestEqual(TEXT("En la partida nueva vuelve a darse tirar"), Tracker.Observe(Next, InsHumans()).Num(), 1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInstructionBoardTest, "Soulbound.Game.Presentation.Instrucciones.TableroYFuente", InstructionTestFlags)

bool FInstructionBoardTest::RunTest(const FString& Parameters)
{
	// CA-INS-1: la primera Fuente de un dummy no da nada; la primera de un humano, `fuente`; un Nicho libre al alcance da `nicho` una vez.
	// Elegir casilla (primera tirada propia) va con la tirada del humano, en el orden en que ocurren.
	{
		FMatchState State = InsMakeState();
		FInstructionTracker Tracker;
		InsAdd(State, EMatchEventType::Moved, InsDummy, InsSourceTile);
		TestEqual(TEXT("La primera Fuente de un dummy no da nada"), Tracker.Observe(State, InsHumans()).Num(), 0);
		InsAdd(State, EMatchEventType::Moved, InsHuman, 0);
		TestEqual(TEXT("Moverse a una casilla normal no da nada"), Tracker.Observe(State, InsHumans()).Num(), 0);
		InsAdd(State, EMatchEventType::Moved, InsHuman, InsSourceTile);
		const TArray<FInstructionCue> Cues = Tracker.Observe(State, InsHumans());
		TestEqual(TEXT("La primera Fuente de un humano da fuente"), Cues.Num(), 1);
		if (Cues.Num() == 1)
		{
			TestEqual(TEXT("Es fuente"), static_cast<int32>(Cues[0].Moment), static_cast<int32>(EInstructionMoment::Source));
			TestEqual(TEXT("Para su asiento"), Cues[0].Seat, InsHuman);
		}
		InsAdd(State, EMatchEventType::Moved, InsHuman, InsSourceTile);
		TestEqual(TEXT("La segunda ya no"), Tracker.Observe(State, InsHumans()).Num(), 0);
	}
	{
		FMatchState State = InsMakeState();
		FInstructionTracker Tracker;
		InsAdd(State, EMatchEventType::TurnStarted, InsHuman);
		InsAdd(State, EMatchEventType::Rolled, InsHuman, 3);
		State.ActiveSeat = InsHuman;
		State.TurnPhase = ETurnPhase::RollAndChoose;
		State.ReachableOptions = {0, InsNicheTile};
		const TArray<EInstructionMoment> Moments = InsMoments(Tracker.Observe(State, InsHumans()));
		TestEqual(TEXT("Tirar, elegir casilla y el Nicho libre al alcance, en ese orden"), Moments,
			TArray<EInstructionMoment>{EInstructionMoment::Roll, EInstructionMoment::ChooseTile, EInstructionMoment::Niche});
		TestEqual(TEXT("El Nicho se da una vez"), Tracker.Observe(State, InsHumans()).Num(), 0);
	}
	{
		// Un Nicho reclamado o cerrado no cuenta, ni uno al alcance de un dummy.
		FMatchState State = InsMakeState();
		State.Board.Domains[1].NicheState = ENicheState::Claimed;
		FInstructionTracker Tracker;
		State.ActiveSeat = InsHuman;
		State.TurnPhase = ETurnPhase::RollAndChoose;
		State.ReachableOptions = {InsNicheTile};
		TestEqual(TEXT("Un Nicho reclamado no da nada"), Tracker.Observe(State, InsHumans()).Num(), 0);
		State.Board.Domains[1].NicheState = ENicheState::Free;
		State.ActiveSeat = InsDummy;
		TestEqual(TEXT("Un Nicho al alcance de un dummy no da nada"), Tracker.Observe(State, InsHumans()).Num(), 0);
		State.ActiveSeat = InsHuman;
		TestEqual(TEXT("Y al del humano, nicho"), Tracker.Observe(State, InsHumans()).Num(), 1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInstructionTableMomentsTest, "Soulbound.Game.Presentation.Instrucciones.MesaYWarden", InstructionTestFlags)

bool FInstructionTableMomentsTest::RunTest(const FString& Parameters)
{
	// CA-INS-1: la primera vez que cualquiera junta seis tipos da `seis-runas` al humano. Los demás momentos de la tabla de la story (dilema,
	// duelo, orbe, voto, insolencia) se dan al primer humano al que le tocan, una vez, y no a un dummy.
	{
		FMatchState State = InsMakeState();
		FInstructionTracker Tracker;
		State.Seats[InsDummy].Runes.Init(1, Soulbound::Board::DomainCount - 1);
		TestEqual(TEXT("Cinco tipos no son seis"), Tracker.Observe(State, InsHumans(true)).Num(), 0);
		State.Seats[InsDummy].Runes.Init(1, Soulbound::Board::DomainCount);
		const TArray<FInstructionCue> Cues = Tracker.Observe(State, InsHumans(true));
		TestEqual(TEXT("Seis tipos en un dummy dan seis-runas"), Cues.Num(), 1);
		if (Cues.Num() == 1)
		{
			TestEqual(TEXT("Es seis-runas"), static_cast<int32>(Cues[0].Moment), static_cast<int32>(EInstructionMoment::SixRunes));
			TestEqual(TEXT("Para el primer humano"), Cues[0].Seat, InsHuman);
		}
		State.Seats[InsSecondHuman].Runes.Init(1, Soulbound::Board::DomainCount);
		TestEqual(TEXT("Una sola vez en la partida"), Tracker.Observe(State, InsHumans(true)).Num(), 0);
	}
	{
		FMatchState State = InsMakeState();
		FInstructionTracker Tracker;
		State.PendingChoice.Kind = EChoiceKind::Dilemma;
		State.PendingChoice.Seat = InsDummy;
		TestEqual(TEXT("Un dilema de un dummy no da nada"), Tracker.Observe(State, InsHumans()).Num(), 0);
		State.PendingChoice.Seat = InsHuman;
		TestEqual(TEXT("Un dilema del humano da dilema"), InsMoments(Tracker.Observe(State, InsHumans())), TArray<EInstructionMoment>{EInstructionMoment::Dilemma});
		TestEqual(TEXT("Una vez"), Tracker.Observe(State, InsHumans()).Num(), 0);
	}
	{
		FMatchState State = InsMakeState();
		FInstructionTracker Tracker;
		InsAdd(State, EMatchEventType::DuelChallenged, InsDummy, 1, InsSecondHuman);
		TestEqual(TEXT("Un duelo entre un dummy y un asiento que no es humano de esta máquina no da nada"), Tracker.Observe(State, InsHumans()).Num(), 0);
		InsAdd(State, EMatchEventType::DuelChallenged, InsDummy, 1, InsHuman);
		const TArray<FInstructionCue> Cues = Tracker.Observe(State, InsHumans());
		TestEqual(TEXT("Un duelo en que el humano es el retado da duelo"), InsMoments(Cues), TArray<EInstructionMoment>{EInstructionMoment::Duel});
		if (Cues.Num() == 1)
		{
			TestEqual(TEXT("Para el retado"), Cues[0].Seat, InsHuman);
		}
		InsAdd(State, EMatchEventType::DuelChallenged, InsHuman, 1, InsDummy);
		TestEqual(TEXT("El segundo duelo ya no"), Tracker.Observe(State, InsHumans()).Num(), 0);
	}
	{
		FMatchState State = InsMakeState();
		FInstructionTracker Tracker;
		InsAdd(State, EMatchEventType::OracleOpened, InsDummy);
		TestEqual(TEXT("La consulta de un dummy no da nada"), Tracker.Observe(State, InsHumans()).Num(), 0);
		InsAdd(State, EMatchEventType::OracleOpened, InsHuman);
		TestEqual(TEXT("La del humano da orbe"), InsMoments(Tracker.Observe(State, InsHumans())), TArray<EInstructionMoment>{EInstructionMoment::Orb});
	}
	{
		FMatchState State = InsMakeState();
		FInstructionTracker Tracker;
		InsAdd(State, EMatchEventType::VoteStarted, INDEX_NONE);
		const TArray<FInstructionCue> Cues = Tracker.Observe(State, InsHumans(true));
		TestEqual(TEXT("Una votación da voto"), InsMoments(Cues), TArray<EInstructionMoment>{EInstructionMoment::Vote});
		if (Cues.Num() == 1)
		{
			TestEqual(TEXT("Al primer humano"), Cues[0].Seat, InsHuman);
		}
		InsAdd(State, EMatchEventType::VoteStarted, INDEX_NONE);
		TestEqual(TEXT("La segunda ya no"), Tracker.Observe(State, InsHumans(true)).Num(), 0);
	}
	{
		FMatchState State = InsMakeState();
		FInstructionTracker Tracker;
		InsAdd(State, EMatchEventType::InsolencePunished, InsDummy, 1, INDEX_NONE);
		TestEqual(TEXT("La insolencia de un dummy no da nada"), Tracker.Observe(State, InsHumans()).Num(), 0);
		InsAdd(State, EMatchEventType::InsolencePunished, InsHuman, 1, INDEX_NONE);
		TestEqual(TEXT("La del humano da insolencia"), InsMoments(Tracker.Observe(State, InsHumans())), TArray<EInstructionMoment>{EInstructionMoment::Insolence});
		InsAdd(State, EMatchEventType::InsolencePunished, InsHuman, 2, INDEX_NONE);
		TestEqual(TEXT("La segunda ya no"), Tracker.Observe(State, InsHumans()).Num(), 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInstructionHintsTest, "Soulbound.Game.Presentation.Instrucciones.Ayudas", InstructionTestFlags)

bool FInstructionHintsTest::RunTest(const FString& Parameters)
{
	// CA-INS-2 (ayudas y perfil): con el perfil vacío y automático, `tirar` se enseña y queda en el perfil; con el perfil que ya lo tiene, no se
	// enseña; en siempre, se enseña aunque esté en el perfil (y se anota: decisión del autor del 28/10/2026); en nunca, no se enseña y no se
	// anota nada.
	const FString RollKey = Soulbound::Rules::InstructionKey(EInstructionMoment::Roll);
	{
		TArray<FString> Seen;
		TestTrue(TEXT("Automático y perfil vacío: se enseña"), Soulbound::Presentation::TeachAndRecord(EHintsMode::Automatic, Seen, EInstructionMoment::Roll));
		TestTrue(TEXT("Y queda en el perfil"), Seen.Contains(RollKey));
		TestFalse(TEXT("Automático y perfil que ya la tiene: no se enseña"), Soulbound::Presentation::TeachAndRecord(EHintsMode::Automatic, Seen, EInstructionMoment::Roll));
		TestEqual(TEXT("Sin duplicados en el perfil"), Seen.Num(), 1);
	}
	{
		TArray<FString> Seen = {RollKey};
		TestTrue(TEXT("Siempre: se enseña aunque esté en el perfil"), Soulbound::Presentation::TeachAndRecord(EHintsMode::Always, Seen, EInstructionMoment::Roll));
		TestEqual(TEXT("Y no se duplica"), Seen.Num(), 1);
		TestTrue(TEXT("Siempre con otra: se enseña"), Soulbound::Presentation::TeachAndRecord(EHintsMode::Always, Seen, EInstructionMoment::Duel));
		TestTrue(TEXT("Y se anota, porque se ha visto"), Seen.Contains(Soulbound::Rules::InstructionKey(EInstructionMoment::Duel)));
	}
	{
		TArray<FString> Seen;
		TestFalse(TEXT("Nunca: no se enseña"), Soulbound::Presentation::TeachAndRecord(EHintsMode::Never, Seen, EInstructionMoment::Roll));
		TestEqual(TEXT("Y no se anota nada"), Seen.Num(), 0);
	}

	// El texto de petición sigue el ajuste: siempre visible en siempre; en automático, hasta que el perfil tiene las diez; oculto en nunca.
	{
		TArray<FString> Seen;
		TestTrue(TEXT("Siempre con el perfil vacío: visible"), Soulbound::Presentation::ShowsRequestHints(EHintsMode::Always, Seen));
		TestTrue(TEXT("Automático con el perfil vacío: visible"), Soulbound::Presentation::ShowsRequestHints(EHintsMode::Automatic, Seen));
		TestFalse(TEXT("Nunca: oculto"), Soulbound::Presentation::ShowsRequestHints(EHintsMode::Never, Seen));
		Seen = InsAllKeys();
		TestEqual(TEXT("Las diez claves"), Seen.Num(), 10);
		TestFalse(TEXT("Automático con las diez instrucciones: oculto"), Soulbound::Presentation::ShowsRequestHints(EHintsMode::Automatic, Seen));
		TestTrue(TEXT("Siempre con las diez: visible"), Soulbound::Presentation::ShowsRequestHints(EHintsMode::Always, Seen));
		Seen.RemoveAt(9);
		TestTrue(TEXT("Automático con nueve: visible"), Soulbound::Presentation::ShowsRequestHints(EHintsMode::Automatic, Seen));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInstructionHintsParseTest, "Soulbound.Game.Presentation.Instrucciones.ComandoSbHints", InstructionTestFlags)

bool FInstructionHintsParseTest::RunTest(const FString& Parameters)
{
	// SbHints auto|siempre|nunca: el parseo es puro y vive en la presentación (el controlador solo lo llama).
	EHintsMode Mode = EHintsMode::Never;
	TestTrue(TEXT("auto"), Soulbound::Presentation::ParseHintsMode(TEXT("auto"), Mode));
	TestEqual(TEXT("auto es automático"), static_cast<int32>(Mode), static_cast<int32>(EHintsMode::Automatic));
	TestTrue(TEXT("siempre"), Soulbound::Presentation::ParseHintsMode(TEXT("siempre"), Mode));
	TestEqual(TEXT("siempre es siempre"), static_cast<int32>(Mode), static_cast<int32>(EHintsMode::Always));
	TestTrue(TEXT("sin distinguir mayúsculas ni espacios"), Soulbound::Presentation::ParseHintsMode(TEXT(" NUNCA "), Mode));
	TestEqual(TEXT("nunca es nunca"), static_cast<int32>(Mode), static_cast<int32>(EHintsMode::Never));
	Mode = EHintsMode::Always;
	TestFalse(TEXT("Otra cosa se rechaza"), Soulbound::Presentation::ParseHintsMode(TEXT("quizas"), Mode));
	TestFalse(TEXT("Vacío se rechaza"), Soulbound::Presentation::ParseHintsMode(TEXT(""), Mode));
	TestEqual(TEXT("Y no toca el valor"), static_cast<int32>(Mode), static_cast<int32>(EHintsMode::Always));
	TestEqual(TEXT("Nombre de automático"), Soulbound::Presentation::HintsModeText(EHintsMode::Automatic), FString(TEXT("automático")));
	TestEqual(TEXT("Nombre de siempre"), Soulbound::Presentation::HintsModeText(EHintsMode::Always), FString(TEXT("siempre")));
	TestEqual(TEXT("Nombre de nunca"), Soulbound::Presentation::HintsModeText(EHintsMode::Never), FString(TEXT("nunca")));
	return true;
}

#endif
