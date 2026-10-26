#include "BoardViewActor.h"

#include "../Match/MatchHostSubsystem.h"
#include "../Warden/WardenHostSubsystem.h"
#include "BoardTileActor.h"
#include "BoardViewState.h"
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
#include "Components/InputComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"

DEFINE_LOG_CATEGORY_STATIC(LogSoulboundView, Log, All);

namespace
{
	// Medidas de las formas de respaldo que no son parámetros (de S-005 a S-007), por 6,3/175 desde S-019: la mesa mide lo
	// que mide el kit. El grosor de la losa es LayoutParams.SlabTop.
	constexpr float EdgeThickness = 0.15f;
	/** Alto de las marcas de tipo sobre la losa. */
	constexpr float InlayHeight = 0.22f;
	/** Holgura de los textos sobre las marcas. */
	constexpr float TextClearance = 0.1f;
	/** Altura y alto del halo de alcanzable y del aro de Olvidado, bajo la losa. */
	constexpr float HaloZ = 0.07f;
	constexpr float HaloHeight = 0.15f;
	constexpr float ForgottenRingZ = 0.1f;
	constexpr float ForgottenRingHeight = 0.22f;
	/** Segmentos del aro de un Nicho reclamado. */
	constexpr int32 NicheRingSegmentCount = 24;

	/** Ruta inicial de una pieza o un material del kit, según kit.json (R-VIS-8). */
	FSoftObjectPath KitPath(const TCHAR* Name)
	{
		return FSoftObjectPath(FString::Printf(TEXT("/Game/Art/Tablero/%s.%s"), Name, Name));
	}

	/** Clave base de los mensajes de pantalla de esta vista: una línea por clave, se sustituyen al refrescar. */
	constexpr uint64 TextKeyBase = 0x53420500;
	/** Líneas de texto de pantalla de esta vista (claves TextKeyBase + 0 a + 12), para quitarlas todas a la vez. */
	constexpr int32 DebugTextLineCount = 13;

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
		case ETurnPhase::Moving: return TEXT("movimiento, por el camino");
		case ETurnPhase::MoveAndResolve: return TEXT("llegada y casilla");
		case ETurnPhase::SkillTrial: return TEXT("minijuego (Fuente, escape del Vacío o Prueba del Warden)");
		case ETurnPhase::ShellGame: return TEXT("trilero");
		case ETurnPhase::Duel: return TEXT("duelo");
		case ETurnPhase::End: return TEXT("fin de turno");
		case ETurnPhase::OracleConsult: return TEXT("consulta del Oráculo");
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

	// Piezas y materiales del kit (S-019, R-VIS-8), con los nombres de kit.json. Config los sustituye si DefaultGame.ini los trae.
	SlabMesh = KitPath(TEXT("SM_losa"));
	CrackedSlabMesh = KitPath(TEXT("SM_losa_partida"));
	NicheSlabMesh = KitPath(TEXT("SM_losa_nicho"));
	SourceSlabMesh = KitPath(TEXT("SM_losa_fuente"));
	NodeSlabMesh = KitPath(TEXT("SM_losa_nodo"));
	PathSegmentMesh = KitPath(TEXT("SM_sendero"));
	PedestalMesh = KitPath(TEXT("SM_peana"));
	PlainFloorMesh = KitPath(TEXT("SM_suelo_liso"));
	AltarMesh = KitPath(TEXT("SM_altar"));
	NicheThroneMesh = KitPath(TEXT("SM_trono"));
	PylonMesh = KitPath(TEXT("SM_pilon"));
	FloatingRuneMesh = KitPath(TEXT("SM_runa_flotante"));
	PlanarArchMesh = KitPath(TEXT("SM_nodo_planar"));
	DuelSwordsMesh = KitPath(TEXT("SM_nodo_duelo"));
	OracleOrbMesh = KitPath(TEXT("SM_nodo_oraculo"));
	SealChainsMesh = KitPath(TEXT("SM_sello"));
	TokenCardMesh = KitPath(TEXT("SM_ficha_prueba"));
	VertexColorMaterial = KitPath(TEXT("M_ColorVertice"));
	EmissiveMaterial = KitPath(TEXT("M_Emisivo"));
	DimMaterial = KitPath(TEXT("M_Tenue"));

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

