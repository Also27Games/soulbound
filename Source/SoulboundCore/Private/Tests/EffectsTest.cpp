// Tests del catálogo de efectos (S-014, GDD 05): el catálogo cerrado (R-EFE-0) y los trece efectos que se resuelven en el
// acto, cada uno lanzado como evento del Warden entre turnos (Match/WardenEventRules.h), con sus objetivos válidos
// (R-DIR-2, R-DIR-3): Revelar Brazal (R-EFE-2), Vuelta al hogar (R-EFE-4), Turno perdido (R-EFE-5), Turno extra (R-EFE-15),
// Sello (R-EFE-6), Sello contra rival (R-EFE-7), Marcado (R-EFE-8), Perder una runa (R-EFE-11), Runa regalada (R-EFE-16),
// Noche de los Ecos y Ley de Sombras (R-EFE-18, R-EFE-9), y los cinco de otras stories, que no hacen nada. Cada test cita el
// criterio de aceptación (CA-XXX-n) y la regla (R-XXX-n) que cubre. Filtro: "Soulbound.Core.Effects".
//
// Montaje: EventFixture.h. El asiento 0 es Fenrir, el 1 Koschei, el 2 y el 3 Ecos; acabada la carrera, sin runas, cada uno
// en su hogar y entre turnos. Las runas, las posiciones, las reservas y el Sello se fijan a mano: es lo que el test necesita.

#include "Board/SealRules.h"
#include "EventFixture.h"
#include "Match/BondRaceRules.h"
#include "Match/DuelRules.h"
#include "Match/EffectCatalog.h"
#include "Match/EffectRules.h"
#include "Match/RuneRules.h"
#include "Match/ShellGameRules.h"
#include "Match/WardenEventRules.h"
#include "Math/RandomStream.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	using Soulbound::Tests::App;
	using Soulbound::Tests::FEventFixture;
	using Soulbound::Tests::SameState;

	constexpr int32 Fenrir = FEventFixture::Fenrir;
	constexpr int32 Koschei = FEventFixture::Koschei;
	constexpr int32 EchoA = FEventFixture::EchoA;
	constexpr int32 EchoB = FEventFixture::EchoB;

	int32 AsInt(EEffectResult Result) { return static_cast<int32>(Result); }
	int32 AsInt(EWardenEventStart Result) { return static_cast<int32>(Result); }
	int32 AsInt(EIntentResult Result) { return static_cast<int32>(Result); }
	int32 AsInt(EChoiceKind Kind) { return static_cast<int32>(Kind); }
	int32 AsInt(EEffect Effect) { return static_cast<int32>(Effect); }
	int32 AsInt(EEffectTarget Target) { return static_cast<int32>(Target); }
	int32 AsInt(EEffectSeverity Severity) { return static_cast<int32>(Severity); }

	/** Pasos del grafo desde From a cada casilla (sin mirar Sellos): el oráculo de los tests de Sello contra rival. */
	TArray<int32> Distances(const FBoardState& Board, int32 From)
	{
		TArray<int32> Steps;
		Steps.Init(MAX_int32, Board.Tiles.Num());
		TArray<int32> Queue = {From};
		Steps[From] = 0;
		for (int32 Index = 0; Index < Queue.Num(); ++Index)
		{
			for (const int32 Next : Board.Tiles[Queue[Index]].Neighbours)
			{
				if (Steps[Next] == MAX_int32)
				{
					Steps[Next] = Steps[Queue[Index]] + 1;
					Queue.Add(Next);
				}
			}
		}
		return Steps;
	}

	/** Casillas que se pueden sellar ahora, según las reglas de Sello (que este test no comprueba). */
	TArray<int32> SealableNow(const FMatchState& State)
	{
		TArray<int32> Tiles;
		for (const FTileState& Tile : State.Board.Tiles)
		{
			if (Soulbound::Rules::CanSealTile(State, Tile.TileId) == ESealResult::Accepted)
			{
				Tiles.Add(Tile.TileId);
			}
		}
		return Tiles;
	}

	/** Casilla del último SealPlaced desde From, o INDEX_NONE. */
	int32 SealedSince(const FEventFixture& F, int32 From)
	{
		const TArray<FMatchEvent> Seals = F.Since(From, EMatchEventType::SealPlaced);
		return Seals.IsEmpty() ? INDEX_NONE : Seals.Last().Value;
	}
}

