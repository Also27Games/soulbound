#include "BoardViewActor.h"

#include "../Match/MatchHostSubsystem.h"
#include "../Warden/WardenHostSubsystem.h"
#include "BoardTileActor.h"
#include "ChroniclePanel.h"
#include "EventText.h"
#include "LegendPanel.h"
#include "Match/BondRaceRules.h"
#include "Match/RuneRules.h"
#include "PresentationShapes.h"
#include "Warden/WardenBrain.h"
#include "SeatTokenActor.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"

namespace
{
	/** Casilla a la altura del suelo; las conexiones y las fichas se miden desde aquí. */
	constexpr float TileThickness = 10.f;
	constexpr float EdgeThickness = 4.f;
	/** Alto de las marcas de tipo sobre la losa. */
	constexpr float InlayHeight = 6.f;
	/** Segmentos del aro de un Nicho reclamado. */
	constexpr int32 NicheRingSegmentCount = 24;

	/** Clave base de los mensajes de pantalla de esta vista: una línea por clave, se sustituyen al refrescar. */
	constexpr uint64 TextKeyBase = 0x53420500;

	/** Atenuado de una casilla de un Dominio Olvidado: mismo tono, más oscuro. */
	FLinearColor Dimmed(const FLinearColor& Color, float Factor)
	{
		return FLinearColor(Color.R * Factor, Color.G * Factor, Color.B * Factor);
	}

	const TCHAR* PhaseName(ETurnPhase Phase)
	{
		switch (Phase)
		{
		case ETurnPhase::BetweenTurns: return TEXT("entre turnos");
		case ETurnPhase::Start: return TEXT("inicio");
		case ETurnPhase::PreRoll: return TEXT("antes de tirar");
		case ETurnPhase::RollAndChoose: return TEXT("tirada y destino");
		case ETurnPhase::MoveAndResolve: return TEXT("movimiento");
		case ETurnPhase::SkillTrial: return TEXT("minijuego (Fuente, escape del Vacío o Prueba del Warden)");
		case ETurnPhase::ShellGame: return TEXT("trilero");
		case ETurnPhase::Duel: return TEXT("duelo");
		case ETurnPhase::End: return TEXT("fin de turno");
		}
		return TEXT("?");
	}

	/** Marca de tiempo de la crónica: minutos y segundos del reloj de partida. */
	FString Stamp(double Seconds)
	{
		const int32 Total = FMath::Max(0, FMath::FloorToInt(Seconds));
		return FString::Printf(TEXT("%d:%02d"), Total / 60, Total % 60);
	}

	/** Efecto de un Nodo de Poder para su etiqueta (S-009); vacío si no es un Nodo. */
	const TCHAR* NodeEffectName(EPowerNodeEffect Effect)
	{
		switch (Effect)
		{
		case EPowerNodeEffect::Duel: return TEXT("DUELO");
		case EPowerNodeEffect::Oracle: return TEXT("ORÁCULO");
		case EPowerNodeEffect::Planar: return TEXT("PLANAR");
		case EPowerNodeEffect::None:
		default: return TEXT("");
		}
	}

	/**
	 * Etiqueta de una casilla: identificador y Dominio ("D3·41"), con el prefijo del tipo; el Altar, "ALTAR". Un Nodo de
	 * Poder enseña su efecto debajo desde el Despertar (R-TAB-9, S-009); antes, no.
	 */
	FString TileLabelText(const FBoardState& Board, const FTileState& Tile)
	{
		const FString Plain = Soulbound::Presentation::TileShortName(Board, Tile.TileId);
		switch (Tile.Type)
		{
		case ETileType::Niche: return TEXT("N ") + Plain;
		case ETileType::PowerSource:
			// La reserva no va en la etiqueta: tiene su propio contador azul sobre la Fuente (S-008).
			return TEXT("F ") + Plain;
		case ETileType::PowerNode:
			return Board.AreNodesRevealed() ? FString::Printf(TEXT("X %s\n%s"), *Plain, NodeEffectName(Tile.PowerNodeEffect)) : TEXT("X ") + Plain;
		case ETileType::Altar:
		case ETileType::Normal:
		default: return Plain;
		}
	}

	/** Tecla de la opción en la posición Index (S-010): 1 a 9, y 0 para la décima si la elección no admite "no". Vacía si no tiene. */
	FString OptionKey(const FPendingChoice& Choice, int32 Index)
	{
		if (Index < 9)
		{
			return FString::Printf(TEXT("[%d]"), Index + 1);
		}
		return Index == 9 && !Choice.CanDecline() ? FString(TEXT("[0]")) : FString(TEXT("[sin tecla]"));
	}

	/** Texto de la opción en la posición Index de la elección pendiente, para la línea de petición (S-009, S-010). */
	FString OptionText(const FMatchState& State, int32 Index)
	{
		const FPendingChoice& Choice = State.PendingChoice;
		switch (Choice.Kind)
		{
		case EChoiceKind::DiceCount: return Choice.Options[Index] > 1 ? TEXT("dos dados") : TEXT("un dado");
		case EChoiceKind::PlanarJump: return Soulbound::Presentation::TileShortName(State.Board, Choice.Options[Index]);
		case EChoiceKind::PlayerAction:
		{
			const FPlayerActionOption& Action = Choice.ActionOptions[Index];
			if (Action.Action == EPlayerAction::Duel)
			{
				// S-012: con quien comparte casilla, duelo; con alguien de otra casilla, solo desde un Nodo de Duelo.
				const bool bSameTile = State.Seats.IsValidIndex(Choice.Seat) && State.Seats.IsValidIndex(Action.TargetSeat)
					&& State.Seats[Choice.Seat].TileId == State.Seats[Action.TargetSeat].TileId;
				return bSameTile ? FString::Printf(TEXT("Duelo con el asiento %d"), Action.TargetSeat) : FString::Printf(TEXT("Retar al asiento %d (Nodo)"), Action.TargetSeat);
			}
			return FString::Printf(TEXT("%s al asiento %d"), Action.Action == EPlayerAction::Steal ? TEXT("Robar") : TEXT("Expulsar"), Action.TargetSeat);
		}
		case EChoiceKind::SafeRune: return FString::Printf(TEXT("D%d"), Choice.Options[Index]);
		case EChoiceKind::ShellPick: return FString::Printf(TEXT("esfera %d"), Index + 1);
		case EChoiceKind::Deflection: return TEXT("usar Desvío");
		// S-014: las de los eventos del Warden. El dilema enseña lo que hace cada opción.
		case EChoiceKind::Dilemma:
		{
			const int32 EventIndex = State.CurrentEvent.EventIndex;
			const FWardenEventDef* Def = State.WardenEvents.Events.IsValidIndex(EventIndex) ? &State.WardenEvents.Events[EventIndex] : nullptr;
			return Def && Def->Options.IsValidIndex(Choice.Options[Index]) ? Soulbound::Presentation::DescribeOption(Def->Options[Choice.Options[Index]])
				: FString::Printf(TEXT("opción %d"), Index + 1);
		}
		case EChoiceKind::ChosenRival: return FString::Printf(TEXT("asiento %d"), Choice.Options[Index]);
		case EChoiceKind::GiftRune:
		case EChoiceKind::LoseRune: return FString::Printf(TEXT("D%d"), Choice.Options[Index]);
		default: return FString::FromInt(Choice.AnswerAt(Index));
		}
	}

	/**
	 * Efecto del evento del Warden que espera ahora a su duelo o a su trilero (S-015): la aplicación en curso de la opción que
	 * se aplica. Falso si no hay evento esperando una acción. Solo lee el estado.
	 */
	bool EventActionEffect(const FMatchState& State, EEffect& OutEffect)
	{
		const FCurrentWardenEvent& Event = State.CurrentEvent;
		if (!Event.IsActive() || !Event.bWaitsForAction || !State.WardenEvents.Events.IsValidIndex(Event.EventIndex))
		{
			return false;
		}
		const FWardenEventDef& Def = State.WardenEvents.Events[Event.EventIndex];
		if (!Def.Options.IsValidIndex(Event.Option) || !Def.Options[Event.Option].Applications.IsValidIndex(Event.NextApplication - 1))
		{
			return false;
		}
		OutEffect = Def.Options[Event.Option].Applications[Event.NextApplication - 1].Effect;
		return true;
	}

	/**
	 * Qué se elige al señalar a un rival en el evento en curso (S-015), para la línea de petición: a quién se pone a prueba, a
	 * quién se roba, con quién se bate o, con dos rivales, quién reta y a quién. Solo lee el estado.
	 */
	FString ChosenRivalText(const FMatchState& State)
	{
		const FCurrentWardenEvent& Event = State.CurrentEvent;
		const FWardenEventDef* Def = State.WardenEvents.Events.IsValidIndex(Event.EventIndex) ? &State.WardenEvents.Events[Event.EventIndex] : nullptr;
		if (Def && Def->Options.IsValidIndex(Event.Option) && Def->Options[Event.Option].Applications.IsValidIndex(Event.NextApplication - 1))
		{
			const FEffectApplication& Application = Def->Options[Event.Option].Applications[Event.NextApplication - 1];
			if (Application.Target == EEffectTarget::TwoChosenRivals)
			{
				return Event.FirstRival == INDEX_NONE ? FString(TEXT("el primer rival, que reta al segundo"))
					: FString::Printf(TEXT("el segundo rival, al que reta el asiento %d"), Event.FirstRival);
			}
			switch (Application.Effect)
			{
			case EEffect::WardenTrial: return TEXT("a quién pones a prueba");
			case EEffect::DecreedTheft: return TEXT("a quién robas");
			case EEffect::ForcedDuel: return TEXT("con quién te bates");
			default: break;
			}
		}
		return TEXT("a quién señalas");
	}