	// S-019: la colocación es una función pura del tablero; la vista solo la lee.
	LoadKit();
	Layout = Soulbound::Presentation::ComputeBoardLayout(InHost->GetState().Board, LayoutParams);
	if (!Layout.bRadial)
	{
		UE_LOG(LogSoulboundView, Warning, TEXT("Vista del tablero: el trazado no es radial (dos Dominios en el mismo sector o uno repartido en más de 60 grados); las cuñas van de 60 en 60 por identificador de Dominio."));
	}
	TilePositions.Reset(Layout.Tiles.Num());
	for (const FTileLayout& Tile : Layout.Tiles)
	{
		TilePositions.Add(Tile.Location);
	}
	BuildEdges();
	BuildTiles();
	BuildKit();
	BuildTokens();
	BuildCamera();
	BuildLighting();
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

const TSoftObjectPtr<UStaticMesh>& ABoardViewActor::KitReference(EBoardKitPiece Piece, const TCHAR*& OutName) const
{
	switch (Piece)
	{
	case EBoardKitPiece::Slab: OutName = TEXT("losa"); return SlabMesh;
	case EBoardKitPiece::CrackedSlab: OutName = TEXT("losa partida"); return CrackedSlabMesh;
	case EBoardKitPiece::NicheSlab: OutName = TEXT("losa del Nicho"); return NicheSlabMesh;
	case EBoardKitPiece::SourceSlab: OutName = TEXT("losa de la Fuente"); return SourceSlabMesh;
	case EBoardKitPiece::NodeSlab: OutName = TEXT("losa del Nodo"); return NodeSlabMesh;
	case EBoardKitPiece::PathSegment: OutName = TEXT("sendero"); return PathSegmentMesh;
	case EBoardKitPiece::Pedestal: OutName = TEXT("peana"); return PedestalMesh;
	case EBoardKitPiece::PlainFloor: OutName = TEXT("suelo liso"); return PlainFloorMesh;
	case EBoardKitPiece::Altar: OutName = TEXT("Altar"); return AltarMesh;
	case EBoardKitPiece::NicheThrone: OutName = TEXT("trono"); return NicheThroneMesh;
	case EBoardKitPiece::Pylon: OutName = TEXT("pilón"); return PylonMesh;
	case EBoardKitPiece::FloatingRune: OutName = TEXT("runa flotante"); return FloatingRuneMesh;
	case EBoardKitPiece::PlanarArch: OutName = TEXT("arco Planar"); return PlanarArchMesh;
	case EBoardKitPiece::DuelSwords: OutName = TEXT("espadas de Duelo"); return DuelSwordsMesh;
	case EBoardKitPiece::OracleOrb: OutName = TEXT("orbe del Oráculo"); return OracleOrbMesh;
	case EBoardKitPiece::SealChains: OutName = TEXT("cadenas del Sello"); return SealChainsMesh;
	case EBoardKitPiece::TokenCard:
	default: OutName = TEXT("carta de la ficha"); return TokenCardMesh;
	}
}

UStaticMesh* ABoardViewActor::KitMesh(EBoardKitPiece Piece) const
{
	const int32 Index = static_cast<int32>(Piece);
	return KitMeshes.IsValidIndex(Index) ? KitMeshes[Index].Get() : nullptr;
}

EBoardKitPiece ABoardViewActor::SlabPiece(EBoardSlab Slab)
{
	switch (Slab)
	{
	case EBoardSlab::Cracked: return EBoardKitPiece::CrackedSlab;
	case EBoardSlab::Niche: return EBoardKitPiece::NicheSlab;
	case EBoardSlab::PowerSource: return EBoardKitPiece::SourceSlab;
	case EBoardSlab::PowerNode: return EBoardKitPiece::NodeSlab;
	case EBoardSlab::Altar: return EBoardKitPiece::Altar;
	case EBoardSlab::Normal:
	default: return EBoardKitPiece::Slab;
	}
}

EBoardKitPiece ABoardViewActor::StructurePiece(EBoardStructure Structure)
{
	switch (Structure)
	{
	case EBoardStructure::NicheThrone: return EBoardKitPiece::NicheThrone;
	case EBoardStructure::Pylon: return EBoardKitPiece::Pylon;
	case EBoardStructure::PlanarArch: return EBoardKitPiece::PlanarArch;
	case EBoardStructure::DuelSwords: return EBoardKitPiece::DuelSwords;
	case EBoardStructure::OracleOrb: return EBoardKitPiece::OracleOrb;
	case EBoardStructure::None:
	default: return EBoardKitPiece::Count;
	}
}

void ABoardViewActor::LoadKit()
{
	// R-VIS-8: cada pieza que falte se avisa una vez, aquí, al iniciar; la vista usa la forma básica de hoy para ese elemento.
	KitMeshes.Init(nullptr, static_cast<int32>(EBoardKitPiece::Count));
	int32 Missing = 0;
	for (int32 Index = 0; Index < KitMeshes.Num(); ++Index)
	{
		const EBoardKitPiece Piece = static_cast<EBoardKitPiece>(Index);
		const TCHAR* Name = TEXT("");
		const TSoftObjectPtr<UStaticMesh>& Reference = KitReference(Piece, Name);
		if (Reference.IsNull())
		{
			// Vacía: no hay pieza. Con el trono es una variante de diseño ("el Nicho es el trono"), no un fallo.
			if (Piece != EBoardKitPiece::NicheThrone)
			{
				UE_LOG(LogSoulboundView, Warning, TEXT("Vista del tablero: la pieza %s no está configurada; se usa la forma básica."), Name);
				++Missing;
			}
			continue;
		}
		KitMeshes[Index] = Reference.LoadSynchronous();
		if (!KitMeshes[Index])
		{
			UE_LOG(LogSoulboundView, Warning, TEXT("Vista del tablero: la pieza %s (%s) no existe; se usa la forma básica."), Name, *Reference.ToString());
			++Missing;
		}
	}
	bNicheThroneConfigured = !NicheThroneMesh.IsNull();
	if (!bNicheThroneConfigured)
	{
		UE_LOG(LogSoulboundView, Log, TEXT("Vista del tablero: sin trono configurado; el Nicho no lleva estructura."));
	}

	auto LoadMaterial = [&Missing](const TSoftObjectPtr<UMaterialInterface>& Reference, const TCHAR* Name) -> UMaterialInterface*
	{
		UMaterialInterface* Material = Reference.IsNull() ? nullptr : Reference.LoadSynchronous();
		if (!Material)
		{
			UE_LOG(LogSoulboundView, Warning, TEXT("Vista del tablero: falta el material %s (%s); las piezas llevan el suyo."), Name, *Reference.ToString());
			++Missing;
		}
		return Material;
	};
	KitVertexColorMaterial = LoadMaterial(VertexColorMaterial, TEXT("de color por vértice"));
	KitEmissiveMaterial = LoadMaterial(EmissiveMaterial, TEXT("emisivo"));
	KitDimMaterial = LoadMaterial(DimMaterial, TEXT("tenue"));
	if (Missing > 0)
	{
		UE_LOG(LogSoulboundView, Warning, TEXT("Vista del tablero: faltan %d piezas o materiales del kit; el tablero usa formas básicas donde faltan."), Missing);
	}
}

void ABoardViewActor::ApplyKitMaterials(UStaticMeshComponent& Component) const
{
	const UStaticMesh* Mesh = Component.GetStaticMesh();
	if (!Mesh)
	{
		return;
	}
	const TArray<FStaticMaterial>& Slots = Mesh->GetStaticMaterials();
	for (int32 Index = 0; Index < FMath::Max(1, Slots.Num()); ++Index)
	{
		const FString Slot = Slots.IsValidIndex(Index) ? Slots[Index].MaterialSlotName.ToString() : FString();
		UMaterialInterface* Material = !EmissiveSlotTag.IsEmpty() && Slot.Contains(EmissiveSlotTag) ? KitEmissiveMaterial.Get()
			: !DimSlotTag.IsEmpty() && Slot.Contains(DimSlotTag) ? KitDimMaterial.Get()
			: KitVertexColorMaterial.Get();
		if (Material)
		{
			Component.SetMaterial(Index, Material);
		}
	}
}

UStaticMeshComponent* ABoardViewActor::AddKitPiece(UStaticMesh* Mesh, const FVector& Location, float Yaw, bool bVisible)
{
	UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(this);
	Component->SetStaticMesh(Mesh);
	Component->SetupAttachment(GetRootComponent());
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// Las piezas del kit no se escalan (R-VIS-1), salvo el suelo de la cuña (BuildKit): un metro del kit son 100 cm.
	Component->SetRelativeLocationAndRotation(Location, FRotator(0.f, Yaw, 0.f));
	Component->SetVisibility(bVisible);
	ApplyKitMaterials(*Component);
	Component->RegisterComponent();
	return Component;
}

void ABoardViewActor::BuildKit()
{
	const FBoardState& Board = Host->GetState().Board;

	// R-VIS-2: la peana y el suelo liso de cada Dominio, girados a su sector, con el centro en el Altar y a la escala del suelo.
	for (const FDomainWedgeLayout& Wedge : Layout.Wedges)
	{
		for (const EBoardKitPiece Piece : {EBoardKitPiece::Pedestal, EBoardKitPiece::PlainFloor})
		{
			if (UStaticMesh* Mesh = KitMesh(Piece))
			{
				UStaticMeshComponent* Floor = AddKitPiece(Mesh, FVector::ZeroVector, Wedge.Yaw, true);
				// R-VIS-1: el suelo de la cuña se escala en X e Y al radio de la mesa; el resto del kit, no.
				Floor->SetRelativeScale3D(FVector(Layout.KitScale, Layout.KitScale, 1.f));
				KitFixedPieces.Add(Floor);
			}
		}
	}

	// R-VIS-3: una losa por casilla, una malla instanciada por pieza; el Altar es su pieza propia.
	TMap<EBoardKitPiece, UInstancedStaticMeshComponent*> Instances;
	auto InstancesOf = [this, &Instances](EBoardKitPiece Piece) -> UInstancedStaticMeshComponent*
	{
		if (UInstancedStaticMeshComponent** Found = Instances.Find(Piece))
		{
			return *Found;
		}
		UInstancedStaticMeshComponent* Component = NewObject<UInstancedStaticMeshComponent>(this);
		Component->SetStaticMesh(KitMesh(Piece));
		Component->SetupAttachment(GetRootComponent());
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ApplyKitMaterials(*Component);
		Component->RegisterComponent();
		KitInstances.Add(Component);
		Instances.Add(Piece, Component);
		return Component;
	};
	for (const FTileState& Tile : Board.Tiles)
	{
		const FTileLayout& Placed = Layout.Tiles[Tile.TileId];
		const EBoardKitPiece Piece = SlabPiece(Placed.Slab);
		if (!TileHasKitSlab.IsValidIndex(Tile.TileId) || !TileHasKitSlab[Tile.TileId])
		{
			continue; // la forma básica de BuildTiles hace de respaldo
		}
		if (Placed.Slab == EBoardSlab::Altar)
		{
			// R-VIS-1: el Altar crece con el kit, en X, Y y Z.
			UStaticMeshComponent* Altar = AddKitPiece(KitMesh(Piece), Placed.Location, Placed.Yaw, true);
			Altar->SetRelativeScale3D(FVector(Layout.KitScale));
			KitFixedPieces.Add(Altar);
		}
		else
		{
			InstancesOf(Piece)->AddInstance(FTransform(FRotator(0.f, Placed.Yaw, 0.f), Placed.Location), false);
		}
	}

	// R-VIS-4: los tramos de sendero de cada conexión, en una sola malla instanciada.
	if (KitMesh(EBoardKitPiece::PathSegment))
	{
		UInstancedStaticMeshComponent* Paths = InstancesOf(EBoardKitPiece::PathSegment);
		for (const FPathSegmentLayout& Segment : Layout.PathSegments)
		{
			Paths->AddInstance(FTransform(FRotator(0.f, Segment.Yaw, 0.f), Segment.Location), false);
		}
	}

	// R-VIS-5: estructuras, runas flotantes y cadenas, ocultas hasta que el estado de vista las pide (RefreshKit).
	Structures.Init(nullptr, Board.Tiles.Num());
	FloatingRunes.Init(nullptr, Board.Tiles.Num());
	SealChains.Init(nullptr, Board.Tiles.Num());
	for (const FTileState& Tile : Board.Tiles)
	{
		const FTileLayout& Placed = Layout.Tiles[Tile.TileId];
		if (UStaticMesh* Mesh = KitMesh(StructurePiece(Placed.Structure)))
		{
			Structures[Tile.TileId] = AddKitPiece(Mesh, Placed.StructureLocation, Placed.StructureYaw, false);
		}
		if (Placed.Structure == EBoardStructure::Pylon && KitMesh(EBoardKitPiece::FloatingRune))
		{
			FloatingRunes[Tile.TileId] = AddKitPiece(KitMesh(EBoardKitPiece::FloatingRune), Placed.StructureLocation + FVector(0.f, 0.f, LayoutParams.FloatingRuneHeight),
				Placed.StructureYaw, false);
		}
		// El Altar no se sella (glosario "Sello"); las cadenas, sobre la cara superior de la losa.
		if (Tile.Type != ETileType::Altar && KitMesh(EBoardKitPiece::SealChains))
		{
			SealChains[Tile.TileId] = AddKitPiece(KitMesh(EBoardKitPiece::SealChains), Placed.Location + FVector(0.f, 0.f, LayoutParams.SlabTop), Placed.Yaw, false);
		}
	}
}

void ABoardViewActor::BuildEdges()
{
	// S-019: con la pieza del sendero, los tramos del kit (BuildKit); la tira básica es el respaldo.
	if (KitMesh(EBoardKitPiece::PathSegment))
	{
		return;
	}
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
	TileTops.Init(LayoutParams.SlabTop, Board.Tiles.Num());
	TileHasKitSlab.Init(false, Board.Tiles.Num());
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
		// S-019: si está la pieza de la losa (o del Altar), la forma básica queda oculta y solo recibe el clic.
		UStaticMesh* KitSlab = Layout.Tiles.IsValidIndex(Tile.TileId) ? KitMesh(SlabPiece(Layout.Tiles[Tile.TileId].Slab)) : nullptr;
		TileHasKitSlab[Tile.TileId] = KitSlab != nullptr;
		// La cara superior es la cota de la ficha (R-VIS-3): la de la losa del kit, o el alto de la pieza del Altar.
		float Thickness = bAltar ? AltarThickness : LayoutParams.SlabTop;
		if (bAltar && KitSlab)
		{
			// De los datos (AltarTop * KitScale), no de la malla: sus dimensiones llegan a la punta de la aguja.
			Thickness = Layout.Tiles[Tile.TileId].Top;
		}
		const float Diameter = bAltar ? Layout.AltarApothem * 2.f : SlabSize();
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
			Actor->SetKitSlabShown(KitSlab != nullptr);
			Actor->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
		}
		TileActors.Add(Actor);