// CA-EFE-1, R-EFE-0: el catálogo tiene exactamente los 18 efectos de la tabla del GDD 05, con su gravedad y sus direcciones,
// y solo los trece de esta story se aplican.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEffectsCatalogoCerradoTest, "Soulbound.Core.Effects.CatalogoCerrado",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FEffectsCatalogoCerradoTest::RunTest(const FString& Parameters)
{
	using T = EEffectTarget;
	using S = EEffectSeverity;
	struct FRow
	{
		EEffect Effect;
		S Severity;
		TArray<T> Targets;
		bool bImplemented;
	};
	// La tabla de la story (R-EFE-0), escrita aquí como oráculo independiente.
	const TArray<FRow> Expected = {
		// S-016: el Alto se aplica (su efecto es el corte, R-TAB-28).
		{EEffect::Halt, S::Light, {T::ActivePlayer}, true},
		{EEffect::RevealBracer, S::Light, {T::ActivePlayer, T::ChosenRival, T::Leader, T::Everyone}, true},
		{EEffect::ForcedOracle, S::Light, {T::ActivePlayer, T::ChosenRival, T::NextPlayer}, false},
		{EEffect::SendHome, S::Medium, {T::ActivePlayer, T::ChosenRival, T::Leader, T::NextPlayer, T::Everyone, T::RandomPlayer}, true},
		{EEffect::LoseTurn, S::Medium, {T::ActivePlayer, T::ChosenRival, T::Leader, T::NextPlayer, T::RandomPlayer}, true},
		{EEffect::PlaceSeal, S::Medium, {T::None}, true},
		{EEffect::SealAgainstRival, S::Medium, {T::ChosenRival, T::Leader}, true},
		{EEffect::Mark, S::Medium, {T::ChosenRival, T::Leader, T::RandomPlayer}, true},
		{EEffect::ShadowLaw, S::Medium, {T::Echoes}, true},
		{EEffect::WardenTrial, S::Medium, {T::ActivePlayer, T::ChosenRival, T::NextPlayer}, true},
		{EEffect::LoseRune, S::Severe, {T::ActivePlayer, T::ChosenRival, T::Leader, T::Everyone, T::RandomPlayer}, true},
		{EEffect::DecreedTheft, S::Severe, {T::ChosenRival, T::Everyone, T::RandomPlayer}, true},
		{EEffect::ForcedDuel, S::Severe, {T::ChosenRival, T::TwoChosenRivals, T::RandomPlayer}, true},
		{EEffect::Banish, S::Severe, {T::ActivePlayer, T::ChosenRival, T::Leader, T::RandomPlayer}, true},
		{EEffect::ExtraTurn, S::Gift, {T::ActivePlayer, T::NextPlayer}, true},
		{EEffect::GiftRune, S::Gift, {T::ActivePlayer, T::NextPlayer, T::RandomPlayer}, true},
		{EEffect::ReleaseVoid, S::Gift, {T::Everyone}, true},
		{EEffect::EchoNight, S::Gift, {T::Echoes}, true},
	};

	const TArray<FEffectDef>& Catalog = Soulbound::Rules::EffectCatalog();
	if (!TestEqual(TEXT("El catálogo tiene exactamente 18 efectos"), Catalog.Num(), Soulbound::Effects::EffectCount)
		|| !TestEqual(TEXT("La tabla del test también"), Expected.Num(), Soulbound::Effects::EffectCount))
	{
		return false;
	}
	int32 Implemented = 0;
	for (int32 Index = 0; Index < Expected.Num(); ++Index)
	{
		const FRow& Row = Expected[Index];
		const FEffectDef& Def = Catalog[Index];
		const FString Name = Soulbound::Rules::EffectCode(Row.Effect);
		TestEqual(FString::Printf(TEXT("La entrada %d es %s"), Index, *Name), AsInt(Def.Effect), AsInt(Row.Effect));
		TestEqual(FString::Printf(TEXT("%s: gravedad"), *Name), AsInt(Def.Severity), AsInt(Row.Severity));
		TestEqual(FString::Printf(TEXT("%s: número de direcciones"), *Name), Def.AllowedTargets.Num(), Row.Targets.Num());
		for (const T Target : Row.Targets)
		{
			TestTrue(FString::Printf(TEXT("%s admite %s"), *Name, *Soulbound::Rules::EffectTargetCode(Target)), Def.AllowedTargets.Contains(Target));
		}
		TestEqual(FString::Printf(TEXT("%s: se aplica en esta story"), *Name), Soulbound::Rules::IsEffectImplemented(Row.Effect), Row.bImplemented);
		TestEqual(FString::Printf(TEXT("%s: es regalo"), *Name), Soulbound::Rules::IsGift(Row.Effect), Row.Severity == S::Gift);
		TestEqual(FString::Printf(TEXT("%s: admite cantidad"), *Name), Def.bTakesAmount, Row.Effect == EEffect::LoseTurn || Row.Effect == EEffect::ExtraTurn);
		EEffect Parsed = EEffect::Halt;
		TestTrue(FString::Printf(TEXT("%s: su código se lee"), *Name), Soulbound::Rules::ParseEffectCode(Name, Parsed) && Parsed == Row.Effect);
		Implemented += Row.bImplemented ? 1 : 0;
	}
	TestEqual(TEXT("Diecisiete se aplican desde S-016: solo la Consulta forzada espera a su story"), Implemented, 17);
	TestEqual(TEXT("Perder una runa: al azar o a elección"), Catalog[AsInt(EEffect::LoseRune)].Variants,
		TArray<EEffectVariant>{EEffectVariant::Random, EEffectVariant::Chosen});
	TestEqual(TEXT("Duelo forzado: una runa o todas"), Catalog[AsInt(EEffect::ForcedDuel)].Variants,
		TArray<EEffectVariant>{EEffectVariant::OneRune, EEffectVariant::AllRunes});
	return true;
}

// CA-EFE-2, R-EFE-2, R-DIR-3: Revelar Brazal se ve, caduca por rondas y se prolonga si ya estaba; sin runas no vale.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEffectsRevelarBrazalTest, "Soulbound.Core.Effects.RevelarBrazal",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FEffectsRevelarBrazalTest::RunTest(const FString& Parameters)
{
	FEventFixture F;
	if (!F.StartAfterRace(*this))
	{
		return false;
	}
	F.State.Seats[Fenrir].Runes[2] = 1;
	const int32 Rounds = F.State.Params.RevealBracerRounds;
	TestEqual(TEXT("Valor inicial de los datos: 1 ronda"), Rounds, 1);

	const int32 From = F.State.EventLog.Num();
	TestEqual(TEXT("El decreto se lanza"), AsInt(F.Decree(Fenrir, App(EEffect::RevealBracer, EEffectTarget::ActivePlayer))), AsInt(EWardenEventStart::Started));
	TestTrue(TEXT("El Brazal de Fenrir queda revelado"), Soulbound::Rules::IsBracerRevealed(F.State, Fenrir));
	TestEqual(TEXT("Hasta la ronda siguiente"), F.State.Seats[Fenrir].RevealedUntilRound, F.State.Round + Rounds);
	TestEqual(TEXT("Queda BracerRevealed"), F.Since(From, EMatchEventType::BracerRevealed).Num(), 1);

	Soulbound::Rules::StartRound(F.State, F.Now);
	TestFalse(TEXT("Caduca con la ronda"), Soulbound::Rules::IsBracerRevealed(F.State, Fenrir));
	TestEqual(TEXT("Y queda BracerHidden"), F.Since(From, EMatchEventType::BracerHidden).Num(), 1);

	// Revelar a un revelado prolonga: dos revelaciones en la misma ronda duran dos rondas.
	F.Decree(Fenrir, App(EEffect::RevealBracer, EEffectTarget::ActivePlayer));
	F.Decree(Fenrir, App(EEffect::RevealBracer, EEffectTarget::ActivePlayer));
	TestEqual(TEXT("Se prolonga"), F.State.Seats[Fenrir].RevealedUntilRound, F.State.Round + 2 * Rounds);
	Soulbound::Rules::StartRound(F.State, F.Now);
	TestTrue(TEXT("Sigue revelado tras una ronda"), Soulbound::Rules::IsBracerRevealed(F.State, Fenrir));
	Soulbound::Rules::StartRound(F.State, F.Now);
	TestFalse(TEXT("Y se oculta tras la segunda"), Soulbound::Rules::IsBracerRevealed(F.State, Fenrir));

	// R-DIR-3: sin runas no vale; queda la burla.
	const int32 Before = F.State.EventLog.Num();
	F.Decree(Koschei, App(EEffect::RevealBracer, EEffectTarget::ActivePlayer));
	TestFalse(TEXT("Sin runas no se revela"), Soulbound::Rules::IsBracerRevealed(F.State, Koschei));
	TestEqual(TEXT("Queda la burla"), F.Since(Before, EMatchEventType::WardenTaunt).Num(), 1);
	TestEqual(TEXT("Y ningún EffectApplied"), F.Since(Before, EMatchEventType::EffectApplied).Num(), 0);
	return true;
}