	/**
	 * Estados del asiento Seat que dejan los efectos del Warden (S-014): Marcado y Brazal revelado, con la ronda en que
	 * caducan, para la leyenda de asientos. Vacío si no tiene ninguno. Solo lee el estado.
	 */
	FString EffectStatusText(const FMatchState& State, int32 Seat)
	{
		FString Text;
		if (State.Seats.IsValidIndex(Seat))
		{
			// S-016 (B), decisión del autor del 05/10/2026: la insolencia de cada uno, a la vista, y los castigos que esperan.
			const FSeatState& SeatState = State.Seats[Seat];
			const int32 Waiting = SeatState.PendingPunishments.Num() + SeatState.VoidPunishments.Num();
			Text += FString::Printf(TEXT(" | insolencia %d de %d"), SeatState.Insolence, Soulbound::Match::MaxInsolence);
			if (Waiting > 0)
			{
				Text += FString::Printf(TEXT(" | %d castigo%s pendiente%s"), Waiting, Waiting > 1 ? TEXT("s") : TEXT(""), Waiting > 1 ? TEXT("s") : TEXT(""));
			}
		}
		if (State.Seats.IsValidIndex(Seat) && State.Seats[Seat].bWardenTrialPending)
		{
			// S-015: la Prueba del Warden pendiente, hasta que la juegue al empezar su siguiente turno.
			Text += TEXT(" | PRUEBA DEL WARDEN");
		}
		if (Soulbound::Rules::IsMarked(State, Seat))
		{
			Text += FString::Printf(TEXT(" | MARCADO hasta la ronda %d"), State.Seats[Seat].MarkedUntilRound);
		}
		if (Soulbound::Rules::IsBracerRevealed(State, Seat))
		{
			Text += FString::Printf(TEXT(" | BRAZAL REVELADO hasta la ronda %d"), State.Seats[Seat].RevealedUntilRound);
		}
		return Text;
	}

	/**
	 * Línea del evento del Warden en curso (S-014): su texto y quién lo dirige; si es un dilema que espera, lo dice (las
	 * opciones, numeradas, van en la línea de petición). Vacía sin evento. Solo lee el estado.
	 */
	FString WardenEventLine(const FMatchState& State)
	{
		const FCurrentWardenEvent& Event = State.CurrentEvent;
		if (!Event.IsActive() || !State.WardenEvents.Events.IsValidIndex(Event.EventIndex))
		{
			return FString();
		}
		const FWardenEventDef& Def = State.WardenEvents.Events[Event.EventIndex];
		const TCHAR* Kind = Def.IsDilemma() ? (Event.Option == INDEX_NONE ? TEXT("DILEMA, elige una opción") : TEXT("DILEMA")) : TEXT("DECRETO");
		const FString Against = Event.bAgainst ? FString(TEXT(" | resuelto EN SU CONTRA")) : FString();
		return FString::Printf(TEXT("EL WARDEN (%s): \"%s\" | dirige el asiento %d%s"), Kind, *Def.Text, Event.Director, *Against);
	}

	/**
	 * Estado del Vacío del asiento Seat para la vista (S-013): "EN EL VACÍO, turno n", con n el turno dentro que juega o, si
	 * no lo está jugando, el siguiente. Vacío si no está en el Vacío. Solo lee el estado.
	 */
	FString VoidText(const FMatchState& State, int32 Seat)
	{
		if (!State.Seats.IsValidIndex(Seat) || !State.Seats[Seat].IsInVoid())
		{
			return FString();
		}
		const bool bPlayingVoidTurn = Seat == State.ActiveSeat && (State.TurnPhase == ETurnPhase::Start || State.TurnPhase == ETurnPhase::SkillTrial)
			&& State.Seats[Seat].VoidTurns > 0;
		// S-016 (B): el Vacío de la Furia, donde no funcionan Desvío ni Liberación.
		return FString::Printf(TEXT("%s, turno %d"), State.Seats[Seat].bFuryVoid ? TEXT("EN EL VACÍO DE LA FURIA (sin Desvío ni Liberación)") : TEXT("EN EL VACÍO"),
			State.Seats[Seat].VoidTurns + (bPlayingVoidTurn ? 0 : 1));
	}

	/**
	 * Línea de petición de la elección pendiente (S-009): qué se elige, las opciones numeradas como las contestan las
	 * teclas (1 a 9, y 0 para "no" o, si no se admite, la décima) y el tiempo que queda. Para el humano, en imperativo y con
	 * sus teclas; para un dummy, qué está decidiendo. Desde S-010 la elección puede ser de un asiento sin turno (Koschei
	 * víctima): bHuman es si se le pide al humano, tenga o no el turno. Solo lee el estado.
	 */
	FString ChoicePrompt(const FMatchState& State, bool bHuman)
	{
		const FPendingChoice& Choice = State.PendingChoice;
		const FString Left = FString::Printf(TEXT("quedan %.0f s"), FMath::Max(0.0, Choice.DeadlineSeconds - State.ClockSeconds));
		if (Choice.Kind == EChoiceKind::Reroll)
		{
			return bHuman ? FString::Printf(TEXT("REPETIR: [R] repite la tirada (una vez por turno), o clic en una casilla blanca | %s"), *Left)
				: FString::Printf(TEXT("el asiento %d decide si repite la tirada | %s"), Choice.Seat, *Left);
		}
		const FString RivalWhat = Choice.Kind == EChoiceKind::ChosenRival ? ChosenRivalText(State) : FString();
		const TCHAR* What = Choice.Kind == EChoiceKind::Dilemma ? TEXT("una opción del dilema")
			: Choice.Kind == EChoiceKind::ChosenRival ? *RivalWhat
			: Choice.Kind == EChoiceKind::GiftRune ? TEXT("qué runa te regala el Warden")
			: Choice.Kind == EChoiceKind::LoseRune ? TEXT("qué runa pierdes")
			: Choice.Kind == EChoiceKind::DiceCount ? TEXT("dados")
			: Choice.Kind == EChoiceKind::PlanarJump ? TEXT("salto planar")
			: Choice.Kind == EChoiceKind::SafeRune ? TEXT("la runa que Koschei pone a salvo del trilero")
			: Choice.Kind == EChoiceKind::ShellPick ? TEXT("esfera")
			: Choice.Kind == EChoiceKind::Deflection ? TEXT("si sales del Vacío con Desvío (la runa vuelve a su Fuente y juegas el turno)")
			: TEXT("accion contra un jugador");
		const TCHAR* No = Choice.Kind == EChoiceKind::PlanarJump ? TEXT("no saltar") : Choice.Kind == EChoiceKind::Deflection ? TEXT("no") : TEXT("nada");
		TArray<FString> Parts;
		for (int32 Index = 0; Index < Choice.AnswerCount(); ++Index)
		{
			Parts.Add(FString::Printf(TEXT("%s %s"), *OptionKey(Choice, Index), *OptionText(State, Index)));
		}
		if (Choice.CanDecline())
		{
			Parts.Add(FString::Printf(TEXT("[0] %s"), No));
		}
		const FString Hint = bHuman && Choice.Kind == EChoiceKind::PlanarJump ? TEXT(" (o clic en un Nodo con halo)") : TEXT("");
		return bHuman ? FString::Printf(TEXT("ELIGE %s: %s%s | %s"), What, *FString::Join(Parts, TEXT(" ")), *Hint, *Left)
			: FString::Printf(TEXT("el asiento %d elige %s: %s | %s"), Choice.Seat, What, *FString::Join(Parts, TEXT(" ")), *Left);
	}

	/**
	 * Línea del trilero en curso (S-010), sin animación (decisión del autor del 05/10/2026): quién roba a quién; durante la
	 * muestra, las runas de la víctima a la vista (y la que apartó Koschei); durante la mezcla, solo cuántas esferas; en la
	 * elección, las esferas numeradas. Con el tiempo de cada etapa. Vacía si no hay trilero. Solo lee el estado.
	 */
	FString ShellGameLine(const FMatchState& State)
	{
		const FShellGameState& Game = State.ShellGame;
		// S-015: el de un evento del Warden lo dice: el Robo decretado, o el botín de un Duelo forzado.
		EEffect EventEffect = EEffect::Halt;
		const bool bFromEvent = EventActionEffect(State, EventEffect);
		const TCHAR* Header = !bFromEvent ? TEXT("TRILERO") : EventEffect == EEffect::DecreedTheft ? TEXT("ROBO DECRETADO") : TEXT("DUELO FORZADO (botín: trilero)");
		const FString Who = FString::Printf(TEXT("%s: el asiento %d roba al asiento %d"), Header, Game.Thief, Game.Victim);
		const double StageLeft = FMath::Max(0.0, Game.StageDeadlineSeconds - State.ClockSeconds);
		switch (Game.Stage)
		{
		case EShellGameStage::SafeRune:
			return Who + TEXT(" (Koschei): antes aparta una runa, que no entra en la mezcla");
		case EShellGameStage::Reveal:
		{
			const FString Runes = FString::JoinBy(Game.Orbs, TEXT(" "), [](int32 Rune) { return FString::Printf(TEXT("D%d"), Rune); });
			const FString Aside = Game.SetAsideRune != INDEX_NONE ? FString::Printf(TEXT(" (a salvo: D%d)"), Game.SetAsideRune) : FString();
			return FString::Printf(TEXT("%s | runas a la vista: %s%s | muestra: quedan %.0f s"), *Who, *Runes, *Aside, StageLeft);
		}
		case EShellGameStage::Shuffle:
			return FString::Printf(TEXT("%s | mezclando %d esferas | quedan %.0f s"), *Who, Game.Orbs.Num(), StageLeft);
		case EShellGameStage::Pick:
		{
			TArray<FString> Orbs;
			for (int32 Index = 0; Index < Game.Orbs.Num(); ++Index)
			{
				Orbs.Add(FString::Printf(TEXT("(%d)"), Index + 1));
			}
			return FString::Printf(TEXT("%s | esferas: %s | el ladron elige a ciegas"), *Who, *FString::Join(Orbs, TEXT(" ")));
		}
		case EShellGameStage::None:
		default:
			return FString();
		}
	}