		// Halo de "alcanzable": un disco por debajo de la losa, más ancho, que solo se ve en el borde. La losa
		// conserva su color; el halo se enciende cuando la tirada la permite.
		UStaticMeshComponent* Halo = AddShape(CylinderMesh, Position + FVector(0.f, 0.f, HaloZ), FRotator::ZeroRotator, FVector(Diameter * HaloScale, Diameter * HaloScale, HaloHeight));
		Soulbound::Presentation::SetComponentColor(*Halo, BaseMaterial, ReachableColor);
		Halo->SetVisibility(false);
		Halos[Tile.TileId] = Halo;

		// Aro de Dominio Olvidado: más alto que el halo, para que se vea también en una casilla alcanzable.
		UStaticMeshComponent* Ring = AddShape(CylinderMesh, Position + FVector(0.f, 0.f, ForgottenRingZ), FRotator::ZeroRotator,
			FVector(Diameter * ForgottenRingScale, Diameter * ForgottenRingScale, ForgottenRingHeight));
		Soulbound::Presentation::SetComponentColor(*Ring, BaseMaterial, ForgottenRingColor);
		Ring->SetVisibility(false);
		ForgottenRings[Tile.TileId] = Ring;

		// Marca del tipo sobre la losa, del tamaño justo para no tapar la ficha del centro, con contorno negro
		// para que se distinga sobre cualquier Dominio. Con la losa del kit no hace falta: la pieza ya dice el tipo.
		const float Inlay = SlabSize() * InlayScale;
		const float Outline = Inlay * OutlineScale;
		const FVector OnTop = Position + FVector(0.f, 0.f, Thickness);
		const FVector Mid = OnTop + FVector(0.f, 0.f, InlayHeight * 0.5f);
		switch (KitSlab ? ETileType::Normal : Tile.Type)
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
		Label->SetWorldLocation(OnTop + FVector(-Diameter * LabelOffsetFraction, 0.f, InlayHeight + TextClearance));
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
			Reserve->SetWorldLocation(OnTop + FVector(0.f, 0.f, InlayHeight + TextClearance));
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
			NicheNames[Tile.TileId] = AddText(NicheNameSize, FVector(Diameter * NicheNameOffsetFraction, 0.f, InlayHeight + TextClearance));
			NicheNames[Tile.TileId]->SetText(FText::FromString(ElectName(Host->GetState(), Tile.DomainId)));
			NicheNumbers[Tile.TileId] = AddText(NicheNumberSize, FVector(0.f, 0.f, InlayHeight + TextClearance));
			NicheNumbers[Tile.TileId]->SetText(FText::GetEmpty());