// CA-EFE-3, R-EFE-4, R-DIR-2: Vuelta al hogar manda a casa a los válidos; ni quien ya está en su hogar ni quien está en el
// Vacío son objetivo.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEffectsVueltaAlHogarTest, "Soulbound.Core.Effects.VueltaAlHogar",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FEffectsVueltaAlHogarTest::RunTest(const FString& Parameters)
{
	FEventFixture F;
	if (!F.StartAfterRace(*this))
	{
		return false;
	}
	F.MoveAway(EchoA);
	TestEqual(TEXT("Koschei va al Vacío"), AsInt(Soulbound::Rules::Banish(F.State, Koschei, false, F.Now)), AsInt(EEffectResult::Applied));

	const int32 From = F.State.EventLog.Num();
	F.Decree(Fenrir, App(EEffect::SendHome, EEffectTarget::Everyone));
	const TArray<FMatchEvent> Applied = F.Since(From, EMatchEventType::EffectApplied);
	TestEqual(TEXT("Solo un objetivo válido: el Eco fuera de casa"), Applied.Num(), 1);
	TestEqual(TEXT("Es el asiento 2"), Applied.IsEmpty() ? INDEX_NONE : Applied[0].Seat, EchoA);
	TestEqual(TEXT("Está en su hogar"), F.State.Seats[EchoA].TileId, Soulbound::Rules::HomeTile(F.State, EchoA));
	TestTrue(TEXT("Koschei sigue en el Vacío"), F.State.Seats[Koschei].IsInVoid());

	// En su hogar no vale: burla.
	const int32 Before = F.State.EventLog.Num();
	F.Decree(EchoB, App(EEffect::SendHome, EEffectTarget::ActivePlayer));
	TestEqual(TEXT("A quien ya está en su hogar, burla"), F.Since(Before, EMatchEventType::WardenTaunt).Num(), 1);
	TestEqual(TEXT("Sin SentHome"), F.Since(Before, EMatchEventType::SentHome).Num(), 0);
	return true;
}

// CA-EFE-4, R-EFE-5: Turno perdido con el tope de los datos; lo que excede se convierte en Vuelta al hogar, y si ya está en
// su hogar, burla (decisión del autor del 12/10/2026).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEffectsTurnoPerdidoTest, "Soulbound.Core.Effects.TurnoPerdidoTope",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FEffectsTurnoPerdidoTest::RunTest(const FString& Parameters)
{
	FEventFixture F;
	if (!F.StartAfterRace(*this))
	{
		return false;
	}
	const int32 Cap = F.State.Params.MaxLostTurns;
	TestEqual(TEXT("Valor inicial del tope: 2"), Cap, 2);
	F.MoveAway(EchoA);

	int32 From = F.State.EventLog.Num();
	F.Decree(EchoA, App(EEffect::LoseTurn, EEffectTarget::ActivePlayer, 2));
	TestEqual(TEXT("Dos turnos perdidos"), F.State.Seats[EchoA].LostTurns, 2);
	TestEqual(TEXT("Sin pasar del tope no va a casa"), F.Since(From, EMatchEventType::SentHome).Num(), 0);

	From = F.State.EventLog.Num();
	F.Decree(EchoA, App(EEffect::LoseTurn, EEffectTarget::ActivePlayer, 1));
	TestEqual(TEXT("El tope se respeta"), F.State.Seats[EchoA].LostTurns, Cap);
	TestEqual(TEXT("Lo que excede es una Vuelta al hogar"), F.Since(From, EMatchEventType::SentHome).Num(), 1);
	TestEqual(TEXT("Está en su hogar"), F.State.Seats[EchoA].TileId, Soulbound::Rules::HomeTile(F.State, EchoA));

	From = F.State.EventLog.Num();
	F.Decree(EchoA, App(EEffect::LoseTurn, EEffectTarget::ActivePlayer, 1));
	TestEqual(TEXT("Ya en su hogar, la Vuelta al hogar no vale: burla"), F.Since(From, EMatchEventType::WardenTaunt).Num(), 1);
	const TArray<FMatchEvent> Taunts = F.Since(From, EMatchEventType::WardenTaunt);
	TestEqual(TEXT("La burla es de la Vuelta al hogar"), Taunts.IsEmpty() ? INDEX_NONE : Taunts[0].Value, AsInt(EEffect::SendHome));
	TestEqual(TEXT("Sin SentHome"), F.Since(From, EMatchEventType::SentHome).Num(), 0);
	return true;
}

// CA-EFE-11, R-EFE-15: Turno extra con el tope que ya existe, que cancela un perdido al empezar el turno; a quien está en el
// Vacío no vale.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEffectsTurnoExtraTest, "Soulbound.Core.Effects.TurnoExtra",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FEffectsTurnoExtraTest::RunTest(const FString& Parameters)
{
	FEventFixture F;
	if (!F.StartAfterRace(*this))
	{
		return false;
	}
	F.Decree(EchoA, App(EEffect::LoseTurn, EEffectTarget::ActivePlayer, 1));
	F.Decree(EchoA, App(EEffect::ExtraTurn, EEffectTarget::ActivePlayer, 1));
	TestEqual(TEXT("Un turno extra"), F.State.Seats[EchoA].ExtraTurns, 1);
	TestEqual(TEXT("Y el perdido sigue hasta su turno"), F.State.Seats[EchoA].LostTurns, 1);

	// Al empezar su turno se cancelan uno a uno: juega sin saltarse nada.
	const int32 From = F.State.EventLog.Num();
	F.BeginTurn(EchoA);
	TestEqual(TEXT("Se cancelan: ningún extra"), F.State.Seats[EchoA].ExtraTurns, 0);
	TestEqual(TEXT("Ni perdido"), F.State.Seats[EchoA].LostTurns, 0);
	TestEqual(TEXT("Nadie salta turno"), F.Since(From, EMatchEventType::TurnSkipped).Num(), 0);
	F.FinishTurn();

	// El tope que ya existe.
	F.Decree(EchoA, App(EEffect::ExtraTurn, EEffectTarget::ActivePlayer, 2));
	F.Decree(EchoA, App(EEffect::ExtraTurn, EEffectTarget::ActivePlayer, 2));
	TestEqual(TEXT("Con el tope de los datos"), F.State.Seats[EchoA].ExtraTurns, F.State.Params.MaxExtraTurns);

	// Decisión del autor del 08/10/2026: a quien está en el Vacío no se le da turno extra.
	Soulbound::Rules::Banish(F.State, EchoB, false, F.Now);
	const int32 Before = F.State.EventLog.Num();
	F.Decree(F.PreviousOf(EchoB), App(EEffect::ExtraTurn, EEffectTarget::NextPlayer, 1));
	TestEqual(TEXT("En el Vacío no recibe turno extra"), F.State.Seats[EchoB].ExtraTurns, 0);
	TestEqual(TEXT("Burla"), F.Since(Before, EMatchEventType::WardenTaunt).Num(), 1);
	return true;
}