	/**
	 * Línea del duelo (S-012): mientras dura, quién reta a quién, de dónde sale el reto y quién ha rendido ya; resuelto, el
	 * resultado del último duelo del turno en curso, hasta que acabe. Vacía si el turno no tiene duelo. Solo lee el estado.
	 */
	FString DuelLine(const FMatchState& State, int32 HumanSeat)
	{
		const FDuelState& Duel = State.Duel;
		auto Who = [HumanSeat](int32 Seat) { return Seat == HumanSeat ? FString::Printf(TEXT("%d (tú)"), Seat) : FString::FromInt(Seat); };
		if (Duel.IsActive())
		{
			auto Status = [&Duel](int32 Seat) { return Duel.HasPerformed(Seat) ? TEXT("ha rendido") : TEXT("rindiendo"); };
			// S-015: el Duelo contra el Warden (rinde un valor fijo) y el Duelo forzado de un evento, entre turnos.
			if (Duel.bAgainstWarden)
			{
				return FString::Printf(TEXT("DUELO CONTRA EL WARDEN: el asiento %s se bate contra el Warden (rinde %.2f) por %s | %s | resuelto al azar (sin minijuego aún)"),
					*Who(Duel.Challenged), Duel.ChallengerPerformance, Duel.bAllRunes ? TEXT("todas sus runas") : TEXT("una de sus runas"), Status(Duel.Challenged));
			}
			if (State.CurrentEvent.IsActive() && State.CurrentEvent.bWaitsForAction)
			{
				return FString::Printf(TEXT("DUELO FORZADO: el asiento %s reta al asiento %s por %s | retador %s, retado %s | resuelto al azar (sin minijuego aún)"),
					*Who(Duel.Challenger), *Who(Duel.Challenged), Duel.bAllRunes ? TEXT("todas sus runas") : TEXT("una runa"),
					Status(Duel.Challenger), Status(Duel.Challenged));
			}
			return FString::Printf(TEXT("DUELO: el asiento %s reta al asiento %s%s | retador %s, retado %s | resuelto al azar (sin minijuego aún)"),
				*Who(Duel.Challenger), *Who(Duel.Challenged), Duel.bFromDuelNode ? TEXT(" desde un Nodo de Duelo") : TEXT(""),
				Status(Duel.Challenger), Status(Duel.Challenged));
		}
		if (State.TurnPhase == ETurnPhase::BetweenTurns)
		{
			return FString();
		}
		// El último resultado, si es de este turno: nada desde el último TurnStarted.
		for (int32 Index = State.EventLog.Num() - 1; Index >= 0; --Index)
		{
			const FMatchEvent& Event = State.EventLog[Index];
			if (Event.Type == EMatchEventType::TurnStarted)
			{
				break;
			}
			if (Event.Type == EMatchEventType::DuelWon)
			{
				return FString::Printf(TEXT("DUELO: gana el asiento %s al asiento %s, siendo el %s (resuelto al azar)"), *Who(Event.Seat), *Who(Event.OtherSeat),
					Event.Value == 1 ? TEXT("retado") : TEXT("retador"));
			}
		}
		return FString();
	}

	/** Nombre del Elegido del Dominio, de elegidos.json (S-008); vacío si el Dominio no tiene Elegido. */
	FString ElectName(const FMatchState& State, int32 DomainId)
	{
		return State.Elects.IsValidDomain(DomainId) ? State.Elects.Elects[DomainId].Name : FString();
	}

	/** "El Banquete · Shuten-dōji": el nombre del Dominio (del tablero) y el de su Elegido (de elegidos.json). */
	FString DomainTitle(const FMatchState& State, int32 DomainId)
	{
		const FString Domain = State.Board.Domains.IsValidIndex(DomainId) ? State.Board.Domains[DomainId].Name : FString();
		const FString Elect = ElectName(State, DomainId);
		return Elect.IsEmpty() ? Domain : FString::Printf(TEXT("%s · %s"), *Domain, *Elect);
	}

	const TCHAR* PowerName(EPower Power)
	{
		switch (Power)
		{
		case EPower::Duel: return TEXT("Duelo");
		case EPower::Haste: return TEXT("Celeridad");
		case EPower::FreePassage: return TEXT("Paso Libre");
		case EPower::Expel: return TEXT("Expulsión");
		case EPower::PlanarFlight: return TEXT("Vuelo Planar");
		case EPower::Deflection: return TEXT("Desvío");
		}
		return TEXT("?");
	}

	/**
	 * Brazal de un asiento (S-008, solo lectura): sus runas por tipo (D<n>×cantidad, solo las que tiene) y sus poderes
	 * activos. Paso Libre por afinidad (Fenrir) se marca aparte, porque no depende de ninguna runa.
	 */
	FString BracerText(const FMatchState& State, int32 Seat)
	{
		const FSeatState& SeatState = State.Seats[Seat];
		TArray<FString> Runes;
		for (int32 RuneType = 0; RuneType < SeatState.Runes.Num(); ++RuneType)
		{
			if (SeatState.Runes[RuneType] > 0)
			{
				Runes.Add(FString::Printf(TEXT("D%d×%d"), RuneType, SeatState.Runes[RuneType]));
			}
		}
		TArray<FString> Powers;
		for (const EPower Power : Soulbound::Rules::GetActivePowers(State, Seat))
		{
			Powers.Add(PowerName(Power));
		}
		if (Soulbound::Rules::MakeMoverTraits(State, Seat).bFreePassage && !Soulbound::Rules::IsPowerActive(State, Seat, EPower::FreePassage))
		{
			Powers.Add(TEXT("Paso Libre (afinidad)"));
		}
		return FString::Printf(TEXT("%d: runas %s | poderes %s"), Seat,
			Runes.IsEmpty() ? TEXT("-") : *FString::Join(Runes, TEXT(" ")), Powers.IsEmpty() ? TEXT("-") : *FString::Join(Powers, TEXT(", ")));
	}
}

ABoardViewActor::ABoardViewActor()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(Soulbound::Presentation::CubePath);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(Soulbound::Presentation::CylinderPath);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(Soulbound::Presentation::SpherePath);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Material(Soulbound::Presentation::BasicMaterialPath);
	CubeMesh = Cube.Succeeded() ? Cube.Object : nullptr;
	CylinderMesh = Cylinder.Succeeded() ? Cylinder.Object : nullptr;
	SphereMesh = Sphere.Succeeded() ? Sphere.Object : nullptr;
	BaseMaterial = Material.Succeeded() ? Material.Object : nullptr;

	// Seis tonos a 60 grados de tono entre sí, del mismo brillo medio, para distinguirse a un metro.
	DomainColors = {
		FLinearColor(0.03f, 0.15f, 0.90f), // 0 azul
		FLinearColor(0.85f, 0.03f, 0.03f), // 1 rojo
		FLinearColor(0.04f, 0.60f, 0.08f), // 2 verde
		FLinearColor(0.75f, 0.05f, 0.60f), // 3 magenta
		FLinearColor(0.03f, 0.60f, 0.70f), // 4 cian
		FLinearColor(0.80f, 0.65f, 0.02f), // 5 amarillo
	};
	// Claros y algo lavados: la ficha se distingue de la casilla saturada que pisa.
	SeatColors = {
		FLinearColor(0.40f, 0.70f, 1.00f), // 0 celeste
		FLinearColor(1.00f, 0.55f, 0.75f), // 1 rosa
		FLinearColor(0.60f, 1.00f, 0.40f), // 2 lima
		FLinearColor(1.00f, 0.95f, 0.60f), // 3 crema
	};
}

FLinearColor ABoardViewActor::DomainColor(int32 DomainId) const
{
	return DomainColors.IsValidIndex(DomainId) ? DomainColors[DomainId] : FLinearColor(0.1f, 0.1f, 0.1f);
}

FLinearColor ABoardViewActor::SeatColor(int32 Seat) const
{
	return SeatColors.IsValidIndex(Seat) ? SeatColors[Seat] : FLinearColor::White;
}

void ABoardViewActor::Initialize(UMatchHostSubsystem* InHost)
{
	Host = InHost;
	if (!InHost || !InHost->HasMatch())
	{
		return;
	}

	TilePositions = ComputeTilePositions();
	BuildEdges();
	BuildTiles();
	BuildTokens();
	BuildCamera();
	BuildPostProcess();
	BuildOverlay();
	Refresh();

	ChangedHandle = InHost->OnMatchStateChanged.AddUObject(this, &ABoardViewActor::Refresh);
	GetWorldTimerManager().SetTimer(HudTimer, this, &ABoardViewActor::TickHud, FMath::Max(HudIntervalSeconds, 0.05f), true, 0.f);
}

void ABoardViewActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UMatchHostSubsystem* HostPtr = Host.Get())
	{
		HostPtr->OnMatchStateChanged.Remove(ChangedHandle);
	}
	GetWorldTimerManager().ClearTimer(HudTimer);
	if (Overlay.IsValid())
	{
		if (UWorld* World = GetWorld())
		{
			if (UGameViewportClient* Viewport = World->GetGameViewport())
			{
				Viewport->RemoveViewportWidgetContent(Overlay.ToSharedRef());
			}
		}
		Overlay.Reset();
	}
	Super::EndPlay(EndPlayReason);
}