			// Aro del asiento: segmentos en círculo alrededor de la casilla, apagados hasta que se reclama.
			const float RingRadius = SlabSize() * NicheRingRadiusScale;
			const float SegmentLength = 2.f * PI * RingRadius / NicheRingSegmentCount * 1.15f;
			NicheRingFirst[Tile.TileId] = NicheRingSegments.Num();
			for (int32 Index = 0; Index < NicheRingSegmentCount; ++Index)
			{
				const float Angle = 2.f * PI * Index / NicheRingSegmentCount;
				UStaticMeshComponent* Segment = AddShape(CubeMesh, Position + FVector(FMath::Cos(Angle) * RingRadius, FMath::Sin(Angle) * RingRadius, ForgottenRingZ),
					FRotator(0.f, FMath::RadiansToDegrees(Angle) + 90.f, 0.f), FVector(SegmentLength, NicheRingWidth, ForgottenRingHeight));
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
	UStaticMesh* Card = KitMesh(EBoardKitPiece::TokenCard);
	for (int32 Seat = 0; Seat < Host->GetState().Seats.Num(); ++Seat)
	{
		ASeatTokenActor* Token = GetWorld()->SpawnActor<ASeatTokenActor>(ASeatTokenActor::StaticClass(), FTransform::Identity, Params);
		if (Token)
		{
			Token->SetSeat(Seat);
			Token->Configure(TokenLabelSize, ActiveLift, ActiveSpinDegreesPerSecond, RingDiameter, RingWidth, ActiveScale, TokenDiameter);
			// R-VIS-6: la carta de prueba del kit, con el color del asiento en el aro de su base (sin carta, la esfera o el cono).
			if (UStaticMeshComponent* CardMesh = Token->UseCard(Card, KitEmissiveMaterial, TokenRingScale, TokenRingWidth, EchoRingBrightness))
			{
				ApplyKitMaterials(*CardMesh);
			}
			Token->SetColor(SeatColor(Seat));
			Token->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
			Token->OnTileEntered.BindUObject(this, &ABoardViewActor::OnTokenTileEntered);
		}
		TokenActors.Add(Token);
	}

	// S-017: el orbe de la consulta del Oráculo, una esfera que solo existe para verse y recibir el clic (vista de depuración, sin
	// asset: la presentación del Oráculo con su barra es de la interfaz, GDD 07).
	OracleOrb = NewObject<UStaticMeshComponent>(this);
	OracleOrb->SetStaticMesh(SphereMesh);
	OracleOrb->SetupAttachment(GetRootComponent());
	OracleOrb->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	OracleOrb->SetCollisionResponseToAllChannels(ECR_Ignore);
	OracleOrb->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	OracleOrb->SetCastShadow(false);
	OracleOrb->RegisterComponent();
	OracleOrb->SetWorldScale3D(FVector(OracleOrbDiameter / 100.f));
	OracleOrb->SetVisibility(false);
}

bool ABoardViewActor::IsOracleOrb(const UPrimitiveComponent* Component) const
{
	return Component && Component == OracleOrb.Get() && OracleOrb->IsVisible();
}

int32 ABoardViewActor::NearestOracleNode(const FMatchState& State, int32 Seat) const
{
	if (!State.Seats.IsValidIndex(Seat) || !State.Board.AreNodesRevealed() || !TilePositions.IsValidIndex(State.Seats[Seat].TileId))
	{
		return INDEX_NONE;
	}
	const FVector From = TilePositions[State.Seats[Seat].TileId];
	int32 Nearest = INDEX_NONE;
	for (const FTileState& Tile : State.Board.Tiles)
	{
		if (Tile.PowerNodeEffect == EPowerNodeEffect::Oracle && TilePositions.IsValidIndex(Tile.TileId)
			&& (Nearest == INDEX_NONE || FVector::DistSquared(From, TilePositions[Tile.TileId]) < FVector::DistSquared(From, TilePositions[Nearest])))
		{
			Nearest = Tile.TileId;
		}
	}
	return Nearest;
}

void ABoardViewActor::RefreshOracleOrb(const FMatchState& State)
{
	if (!OracleOrb)
	{
		return;
	}
	const int32 Seat = State.Oracle.Consultant;
	if (!State.Oracle.IsWaiting() || !State.Seats.IsValidIndex(Seat))
	{
		OracleOrb->SetVisibility(false);
		return;
	}
	// En el Nodo del Oráculo más cercano a su ficha (el de su casilla, si consulta en un Nodo); si no hay, sobre la ficha.
	const int32 Node = NearestOracleNode(State, Seat);
	const ASeatTokenActor* Token = TokenActors.IsValidIndex(Seat) ? TokenActors[Seat].Get() : nullptr;
	const FVector Base = Node != INDEX_NONE ? TokenBase(Node) : Token ? Token->GetActorLocation() : TokenBase(State.Seats[Seat].TileId);
	OracleOrb->SetWorldLocation(Base + FVector(0.f, 0.f, OracleOrbHeight));
	Soulbound::Presentation::SetComponentColor(*OracleOrb, BaseMaterial, SeatColor(Seat));
	OracleOrb->SetVisibility(true);
}

void ABoardViewActor::BuildCamera()
{
	// R-VIS-9: la cenital de siempre, ajustada a la mesa, con el Altar en el centro.
	BoardCenter = FVector2D::ZeroVector;
	BoardHalf = FVector2D(LayoutParams.TableRadius);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Camera = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform(FRotator(-90.f, 0.f, 0.f), FVector(BoardCenter.X, BoardCenter.Y, 1000.0)), Params);
	if (Camera)
	{
		Camera->GetCameraComponent()->SetFieldOfView(60.f);
		Camera->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
	}
	UpdateCamera();