// R-EFE-6: el Sello del Warden sella una casilla válida al azar con el sorteo que se le pasa; sin ninguna, burla.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEffectsSelloAlAzarTest, "Soulbound.Core.Effects.SelloAlAzar",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FEffectsSelloAlAzarTest::RunTest(const FString& Parameters)
{
	FEventFixture F;
	if (!F.StartAfterRace(*this))
	{
		return false;
	}
	const TArray<int32> Valid = SealableNow(F.State);
	FEventFixture Copy = F;
	const int32 From = F.State.EventLog.Num();
	TestEqual(TEXT("El Sello va sin dirección"), AsInt(F.Decree(Fenrir, App(EEffect::PlaceSeal, EEffectTarget::None))), AsInt(EWardenEventStart::Started));
	const int32 Sealed = SealedSince(F, From);
	TestTrue(TEXT("Sella una casilla válida"), Valid.Contains(Sealed));
	Soulbound::Tests::CheckSealInvariants(*this, F.State, TEXT("Tras el Sello del Warden"));

	// Mismo sorteo del Warden, misma casilla.
	Copy.Decree(Fenrir, App(EEffect::PlaceSeal, EEffectTarget::None));
	TestEqual(TEXT("El mismo sorteo da la misma casilla"), SealedSince(Copy, From), Sealed);

	// Sin casillas válidas: burla.
	F.State.Board.Params.MaxSealedTiles = 0;
	const int32 Before = F.State.EventLog.Num();
	F.Decree(Fenrir, App(EEffect::PlaceSeal, EEffectTarget::None));
	TestEqual(TEXT("Sin casilla válida, burla"), F.Since(Before, EMatchEventType::WardenTaunt).Num(), 1);
	TestEqual(TEXT("Sin Sello"), F.Since(Before, EMatchEventType::SealPlaced).Num(), 0);
	return true;
}

// CA-EFE-5, R-EFE-7: Sello contra rival sella una casilla válida del camino más corto entre el rival y su objetivo probable:
// la Fuente más cercana de un tipo que le falta o, con los seis tipos, el Altar.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEffectsSelloContraRivalCaminoTest, "Soulbound.Core.Effects.SelloContraRivalEnSuCamino",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FEffectsSelloContraRivalCaminoTest::RunTest(const FString& Parameters)
{
	FEventFixture F;
	if (!F.StartAfterRace(*this))
	{
		return false;
	}
	// Fenrir en el Altar: desde su Nicho, al final de un ramal, ninguna casilla del ramal se puede sellar (dejaría el Nicho sin
	// camino al Altar) y el camino no tendría casillas válidas, que es el caso del test siguiente.
	const FBoardState& Board = F.State.Board;
	F.State.Seats[Fenrir].TileId = Board.FindAltar();
	TArray<int32> FromRival = Distances(Board, Board.FindAltar());

	// Fenrir no tiene runas: su objetivo es la Fuente con reserva más cercana.
	int32 Nearest = MAX_int32;
	for (const FTileState& Tile : Board.Tiles)
	{
		Nearest = Tile.Type == ETileType::PowerSource && Tile.RuneReserve > 0 ? FMath::Min(Nearest, FromRival[Tile.TileId]) : Nearest;
	}
	const TArray<int32> Valid = SealableNow(F.State);
	const bool bPathHasValid = Valid.ContainsByPredicate([&](int32 Tile)
	{
		return Board.Tiles.ContainsByPredicate([&](const FTileState& Source)
		{
			return Source.Type == ETileType::PowerSource && Source.RuneReserve > 0 && FromRival[Source.TileId] == Nearest
				&& FromRival[Tile] + Distances(Board, Tile)[Source.TileId] == Nearest;
		});
	});
	if (!TestTrue(TEXT("Montaje: el camino tiene casillas que se pueden sellar"), bPathHasValid))
	{
		return false;
	}
	int32 From = F.State.EventLog.Num();
	F.Director(EchoA);
	F.Launch(F.AddEvent(TEXT("sello-contra-rival"), {{App(EEffect::SealAgainstRival, EEffectTarget::ChosenRival)}}));
	TestEqual(TEXT("El activo señala"), AsInt(F.State.PendingChoice.Kind), AsInt(EChoiceKind::ChosenRival));
	TestEqual(TEXT("Señala a Fenrir"), AsInt(F.Answer(Fenrir)), AsInt(EIntentResult::Accepted));
	int32 Sealed = SealedSince(F, From);
	TestTrue(TEXT("Sella una casilla que era válida"), Valid.Contains(Sealed));
	bool bOnPath = false;
	for (const FTileState& Tile : Board.Tiles)
	{
		if (Sealed != INDEX_NONE && Tile.Type == ETileType::PowerSource && Tile.RuneReserve > 0 && FromRival[Tile.TileId] == Nearest)
		{
			bOnPath |= FromRival[Sealed] + Distances(Board, Sealed)[Tile.TileId] == Nearest;
		}
	}
	TestTrue(TEXT("En un camino más corto hasta la Fuente más cercana"), bOnPath);
	Soulbound::Tests::CheckSealInvariants(*this, F.State, TEXT("Sello contra rival"));

	// Con los seis tipos, su objetivo es el Altar; desde su hogar, que en este tablero tiene casillas sellables hasta el Altar.
	Soulbound::Rules::ReleaseSeal(F.State, Sealed, F.Now);
	F.State.Seats[Fenrir].Runes.Init(1, Soulbound::Board::DomainCount);
	F.State.Seats[Fenrir].TileId = Soulbound::Rules::HomeTile(F.State, Fenrir);
	FromRival = Distances(Board, F.State.Seats[Fenrir].TileId);
	const int32 Altar = Board.FindAltar();
	From = F.State.EventLog.Num();
	F.Director(EchoA);
	F.Launch(TEXT("sello-contra-rival"));
	F.Answer(Fenrir);
	Sealed = SealedSince(F, From);
	TestTrue(TEXT("Con seis tipos, en un camino más corto hasta el Altar"),
		Sealed != INDEX_NONE && FromRival[Sealed] + Distances(Board, Sealed)[Altar] == FromRival[Altar]);
	return true;
}

// CA-EFE-5, R-EFE-7: sin casilla válida en el camino, una válida a distancia mínima del rival.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEffectsSelloContraRivalSinCaminoTest, "Soulbound.Core.Effects.SelloContraRivalSinCasillaEnElCamino",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FEffectsSelloContraRivalSinCaminoTest::RunTest(const FString& Parameters)
{
	FEventFixture F;
	if (!F.StartAfterRace(*this))
	{
		return false;
	}
	// Fenrir, sin runas, junto a la Fuente de su Dominio: el camino es su casilla (ocupada) y la Fuente (no se sella).
	const int32 Source = F.SourceOf(FEventFixture::FenrirDomain);
	const TArray<int32>& Neighbours = F.State.Board.Tiles[Source].Neighbours;
	if (!TestTrue(TEXT("La Fuente tiene vecinas"), !Neighbours.IsEmpty()))
	{
		return false;
	}
	F.State.Seats[Fenrir].TileId = Neighbours[0];
	const TArray<int32> Valid = SealableNow(F.State);
	const TArray<int32> FromRival = Distances(F.State.Board, Neighbours[0]);
	int32 Closest = MAX_int32;
	for (const int32 Tile : Valid)
	{
		Closest = FMath::Min(Closest, FromRival[Tile]);
	}

	const int32 From = F.State.EventLog.Num();
	F.Director(EchoA);
	F.Launch(F.AddEvent(TEXT("sello-contra-rival"), {{App(EEffect::SealAgainstRival, EEffectTarget::ChosenRival)}}));
	F.Answer(Fenrir);
	const int32 Sealed = SealedSince(F, From);
	TestTrue(TEXT("Sella una casilla válida"), Valid.Contains(Sealed));
	TestEqual(TEXT("A distancia mínima del rival"), Sealed != INDEX_NONE ? FromRival[Sealed] : MAX_int32, Closest);
	return true;
}