TArray<FVector> ABoardViewActor::ComputeTilePositions() const
{
	const FBoardState& Board = Host->GetState().Board;
	TArray<FVector> Positions;
	Positions.SetNum(Board.Tiles.Num());

	const bool bAllPositioned = Board.Tiles.FindByPredicate([](const FTileState& Tile) { return !Tile.bHasLayoutPosition; }) == nullptr;
	if (bAllPositioned)
	{
		// El editor dibuja con la Y hacia abajo; la cámara cenital tiene el +X del mundo hacia arriba de la pantalla.
		FVector2D Min(TNumericLimits<double>::Max(), TNumericLimits<double>::Max());
		FVector2D Max(TNumericLimits<double>::Lowest(), TNumericLimits<double>::Lowest());
		for (const FTileState& Tile : Board.Tiles)
		{
			Min = FVector2D(FMath::Min(Min.X, Tile.LayoutPosition.X), FMath::Min(Min.Y, Tile.LayoutPosition.Y));
			Max = FVector2D(FMath::Max(Max.X, Tile.LayoutPosition.X), FMath::Max(Max.Y, Tile.LayoutPosition.Y));
		}
		const FVector2D Center = (Min + Max) * 0.5;
		for (const FTileState& Tile : Board.Tiles)
		{
			Positions[Tile.TileId] = FVector(-(Tile.LayoutPosition.Y - Center.Y) * PositionScale, (Tile.LayoutPosition.X - Center.X) * PositionScale, 0.0);
		}
		return Positions;
	}

	// Sin posiciones en el archivo: el Altar en el centro y cada Dominio en un sector del anillo, en filas de cinco.
	constexpr int32 PerRow = 5;
	constexpr float FirstRowRadius = 600.f;
	constexpr float RowSpacing = 350.f;
	constexpr float AngleStepDegrees = 12.f;
	TArray<int32> IndexInDomain;
	IndexInDomain.Init(0, Soulbound::Board::DomainCount);
	for (const FTileState& Tile : Board.Tiles)
	{
		if (Tile.DomainId == INDEX_NONE)
		{
			Positions[Tile.TileId] = FVector::ZeroVector;
			continue;
		}
		const int32 K = IndexInDomain[Tile.DomainId]++;
		const float Angle = FMath::DegreesToRadians(Tile.DomainId * (360.f / Soulbound::Board::DomainCount) + ((K % PerRow) - PerRow / 2) * AngleStepDegrees);
		const float Radius = FirstRowRadius + (K / PerRow) * RowSpacing;
		Positions[Tile.TileId] = FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0.f);
	}
	return Positions;
}

void ABoardViewActor::BuildEdges()
{
	const FBoardState& Board = Host->GetState().Board;
	for (const FTileState& Tile : Board.Tiles)
	{
		for (const int32 Other : Tile.Neighbours)
		{
			if (Other <= Tile.TileId)
			{
				continue; // cada conexión una sola vez
			}
			const FVector A = TilePositions[Tile.TileId];
			const FVector B = TilePositions[Other];
			const FVector Delta = B - A;

			UStaticMeshComponent* Strip = NewObject<UStaticMeshComponent>(this);
			Strip->SetStaticMesh(CubeMesh);
			Strip->SetupAttachment(GetRootComponent());
			Strip->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Strip->SetCastShadow(false);
			Strip->RegisterComponent();
			// Una forma básica estirada entre las dos casillas (el cubo del motor mide 100 cm).
			Strip->SetWorldLocation((A + B) * 0.5f + FVector(0.f, 0.f, EdgeThickness * 0.5f));
			Strip->SetWorldRotation(FRotator(0.f, FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X)), 0.f));
			Strip->SetWorldScale3D(FVector(Delta.Size() / 100.f, EdgeWidth / 100.f, EdgeThickness / 100.f));
			Soulbound::Presentation::SetComponentColor(*Strip, BaseMaterial, FLinearColor(0.04f, 0.04f, 0.05f));
			EdgeStrips.Add(Strip);
		}
	}
}

