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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInstructionEchoTest, "Soulbound.Game.Presentation.Instrucciones.Eco", InstructionTestFlags)

bool FInstructionEchoTest::RunTest(const FString& Parameters)
{
	// S-027 CA-INS-1 (R-INS-1): con un registro donde la carrera acaba y el asiento humano queda sin vínculo, `eco` para ese asiento, una sola
	// vez; si el humano se vinculó, no. Decisión del autor del 9/11/2026: si la carrera acaba con cero Elegidos (gana el Warden), tampoco.
	{
		FMatchState State = InsMakeState();
		FInstructionTracker Tracker;
		State.Seats[InsDummy].Role = ESeatRole::Elect;
		InsAdd(State, EMatchEventType::BondRaceEnded, INDEX_NONE, 1);
		const TArray<FInstructionCue> Cues = Tracker.Observe(State, InsHumans());
		TestEqual(TEXT("El humano sin vínculo al acabar la carrera: eco"), InsMoments(Cues), TArray<EInstructionMoment>{EInstructionMoment::Echo});
		if (Cues.Num() == 1)
		{
			TestEqual(TEXT("Para su asiento"), Cues[0].Seat, InsHuman);
		}
		InsAdd(State, EMatchEventType::BondRaceEnded, INDEX_NONE, 1);
		TestEqual(TEXT("Una sola vez"), Tracker.Observe(State, InsHumans()).Num(), 0);
	}
	{
		FMatchState State = InsMakeState();
		FInstructionTracker Tracker;
		State.Seats[InsHuman].Role = ESeatRole::Elect;
		InsAdd(State, EMatchEventType::BondRaceEnded, INDEX_NONE, 1);
		TestEqual(TEXT("El humano vinculado: nada"), Tracker.Observe(State, InsHumans()).Num(), 0);
	}
	{
		FMatchState State = InsMakeState();
		FInstructionTracker Tracker;
		InsAdd(State, EMatchEventType::BondRaceEnded, INDEX_NONE, 0);
		TestEqual(TEXT("Cero Elegidos (gana el Warden): nada"), Tracker.Observe(State, InsHumans()).Num(), 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInstructionStealTest, "Soulbound.Game.Presentation.Instrucciones.Robar", InstructionTestFlags)

bool FInstructionStealTest::RunTest(const FString& Parameters)
{
	// S-027 CA-INS-2 (R-INS-1): la primera elección PlayerAction ofrecida al humano que incluye robar da `robar`; la segunda, no. Una que no
	// incluye robar, o la de un dummy, no da nada.
	FMatchState State = InsMakeState();
	FInstructionTracker Tracker;
	State.PendingChoice.Kind = EChoiceKind::PlayerAction;
	State.PendingChoice.Seat = InsHuman;
	State.PendingChoice.ActionOptions = {{EPlayerAction::Expel, InsDummy}};
	TestEqual(TEXT("Una acción sin robo: nada"), Tracker.Observe(State, InsHumans()).Num(), 0);
	State.PendingChoice.Seat = InsDummy;
	State.PendingChoice.ActionOptions = {{EPlayerAction::Steal, InsHuman}};
	TestEqual(TEXT("El robo ofrecido a un dummy: nada"), Tracker.Observe(State, InsHumans()).Num(), 0);
	State.PendingChoice.Seat = InsHuman;
	State.PendingChoice.ActionOptions = {{EPlayerAction::Expel, InsDummy}, {EPlayerAction::Steal, InsDummy}};
	const TArray<FInstructionCue> Cues = Tracker.Observe(State, InsHumans());
	TestEqual(TEXT("La primera con robo ofrecida al humano: robar"), InsMoments(Cues), TArray<EInstructionMoment>{EInstructionMoment::Steal});
	if (Cues.Num() == 1)
	{
		TestEqual(TEXT("Para su asiento"), Cues[0].Seat, InsHuman);
	}
	State.PendingChoice = FPendingChoice();
	Tracker.Observe(State, InsHumans());
	State.PendingChoice.Kind = EChoiceKind::PlayerAction;
	State.PendingChoice.Seat = InsHuman;
	State.PendingChoice.ActionOptions = {{EPlayerAction::Steal, InsDummy}};
	TestEqual(TEXT("La segunda ya no"), Tracker.Observe(State, InsHumans()).Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInstructionVoidTest, "Soulbound.Game.Presentation.Instrucciones.Vacio", InstructionTestFlags)

bool FInstructionVoidTest::RunTest(const FString& Parameters)
{
	// S-027 CA-INS-3 (R-INS-1): el primer Banished sobre el humano da `vacio`; el de un dummy, no; el segundo del humano, tampoco.
	FMatchState State = InsMakeState();
	FInstructionTracker Tracker;
	InsAdd(State, EMatchEventType::Banished, InsDummy);
	TestEqual(TEXT("El destierro de un dummy: nada"), Tracker.Observe(State, InsHumans()).Num(), 0);
	InsAdd(State, EMatchEventType::Banished, InsHuman);
	const TArray<FInstructionCue> Cues = Tracker.Observe(State, InsHumans());
	TestEqual(TEXT("El primero del humano: vacio"), InsMoments(Cues), TArray<EInstructionMoment>{EInstructionMoment::Void});
	if (Cues.Num() == 1)
	{
		TestEqual(TEXT("Para su asiento"), Cues[0].Seat, InsHuman);
	}
	InsAdd(State, EMatchEventType::Banished, InsHuman, 1);
	TestEqual(TEXT("El segundo ya no"), Tracker.Observe(State, InsHumans()).Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInstructionRingTest, "Soulbound.Game.Presentation.Instrucciones.Anillo", InstructionTestFlags)

bool FInstructionRingTest::RunTest(const FString& Parameters)
{
	// S-027 CA-INS-4 (R-INS-1): la primera apertura de un anillo para el humano da `anillo`; la segunda, no. Sin humanos, nada. En una partida
	// nueva (registro más corto que lo recorrido), vuelve a darse.
	FMatchState State = InsMakeState();
	FInstructionTracker Tracker;
	TestEqual(TEXT("Sin humanos: nada"), Tracker.RingOpened({}).Num(), 0);
	const TArray<FInstructionCue> Cues = Tracker.RingOpened(InsHumans());
	TestEqual(TEXT("La primera apertura: anillo"), InsMoments(Cues), TArray<EInstructionMoment>{EInstructionMoment::Ring});
	if (Cues.Num() == 1)
	{
		TestEqual(TEXT("Para el humano"), Cues[0].Seat, InsHuman);
	}
	TestEqual(TEXT("La segunda ya no"), Tracker.RingOpened(InsHumans()).Num(), 0);
	TestEqual(TEXT("Y Observe no la repite"), Tracker.Observe(State, InsHumans()).Num(), 0);

	FInstructionTracker Fresh;
	InsAdd(State, EMatchEventType::TurnStarted, InsDummy);
	InsAdd(State, EMatchEventType::TurnStarted, InsDummy);
	Fresh.Observe(State, InsHumans());
	Fresh.RingOpened(InsHumans());
	FMatchState Next = InsMakeState();
	Fresh.Observe(Next, InsHumans());
	TestEqual(TEXT("En la partida nueva vuelve a darse"), InsMoments(Fresh.RingOpened(InsHumans())), TArray<EInstructionMoment>{EInstructionMoment::Ring});
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
		// S-027 (decisión del autor del 9/11/2026): las cuatro Ayudas nuevas no cuentan; nueve de S-021 y las cuatro nuevas siguen siendo nueve.
		for (const EInstructionMoment Moment : {EInstructionMoment::Echo, EInstructionMoment::Steal, EInstructionMoment::Void, EInstructionMoment::Ring})
		{
			Seen.Add(Soulbound::Rules::InstructionKey(Moment));
		}
		TestTrue(TEXT("Automático con nueve y las cuatro Ayudas nuevas: visible"), Soulbound::Presentation::ShowsRequestHints(EHintsMode::Automatic, Seen));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInstructionTextTest, "Soulbound.Game.Presentation.Instrucciones.Texto", InstructionTestFlags)

bool FInstructionTextTest::RunTest(const FString& Parameters)
{
	// R-VOZ-5 y la story: la instrucción se enseña como línea del Warden, `instruccion:<momento>` de la tabla, sin sorteo y sin acotaciones.
	FWardenLineTable Lines;
	FWardenLineEntry& Entry = Lines.Entries.AddDefaulted_GetRef();
	Entry.Id = Soulbound::Rules::InstructionKey(EInstructionMoment::Roll);
	Entry.Lines = {TEXT("[susurro] Tira el dado, pequeño. [pausa]"), TEXT("Otra variante")};
	TestEqual(TEXT("La primera variante, sin corchetes"), Soulbound::Presentation::InstructionText(Lines, EInstructionMoment::Roll),
		FString(TEXT("Tira el dado, pequeño.")));
	TestTrue(TEXT("Sin la entrada, vacía"), Soulbound::Presentation::InstructionText(Lines, EInstructionMoment::Duel).IsEmpty());
	TestTrue(TEXT("Con la tabla vacía, vacía"), Soulbound::Presentation::InstructionText(FWardenLineTable(), EInstructionMoment::Roll).IsEmpty());
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInstructionHintsIndexTest, "Soulbound.Game.Presentation.Instrucciones.IndiceDeAyudas", InstructionTestFlags)

bool FInstructionHintsIndexTest::RunTest(const FString& Parameters)
{
	// CA-AYU-1 (S-022): el combo de opciones de UGT trabaja con un índice; automático 0, siempre 1, nunca 2, y las dos funciones son inversas.
	const EHintsMode Modes[] = {EHintsMode::Automatic, EHintsMode::Always, EHintsMode::Never};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Modes); ++Index)
	{
		TestEqual(FString::Printf(TEXT("%s está en %d"), *Soulbound::Presentation::HintsModeText(Modes[Index]), Index),
			Soulbound::Presentation::HintsModeToIndex(Modes[Index]), Index);
		TestEqual(FString::Printf(TEXT("%d vuelve a %s"), Index, *Soulbound::Presentation::HintsModeText(Modes[Index])),
			static_cast<int32>(Soulbound::Presentation::HintsModeFromIndex(Soulbound::Presentation::HintsModeToIndex(Modes[Index]))), static_cast<int32>(Modes[Index]));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInstructionHintsIndexOutOfRangeTest, "Soulbound.Game.Presentation.Instrucciones.IndiceDeAyudasFueraDeRango", InstructionTestFlags)

bool FInstructionHintsIndexOutOfRangeTest::RunTest(const FString& Parameters)
{
	// CA-AYU-1 (S-022): un índice fuera de rango (el combo sin selección da -1) es automático, el valor inicial.
	TestEqual(TEXT("-1 es automático"), static_cast<int32>(Soulbound::Presentation::HintsModeFromIndex(INDEX_NONE)), static_cast<int32>(EHintsMode::Automatic));
	TestEqual(TEXT("3 es automático"), static_cast<int32>(Soulbound::Presentation::HintsModeFromIndex(3)), static_cast<int32>(EHintsMode::Automatic));
	return true;
}

#endif