// CA-EFE-5, R-DIR-4: si el activo no elige a tiempo a quién, el Sello contra rival se aplica a él mismo, con insolencia.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEffectsSelloContraRivalSinElegirTest, "Soulbound.Core.Effects.SelloContraRivalSinElegirCaeEnUnoMismo",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FEffectsSelloContraRivalSinElegirTest::RunTest(const FString& Parameters)
{
	FEventFixture F;
	if (!F.StartAfterRace(*this))
	{
		return false;
	}
	const int32 From = F.State.EventLog.Num();
	F.Director(EchoA);
	F.Launch(F.AddEvent(TEXT("sello-contra-rival"), {{App(EEffect::SealAgainstRival, EEffectTarget::ChosenRival)}}));
	TestEqual(TEXT("Con el reloj corto"), F.State.PendingChoice.DeadlineSeconds, F.Now + F.State.Params.ShortChoiceClockSeconds);
	TestTrue(TEXT("Deja pasar el plazo"), F.Expire());
	const TArray<FMatchEvent> Ignored = F.Since(From, EMatchEventType::WardenIgnored);
	TestEqual(TEXT("Desatiende al Warden"), Ignored.Num(), 1);
	TestEqual(TEXT("Sube su insolencia"), F.State.Seats[EchoA].Insolence, 1);
	const TArray<FMatchEvent> Applied = F.Since(From, EMatchEventType::EffectApplied);
	TestEqual(TEXT("El Sello contra rival cae sobre él"), Applied.IsEmpty() ? INDEX_NONE : Applied[0].Seat, EchoA);
	TestEqual(TEXT("Y se sella una casilla"), F.Since(From, EMatchEventType::SealPlaced).Num(), 1);
	TestFalse(TEXT("El evento termina"), F.State.CurrentEvent.IsActive());
	return true;
}

// CA-EFE-6, R-EFE-8: la marca se cobra con cada acción dirigida de la lista (robar, expulsar, ganar un duelo, mandar a casa o
// al Vacío con un efecto señalado): turno extra al que la cobra y la marca termina.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEffectsMarcadoSeCobraTest, "Soulbound.Core.Effects.MarcadoSeCobra",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FEffectsMarcadoSeCobraTest::RunTest(const FString& Parameters)
{
	FEventFixture F;
	if (!F.StartAfterRace(*this))
	{
		return false;
	}
	const FString MarkRival = F.AddEvent(TEXT("marca-al-señalado"), {{App(EEffect::Mark, EEffectTarget::ChosenRival)}});
	auto MarkSeat = [&](int32 DirectorSeat, int32 Target)
	{
		F.Director(DirectorSeat);
		F.Launch(MarkRival);
		F.Answer(Target);
		return TestTrue(FString::Printf(TEXT("El asiento %d queda Marcado"), Target), Soulbound::Rules::IsMarked(F.State, Target));
	};
	auto CheckCollected = [&](int32 From, int32 Collector, int32 Marked, const TCHAR* How)
	{
		const TArray<FMatchEvent> Collected = F.Since(From, EMatchEventType::MarkCollected);
		TestEqual(FString::Printf(TEXT("%s: se cobra la marca"), How), Collected.Num(), 1);
		TestEqual(FString::Printf(TEXT("%s: la cobra el asiento %d"), How, Collector), Collected.IsEmpty() ? INDEX_NONE : Collected[0].Seat, Collector);
		TestEqual(FString::Printf(TEXT("%s: al asiento %d"), How, Marked), Collected.IsEmpty() ? INDEX_NONE : Collected[0].OtherSeat, Marked);
		TestEqual(FString::Printf(TEXT("%s: turno extra"), How), F.State.Seats[Collector].ExtraTurns, 1);
		TestFalse(FString::Printf(TEXT("%s: la marca termina"), How), Soulbound::Rules::IsMarked(F.State, Marked));
		F.State.Seats[Collector].ExtraTurns = 0;
	};

	// Expulsar (R-POD-4).
	MarkSeat(Fenrir, Koschei);
	int32 From = F.State.EventLog.Num();
	Soulbound::Rules::ApplyExpel(F.State, Fenrir, Koschei, F.Now);
	CheckCollected(From, Fenrir, Koschei, TEXT("Expulsión"));

	// Robar (R-ECO-4): la runa pasa al ladrón.
	MarkSeat(Koschei, Fenrir);
	F.State.Seats[Fenrir].Runes[0] = 1;
	From = F.State.EventLog.Num();
	TestEqual(TEXT("Empieza el robo"), static_cast<int32>(Soulbound::Rules::BeginSteal(F.State, EchoA, Fenrir, F.Now)), static_cast<int32>(EStealStart::Started));
	Soulbound::Rules::ResolveShellPick(F.State, 0, F.Now);
	CheckCollected(From, EchoA, Fenrir, TEXT("Robo"));

	// Ganarle un duelo (R-DUE-1).
	MarkSeat(EchoA, Fenrir);
	F.State.Seats[Fenrir].Runes[0] = 1;
	From = F.State.EventLog.Num();
	Soulbound::Rules::BeginDuel(F.State, Koschei, Fenrir, F.Now);
	Soulbound::Rules::RecordDuelPerformance(F.State, Koschei, 1.0);
	Soulbound::Rules::RecordDuelPerformance(F.State, Fenrir, 0.0);
	Soulbound::Rules::ResolveDuel(F.State, F.Now);
	CheckCollected(From, Koschei, Fenrir, TEXT("Duelo"));

	// Mandarle a casa con un efecto que señaló.
	MarkSeat(Fenrir, EchoB);
	F.MoveAway(EchoB);
	From = F.State.EventLog.Num();
	F.Director(Koschei);
	F.Launch(F.AddEvent(TEXT("a-casa-el-señalado"), {{App(EEffect::SendHome, EEffectTarget::ChosenRival)}}));
	F.Answer(EchoB);
	CheckCollected(From, Koschei, EchoB, TEXT("Vuelta al hogar señalada"));

	// Mandarle al Vacío con un efecto que señaló.
	MarkSeat(Fenrir, EchoA);
	From = F.State.EventLog.Num();
	F.Director(Koschei);
	F.Launch(F.AddEvent(TEXT("al-vacio-el-señalado"), {{App(EEffect::Banish, EEffectTarget::ChosenRival)}}));
	F.Answer(EchoA);
	TestTrue(TEXT("El señalado va al Vacío"), F.State.Seats[EchoA].IsInVoid());
	CheckCollected(From, Koschei, EchoA, TEXT("Vacío señalado"));
	return true;
}