void ABoardViewActor::BuildTiles()
{
	const FBoardState& Board = Host->GetState().Board;
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	TileTops.Init(TileThickness, Board.Tiles.Num());
	Inlays.Init(nullptr, Board.Tiles.Num());
	InlayOutlines.Init(nullptr, Board.Tiles.Num());
	InlayOutlinesB.Init(nullptr, Board.Tiles.Num());
	Halos.Init(nullptr, Board.Tiles.Num());
	ForgottenRings.Init(nullptr, Board.Tiles.Num());
	NicheNames.Init(nullptr, Board.Tiles.Num());
	NicheNumbers.Init(nullptr, Board.Tiles.Num());
	SourceReserves.Init(nullptr, Board.Tiles.Num());
	NicheRingFirst.Init(INDEX_NONE, Board.Tiles.Num());
	InlayHoles.Init(nullptr, Board.Tiles.Num());
	Labels.Init(nullptr, Board.Tiles.Num());

	// Las marcas y las etiquetas son componentes de la vista, en coordenadas del mundo: la losa se aplasta y se
	// escala, y lo que colgara de ella se deformaría con ella.
	auto AddShape = [this](UStaticMesh* Mesh, const FVector& Location, const FRotator& Rotation, const FVector& Size)
	{
		UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(this);
		Component->SetStaticMesh(Mesh);
		Component->SetupAttachment(GetRootComponent());
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetCastShadow(false);
		Component->RegisterComponent();
		Component->SetWorldLocation(Location);
		Component->SetWorldRotation(Rotation);
		Component->SetWorldScale3D(Size / 100.f);
		return Component;
	};

	for (const FTileState& Tile : Board.Tiles)
	{
		const FVector Position = TilePositions[Tile.TileId];
		const bool bAltar = Tile.Type == ETileType::Altar;
		const float Thickness = bAltar ? AltarThickness : TileThickness;
		const float Diameter = TileDiameter * (bAltar ? AltarScale : 1.f);
		TileTops[Tile.TileId] = Thickness;

		// El tipo se ve por la silueta: Nicho cuadrado, Nodo en rombo, Altar grande y alto, el resto redondo.
		FRotator Rotation = FRotator::ZeroRotator;
		float Width = Diameter;
		if (Tile.Type == ETileType::PowerNode)
		{
			Rotation = FRotator(0.f, 45.f, 0.f);
			Width = Diameter * 0.8f;
		}
		else if (Tile.Type == ETileType::Niche)
		{
			Width = Diameter * 0.9f;
		}
		// El cilindro del motor mide 100 cm de diámetro y 100 de alto: se aplasta a una losa.
		const FTransform Transform(Rotation, Position + FVector(0.f, 0.f, Thickness * 0.5f), FVector(Width / 100.f, Width / 100.f, Thickness / 100.f));
		ABoardTileActor* Actor = GetWorld()->SpawnActor<ABoardTileActor>(ABoardTileActor::StaticClass(), Transform, Params);
		if (Actor)
		{
			Actor->SetTileId(Tile.TileId);
			Actor->SetSquareShape(Tile.Type == ETileType::Niche || Tile.Type == ETileType::PowerNode);
			Actor->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
		}
		TileActors.Add(Actor);

		// Halo de "alcanzable": un disco por debajo de la losa, más ancho, que solo se ve en el borde. La losa
		// conserva su color; el halo se enciende cuando la tirada la permite.
		UStaticMeshComponent* Halo = AddShape(CylinderMesh, Position + FVector(0.f, 0.f, 2.f), FRotator::ZeroRotator, FVector(Diameter * HaloScale, Diameter * HaloScale, 4.f));
		Soulbound::Presentation::SetComponentColor(*Halo, BaseMaterial, ReachableColor);
		Halo->SetVisibility(false);
		Halos[Tile.TileId] = Halo;

		// Aro de Dominio Olvidado: más alto que el halo, para que se vea también en una casilla alcanzable.
		UStaticMeshComponent* Ring = AddShape(CylinderMesh, Position + FVector(0.f, 0.f, 3.f), FRotator::ZeroRotator, FVector(Diameter * ForgottenRingScale, Diameter * ForgottenRingScale, 6.f));
		Soulbound::Presentation::SetComponentColor(*Ring, BaseMaterial, ForgottenRingColor);
		Ring->SetVisibility(false);
		ForgottenRings[Tile.TileId] = Ring;

		// Marca del tipo sobre la losa, del tamaño justo para no tapar la ficha del centro, con contorno negro
		// para que se distinga sobre cualquier Dominio.
		const float Inlay = TileDiameter * InlayScale;
		const float Outline = Inlay * OutlineScale;
		const FVector OnTop = Position + FVector(0.f, 0.f, Thickness);
		const FVector Mid = OnTop + FVector(0.f, 0.f, InlayHeight * 0.5f);
		switch (Tile.Type)
		{
		case ETileType::Niche:
			InlayOutlines[Tile.TileId] = AddShape(CubeMesh, OnTop + FVector(0.f, 0.f, InlayHeight * 0.4f), FRotator::ZeroRotator, FVector(Outline, Outline, InlayHeight * 0.8f));
			Inlays[Tile.TileId] = AddShape(CubeMesh, Mid, FRotator::ZeroRotator, FVector(Inlay, Inlay, InlayHeight));
			break;
		case ETileType::PowerSource:
		{
			// Cruz: dos brazos cruzados, con contorno negro. No se parece al rombo con esfera del Nodo.
			const float Arm = Inlay * CrossArmThickness;
			const float OutlineArm = Arm + (Outline - Inlay);
			const FVector LowMid = OnTop + FVector(0.f, 0.f, InlayHeight * 0.4f);
			InlayOutlines[Tile.TileId] = AddShape(CubeMesh, LowMid, FRotator::ZeroRotator, FVector(Outline, OutlineArm, InlayHeight * 0.8f));
			InlayOutlinesB[Tile.TileId] = AddShape(CubeMesh, LowMid, FRotator::ZeroRotator, FVector(OutlineArm, Outline, InlayHeight * 0.8f));
			Inlays[Tile.TileId] = AddShape(CubeMesh, Mid, FRotator::ZeroRotator, FVector(Inlay, Arm, InlayHeight));
			InlayHoles[Tile.TileId] = AddShape(CubeMesh, Mid, FRotator::ZeroRotator, FVector(Arm, Inlay, InlayHeight));
			break;
		}
		case ETileType::PowerNode:
			InlayOutlines[Tile.TileId] = AddShape(SphereMesh, OnTop + FVector(0.f, 0.f, InlayHeight * 0.4f), FRotator::ZeroRotator, FVector(Outline, Outline, InlayHeight * 1.6f));
			Inlays[Tile.TileId] = AddShape(SphereMesh, Mid, FRotator::ZeroRotator, FVector(Inlay, Inlay, InlayHeight * 2.f));
			break;
		default:
			break;
		}
		if (UStaticMeshComponent* OutlineMesh = InlayOutlines[Tile.TileId].Get())
		{
			Soulbound::Presentation::SetComponentColor(*OutlineMesh, BaseMaterial, OutlineColor);
		}
		if (UStaticMeshComponent* OutlineMeshB = InlayOutlinesB[Tile.TileId].Get())
		{
			Soulbound::Presentation::SetComponentColor(*OutlineMeshB, BaseMaterial, OutlineColor);
		}

		// Etiqueta plana sobre la losa, en la franja de abajo (la parte baja de la pantalla), orientada a la cámara cenital.
		UTextRenderComponent* Label = NewObject<UTextRenderComponent>(this);
		Label->SetupAttachment(GetRootComponent());
		Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Label->SetCastShadow(false);
		Label->RegisterComponent();
		Label->SetHorizontalAlignment(EHTA_Center);
		Label->SetVerticalAlignment(EVRTA_TextCenter);
		Label->SetWorldSize(LabelSize);
		Label->SetTextRenderColor(FColor::White);
		// El texto del motor se ve desde +X: tumbado con el frente hacia arriba (pitch 90) y la cabeza hacia +X, arriba
		// de la pantalla (yaw 180). Con pitch -90 se veía por detrás, como en un espejo.
		Label->SetWorldRotation(FRotator(90.f, 180.f, 0.f));
		Label->SetWorldLocation(OnTop + FVector(-Diameter * LabelOffsetFraction * (bAltar ? 1.f : 1.f), 0.f, InlayHeight + 3.f));
		Label->SetText(FText::FromString(TileLabelText(Board, Tile)));
		Labels[Tile.TileId] = Label;

		if (Tile.Type == ETileType::PowerSource)
		{
			// Contador de reserva en el centro de la Fuente, en azul: se distingue de la etiqueta blanca (S-008).
			UTextRenderComponent* Reserve = NewObject<UTextRenderComponent>(this);
			Reserve->SetupAttachment(GetRootComponent());
			Reserve->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Reserve->SetCastShadow(false);
			Reserve->RegisterComponent();
			Reserve->SetHorizontalAlignment(EHTA_Center);
			Reserve->SetVerticalAlignment(EVRTA_TextCenter);
			Reserve->SetWorldSize(NicheNumberSize);
			Reserve->SetTextRenderColor(SourceReserveColor);
			Reserve->SetWorldRotation(FRotator(90.f, 180.f, 0.f));
			Reserve->SetWorldLocation(OnTop + FVector(0.f, 0.f, InlayHeight + 3.f));
			Reserve->SetText(FText::GetEmpty());
			SourceReserves[Tile.TileId] = Reserve;
		}

		if (Tile.Type == ETileType::Niche)
		{
			// Nombre del Elegido (de elegidos.json) arriba de la casilla; número del asiento sobre el cubo.
			auto AddText = [this, &OnTop](float Size, const FVector& Offset)
			{
				UTextRenderComponent* Text = NewObject<UTextRenderComponent>(this);
				Text->SetupAttachment(GetRootComponent());
				Text->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Text->SetCastShadow(false);
				Text->RegisterComponent();
				Text->SetHorizontalAlignment(EHTA_Center);
				Text->SetVerticalAlignment(EVRTA_TextCenter);
				Text->SetWorldSize(Size);
				Text->SetTextRenderColor(FColor::White);
				Text->SetWorldRotation(FRotator(90.f, 180.f, 0.f));
				Text->SetWorldLocation(OnTop + Offset);
				return Text;
			};
			NicheNames[Tile.TileId] = AddText(NicheNameSize, FVector(Diameter * NicheNameOffsetFraction, 0.f, InlayHeight + 3.f));
			NicheNames[Tile.TileId]->SetText(FText::FromString(ElectName(Host->GetState(), Tile.DomainId)));
			NicheNumbers[Tile.TileId] = AddText(NicheNumberSize, FVector(0.f, 0.f, InlayHeight + 3.f));
			NicheNumbers[Tile.TileId]->SetText(FText::GetEmpty());

			// Aro del asiento: segmentos en círculo alrededor de la casilla, apagados hasta que se reclama.
			const float RingRadius = TileDiameter * NicheRingRadiusScale;
			const float SegmentLength = 2.f * PI * RingRadius / NicheRingSegmentCount * 1.15f;
			NicheRingFirst[Tile.TileId] = NicheRingSegments.Num();
			for (int32 Index = 0; Index < NicheRingSegmentCount; ++Index)
			{
				const float Angle = 2.f * PI * Index / NicheRingSegmentCount;
				UStaticMeshComponent* Segment = AddShape(CubeMesh, Position + FVector(FMath::Cos(Angle) * RingRadius, FMath::Sin(Angle) * RingRadius, 3.f),
					FRotator(0.f, FMath::RadiansToDegrees(Angle) + 90.f, 0.f), FVector(SegmentLength, NicheRingWidth, 6.f));
				Soulbound::Presentation::SetComponentColor(*Segment, BaseMaterial, FLinearColor::White);
				Segment->SetVisibility(false);
				NicheRingSegments.Add(Segment);
			}
		}
	}
}

void ABoardViewActor::BuildTokens()
{
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	for (int32 Seat = 0; Seat < Host->GetState().Seats.Num(); ++Seat)
	{
		ASeatTokenActor* Token = GetWorld()->SpawnActor<ASeatTokenActor>(ASeatTokenActor::StaticClass(),
			FTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector(TokenDiameter / 100.f)), Params);
		if (Token)
		{
			Token->SetSeat(Seat);
			Token->Configure(TokenLabelSize, ActiveLift, ActiveSpinDegreesPerSecond, RingDiameter, RingWidth, ActiveScale);
			Token->SetColor(SeatColor(Seat));
			Token->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
		}
		TokenActors.Add(Token);
	}
}

void ABoardViewActor::BuildCamera()
{
	FVector2D Min(TNumericLimits<double>::Max(), TNumericLimits<double>::Max());
	FVector2D Max(TNumericLimits<double>::Lowest(), TNumericLimits<double>::Lowest());
	for (const FVector& Position : TilePositions)
	{
		Min = FVector2D(FMath::Min(Min.X, Position.X), FMath::Min(Min.Y, Position.Y));
		Max = FVector2D(FMath::Max(Max.X, Position.X), FMath::Max(Max.Y, Position.Y));
	}
	BoardCenter = (Min + Max) * 0.5;
	BoardHalf = (Max - Min) * 0.5 + FVector2D(TileDiameter * AltarScale * 0.5f + RingDiameter * 0.1f);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Camera = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform(FRotator(-90.f, 0.f, 0.f), FVector(BoardCenter.X, BoardCenter.Y, 1000.0)), Params);
	if (Camera)
	{
		Camera->GetCameraComponent()->SetFieldOfView(60.f);
		Camera->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
	}
	UpdateCamera();
}

void ABoardViewActor::UpdateCamera()
{
	if (!Camera)
	{
		return;
	}
	FVector2D Viewport(1280.0, 720.0);
	if (const UGameViewportClient* Client = GetWorld()->GetGameViewport())
	{
		Client->GetViewportSize(Viewport);
	}
	if (Viewport.X < 1.0 || Viewport.Y < 1.0)
	{
		return;
	}
	LastViewportSize = Viewport;

	// Cámara cenital fija: el tablero entero, con el margen CameraMargin, en el hueco que dejan los paneles a la derecha.
	// Con FOV horizontal de 60 grados, a la altura H se ve media anchura H*tan(30); la media altura es esa entre el aspecto.
	constexpr float Fov = 60.f;
	const float Aspect = Viewport.X / Viewport.Y;
	const float HalfTan = FMath::Tan(FMath::DegreesToRadians(Fov * 0.5f));
	const float FreeFraction = 1.f - FMath::Clamp(PanelWidthPixels / static_cast<float>(Viewport.X), 0.f, 0.6f);
	const float Height = CameraMargin * FMath::Max(BoardHalf.Y / (FreeFraction * HalfTan), BoardHalf.X * Aspect / HalfTan);
	// La pantalla crece hacia +Y a la derecha: para que el tablero quede a la izquierda, la cámara se corre hacia +Y.
	const float VisibleHalfWidth = Height * HalfTan;
	const float ShiftY = VisibleHalfWidth * (1.f - FreeFraction);
	Camera->SetActorLocation(FVector(BoardCenter.X, BoardCenter.Y + ShiftY, Height));
}