	// La vista sentada del asiento humano. No es la cámara de la interfaz (R-CAM-1 a 5).
	SeatedCamera = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity, Params);
	if (SeatedCamera)
	{
		SeatedCamera->GetCameraComponent()->SetFieldOfView(60.f);
		SeatedCamera->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
	}
	PlaceSeatedCamera();
}

void ABoardViewActor::PlaceSeatedCamera()
{
	if (!SeatedCamera)
	{
		return;
	}
	// Fuera del borde de la mesa, sobre ella, inclinada hacia abajo y girada al centro (R-VIS-9).
	const float Yaw = SeatedViewYawDegrees;
	const FVector2D Direction(FMath::Cos(FMath::DegreesToRadians(Yaw)), FMath::Sin(FMath::DegreesToRadians(Yaw)));
	const FVector Location(Direction * (LayoutParams.TableRadius + SeatedViewDistance), SeatedViewHeight);
	SeatedCamera->SetActorLocationAndRotation(Location, FRotator(-SeatedViewPitchDegrees, Yaw + 180.f, 0.f));
}

void ABoardViewActor::SetSeatedView(float Height, float Distance, float PitchDegrees, TOptional<float> YawDegrees)
{
	SeatedViewHeight = Height;
	SeatedViewDistance = Distance;
	SeatedViewPitchDegrees = PitchDegrees;
	if (YawDegrees.IsSet())
	{
		SeatedViewYawDegrees = YawDegrees.GetValue();
	}
	PlaceSeatedCamera();
	bSeatedView = true;
	ApplyViewTarget();
}

FString ABoardViewActor::DescribeSeatedView() const
{
	return FString::Printf(TEXT("altura %.1f cm, distancia al borde %.1f cm, inclinación %.1f°, ángulo del asiento %.1f° (%s)"), SeatedViewHeight, SeatedViewDistance,
		SeatedViewPitchDegrees, SeatedViewYawDegrees, bSeatedView ? TEXT("activa") : TEXT("inactiva: tecla C"));
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
	// Con los paneles ocultos (tecla H), el tablero usa todo el ancho.
	const float PanelPixels = bPanelsHidden ? 0.f : PanelWidthPixels;
	const float FreeFraction = 1.f - FMath::Clamp(PanelPixels / static_cast<float>(Viewport.X), 0.f, 0.6f);
	const float Height = CameraMargin * FMath::Max(BoardHalf.Y / (FreeFraction * HalfTan), BoardHalf.X * Aspect / HalfTan);
	// La pantalla crece hacia +Y a la derecha: para que el tablero quede a la izquierda, la cámara se corre hacia +Y.
	const float VisibleHalfWidth = Height * HalfTan;
	const float ShiftY = VisibleHalfWidth * (1.f - FreeFraction);
	Camera->SetActorLocation(FVector(BoardCenter.X, BoardCenter.Y + ShiftY, Height));
}