// CA-EFE-6, R-EFE-8: la marca caduca por rondas sin cobrarse; dos Marcados conviven con duraciones propias; re-marcar reinicia.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEffectsMarcadoCaducaTest, "Soulbound.Core.Effects.MarcadoCaducaConviveYSeReinicia",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FEffectsMarcadoCaducaTest::RunTest(const FString& Parameters)
{
	FEventFixture F;
	if (!F.StartAfterRace(*this))
	{
		return false;
	}
	const int32 Rounds = F.State.Params.MarkRounds;
	TestEqual(TEXT("Valor inicial de los datos: 3 rondas"), Rounds, 3);
	const FString MarkRival = F.AddEvent(TEXT("marca-al-señalado"), {{App(EEffect::Mark, EEffectTarget::ChosenRival)}});
	auto MarkSeat = [&](int32 Target)
	{
		F.Director(Koschei);
		F.Launch(MarkRival);
		F.Answer(Target);
	};

	MarkSeat(Fenrir);
	const int32 From = F.State.EventLog.Num();
	for (int32 Round = 1; Round < Rounds; ++Round)
	{
		Soulbound::Rules::StartRound(F.State, F.Now);
	}
	TestTrue(TEXT("Sigue Marcado hasta su última ronda"), Soulbound::Rules::IsMarked(F.State, Fenrir));
	MarkSeat(EchoA);
	Soulbound::Rules::StartRound(F.State, F.Now);
	TestFalse(TEXT("Caduca sin cobrarse"), Soulbound::Rules::IsMarked(F.State, Fenrir));
	TestEqual(TEXT("Queda MarkExpired"), F.Since(From, EMatchEventType::MarkExpired).Num(), 1);
	TestTrue(TEXT("La otra marca convive, con su duración"), Soulbound::Rules::IsMarked(F.State, EchoA));

	// Re-marcar reinicia la duración.
	MarkSeat(EchoA);
	TestEqual(TEXT("Re-marcar reinicia"), F.State.Seats[EchoA].MarkedUntilRound, F.State.Round + Rounds);
	for (int32 Round = 1; Round < Rounds; ++Round)
	{
		Soulbound::Rules::StartRound(F.State, F.Now);
	}
	TestTrue(TEXT("Dura sus rondas desde la segunda marca"), Soulbound::Rules::IsMarked(F.State, EchoA));
	Soulbound::Rules::StartRound(F.State, F.Now);
	TestFalse(TEXT("Y caduca"), Soulbound::Rules::IsMarked(F.State, EchoA));
	return true;
}

// CA-EFE-6, R-EFE-8: un efecto "al azar" o "al líder" no cobra la marca de nadie; nadie se marca a sí mismo.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEffectsMarcadoNoDirigidoTest, "Soulbound.Core.Effects.MarcadoAlAzarOAlLiderNoCobra",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FEffectsMarcadoNoDirigidoTest::RunTest(const FString& Parameters)
{
	FEventFixture F;
	if (!F.StartAfterRace(*this))
	{
		return false;
	}
	F.Director(Fenrir);
	F.Launch(F.AddEvent(TEXT("marca-al-señalado"), {{App(EEffect::Mark, EEffectTarget::ChosenRival)}}));
	F.Answer(Koschei);

	// Al líder: Koschei es el único con runas.
	F.State.Seats[Koschei].Runes[0] = 1;
	F.MoveAway(Koschei);
	int32 From = F.State.EventLog.Num();
	F.Decree(Fenrir, App(EEffect::SendHome, EEffectTarget::Leader));
	TestEqual(TEXT("Al líder: va a casa"), F.Since(From, EMatchEventType::SentHome).Num(), 1);
	TestEqual(TEXT("Pero no cobra la marca"), F.Since(From, EMatchEventType::MarkCollected).Num(), 0);
	TestTrue(TEXT("Sigue Marcado"), Soulbound::Rules::IsMarked(F.State, Koschei));

	// Al azar: Koschei es el único fuera de su hogar.
	F.MoveAway(Koschei);
	From = F.State.EventLog.Num();
	F.Decree(Fenrir, App(EEffect::SendHome, EEffectTarget::RandomPlayer));
	TestEqual(TEXT("Al azar: va a casa"), F.Since(From, EMatchEventType::SentHome).Num(), 1);
	TestEqual(TEXT("Pero no cobra la marca"), F.Since(From, EMatchEventType::MarkCollected).Num(), 0);
	TestEqual(TEXT("Nadie gana turno extra"), F.State.Seats[Fenrir].ExtraTurns, 0);

	// Nadie se marca a sí mismo: el líder dirige el evento.
	From = F.State.EventLog.Num();
	F.Decree(Koschei, App(EEffect::Mark, EEffectTarget::Leader));
	TestEqual(TEXT("Marcar al líder que dirige es burla"), F.Since(From, EMatchEventType::WardenTaunt).Num(), 1);
	TestEqual(TEXT("Sin Marked"), F.Since(From, EMatchEventType::Marked).Num(), 0);
	return true;
}

// CA-EFE-8, R-EFE-11: Perder una runa al azar la devuelve a la reserva de su Fuente; Koschei no aparta nada (no es un robo);
// sin runas, burla.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEffectsPerderUnaRunaAlAzarTest, "Soulbound.Core.Effects.PerderUnaRunaAlAzar",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FEffectsPerderUnaRunaAlAzarTest::RunTest(const FString& Parameters)
{
	FEventFixture F;
	if (!F.StartAfterRace(*this))
	{
		return false;
	}
	F.State.Seats[Koschei].Runes[0] = 2;
	F.State.Seats[Koschei].Runes[3] = 1;
	const int32 Reserve0 = F.State.Board.Tiles[F.SourceOf(0)].RuneReserve;
	const int32 Reserve3 = F.State.Board.Tiles[F.SourceOf(3)].RuneReserve;

	const int32 From = F.State.EventLog.Num();
	F.Decree(Koschei, App(EEffect::LoseRune, EEffectTarget::ActivePlayer, 1, EEffectVariant::Random));
	TestEqual(TEXT("Pierde una runa"), F.RuneTotal(Koschei), 2);
	const TArray<FMatchEvent> Returned = F.Since(From, EMatchEventType::RuneReturned);
	TestEqual(TEXT("Vuelve a su Fuente"), Returned.Num(), 1);
	const int32 Type = Returned.IsEmpty() ? INDEX_NONE : Returned[0].Value;
	TestEqual(TEXT("La reserva de su tipo sube uno"),
		F.State.Board.Tiles[F.SourceOf(0)].RuneReserve + F.State.Board.Tiles[F.SourceOf(3)].RuneReserve, Reserve0 + Reserve3 + 1);
	TestTrue(TEXT("De un tipo que tenía"), Type == 0 || Type == 3);
	TestEqual(TEXT("Koschei no aparta"), F.Since(From, EMatchEventType::SafeRuneSetAside).Num(), 0);
	TestFalse(TEXT("Ni se le pide nada"), F.State.PendingChoice.IsOpen());

	const int32 Before = F.State.EventLog.Num();
	F.Decree(EchoA, App(EEffect::LoseRune, EEffectTarget::ActivePlayer, 1, EEffectVariant::Random));
	TestEqual(TEXT("Sin runas, burla"), F.Since(Before, EMatchEventType::WardenTaunt).Num(), 1);
	return true;
}