void ABoardViewActor::BuildPostProcess()
{
	// Exposición manual fija en un volumen sin límites, generado por código: el nivel del spike lleva la suya
	// automática y los colores salían pastel. Prioridad alta para ganar a cualquier otro volumen del mapa.
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	PostProcess = GetWorld()->SpawnActor<APostProcessVolume>(APostProcessVolume::StaticClass(), FTransform::Identity, Params);
	if (PostProcess)
	{
		PostProcess->bUnbound = true;
		PostProcess->Priority = 1000.f;
		PostProcess->Settings.bOverride_AutoExposureMethod = true;
		PostProcess->Settings.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
		PostProcess->Settings.bOverride_AutoExposureBias = true;
		PostProcess->Settings.AutoExposureBias = ExposureEV100;
	}
}

void ABoardViewActor::BuildOverlay()
{
	UGameViewportClient* Viewport = GetWorld()->GetGameViewport();
	if (!Viewport)
	{
		return;
	}
	const FBoardState& Board = Host->GetState().Board;

	// La leyenda sale de los mismos colores que la vista: no pueden contradecirse.
	TArray<FLegendRow> Rows;
	FLegendRow Domains;
	Domains.Title = TEXT("Dominios y su Elegido (color de la casilla, D<n> en la etiqueta). Fuera = Olvidado");
	TWeakObjectPtr<UMatchHostSubsystem> WeakHost = Host;
	for (int32 Id = 0; Id < Board.Domains.Num(); ++Id)
	{
		FLegendChip Chip;
		Chip.Color = DomainColor(Id);
		// Al día: fuera de la partida (Olvidado), en juego, o en juego con el asiento que reclamó su Nicho.
		Chip.TextGetter = [WeakHost, Id]() -> FString
		{
			const UMatchHostSubsystem* HostPtr = WeakHost.Get();
			if (!HostPtr || !HostPtr->HasMatch() || !HostPtr->GetState().Board.Domains.IsValidIndex(Id))
			{
				return FString::Printf(TEXT("D%d"), Id);
			}
			const FDomainState& Domain = HostPtr->GetState().Board.Domains[Id];
			if (Domain.State == EDomainState::Forgotten)
			{
				return FString::Printf(TEXT("D%d %s: OLVIDADO (fuera)"), Id, *DomainTitle(HostPtr->GetState(), Id));
			}
			return Domain.OwnerSeat == INDEX_NONE
				? FString::Printf(TEXT("D%d %s: en juego"), Id, *DomainTitle(HostPtr->GetState(), Id))
				: FString::Printf(TEXT("D%d %s: en juego, asiento %d"), Id, *DomainTitle(HostPtr->GetState(), Id), Domain.OwnerSeat);
		};
		Domains.Chips.Add(Chip);
	}
	Rows.Add(Domains);
	Rows.Add({TEXT("Tipo = forma (etiqueta: ALTAR, N, F, X)"),
		{{AltarColor, TEXT("ALTAR: casilla grande y alta")},
			{NicheFreeColor, TEXT("N Nicho: casilla cuadrada con cubo")},
			{SourceColor, TEXT("F Fuente: casilla redonda con cruz")},
			{NodeColor, TEXT("X Nodo: rombo con esfera")}}});
	Rows.Add({TEXT("Nicho (el cubo)"),
		{{NicheFreeColor, TEXT("Libre")}, {SeatColor(0), TEXT("Reclamado: aro y número del asiento")}, {NicheClosedColor, TEXT("Cerrado")}}});
	Rows.Add({TEXT("Estado de la casilla"),
		{{ReachableColor, TEXT("alcanzable: halo blanco bajo la casilla")},
			{SealedColor, TEXT("sellada (SELLO)")},
			{ForgottenRingColor, TEXT("Dominio Olvidado: casillas apagadas con aro gris oscuro")}}});
	// Una fila por asiento, con su rol al día: Eco (esfera) o Elegido (cono), y el Dominio que lleva.
	FLegendRow Seats;
	Seats.Title = TEXT("Asientos (el número va en la ficha): esfera = Eco, cono = Elegido. El activo crece, sube, gira y lleva aro");
	for (int32 Seat = 0; Seat < Host->GetState().Seats.Num(); ++Seat)
	{
		FLegendChip Chip;
		Chip.Color = SeatColor(Seat);
		Chip.TextGetter = [WeakHost, Seat]() -> FString
		{
			const UMatchHostSubsystem* HostPtr = WeakHost.Get();
			if (!HostPtr || !HostPtr->HasMatch() || !HostPtr->GetState().Seats.IsValidIndex(Seat))
			{
				return FString::Printf(TEXT("%d"), Seat);
			}
			const FSeatState& SeatState = HostPtr->GetState().Seats[Seat];
			const TCHAR* Who = Seat == HostPtr->GetHumanSeat() ? TEXT("humano") : TEXT("dummy");
			const FString DomainName = DomainTitle(HostPtr->GetState(), SeatState.DomainId);
			// S-013: la ficha desterrada no está en el tablero; aquí se dice dónde está.
			const FString Void = VoidText(HostPtr->GetState(), Seat);
			// S-014: Marcado y Brazal revelado van detrás del Vacío.
			const FString InVoid = (Void.IsEmpty() ? FString() : TEXT(" | ") + Void) + EffectStatusText(HostPtr->GetState(), Seat);
			if (SeatState.Role == ESeatRole::Elect)
			{
				return FString::Printf(TEXT("%d (%s): ELEGIDO, D%d %s%s"), Seat, Who, SeatState.DomainId, *DomainName, *InVoid);
			}
			return SeatState.DomainId == INDEX_NONE
				? FString::Printf(TEXT("%d (%s): ECO%s"), Seat, Who, *InVoid)
				: FString::Printf(TEXT("%d (%s): ECO, hogar en D%d %s%s"), Seat, Who, SeatState.DomainId, *DomainName, *InVoid);
		};
		Seats.Chips.Add(Chip);
	}
	Rows.Add(Seats);
	// Brazal de cada asiento (S-008): runas por tipo y poderes activos, al día.
	FLegendRow Bracers;
	Bracers.Title = TEXT("Brazal por asiento: runas (D<n>×cantidad, una por Dominio) y poderes activos. Fuente: F, con su reserva");
	for (int32 Seat = 0; Seat < Host->GetState().Seats.Num(); ++Seat)
	{
		FLegendChip Chip;
		Chip.Color = SeatColor(Seat);
		Chip.TextGetter = [WeakHost, Seat]() -> FString
		{
			const UMatchHostSubsystem* HostPtr = WeakHost.Get();
			if (!HostPtr || !HostPtr->HasMatch() || !HostPtr->GetState().Seats.IsValidIndex(Seat))
			{
				return FString::Printf(TEXT("%d"), Seat);
			}
			return BracerText(HostPtr->GetState(), Seat);
		};
		Bracers.Chips.Add(Chip);
	}
	Rows.Add(Bracers);

	ChroniclePanel = SNew(SChroniclePanel).FontSize(PanelFontSize);
	Overlay = SNew(SBox)
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Fill)
		.Padding(FMargin(0.f, 8.f, 8.f, 8.f))
		.Visibility(EVisibility::SelfHitTestInvisible)
		[
			SNew(SBox)
			.WidthOverride(PanelWidthPixels)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SLegendPanel).FontSize(PanelFontSize).Rows(Rows)
				]
				+ SVerticalBox::Slot().FillHeight(1.f).Padding(0.f, 6.f, 0.f, 0.f)
				[
					ChroniclePanel.ToSharedRef()
				]
			]
		];
	Viewport->AddViewportWidgetContent(Overlay.ToSharedRef(), 10);
}

void ABoardViewActor::RefreshChronicle(const FMatchState& State)
{
	if (!ChroniclePanel.IsValid() || State.EventLog.Num() == ChronicleLineCount)
	{
		return;
	}
	ChronicleLineCount = State.EventLog.Num();

	// La más reciente primero; se conservan solo las últimas ChronicleMaxLines.
	TArray<FChronicleLine> Lines;
	const int32 First = FMath::Max(0, State.EventLog.Num() - FMath::Max(ChronicleMaxLines, 1));
	for (int32 Index = State.EventLog.Num() - 1; Index >= First; --Index)
	{
		const FMatchEvent& Event = State.EventLog[Index];
		FLinearColor Color = SeatColor(Event.Seat);
		switch (Soulbound::Presentation::EventSource(Event.Type))
		{
		case Soulbound::Presentation::EEventSource::Warden: Color = WardenLineColor; break;
		case Soulbound::Presentation::EEventSource::Race: Color = RaceLineColor; break;
		case Soulbound::Presentation::EEventSource::Debug: Color = DebugLineColor; break;
		case Soulbound::Presentation::EEventSource::Seat: break;
		}
		// S-016: las evaluaciones del Warden que no hacen nada no tienen línea (una cada 15 s taparía la crónica).
		const FString Text = Soulbound::Presentation::DescribeEvent(Event, State.Board, &State.WardenEvents);
		if (!Text.IsEmpty())
		{
			Lines.Add({FString::Printf(TEXT("[%s] %s"), *Stamp(Event.ClockSeconds), *Text), Color});
		}
	}
	ChroniclePanel->SetLines(Lines);
}