void ABoardViewActor::BuildLighting()
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

	// R-VIS-7: la lámpara sobre la mesa, un foco cenital, y un cielo tenue; sin sol ni cielo visible. Se crean en partida,
	// así que son luces móviles.
	// TODO(R-VIS-7): las luces estáticas (precalculadas, por la Steam Deck) llegan con la Cripta; hasta entonces, móviles.
	Lamp = NewObject<USpotLightComponent>(this);
	Lamp->SetupAttachment(GetRootComponent());
	Lamp->SetMobility(EComponentMobility::Movable);
	Lamp->SetRelativeLocationAndRotation(FVector(0.f, 0.f, LampHeight), FRotator(-90.f, 0.f, 0.f));
	Lamp->SetIntensityUnits(ELightUnits::Candelas);
	Lamp->SetIntensity(LampIntensityCandelas);
	Lamp->SetInnerConeAngle(LampInnerConeDegrees);
	Lamp->SetOuterConeAngle(LampOuterConeDegrees);
	Lamp->RegisterComponent();

	SkyLight = NewObject<USkyLightComponent>(this);
	SkyLight->SetupAttachment(GetRootComponent());
	SkyLight->SetMobility(EComponentMobility::Movable);
	SkyLight->SourceType = ESkyLightSourceType::SLS_CapturedScene;
	SkyLight->SetIntensity(SkyLightIntensity);
	SkyLight->RegisterComponent();
	SkyLight->RecaptureSky();
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
	// S-019: con el kit, la losa de cada tipo y su estructura dicen el tipo (las formas básicas quedan donde falta la pieza).
	if (TileHasKitSlab.Contains(true))
	{
		Rows.Add({TEXT("Con el kit (color por vértice: sin color de Dominio en la losa)"),
			{{NicheFreeColor, TEXT("Nicho: losa con cabecera y trono")},
				{SourceColor, TEXT("Fuente: pilón, con runa mientras tiene reserva")},
				{NodeColor, TEXT("Nodo: losa con puntas; desde el Despertar, arco, espadas u orbe")},
				{SealedColor, TEXT("sellada: cadenas sobre la losa")}}});
	}
	Rows.Add({TEXT("Nicho (el cubo)"),
		{{NicheFreeColor, TEXT("Libre")}, {SeatColor(0), TEXT("Reclamado: aro y número del asiento")}, {NicheClosedColor, TEXT("Cerrado")}}});
	Rows.Add({TEXT("Estado de la casilla"),
		{{ReachableColor, TEXT("alcanzable: halo blanco bajo la casilla")},
			{SealedColor, TEXT("sellada (SELLO)")},
			{ForgottenRingColor, TEXT("Dominio Olvidado: casillas apagadas con aro gris oscuro")}}});
	// Una fila por asiento, con su rol al día: Eco (esfera) o Elegido (cono), y el Dominio que lleva.
	FLegendRow Seats;
	// S-019: con la carta del kit, el rol va en el aro de su base (decisión del autor del 20/10/2026).
	Seats.Title = KitMesh(EBoardKitPiece::TokenCard)
		? TEXT("Asientos (el número va sobre la carta): aro tenue = Eco, aro vivo = Elegido. El activo crece, sube, gira y lleva aro")
		: TEXT("Asientos (el número va en la ficha): esfera = Eco, cono = Elegido. El activo crece, sube, gira y lleva aro");
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
		// S-016: las evaluaciones del Warden que no hacen nada no tienen línea (una cada 15 s taparía la crónica). S-017: la decisión
		// con efecto sí, con sus disparadores entre paréntesis (los WardenTriggered que la siguen, que no tienen línea propia).
		FString Text;
		if (Event.Type == EMatchEventType::WardenDecided)
		{
			TArray<FMatchEvent> Triggers;
			bool bViaOracle = false;
			for (int32 Next = Index + 1; Next < State.EventLog.Num(); ++Next)
			{
				const FMatchEvent& After = State.EventLog[Next];
				if (After.Type == EMatchEventType::WardenTriggered)
				{
					Triggers.Add(After);
					continue;
				}
				// Lo que aplica la decisión, en el mismo instante: el corte, el fin del turno y, si va por el Oráculo, su consulta.
				if (After.ClockSeconds != Event.ClockSeconds || After.Type == EMatchEventType::WardenEventStarted || After.Type == EMatchEventType::WardenEvaluated)
				{
					break;
				}
				bViaOracle |= After.Type == EMatchEventType::OracleOpened && After.Value == static_cast<int32>(EOracleOrigin::Strike);
			}
			Text = Soulbound::Presentation::DescribeWardenDecision(Event, Triggers, bViaOracle, &State.WardenEvents);
		}
		else
		{
			Text = Soulbound::Presentation::DescribeEvent(Event, State.Board, &State.WardenEvents);
		}
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
		// S-019 (R-VIS-9): la tecla de las capturas oculta todas las etiquetas de la casilla.
		for (const TArray<TObjectPtr<UTextRenderComponent>>* Texts : {&Labels, &NicheNames, &NicheNumbers, &SourceReserves})
		{
			if (UTextRenderComponent* Text = Texts->IsValidIndex(Tile.TileId) ? (*Texts)[Tile.TileId].Get() : nullptr)
			{
				Text->SetVisibility(!bOverlayHidden);
			}
		}
	}

	// S-019: estructuras, runas flotantes, cadenas y fichas fuera en el Vacío, según el estado de vista.
	const FBoardViewState View = Soulbound::Presentation::ComputeBoardViewState(State, bNicheThroneConfigured);
	RefreshKit(View);

	RefreshTokens(State);
	RefreshOracleOrb(State);
	RefreshChronicle(State);
}

FVector ABoardViewActor::TokenBase(int32 TileId) const
{
	return TilePositions.IsValidIndex(TileId) ? TilePositions[TileId] + FVector(0.f, 0.f, TileTops.IsValidIndex(TileId) ? TileTops[TileId] : 0.f) : FVector::ZeroVector;
}

void ABoardViewActor::OnTokenTileEntered(int32 Seat, int32 TileId, bool bLast)
{
	UMatchHostSubsystem* HostPtr = Host.Get();
	if (!HostPtr || !HostPtr->HasMatch())
	{
		return;
	}
	// R-TAB-26: la presentación dice al motor en qué casilla ha entrado la ficha, sea del humano o de un dummy (decisión del autor
	// del 21/10/2026). Aceptada, el anfitrión avisa y la vista se refresca sola; si no (la salvaguarda llegó antes, o el turno se
	// cortó), al acabar el recorrido se recoloca.
	if (HostPtr->SubmitMoveProgress(Seat, TileId) == EIntentResult::Accepted)
	{
		return;
	}
	if (bLast)
	{
		RefreshTokens(HostPtr->GetState());
	}
}