// CA-EFE-8, R-EFE-11, R-DIR-4: la variante "a elección" pide la runa con el reloj corto; sin elegir, una al azar e insolencia.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEffectsPerderUnaRunaAEleccionTest, "Soulbound.Core.Effects.PerderUnaRunaAEleccion",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FEffectsPerderUnaRunaAEleccionTest::RunTest(const FString& Parameters)
{
	FEventFixture F;
	if (!F.StartAfterRace(*this))
	{
		return false;
	}
	F.State.Seats[Fenrir].Runes[0] = 1;
	F.State.Seats[Fenrir].Runes[2] = 1;
	const FEffectApplication Chosen = App(EEffect::LoseRune, EEffectTarget::ActivePlayer, 1, EEffectVariant::Chosen);
	F.Decree(Fenrir, Chosen);
	TestEqual(TEXT("Se le pide la runa"), AsInt(F.State.PendingChoice.Kind), AsInt(EChoiceKind::LoseRune));
	TestEqual(TEXT("A él"), F.State.PendingChoice.Seat, Fenrir);
	TestEqual(TEXT("Entre sus tipos"), F.State.PendingChoice.Options, TArray<int32>{0, 2});
	TestEqual(TEXT("Con el reloj corto"), F.State.PendingChoice.DeadlineSeconds, F.Now + F.State.Params.ShortChoiceClockSeconds);
	TestEqual(TEXT("Elige la 2"), AsInt(F.Answer(2)), AsInt(EIntentResult::Accepted));
	TestEqual(TEXT("Pierde la 2"), F.State.Seats[Fenrir].RuneCountOf(2), 0);
	TestEqual(TEXT("Conserva la 0"), F.State.Seats[Fenrir].RuneCountOf(0), 1);

	const int32 From = F.State.EventLog.Num();
	F.Decree(Fenrir, Chosen);
	TestTrue(TEXT("Deja pasar el plazo"), F.Expire());
	TestEqual(TEXT("Pierde una al azar"), F.RuneTotal(Fenrir), 0);
	TestEqual(TEXT("Desatiende al Warden"), F.Since(From, EMatchEventType::WardenIgnored).Num(), 1);
	TestEqual(TEXT("Con insolencia"), F.State.Seats[Fenrir].Insolence, 1);
	return true;
}

// CA-EFE-12, R-EFE-16: Runa regalada: elige y la recibe con la reserva una menos (un Eco, de cualquier Dominio); sin Fuentes
// con reserva no vale; sin elegir a tiempo, nada, burla e insolencia.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEffectsRunaRegaladaTest, "Soulbound.Core.Effects.RunaRegalada",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FEffectsRunaRegaladaTest::RunTest(const FString& Parameters)
{
	FEventFixture F;
	if (!F.StartAfterRace(*this))
	{
		return false;
	}
	const FEffectApplication Gift = App(EEffect::GiftRune, EEffectTarget::ActivePlayer);
	const int32 Owned = FEventFixture::FenrirDomain;
	const int32 Reserve = F.State.Board.Tiles[F.SourceOf(Owned)].RuneReserve;
	F.Decree(EchoA, Gift);
	TestEqual(TEXT("Se le pide el tipo"), AsInt(F.State.PendingChoice.Kind), AsInt(EChoiceKind::GiftRune));
	TestTrue(TEXT("Entre los de Fuentes con reserva, también de Dominios con dueño"), F.State.PendingChoice.Options.Contains(Owned));
	TestEqual(TEXT("Elige la del Dominio de Fenrir"), AsInt(F.Answer(Owned)), AsInt(EIntentResult::Accepted));
	TestEqual(TEXT("El Eco la recibe"), F.State.Seats[EchoA].RuneCountOf(Owned), 1);
	TestEqual(TEXT("La reserva baja uno"), F.State.Board.Tiles[F.SourceOf(Owned)].RuneReserve, Reserve - 1);

	// Sin elegir a tiempo: nada, burla e insolencia.
	int32 From = F.State.EventLog.Num();
	F.Decree(EchoA, Gift);
	TestTrue(TEXT("Deja pasar el plazo"), F.Expire());
	TestEqual(TEXT("No recibe nada"), F.RuneTotal(EchoA), 1);
	TestEqual(TEXT("Burla"), F.Since(From, EMatchEventType::WardenTaunt).Num(), 1);
	TestEqual(TEXT("Desatiende"), F.Since(From, EMatchEventType::WardenIgnored).Num(), 1);
	TestEqual(TEXT("Insolencia"), F.State.Seats[EchoA].Insolence, 1);

	// Sin Fuentes con reserva no vale.
	for (FTileState& Tile : F.State.Board.Tiles)
	{
		Tile.RuneReserve = 0;
	}
	From = F.State.EventLog.Num();
	F.Decree(EchoA, Gift);
	TestFalse(TEXT("No se pide nada"), F.State.PendingChoice.IsOpen());
	TestEqual(TEXT("Burla"), F.Since(From, EMatchEventType::WardenTaunt).Num(), 1);
	return true;
}