void ABoardViewActor::Refresh()
{
	const UMatchHostSubsystem* HostPtr = Host.Get();
	if (!HostPtr || !HostPtr->HasMatch())
	{
		return;
	}
	const FMatchState& State = HostPtr->GetState();

	// Alcanzables: las que guarda el estado, calculadas por el motor (ReachableTiles) al tirar. Solo valen mientras
	// se espera el destino. Con un salto planar pendiente (S-009), el halo marca los Nodos a los que se puede saltar.
	TSet<int32> Reachable;
	if (State.TurnPhase == ETurnPhase::RollAndChoose)
	{
		Reachable.Append(State.ReachableOptions);
	}
	else if (State.PendingChoice.Kind == EChoiceKind::PlanarJump)
	{
		Reachable.Append(State.PendingChoice.Options);
	}

	for (const FTileState& Tile : State.Board.Tiles)
	{
		ABoardTileActor* Actor = TileActors.IsValidIndex(Tile.TileId) ? TileActors[Tile.TileId].Get() : nullptr;
		if (!Actor)
		{
			continue;
		}
		const FDomainState* Domain = State.Board.Domains.IsValidIndex(Tile.DomainId) ? &State.Board.Domains[Tile.DomainId] : nullptr;
		const bool bForgotten = Domain && Domain->State == EDomainState::Forgotten;
		const float Dim = bForgotten ? ForgottenDim : 1.f;

		// La losa lleva el color del Dominio (el del tipo, en el Altar); la marca de encima, el del tipo.
		FLinearColor Base = Tile.Type == ETileType::Altar ? AltarColor : Dimmed(DomainColor(Tile.DomainId), Dim);
		if (Tile.bSealed)
		{
			Base = SealedColor;
		}
		Actor->SetColor(Base);
		if (UStaticMeshComponent* ForgottenRing = ForgottenRings.IsValidIndex(Tile.TileId) ? ForgottenRings[Tile.TileId].Get() : nullptr)
		{
			ForgottenRing->SetVisibility(bForgotten);
		}
		if (UStaticMeshComponent* Halo = Halos.IsValidIndex(Tile.TileId) ? Halos[Tile.TileId].Get() : nullptr)
		{
			Halo->SetVisibility(Reachable.Contains(Tile.TileId));
		}

		UStaticMeshComponent* Inlay = Inlays.IsValidIndex(Tile.TileId) ? Inlays[Tile.TileId].Get() : nullptr;
		if (Inlay)
		{
			FLinearColor Mark = Base;
			switch (Tile.Type)
			{
			case ETileType::Niche:
				// El cubo no cambia de color por quien lo reclama: el Dominio se sigue viendo. Libre y Reclamado, el color de
				// tipo; Cerrado, gris oscuro. El Reclamado lleva el aro y el número del asiento (más abajo).
				Mark = Domain && Domain->NicheState == ENicheState::Closed ? NicheClosedColor : NicheFreeColor;
				break;
			case ETileType::PowerSource:
				// Agotada (S-008): la cruz se apaga; viva, el color del tipo.
				Mark = Dimmed(SourceColor, State.Board.Phase == EBoardPhase::Awakened && Tile.RuneReserve <= 0 ? Dim * SourceDepletedDim : Dim);
				break;
			case ETileType::PowerNode: Mark = Dimmed(NodeColor, Dim); break;
			default: break;
			}
			Soulbound::Presentation::SetComponentColor(*Inlay, BaseMaterial, Mark);
		}
		if (UStaticMeshComponent* Hole = InlayHoles.IsValidIndex(Tile.TileId) ? InlayHoles[Tile.TileId].Get() : nullptr)
		{
			// Segundo brazo de la cruz de la Fuente: el mismo color que el primero.
			const bool bDepleted = State.Board.Phase == EBoardPhase::Awakened && Tile.RuneReserve <= 0;
			Soulbound::Presentation::SetComponentColor(*Hole, BaseMaterial, Dimmed(SourceColor, bDepleted ? Dim * SourceDepletedDim : Dim));
		}
		if (Tile.Type == ETileType::Niche && NicheRingFirst.IsValidIndex(Tile.TileId) && NicheRingFirst[Tile.TileId] != INDEX_NONE)
		{
			const bool bClaimed = Domain && Domain->NicheState == ENicheState::Claimed;
			const FLinearColor RingColor = bClaimed ? SeatColor(Domain->OwnerSeat) : FLinearColor::White;
			const int32 First = NicheRingFirst[Tile.TileId];
			for (int32 Index = First; Index < First + NicheRingSegmentCount && NicheRingSegments.IsValidIndex(Index); ++Index)
			{
				Soulbound::Presentation::SetComponentColor(*NicheRingSegments[Index], BaseMaterial, RingColor);
				NicheRingSegments[Index]->SetVisibility(bClaimed);
			}
			if (UTextRenderComponent* Number = NicheNumbers[Tile.TileId].Get())
			{
				Number->SetText(bClaimed ? FText::AsNumber(Domain->OwnerSeat) : FText::GetEmpty());
			}
		}
		if (UTextRenderComponent* Reserve = SourceReserves.IsValidIndex(Tile.TileId) ? SourceReserves[Tile.TileId].Get() : nullptr)
		{
			// Desde el Despertar: lo que queda sobre lo que tenía. Agotada: 0/n, y la cruz y la etiqueta se apagan.
			Reserve->SetText(State.Board.Phase == EBoardPhase::Awakened
				? FText::FromString(FString::Printf(TEXT("%d/%d"), Tile.RuneReserve, State.Board.Params.RuneReservePerSource))
				: FText::GetEmpty());
		}
		if (UTextRenderComponent* Label = Labels.IsValidIndex(Tile.TileId) ? Labels[Tile.TileId].Get() : nullptr)
		{
			const FString Text = TileLabelText(State.Board, Tile) + (Tile.bSealed ? TEXT("\nSELLO") : TEXT(""));
			Label->SetText(FText::FromString(Text));
			const bool bDepletedSource = Tile.Type == ETileType::PowerSource && State.Board.Phase == EBoardPhase::Awakened && Tile.RuneReserve <= 0;
			Label->SetTextRenderColor(Tile.bSealed ? FColor::Red : bDepletedSource ? FColor(130, 130, 130) : FColor::White);
		}
	}

	// Asiento activo: solo mientras dura su turno; entre turnos no juega nadie.
	const bool bSomeoneActive = !State.IsMatchOver() && State.TurnPhase != ETurnPhase::BetweenTurns;

	// Fichas: sobre su casilla y, si comparten casilla, repartidas en un círculo pequeño.
	for (int32 Seat = 0; Seat < State.Seats.Num() && Seat < TokenActors.Num(); ++Seat)
	{
		ASeatTokenActor* Token = TokenActors[Seat];
		const int32 TileId = State.Seats[Seat].TileId;
		if (!Token)
		{
			continue;
		}
		// S-013 (R-TAB-24): en el Vacío la ficha sale del tablero; se oculta y reaparece en su hogar al salir.
		Token->SetActorHiddenInGame(State.Seats[Seat].IsInVoid());
		if (!TilePositions.IsValidIndex(TileId))
		{
			continue;
		}
		int32 Sharing = 0;
		int32 MyIndex = 0;
		for (int32 Other = 0; Other < State.Seats.Num(); ++Other)
		{
			if (State.Seats[Other].TileId == TileId)
			{
				MyIndex = Other == Seat ? Sharing : MyIndex;
				++Sharing;
			}
		}
		FVector Offset = FVector::ZeroVector;
		if (Sharing > 1)
		{
			const float Angle = 2.f * PI * MyIndex / Sharing;
			Offset = FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * (TileDiameter * 0.28f);
		}
		Token->MoveTo(TilePositions[TileId] + Offset + FVector(0.f, 0.f, TileTops[TileId] + TokenDiameter * 0.5f), TokenSlideSeconds);
		// Elegido: cono; Eco: esfera (criterio manual "Vínculo visible").
		Token->SetElectShape(State.Seats[Seat].Role == ESeatRole::Elect);
		Token->SetActive(bSomeoneActive && Seat == State.ActiveSeat);
	}

	RefreshChronicle(State);
}

void ABoardViewActor::ShowText(int32 Line, const FColor& Color, const FString& Text) const
{
	if (GEngine)
	{
		// Se sustituye por clave en cada refresco; si la vista deja de refrescar, el mensaje caduca a los dos intervalos.
		GEngine->AddOnScreenDebugMessage(TextKeyBase + Line, HudIntervalSeconds * 2.f + 0.1f, Color, Text);
	}
}