void ABoardViewActor::RefreshTokens(const FMatchState& State)
{
	const UMatchHostSubsystem* HostPtr = Host.Get();
	if (!HostPtr)
	{
		return;
	}
	// Asiento activo: solo mientras dura su turno; entre turnos no juega nadie.
	const bool bSomeoneActive = !State.IsMatchOver() && State.TurnPhase != ETurnPhase::BetweenTurns;
	// S-013, S-019: qué fichas están en el tablero (fuera, en el Vacío). Se mira aquí porque las fichas también se recolocan al
	// acabar un recorrido, sin pasar por Refresh.
	const FBoardViewState View = Soulbound::Presentation::ComputeBoardViewState(State, bNicheThroneConfigured);

	// S-020: el asiento que se mueve, humano o dummy (decisión del autor del 21/10/2026).
	const int32 MovingSeat = State.TurnPhase == ETurnPhase::Moving ? State.ActiveSeat : INDEX_NONE;
	// R-VIS-6 (decisión del autor del 22/10/2026): el trono del Warden ocupa el centro del Altar, así que la ficha no cruza de
	// centro a centro. Al salir del Altar va primero por el círculo de la grada alta, desde su sitio hasta el punto que mira a la
	// primera casilla, en pasos de 30° como mucho; al llegar a él, al revés. Los puntos del círculo (casilla INDEX_NONE) son solo
	// presentación: las intenciones de avance se entregan una por casilla, al llegar a su punto final.
	const FBoardState& Board = Host->GetState().Board;
	auto CirclePoint = [this, &Board](float Angle)
	{
		return TokenBase(Board.FindAltar()) + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * (Layout.AltarApothem * 0.6f);
	};
	auto FacingAngle = [this](int32 Tile) -> float
	{
		const FVector2D Direction(Layout.Tiles[Tile].Location);
		return Direction.IsNearlyZero() ? 0.f : static_cast<float>(FMath::Atan2(Direction.Y, Direction.X));
	};
	// Los ángulos desde From hasta To (el camino corto) en pasos de 30° como mucho, sin From y con To.
	auto ArcAngles = [](float From, float To)
	{
		TArray<float> Angles;
		const float Delta = FMath::UnwindRadians(To - From);
		const int32 Steps = FMath::CeilToInt(FMath::Abs(Delta) / FMath::DegreesToRadians(30.f) - UE_KINDA_SMALL_NUMBER);
		for (int32 Step = 1; Step <= Steps; ++Step)
		{
			Angles.Add(From + Delta * Step / Steps);
		}
		return Angles;
	};
	auto WalkOf = [&](int32 Seat, int32 CurrentTile, const TArray<int32>& Tiles, TArray<int32>& OutTiles, TArray<FVector>& OutPoints)
	{
		const int32 AltarTile = Board.FindAltar();
		const float SeatAngle = 2.f * PI * Seat / Soulbound::Match::MaxPlayers;
		for (int32 Index = 0; Index < Tiles.Num(); ++Index)
		{
			const int32 Tile = Tiles[Index];
			if (Index == 0 && CurrentTile == AltarTile)
			{
				for (const float Angle : ArcAngles(SeatAngle, FacingAngle(Tile)))
				{
					OutTiles.Add(INDEX_NONE);
					OutPoints.Add(CirclePoint(Angle));
				}
			}
			if (Index == Tiles.Num() - 1 && Tile == AltarTile)
			{
				TArray<float> Angles{FacingAngle(Index > 0 ? Tiles[Index - 1] : CurrentTile)};
				Angles.Append(ArcAngles(Angles[0], SeatAngle));
				for (int32 Step = 0; Step < Angles.Num(); ++Step)
				{
					OutTiles.Add(Step == Angles.Num() - 1 ? Tile : INDEX_NONE);
					OutPoints.Add(CirclePoint(Angles[Step]));
				}
				continue;
			}
			OutTiles.Add(Tile);
			OutPoints.Add(TokenBase(Tile));
		}
	};

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
		Token->SetActorHiddenInGame(!View.TokensOnBoard.IsValidIndex(Seat) || !View.TokensOnBoard[Seat]);
		Token->SetLabelVisible(!bOverlayHidden);
		if (!TilePositions.IsValidIndex(TileId))
		{
			Token->StopWalk();
			continue;
		}
		Token->SetElectShape(State.Seats[Seat].Role == ESeatRole::Elect);
		Token->SetActive(bSomeoneActive && Seat == State.ActiveSeat);

		// S-020 (R-TAB-26): la ficha del que se mueve recorre lo que le queda del camino, y cada llegada es su intención de avance.
		// Si el motor va por delante (la salvaguarda entró en una casilla a la que la ficha no había llegado), la ficha se coloca
		// donde dice el estado, sin animar, y sigue desde ahí.
		if (Seat == MovingSeat)
		{
			const TArray<int32>& Path = State.Movement.Path;
			const int32 Done = FMath::Clamp(State.Movement.EnteredCount, 0, Path.Num());
			const TArray<int32> Remaining(Path.GetData() + Done, Path.Num() - Done);
			if (Token->RemainingWalk() != Remaining)
			{
				if (Token->IsWalking())
				{
					Token->StopWalk();
					Token->MoveTo(TokenBase(TileId), 0.f);
				}
				TArray<int32> WalkTiles;
				TArray<FVector> WalkPoints;
				WalkOf(Seat, TileId, Remaining, WalkTiles, WalkPoints);
				Token->WalkPath(WalkTiles, WalkPoints, TokenSpeed);
			}
			continue;
		}
		if (Token->IsWalking())
		{
			// Un recorrido que el estado deja atrás: cortado a medio camino o llegado por la salvaguarda. La ficha va donde dice el
			// estado, sin animar.
			Token->StopWalk();
			Token->MoveTo(TokenBase(TileId), 0.f);
		}

		// Cada asiento tiene siempre su sitio en la casilla, esté sola o acompañada: si una ficha se va, las demás no se mueven
		// (decisión del autor del 22/10/2026). En el Altar, en el círculo de la grada alta, porque el centro es el trono vacío
		// del Warden (R-VIS-6).
		const bool bOnAltar = Host->GetState().Board.IsAltar(TileId);
		const float Angle = 2.f * PI * Seat / Soulbound::Match::MaxPlayers;
		const FVector Offset = FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * (bOnAltar ? Layout.AltarApothem * 0.6f : SlabSize() * 0.28f);
		// La base de la ficha, posada en la cara superior de la losa (R-VIS-3, R-VIS-6). Lo que no es un movimiento (Vacío, hogar,
		// Expulsión, salto planar) desliza en línea recta; tras recorrer un camino, solo el sitio del asiento en la casilla.
		Token->MoveTo(TokenBase(TileId) + Offset, TokenSlideSeconds);
	}
}