// CA-EFE-13, R-EFE-18, R-EFE-9: con la Noche un Eco alcanza Dominios con dueño y no obtiene runas en sus Fuentes; la Ley de
// Sombras lo manda a casa, termina la Noche y lo confina de nuevo.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEffectsNocheYLeyTest, "Soulbound.Core.Effects.NocheYLeyDeSombras",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FEffectsNocheYLeyTest::RunTest(const FString& Parameters)
{
	FEventFixture F;
	if (!F.StartAfterRace(*this))
	{
		return false;
	}
	TestTrue(TEXT("Sin Noche, el Eco está confinado"), Soulbound::Rules::MakeMoverTraits(F.State, EchoA).bConfinedToForgotten);
	int32 From = F.State.EventLog.Num();
	F.Director(Fenrir);
	TestEqual(TEXT("La Noche se lanza"), AsInt(F.Launch(TEXT("noche"))), AsInt(EWardenEventStart::Started));
	TestTrue(TEXT("Es de noche"), F.State.IsEchoNight());
	TestEqual(TEXT("Queda EchoNightBegan"), F.Since(From, EMatchEventType::EchoNightBegan).Num(), 1);
	TestFalse(TEXT("El Eco no está confinado"), Soulbound::Rules::MakeMoverTraits(F.State, EchoA).bConfinedToForgotten);

	// Decisión del autor del 14/10/2026: una Noche sobre otra no vale (AlreadyActive): burla, sin segundo EchoNightBegan ni cambio del tope.
	const int32 UntilRound = F.State.EchoNightUntilRound;
	From = F.State.EventLog.Num();
	F.Director(Fenrir);
	F.Launch(TEXT("noche"));
	TestEqual(TEXT("Con la Noche activa no vale"), AsInt(Soulbound::Rules::CanEchoNight(F.State)), AsInt(EEffectResult::AlreadyActive));
	TestEqual(TEXT("Una Noche sobre otra es burla"), F.Since(From, EMatchEventType::WardenTaunt).Num(), 1);
	TestEqual(TEXT("Sin segundo EchoNightBegan"), F.Since(From, EMatchEventType::EchoNightBegan).Num(), 0);
	TestEqual(TEXT("El tope no cambia"), F.State.EchoNightUntilRound, UntilRound);

	// Llega a la Fuente de un Dominio con dueño: no obtiene runa.
	From = F.State.EventLog.Num();
	TestTrue(TEXT("El Eco llega a la Fuente de Fenrir"), F.Land(EchoA, F.SourceOf(FEventFixture::FenrirDomain)));
	TestEqual(TEXT("Sin runa en una Fuente con dueño"), F.Since(From, EMatchEventType::RuneObtained).Num(), 0);
	F.FinishTurn();

	From = F.State.EventLog.Num();
	F.Director(Fenrir);
	F.Launch(TEXT("ley-de-sombras"));
	TestEqual(TEXT("La Ley lo manda a casa"), F.State.Seats[EchoA].TileId, Soulbound::Rules::HomeTile(F.State, EchoA));
	TestFalse(TEXT("Termina la Noche"), F.State.IsEchoNight());
	const TArray<FMatchEvent> Ended = F.Since(From, EMatchEventType::EchoNightEnded);
	TestEqual(TEXT("Queda EchoNightEnded"), Ended.Num(), 1);
	TestEqual(TEXT("Por la Ley de Sombras"), Ended.IsEmpty() ? INDEX_NONE : Ended[0].Detail, static_cast<int32>(EEchoNightEnd::ShadowLaw));
	TestTrue(TEXT("Y vuelve a estar confinado"), Soulbound::Rules::MakeMoverTraits(F.State, EchoA).bConfinedToForgotten);
	return true;
}

// CA-EFE-13, R-EFE-18: la Noche termina sola por el tope de rondas de los datos, con los Ecos a casa como con la Ley, sin evento
// de Ley de Sombras (decisión del autor del 12/10/2026).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEffectsNocheTopeTest, "Soulbound.Core.Effects.NocheTopeDeRondas",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FEffectsNocheTopeTest::RunTest(const FString& Parameters)
{
	FEventFixture F;
	if (!F.StartAfterRace(*this))
	{
		return false;
	}
	const int32 Rounds = F.State.Params.EchoNightMaxRounds;
	TestEqual(TEXT("Valor inicial de los datos: 8 rondas"), Rounds, 8);
	F.Director(Fenrir);
	F.Launch(TEXT("noche"));
	F.State.Seats[EchoA].TileId = F.NormalTiles(FEventFixture::FenrirDomain)[0];
	const int32 From = F.State.EventLog.Num();
	for (int32 Round = 1; Round < Rounds; ++Round)
	{
		Soulbound::Rules::StartRound(F.State, F.Now);
	}
	TestTrue(TEXT("Sigue la Noche hasta su última ronda"), F.State.IsEchoNight());
	Soulbound::Rules::StartRound(F.State, F.Now);
	TestFalse(TEXT("Termina por el tope"), F.State.IsEchoNight());
	const TArray<FMatchEvent> Ended = F.Since(From, EMatchEventType::EchoNightEnded);
	TestEqual(TEXT("Por el tope de rondas"), Ended.IsEmpty() ? INDEX_NONE : Ended[0].Detail, static_cast<int32>(EEchoNightEnd::RoundLimit));
	TestEqual(TEXT("El Eco vuelve a casa"), F.State.Seats[EchoA].TileId, Soulbound::Rules::HomeTile(F.State, EchoA));
	TestEqual(TEXT("Sin evento de Ley de Sombras"), F.Since(From, EMatchEventType::EffectApplied).Num(), 0);
	TestTrue(TEXT("Confinado otra vez"), Soulbound::Rules::MakeMoverTraits(F.State, EchoA).bConfinedToForgotten);
	return true;
}

// Efectos pendientes de su story: aplicar Alto o Consulta forzada devuelve "no implementado" sin cambiar el estado ni el
// registro (desde S-015, la Prueba del Warden, el Robo decretado y el Duelo forzado ya se aplican); un evento que los usa, en
// cualquier opción, se rechaza entero.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEffectsPendientesTest, "Soulbound.Core.Effects.PendientesDeSuStory",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FEffectsPendientesTest::RunTest(const FString& Parameters)
{
	FEventFixture F;
	if (!F.StartAfterRace(*this))
	{
		return false;
	}
	F.State.Seats[Koschei].Runes[0] = 1;
	F.Director(Fenrir);
	// Desde S-016 el Alto se aplica (su efecto es el corte): solo queda la Consulta forzada, de S-017.
	const TPair<EEffect, EEffectTarget> Pending[] = {
		{EEffect::ForcedOracle, EEffectTarget::ActivePlayer},
	};
	for (const TPair<EEffect, EEffectTarget>& Effect : Pending)
	{
		const FString Name = Soulbound::Rules::EffectCode(Effect.Key);
		const FMatchState Before = F.State;
		FRandomStream Random(3);
		TestEqual(FString::Printf(TEXT("%s: CanApplyEffect dice no implementado"), *Name),
			AsInt(Soulbound::Rules::CanApplyEffect(F.State, Effect.Key, Koschei, Fenrir)), AsInt(EEffectResult::NotImplemented));
		TestEqual(FString::Printf(TEXT("%s: ApplyEffect también"), *Name),
			AsInt(Soulbound::Rules::ApplyEffect(F.State, App(Effect.Key, Effect.Value), Koschei, Fenrir, INDEX_NONE, INDEX_NONE, F.Now, Random)),
			AsInt(EEffectResult::NotImplemented));
		TestTrue(FString::Printf(TEXT("%s: el estado y el registro no cambian"), *Name), SameState(Before, F.State));
	}

	// Un dilema con una Consulta forzada en una de sus opciones se rechaza entero (hasta S-016, con un Alto).
	const FString Id = F.AddEvent(TEXT("con-consulta"), {{App(EEffect::ExtraTurn, EEffectTarget::ActivePlayer)}, {App(EEffect::ForcedOracle, EEffectTarget::ActivePlayer)}});
	const FMatchState Before = F.State;
	TestEqual(TEXT("El evento se rechaza: no implementado"), AsInt(F.Launch(Id)), AsInt(EWardenEventStart::NotImplemented));
	TestTrue(TEXT("Sin tocar nada"), SameState(Before, F.State));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