void ABoardViewActor::TickHud()
{
	const UMatchHostSubsystem* HostPtr = Host.Get();
	if (!HostPtr || !HostPtr->HasMatch())
	{
		return;
	}
	const FMatchState& State = HostPtr->GetState();

	// La cámara es la vista del jugador local. Se pone al iniciar la partida, o en cuanto exista el controlador.
	if (Camera)
	{
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* Controller = It->Get();
			if (Controller && Controller->IsLocalController() && Controller->GetViewTarget() != Camera)
			{
				Controller->SetViewTarget(Camera);
			}
		}
		// Si la ventana cambia de tamaño, el tablero se recoloca en el hueco de los paneles.
		if (const UGameViewportClient* Client = GetWorld()->GetGameViewport())
		{
			FVector2D Viewport;
			Client->GetViewportSize(Viewport);
			if (!Viewport.Equals(LastViewportSize, 0.5))
			{
				UpdateCamera();
			}
		}
	}

	const bool bWaiting = State.TurnPhase == ETurnPhase::RollAndChoose;
	const bool bHumanTurn = bWaiting && State.ActiveSeat == HostPtr->GetHumanSeat();
	const bool bStepWait = HostPtr->IsWaitingForStep();
	const int32 TurnNumber = State.TurnPhase == ETurnPhase::BetweenTurns ? State.TurnCount : State.TurnCount + 1;

	// La línea del turno lleva el color del asiento que juega; entre turnos, blanca (no juega nadie).
	const bool bSomeoneActive = !State.IsMatchOver() && State.TurnPhase != ETurnPhase::BetweenTurns;
	ShowText(0, bSomeoneActive ? SeatColor(State.ActiveSeat).ToFColor(true) : FColor::White,
		FString::Printf(TEXT("Turno %d | juega: asiento %d (%s) | fase: %s"), TurnNumber, State.ActiveSeat,
			State.ActiveSeat == HostPtr->GetHumanSeat() ? TEXT("humano") : TEXT("dummy"), PhaseName(State.TurnPhase)));

	// S-015: un duelo o un trilero de un evento se juega entre turnos; el asiento activo es a quien le toca después.
	if (State.CurrentEvent.IsActive() && State.CurrentEvent.bWaitsForAction && !State.IsMatchOver())
	{
		ShowText(0, FColor::Orange, FString::Printf(TEXT("Entre turnos | evento del Warden en curso | fase: %s | el siguiente turno es del asiento %d"),
			PhaseName(State.TurnPhase), State.ActiveSeat));
	}

	if (State.TurnPhase == ETurnPhase::SkillTrial && State.PendingTrial == ESkillTrial::VoidEscape)
	{
		// Escape del Vacío (S-013): el minijuego aún no existe; el humano lo resuelve al azar, o como fije SbTrial.
		ShowText(1, FColor::Yellow, FString::Printf(TEXT("Escape del Vacío: el asiento %d juega en %s (%s) y espera el resultado (resuelto al azar, o con SbTrial)"),
			State.ActiveSeat, *Soulbound::Presentation::DifficultyText(static_cast<int32>(State.PendingDifficulty)), *VoidText(State, State.ActiveSeat)));
	}
	else if (State.TurnPhase == ETurnPhase::SkillTrial && State.PendingTrial == ESkillTrial::WardenTrial)
	{
		// Prueba del Warden (S-015): el minijuego aún no existe; el humano lo resuelve al azar, o como fije SbTrial.
		ShowText(1, FColor::Yellow, FString::Printf(TEXT("PRUEBA DEL WARDEN: el asiento %d juega en %s antes de tirar y espera el resultado (resuelta al azar, o con SbTrial)"),
			State.ActiveSeat, *Soulbound::Presentation::DifficultyText(static_cast<int32>(State.PendingDifficulty))));
	}
	else if (State.TurnPhase == ETurnPhase::SkillTrial)
	{
		// Minijuego de la Fuente de un Olvidado (S-008): aún no existe; el decisor del asiento responde al azar.
		ShowText(1, FColor::Yellow, FString::Printf(TEXT("Minijuego de la Fuente: el asiento %d espera el resultado (resuelto al azar, probabilidad 0,5)"), State.ActiveSeat));
	}
	else
	{
	const FString Clock = bWaiting ? FString::Printf(TEXT("%.0f s"), FMath::Max(0.0, State.TurnDeadlineSeconds - State.ClockSeconds)) : FString(TEXT("-"));
	ShowText(1, bHumanTurn ? FColor::Green : FColor::White, FString::Printf(TEXT("Tirada: %d%s | reloj de turno: %s%s"),
		State.Roll, State.DiceCount > 1 ? TEXT(" (dos dados)") : TEXT(""), *Clock, bHumanTurn ? TEXT("  <- tu turno: clic en una casilla blanca") : TEXT("")));
	}

	// Elección pendiente (S-009): la línea de petición, en verde si es del humano (teclas 1 a 9 y 0; R para repetir). Desde
	// S-010, también cuando se le pide al humano sin tener el turno (Koschei víctima).
	if (State.PendingChoice.IsOpen() && !State.IsMatchOver())
	{
		const bool bHumanChoice = State.PendingChoice.Seat == HostPtr->GetHumanSeat();
		ShowText(8, bHumanChoice ? FColor::Green : FColor(190, 190, 190), ChoicePrompt(State, bHumanChoice));
	}

	// Trilero (S-010): quién roba a quién y qué se ve en cada etapa; las esferas, numeradas y sin animación.
	if (State.ShellGame.IsActive() && !State.IsMatchOver())
	{
		ShowText(9, FColor::Magenta, ShellGameLine(State));
	}

	// Evento del Warden (S-014): su texto mientras se resuelve, entre turnos; las opciones del dilema van en la petición.
	const FString EventText = State.IsMatchOver() ? FString() : WardenEventLine(State);
	if (!EventText.IsEmpty())
	{
		ShowText(11, FColor::Orange, EventText);
	}

	// Noche de los Ecos (S-014): mientras dura, bien visible.
	if (State.IsEchoNight() && !State.IsMatchOver())
	{
		ShowText(12, FColor(150, 120, 255), FString::Printf(TEXT("NOCHE DE LOS ECOS: los Ecos caminan por cualquier Dominio (como mucho hasta la ronda %d)"),
			State.EchoNightUntilRound));
	}

	// Duelo (S-012): el duelo en curso y, resuelto, su resultado mientras dura el turno.
	const FString DuelText = State.IsMatchOver() ? FString() : DuelLine(State, HostPtr->GetHumanSeat());
	if (!DuelText.IsEmpty())
	{
		ShowText(10, FColor::Orange, DuelText);
	}

	// Paso a paso (S-007): lo que espera, bien visible.
	if (bStepWait)
	{
		ShowText(2, FColor::Yellow, TEXT("PASO A PASO: espacio para el siguiente turno"));
	}
	else if (HostPtr->IsStepMode())
	{
		ShowText(2, FColor(200, 200, 90), TEXT("PASO A PASO activo: la partida se detendra al acabar este turno"));
	}

	const FString Sealed = State.Board.SealedTiles.IsEmpty()
		? FString(TEXT("ninguno"))
		: FString::JoinBy(State.Board.SealedTiles, TEXT(", "), [](int32 Id) { return FString::FromInt(Id); });
	ShowText(3, FColor::Red, FString::Printf(TEXT("Sellos activos: %d (casillas: %s)"), State.Board.SealedTiles.Num(), *Sealed));

	// S-016: el Warden, en una línea: acto, segundos hasta la siguiente evaluación y desde la última intervención, y su modo.
	const UWardenHostSubsystem* WardenHost = GetWorld() ? GetWorld()->GetSubsystem<UWardenHostSubsystem>() : nullptr;
	const bool bWardenOn = WardenHost && WardenHost->GetMode() == EWardenHostMode::Auto;
	static const TCHAR* const ActNames[] = {TEXT("I"), TEXT("II"), TEXT("III")};
	const int32 Act = static_cast<int32>(Soulbound::Rules::CurrentAct(State, State.ClockSeconds));
	const FString Next = !State.WardenParams.HasWarden() ? FString(TEXT("sin Warden"))
		: !bWardenOn ? FString(TEXT("apagado (SbWardenMode auto)"))
		: State.Warden.bDeferred ? FString(TEXT("evaluación aplazada hasta que acabe lo que está en curso"))
		// Vencida (la partida detenida en el paso a paso, que no evalúa): evaluará en cuanto siga.
		: State.Warden.NextEvaluationSeconds <= State.ClockSeconds ? FString(TEXT("siguiente evaluación: pendiente"))
		: FString::Printf(TEXT("siguiente evaluación en %.0f s"), State.Warden.NextEvaluationSeconds - State.ClockSeconds);
	ShowText(4, FColor::Orange, FString::Printf(TEXT("Warden: Acto %s | %s | última intervención hace %.0f s%s"), ActNames[FMath::Clamp(Act, 0, 2)], *Next,
		State.ClockSeconds - State.Warden.LastEffectSeconds, State.Warden.PendingScene != INDEX_NONE ? TEXT(" | una escena espera al hueco") : TEXT("")));

	ShowText(5, FColor::Silver, FString::Printf(TEXT("Semilla: %d (fijala con FixedSeed para repetir la partida) | t=%.0f s | ronda %d"), HostPtr->GetSeed(), State.ClockSeconds,
		State.Round));

	// Carrera por el vínculo (S-006): tiempo que queda, fin, o victoria del Warden; y el rol y el hogar de cada asiento.
	FString Race;
	FColor RaceColor = FColor::Cyan;
	if (State.Outcome == EMatchOutcome::WardenVictory)
	{
		Race = TEXT("VICTORIA DEL WARDEN: nadie se ha vinculado. La partida ha terminado.");
		RaceColor = FColor::Red;
	}
	else if (State.MatchPhase == EMatchPhase::BondRace)
	{
		const double Left = State.Params.RaceDurationSeconds - State.ClockSeconds;
		Race = Left > 0.0
			? FString::Printf(TEXT("Carrera por el vinculo: quedan %.0f s"), Left)
			: FString(TEXT("Carrera por el vinculo: tiempo agotado, termina al acabar este turno"));
	}
	else
	{
		const int32 Elects = State.Seats.FilterByPredicate([](const FSeatState& Seat) { return Seat.Role == ESeatRole::Elect; }).Num();
		Race = FString::Printf(TEXT("FIN DE LA CARRERA: %d Elegidos. Los Ecos quedan confinados a los Dominios Olvidados."), Elects);
	}
	ShowText(6, RaceColor, Race);

	TArray<FString> Roles;
	for (int32 Seat = 0; Seat < State.Seats.Num(); ++Seat)
	{
		const FSeatState& SeatState = State.Seats[Seat];
		const FString Void = VoidText(State, Seat);
		Roles.Add((SeatState.Role == ESeatRole::Elect
			? FString::Printf(TEXT("%d Elegido (Dominio %d)"), Seat, SeatState.DomainId)
			: SeatState.DomainId == INDEX_NONE
				? FString::Printf(TEXT("%d Eco"), Seat)
				: FString::Printf(TEXT("%d Eco, hogar: Nicho del Dominio %d"), Seat, SeatState.DomainId))
			+ (Void.IsEmpty() ? FString() : TEXT(", ") + Void) + EffectStatusText(State, Seat));
	}
	ShowText(7, FColor::Cyan, FString::Printf(TEXT("Asientos: %s"), *FString::Join(Roles, TEXT(" | "))));
}