void ABoardViewActor::RefreshKit(const FBoardViewState& View)
{
	// R-VIS-5: lo que cambia con la partida se lee del estado de vista; la vista no decide nada.
	for (int32 TileId = 0; TileId < View.Tiles.Num(); ++TileId)
	{
		const FTileViewState& Shown = View.Tiles[TileId];
		if (UStaticMeshComponent* Structure = Structures.IsValidIndex(TileId) ? Structures[TileId].Get() : nullptr)
		{
			Structure->SetVisibility(Shown.Structure != EBoardStructure::None);
		}
		if (UStaticMeshComponent* Rune = FloatingRunes.IsValidIndex(TileId) ? FloatingRunes[TileId].Get() : nullptr)
		{
			Rune->SetVisibility(Shown.bFloatingRune);
		}
		if (UStaticMeshComponent* Chains = SealChains.IsValidIndex(TileId) ? SealChains[TileId].Get() : nullptr)
		{
			Chains->SetVisibility(Shown.bSealChains);
		}
	}
}

void ABoardViewActor::ToggleSeatedView()
{
	bSeatedView = !bSeatedView;
	ApplyViewTarget();
}

void ABoardViewActor::ToggleOverlay()
{
	bOverlayHidden = !bOverlayHidden;
	Refresh();
}

void ABoardViewActor::TogglePanels()
{
	bPanelsHidden = !bPanelsHidden;
	if (Overlay.IsValid())
	{
		Overlay->SetVisibility(bPanelsHidden ? EVisibility::Collapsed : EVisibility::SelfHitTestInvisible);
	}
	UpdateCamera();
}

void ABoardViewActor::ToggleDebugText()
{
	bDebugTextHidden = !bDebugTextHidden;
	if (bDebugTextHidden && GEngine)
	{
		// Sin esperar a que caduquen: solo las líneas de esta vista, no los mensajes del controlador. De la última a la primera:
		// el motor reutiliza los huecos liberados en orden inverso, y así, al volver, cada línea ocupa el suyo y el orden se
		// conserva (de la primera a la última, volvían del revés).
		for (int32 Line = DebugTextLineCount - 1; Line >= 0; --Line)
		{
			GEngine->RemoveOnScreenDebugMessage(TextKeyBase + Line);
		}
	}
	else
	{
		TickHud();
	}
}

FString ABoardViewActor::DescribeViewKeys() const
{
	return FString::Printf(TEXT("%s vista sentada | %s etiquetas | %s paneles | %s texto de depuración"), *SeatedViewKey.GetDisplayName().ToString(),
		*OverlayToggleKey.GetDisplayName().ToString(), *PanelsToggleKey.GetDisplayName().ToString(), *DebugTextToggleKey.GetDisplayName().ToString());
}

void ABoardViewActor::ApplyViewTarget()
{
	ACameraActor* Wanted = bSeatedView && SeatedCamera ? SeatedCamera.Get() : Camera.Get();
	if (!Wanted)
	{
		return;
	}
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* Controller = It->Get();
		if (!Controller || !Controller->IsLocalController())
		{
			continue;
		}
		if (Controller->GetViewTarget() != Wanted)
		{
			Controller->SetViewTarget(Wanted);
		}
		// S-019: a escala de mesa, el peón por defecto del modo de juego (una esfera de 70 cm en el PlayerStart) tapa las
		// cámaras. La vista no lo usa: lo oculta y le quita la colisión, para que tampoco se coma el clic en las casillas.
		APawn* Pawn = Controller->GetPawn();
		if (Pawn && !Pawn->IsHidden())
		{
			Pawn->SetActorHiddenInGame(true);
			Pawn->SetActorEnableCollision(false);
		}
		// S-019 (R-VIS-9): las teclas de la vista, enlazadas a la tecla sin assets de entrada, como las del controlador.
		if (!bInputBound)
		{
			EnableInput(Controller);
			if (InputComponent)
			{
				InputComponent->BindKey(SeatedViewKey, IE_Pressed, this, &ABoardViewActor::ToggleSeatedView);
				InputComponent->BindKey(OverlayToggleKey, IE_Pressed, this, &ABoardViewActor::ToggleOverlay);
				InputComponent->BindKey(PanelsToggleKey, IE_Pressed, this, &ABoardViewActor::TogglePanels);
				InputComponent->BindKey(DebugTextToggleKey, IE_Pressed, this, &ABoardViewActor::ToggleDebugText);
				bInputBound = true;
			}
		}
	}
}

void ABoardViewActor::ShowText(int32 Line, const FColor& Color, const FString& Text) const
{
	// S-019: la tecla del texto de depuración lo oculta todo, la línea de petición incluida.
	if (GEngine && !bDebugTextHidden)
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

	// La cámara es la vista del jugador local. Se pone al iniciar la partida, o en cuanto exista el controlador. Desde
	// S-019, la cenital o la sentada, según la tecla.
	if (Camera)
	{
		ApplyViewTarget();
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

	// Oráculo (S-017): a quién le toca consultar, de dónde viene y cuánto le queda; el humano, con lo que tiene que hacer.
	if (State.Oracle.IsWaiting() && !State.IsMatchOver())
	{
		const int32 Consultant = State.Oracle.Consultant;
		const bool bHumanOracle = Consultant == HostPtr->GetHumanSeat();
		const int32 Node = NearestOracleNode(State, Consultant);
		const FString Where = Node != INDEX_NONE ? FString::Printf(TEXT("el orbe sobre el Nodo %s"), *Soulbound::Presentation::TileShortName(State.Board, Node))
			: FString(TEXT("el orbe sobre su ficha"));
		const FString Action = bHumanOracle ? FString::Printf(TEXT("  <- clic en %s para consultar%s"), *Where, State.Oracle.bRefusable ? TEXT(", clic derecho para rechazar (Tifón)") : TEXT(""))
			: FString::Printf(TEXT(" | %s"), *Where);
		ShowText(13, bHumanOracle ? FColor::Green : FColor::Orange, FString::Printf(TEXT("ORÁCULO: consulta el asiento %d (%s) | quedan %.0f s%s"), Consultant,
			*Soulbound::Presentation::OracleOriginText(static_cast<int32>(State.Oracle.Origin)), FMath::Max(0.0, State.Oracle.DeadlineSeconds - State.ClockSeconds), *Action));
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
		ShowText(2, FColor::Yellow, FString::Printf(TEXT("PASO A PASO: espacio para el siguiente turno | teclas: %s"), *DescribeViewKeys()));
	}
	else if (HostPtr->IsStepMode())
	{
		ShowText(2, FColor(200, 200, 90), FString::Printf(TEXT("PASO A PASO activo: la partida se detendra al acabar este turno | teclas: %s"), *DescribeViewKeys()));
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
