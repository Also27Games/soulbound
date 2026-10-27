#include "EventText.h"

#include "Warden/WardenBrain.h"

namespace Soulbound::Presentation
{
	FString TileShortName(const FBoardState& Board, int32 TileId)
	{
		if (!Board.IsValidTile(TileId))
		{
			return FString::Printf(TEXT("casilla %d"), TileId);
		}
		const FTileState& Tile = Board.Tiles[TileId];
		return Tile.Type == ETileType::Altar ? FString(TEXT("ALTAR")) : FString::Printf(TEXT("D%d·%d"), Tile.DomainId, Tile.TileId);
	}

	// Esqueletos del test en rojo (S-021).
	FString SpokenText(const FWardenLineTable& Lines, const FMatchEvent& Spoke) { return FString(); }
	FString WardenLinePrefix(int32 Seat, int32 HumanSeat) { return FString(); }
	FString DescribeChronicleEvent(const FMatchState& State, int32 Index)
	{
		return State.EventLog.IsValidIndex(Index) ? DescribeEvent(State.EventLog[Index], State.Board, &State.WardenEvents, &State.WardenLines) : FString();
	}

	FString DescribeEvent(const FMatchEvent& Event, const FBoardState& Board, const FWardenEventTable* WardenEvents, const FWardenLineTable* WardenLines)
	{
		switch (Event.Type)
		{
		case EMatchEventType::TurnStarted: return FString::Printf(TEXT("empieza el turno del asiento %d"), Event.Seat);
		case EMatchEventType::Rolled: return FString::Printf(TEXT("el asiento %d tira %d"), Event.Seat, Event.Value);
		case EMatchEventType::TurnClockExpired: return FString::Printf(TEXT("el asiento %d AGOTA el reloj de turno: no se mueve"), Event.Seat);
		case EMatchEventType::TurnSkipped: return FString::Printf(TEXT("el asiento %d pierde el turno"), Event.Seat);
		case EMatchEventType::TurnEnded: return FString::Printf(TEXT("termina el turno del asiento %d"), Event.Seat);
		case EMatchEventType::Moved: return FString::Printf(TEXT("el asiento %d se mueve a %s"), Event.Seat, *TileShortName(Board, Event.Value));
		// S-020 (decisión del autor del 20/10/2026): la crónica del juego no lista las casillas del camino; basta la del movimiento.
		case EMatchEventType::TileEntered: return FString();
		case EMatchEventType::SealPlaced: return FString::Printf(TEXT("el Warden sella la casilla %d"), Event.Value);
		case EMatchEventType::SealReleased: return FString::Printf(TEXT("se libera el Sello de la casilla %d"), Event.Value);
		case EMatchEventType::Bonded: return FString::Printf(TEXT("el asiento %d se vincula al Elegido del Dominio %d"), Event.Seat, Event.Value);
		case EMatchEventType::BoardAwakening: return FString::Printf(TEXT("el asiento %d despierta el tablero"), Event.Seat);
		case EMatchEventType::HomeAssigned: return FString::Printf(TEXT("el asiento %d queda Eco, con hogar en el Dominio %d"), Event.Seat, Event.Value);
		case EMatchEventType::ReturnedHome: return FString::Printf(TEXT("el asiento %d vuelve a su hogar, %s"), Event.Seat, *TileShortName(Board, Event.Value));
		// S-018: con el motivo; el tiempo agotado también es victoria del Warden.
		case EMatchEventType::WardenVictory:
			return Event.Value == static_cast<int32>(EMatchEndReason::TimeOut) ? FString(TEXT("VICTORIA DEL WARDEN: se acaba el tiempo"))
				: FString(TEXT("VICTORIA DEL WARDEN: nadie se ha vinculado"));
		case EMatchEventType::BondRaceEnded: return FString::Printf(TEXT("fin de la carrera: %d Elegidos"), Event.Value);
		case EMatchEventType::RuneObtained: return FString::Printf(TEXT("el asiento %d obtiene la runa del Dominio %d"), Event.Seat, Event.Value);
		// El minijuego real no existe aún (su story): el decisor del asiento lo resuelve al azar, y la crónica lo dice.
		case EMatchEventType::SkillMinigamePassed: return FString::Printf(TEXT("el asiento %d SUPERA el minijuego de la Fuente del Dominio %d (resuelto al azar)"), Event.Seat, Event.Value);
		case EMatchEventType::SkillMinigameFailed: return FString::Printf(TEXT("el asiento %d FALLA el minijuego de la Fuente del Dominio %d (resuelto al azar)"), Event.Seat, Event.Value);
		case EMatchEventType::SourceDepleted: return FString::Printf(TEXT("el asiento %d llega a la Fuente del Dominio %d, AGOTADA: sin runa"), Event.Seat, Event.Value);
		case EMatchEventType::RuneReturned: return FString::Printf(TEXT("el asiento %d devuelve la runa del Dominio %d a su Fuente"), Event.Seat, Event.Value);
		case EMatchEventType::DiceChosen: return FString::Printf(TEXT("el asiento %d elige tirar %s"), Event.Seat, Event.Value > 1 ? TEXT("DOS dados") : TEXT("un dado"));
		case EMatchEventType::Rerolled: return FString::Printf(TEXT("el asiento %d REPITE la tirada: %d"), Event.Seat, Event.Value);
		case EMatchEventType::PlanarJumped: return FString::Printf(TEXT("el asiento %d SALTA por Vuelo Planar a %s"), Event.Seat, *TileShortName(Board, Event.Value));
		case EMatchEventType::Expelled: return FString::Printf(TEXT("el asiento %d EXPULSA al asiento %d a su hogar"), Event.Seat, Event.OtherSeat);
		case EMatchEventType::ChoiceExpired:
		{
			const EChoiceKind Kind = static_cast<EChoiceKind>(Event.Value);
			const TCHAR* What = Kind == EChoiceKind::Reroll ? TEXT("repetir")
				: Kind == EChoiceKind::PlanarJump ? TEXT("el salto planar")
				: Kind == EChoiceKind::SafeRune ? TEXT("apartar una runa: el trilero se juega con todas")
				: Kind == EChoiceKind::Deflection ? TEXT("Desvío: juega el escape")
				: TEXT("la acción contra un jugador");
			return FString::Printf(TEXT("el asiento %d deja pasar %s: se agota su elección"), Event.Seat, What);
		}
		// S-010: el robo del Eco y el trilero.
		case EMatchEventType::ShellGameStarted:
			return FString::Printf(TEXT("TRILERO: el asiento %d roba al asiento %d; sus runas salen a la vista en %d esferas"), Event.Seat, Event.OtherSeat, Event.Value);
		case EMatchEventType::SafeRuneSetAside:
			return FString::Printf(TEXT("el asiento %d (Koschei) pone a salvo la runa del Dominio %d: no entra en la mezcla"), Event.Seat, Event.Value);
		case EMatchEventType::RuneStolen:
			return FString::Printf(TEXT("el asiento %d ROBA la runa del Dominio %d al asiento %d"), Event.Seat, Event.Value, Event.OtherSeat);
		case EMatchEventType::ShellGameExpired:
			return FString::Printf(TEXT("el asiento %d no elige esfera a tiempo: no roba nada al asiento %d"), Event.Seat, Event.OtherSeat);
		case EMatchEventType::StealNoRunes:
			return FString::Printf(TEXT("el asiento %d intenta robar al asiento %d, que no tiene runas: no hay trilero"), Event.Seat, Event.OtherSeat);
		case EMatchEventType::StealCancelled:
			return FString::Printf(TEXT("el asiento %d intenta robar a Koschei (asiento %d), que solo tiene una runa: robo anulado"), Event.Seat, Event.OtherSeat);
		// S-012: el duelo. El minijuego de duelo no existe aún: el rendimiento de cada uno lo da su decisor al azar.
		case EMatchEventType::DuelChallenged:
			return FString::Printf(TEXT("%s: el asiento %d reta al asiento %d %s"), Event.Value == 2 ? TEXT("DUELO FORZADO") : TEXT("DUELO"), Event.Seat, Event.OtherSeat,
				Event.Value == 2 ? TEXT("por orden del Warden") : Event.Value == 1 ? TEXT("desde un Nodo de Duelo") : TEXT("con el poder Duelo"));
		case EMatchEventType::DuelNoRunes:
			return FString::Printf(TEXT("el asiento %d reta al asiento %d, que no tiene runas: no hay duelo y la acción se pierde"), Event.Seat, Event.OtherSeat);
		case EMatchEventType::DuelWon:
			return FString::Printf(TEXT("el asiento %d GANA el duelo al asiento %d, siendo el %s (resuelto al azar)"), Event.Seat, Event.OtherSeat,
				Event.Value == 1 ? TEXT("retado") : TEXT("retador"));
		case EMatchEventType::DuelLoserSentHome:
			return FString::Printf(TEXT("el asiento %d pierde el duelo sin runas y vuelve a su hogar, %s"), Event.OtherSeat, *TileShortName(Board, Event.Value));
		// S-013: el Vacío y la Vuelta al hogar. El minijuego de escape no existe aún: lo resuelve el decisor al azar.
		case EMatchEventType::Banished:
			return FString::Printf(TEXT("el asiento %d es DESTERRADO al Vacío%s: su ficha sale del tablero"), Event.Seat, Event.Value == 1 ? TEXT(" por la Furia") : TEXT(""));
		case EMatchEventType::VoidEscapePassed:
		case EMatchEventType::VoidEscapeFailed:
			return FString::Printf(TEXT("el asiento %d %s el escape del Vacío en %s (resuelto al azar)"), Event.Seat,
				Event.Type == EMatchEventType::VoidEscapePassed ? TEXT("SUPERA") : TEXT("FALLA y sigue dentro:"), *DifficultyText(Event.Value));
		case EMatchEventType::LeftVoid:
			return FString::Printf(TEXT("el asiento %d SALE del Vacío %s y vuelve a su hogar"), Event.Seat, *VoidExitText(Event.Value));
		case EMatchEventType::SentHome:
			return FString::Printf(TEXT("Vuelta al hogar: el asiento %d vuelve a %s"), Event.Seat, *TileShortName(Board, Event.Value));
		// S-014: los eventos del Warden y sus efectos.
		case EMatchEventType::WardenEventStarted:
		{
			const FWardenEventDef* Def = WardenEvents && WardenEvents->Events.IsValidIndex(Event.Value) ? &WardenEvents->Events[Event.Value] : nullptr;
			const FString Text = Def ? FString::Printf(TEXT("\"%s\""), *Def->Text) : FString::Printf(TEXT("evento %d"), Event.Value);
			// S-016: el canal y el nombre del evento; los de prueba, fuera de los sacos, por su identificador.
			const FString Name = !Def ? FString(TEXT("EL WARDEN"))
				: !Def->bInBag ? FString::Printf(TEXT("EL WARDEN (%s)"), *Def->Id)
				: FString::Printf(TEXT("EL WARDEN, %s \"%s\""), Def->Channel == EWardenChannel::Strike ? TEXT("GOLPE") : TEXT("ESCENA"), *Def->DisplayName());
			return Event.Detail > 1 ? FString::Printf(TEXT("%s: propone un DILEMA al asiento %d: %s (%d opciones)"), *Name, Event.Seat, *Text, Event.Detail)
				: FString::Printf(TEXT("%s: DECRETA (dirige el asiento %d): %s"), *Name, Event.Seat, *Text);
		}
		// S-016: el corte de un golpe y el comentario; las evaluaciones y las decisiones no tienen línea (sí, su registro).
		case EMatchEventType::TurnInterrupted:
			// S-017 (R-PRO-0): también lo corta el fin de la carrera.
			// S-018: y el fin de la partida, que corta también lo que el Warden no corta.
			if (Event.Detail == Soulbound::Match::MatchEndCut)
			{
				return FString::Printf(TEXT("FIN DE LA PARTIDA: CORTA el turno del asiento %d: lo que estaba a medias no se resuelve"), Event.Seat);
			}
			return Event.Detail == Soulbound::Match::RaceEndCut
				? FString::Printf(TEXT("FIN DE LA CARRERA: CORTA el turno del asiento %d %s: %s"), Event.Seat, *TurnPhaseText(Event.Value), *TurnCutLossText(Event.Value))
				: FString::Printf(TEXT("GOLPE: el Warden CORTA el turno del asiento %d %s: %s"), Event.Seat, *TurnPhaseText(Event.Value), *TurnCutLossText(Event.Value));
		case EMatchEventType::WardenRemark:
			return FString(TEXT("el Warden comenta (la frase llega con la story \"La voz del Warden\")"));
		// S-017: el Oráculo del Caos y los disparadores contextuales.
		case EMatchEventType::OracleOpened:
			return FString::Printf(TEXT("ORÁCULO: el asiento %d tiene que consultarlo (%s)"), Event.Seat, *OracleOriginText(Event.Value));
		case EMatchEventType::OracleConsulted:
			return FString::Printf(TEXT("el asiento %d consulta el Oráculo"), Event.Seat);
		case EMatchEventType::OracleIgnored:
			return FString::Printf(TEXT("el asiento %d no consulta a tiempo: el Oráculo habla igual, por ignorarme (sube su INSOLENCIA)"), Event.Seat);
		case EMatchEventType::OracleRefused:
			return FString::Printf(TEXT("el asiento %d (Tifón) RECHAZA la sentencia: el orbe dicta otra"), Event.Seat);
		case EMatchEventType::OracleSentence:
		{
			const FWardenEventDef* Def = WardenEvents && WardenEvents->Events.IsValidIndex(Event.Value) ? &WardenEvents->Events[Event.Value] : nullptr;
			const FString Name = Def ? FString::Printf(TEXT("\"%s\": %s"), *Def->DisplayName(), *Def->Text) : FString::Printf(TEXT("evento %d"), Event.Value);
			return FString::Printf(TEXT("SENTENCIA del Oráculo para el asiento %d%s: %s"), Event.Seat, Event.Detail == 1 ? TEXT(" (la segunda, sin rechazo)") : TEXT(""), *Name);
		}
		case EMatchEventType::OracleEmpty:
			return FString::Printf(TEXT("el orbe no muestra NADA al asiento %d: no hay sentencia que valga"), Event.Seat);
		// Decisión del autor del 23/10/2026: la crónica del juego no lista los disparadores, uno por disparador en cada evaluación la
		// inundaría (como TileEntered desde S-020); van entre paréntesis en la decisión del Warden (DescribeWardenDecision).
		case EMatchEventType::WardenTriggered:
			return FString();
		// S-017 (PR B): las votaciones. Los votos llegan todos al resolverse (R-VOT-3, decisión del autor del 26/10/2026).
		case EMatchEventType::VoteStarted:
		{
			const FWardenEventDef* Def = WardenEvents && WardenEvents->Events.IsValidIndex(Event.Value) ? &WardenEvents->Events[Event.Value] : nullptr;
			return FString::Printf(TEXT("VOTACIÓN \"%s\": todos votan a otro (%d se pueden votar)"), Def ? *Def->DisplayName() : TEXT("?"), Event.Detail);
		}
		case EMatchEventType::VoteCast:
			return FString::Printf(TEXT("el asiento %d votó al asiento %d%s"), Event.Seat, Event.OtherSeat, Event.Detail == 1 ? TEXT(" (al azar: no votó a tiempo)") : TEXT(""));
		case EMatchEventType::VoteResolved:
			return FString::Printf(TEXT("el más votado: asiento %d, con %d votos%s"), Event.Seat, Event.Value, Event.Detail == 1 ? TEXT(" (desempate del Warden)") : TEXT(""));
		case EMatchEventType::WardenEvaluated:
		case EMatchEventType::WardenDecided:
			return FString();
		// S-016 (B): la insolencia con castigo y la Furia. Lo que hace el castigo va en la línea siguiente (EFECTO o BURLA).
		case EMatchEventType::InsolencePunished:
			return FString::Printf(TEXT("CASTIGO por insolencia: el asiento %d llega a %d (%s)"), Event.Seat, Event.Value, *InsolencePunishmentText(Event.Detail));
		case EMatchEventType::FuryUnleashed:
			return FString::Printf(TEXT("FURIA DEL WARDEN: el asiento %d, al Vacío de la Furia (sin Desvío ni Liberación); su insolencia vuelve a 0"), Event.Seat);
		case EMatchEventType::FuryRefusal:
			return FString::Printf(TEXT("el Warden SE NIEGA: %s no sirve en el Vacío de la Furia del asiento %d"), Event.Value == 0 ? TEXT("Desvío (la runa no se gasta)") : TEXT("la Liberación"), Event.Seat);
		case EMatchEventType::DilemmaChosen:
			return Event.Detail == 1
				? FString::Printf(TEXT("el Warden sortea la opción %d del dilema EN CONTRA del asiento %d"), Event.Value + 1, Event.Seat)
				: FString::Printf(TEXT("el asiento %d elige la opción %d del dilema"), Event.Seat, Event.Value + 1);
		case EMatchEventType::WardenIgnored:
			return FString::Printf(TEXT("el asiento %d no contesta a tiempo (%s): sube su INSOLENCIA"), Event.Seat, *ChoiceKindText(Event.Value));
		case EMatchEventType::EffectApplied:
		{
			const FString Target = EffectTargetName(Event.Detail);
			const FString Pointer = Event.OtherSeat == INDEX_NONE ? FString() : FString::Printf(TEXT(", señalado por el asiento %d"), Event.OtherSeat);
			return Event.Seat == INDEX_NONE ? FString::Printf(TEXT("EFECTO: %s"), *EffectName(Event.Value))
				: FString::Printf(TEXT("EFECTO: %s sobre el asiento %d (%s%s)"), *EffectName(Event.Value), Event.Seat, *Target, *Pointer);
		}
		case EMatchEventType::WardenTaunt:
			return FString::Printf(TEXT("BURLA del Warden: %s %s no tiene objetivo válido"), *EffectName(Event.Value), *EffectTargetName(Event.Detail));
		case EMatchEventType::Marked:
			return FString::Printf(TEXT("el asiento %d queda MARCADO hasta la ronda %d: quien le perjudique, juega otra vez"), Event.Seat, Event.Value);
		case EMatchEventType::MarkCollected:
			return FString::Printf(TEXT("el asiento %d COBRA la marca del asiento %d: gana un turno extra"), Event.Seat, Event.OtherSeat);
		case EMatchEventType::MarkExpired:
			return FString::Printf(TEXT("la marca del asiento %d caduca sin cobrarse"), Event.Seat);
		case EMatchEventType::BracerRevealed:
			return FString::Printf(TEXT("el Brazal del asiento %d queda REVELADO hasta la ronda %d"), Event.Seat, Event.Value);
		case EMatchEventType::BracerHidden:
			return FString::Printf(TEXT("el Brazal del asiento %d vuelve a ocultarse"), Event.Seat);
		case EMatchEventType::EchoNightBegan:
			return FString::Printf(TEXT("NOCHE DE LOS ECOS: los Ecos caminan por cualquier Dominio (como mucho hasta la ronda %d)"), Event.Value);
		case EMatchEventType::EchoNightEnded:
			return Event.Detail == static_cast<int32>(EEchoNightEnd::ShadowLaw)
				? FString(TEXT("LEY DE SOMBRAS: termina la Noche de los Ecos"))
				: FString(TEXT("termina la Noche de los Ecos por el tope de rondas: los Ecos, a casa"));
		// S-015: la Prueba del Warden, el Duelo forzado y el Duelo contra el Warden, y el Robo decretado. El minijuego real no
		// existe aún: la prueba y el rendimiento del jugador los resuelve su decisor al azar.
		case EMatchEventType::WardenTrialPending:
			return FString::Printf(TEXT("PRUEBA DEL WARDEN: el asiento %d la jugará al empezar su próximo turno, antes de tirar"), Event.Seat);
		case EMatchEventType::WardenTrialPassed:
			return FString::Printf(TEXT("el asiento %d SUPERA la Prueba del Warden (%s, resuelta al azar): su turno sigue"), Event.Seat, *DifficultyText(Event.Value));
		case EMatchEventType::WardenTrialFailed:
			return FString::Printf(TEXT("el asiento %d FALLA la Prueba del Warden (%s, resuelta al azar): su turno termina sin tirar"), Event.Seat, *DifficultyText(Event.Value));
		case EMatchEventType::WardenTrialCancelled:
			return FString::Printf(TEXT("la Prueba del Warden del asiento %d se ANULA: entra en el Vacío"), Event.Seat);
		case EMatchEventType::RunesSeized:
			return FString::Printf(TEXT("el asiento %d se lleva la runa del Dominio %d del asiento %d (todas, sin trilero)"), Event.Seat, Event.Value, Event.OtherSeat);
		case EMatchEventType::WardenDuelStarted:
			return FString::Printf(TEXT("DUELO CONTRA EL WARDEN: el asiento %d se bate contra el Warden (rinde %d%%) por %s"), Event.Seat, Event.Value,
				Event.Detail == 1 ? TEXT("todas sus runas") : TEXT("una de sus runas"));
		case EMatchEventType::WardenDuelWon:
			return FString::Printf(TEXT("el WARDEN GANA su duelo al asiento %d: lo que estaba en juego vuelve a su Fuente"), Event.Seat);
		case EMatchEventType::WardenDuelLost:
			return FString::Printf(TEXT("el asiento %d GANA el duelo al Warden y no pierde nada"), Event.Seat);
		case EMatchEventType::RuneTakenByWarden:
			return FString::Printf(TEXT("el WARDEN ROBA al asiento %d: su runa del Dominio %d vuelve a su Fuente"), Event.Seat, Event.Value);
		// S-018: el aspirante, el desafío final y la victoria de un jugador. El minijuego real no existe aún: el resultado es al azar.
		case EMatchEventType::AspirantAnnounced:
			return FString::Printf(TEXT("el asiento %d es ASPIRANTE: está en el Altar con las seis runas; en su próximo turno, el desafío final"), Event.Seat);
		case EMatchEventType::FinalChallengeStarted:
			return FString::Printf(TEXT("el asiento %d juega el DESAFÍO FINAL (%s) en vez de su turno"), Event.Seat, *DifficultyText(Event.Value));
		case EMatchEventType::FinalChallengeFailed:
			return FString::Printf(TEXT("el asiento %d FALLA el desafío final (resuelto al azar): el Warden le arranca la runa del Dominio %d, que vuelve a su Fuente, y lo manda a casa"),
				Event.Seat, Event.Value);
		case EMatchEventType::MatchWon:
			return FString::Printf(TEXT("VICTORIA DEL ASIENTO %d: supera el desafío final (resuelto al azar)"), Event.Seat);
		case EMatchEventType::DebugEdit:
			return Event.Seat == INDEX_NONE ? FString::Printf(TEXT("EDICIÓN DE DEPURACIÓN: %s"), *DebugKindName(Event.Value))
				: FString::Printf(TEXT("EDICIÓN DE DEPURACIÓN: %s (asiento %d)"), *DebugKindName(Event.Value), Event.Seat);
		}
		return FString();
	}

	FString DebugKindName(int32 Kind)
	{
		switch (static_cast<EDebugEditKind>(Kind))
		{
		case EDebugEditKind::GiveRune: return TEXT("dar runas");
		case EDebugEditKind::TakeRune: return TEXT("quitar runas");
		case EDebugEditKind::Move: return TEXT("colocar ficha");
		case EDebugEditKind::Bond: return TEXT("vincular");
		case EDebugEditKind::Seal: return TEXT("sellar");
		case EDebugEditKind::Unseal: return TEXT("liberar Sello");
		case EDebugEditKind::EndRace: return TEXT("terminar la carrera");
		case EDebugEditKind::FixRoll: return TEXT("fijar la tirada");
		case EDebugEditKind::FixNextSeat: return TEXT("fijar el siguiente asiento");
		case EDebugEditKind::FixTrial: return TEXT("fijar el resultado del minijuego");
		case EDebugEditKind::FixDuel: return TEXT("fijar el resultado del duelo");
		case EDebugEditKind::Banish: return TEXT("desterrar al Vacío");
		case EDebugEditKind::ReleaseVoid: return TEXT("Liberación del Vacío");
		case EDebugEditKind::SendHome: return TEXT("Vuelta al hogar");
		case EDebugEditKind::LaunchEvent: return TEXT("lanzar un evento del Warden");
		case EDebugEditKind::SetInsolence: return TEXT("fijar la insolencia");
		case EDebugEditKind::StrikeEvent: return TEXT("golpe del Warden ahora");
		case EDebugEditKind::SetClock: return TEXT("adelantar el reloj de partida");
		}
		return TEXT("edición desconocida");
	}

	FString EffectName(int32 Effect)
	{
		static const TCHAR* const Names[] = {
			TEXT("Alto"), TEXT("Revelar Brazal"), TEXT("Consulta forzada"), TEXT("Vuelta al hogar"), TEXT("Turno perdido"), TEXT("Sello"),
			TEXT("Sello contra rival"), TEXT("Marcado"), TEXT("Ley de Sombras"), TEXT("Prueba del Warden"), TEXT("Perder una runa"),
			TEXT("Robo decretado"), TEXT("Duelo forzado"), TEXT("el Vacío"), TEXT("Turno extra"), TEXT("Runa regalada"), TEXT("Liberación"),
			TEXT("Noche de los Ecos"),
		};
		static_assert(UE_ARRAY_COUNT(Names) == Soulbound::Effects::EffectCount, "un nombre por efecto del catálogo");
		return Effect >= 0 && Effect < UE_ARRAY_COUNT(Names) ? FString(Names[Effect]) : FString(TEXT("efecto desconocido"));
	}

	FString EffectTargetName(int32 Target)
	{
		switch (static_cast<EEffectTarget>(Target))
		{
		case EEffectTarget::ActivePlayer: return TEXT("al activo");
		case EEffectTarget::ChosenRival: return TEXT("al rival señalado");
		case EEffectTarget::TwoChosenRivals: return TEXT("a dos rivales señalados");
		case EEffectTarget::Leader: return TEXT("al líder");
		case EEffectTarget::NextPlayer: return TEXT("al siguiente");
		case EEffectTarget::Everyone: return TEXT("a todos");
		case EEffectTarget::RandomPlayer: return TEXT("al azar");
		case EEffectTarget::Echoes: return TEXT("a los Ecos");
		case EEffectTarget::MostVoted: return TEXT("al más votado");
		case EEffectTarget::None: return TEXT("");
		}
		return TEXT("");
	}

	FString DescribeApplication(const FEffectApplication& Application)
	{
		const FString Amount = Application.Amount > 1 ? FString::Printf(TEXT(" ×%d"), Application.Amount) : FString();
		const FString Variant = Application.Variant == EEffectVariant::Chosen ? FString(TEXT(" (a elección)"))
			: Application.Variant == EEffectVariant::AllRunes ? FString(TEXT(" (por todas las runas)")) : FString();
		const FString Target = EffectTargetName(static_cast<int32>(Application.Target));
		return EffectName(static_cast<int32>(Application.Effect)) + Amount + Variant + (Target.IsEmpty() ? FString() : TEXT(" ") + Target);
	}

	FString DescribeOption(const FWardenEventOption& Option)
	{
		return FString::JoinBy(Option.Applications, TEXT(" y luego "), [](const FEffectApplication& Application) { return DescribeApplication(Application); });
	}

	FString DescribeWardenEventStart(EWardenEventStart Result)
	{
		switch (Result)
		{
		case EWardenEventStart::Started: return TEXT("lanzado");
		case EWardenEventStart::UnknownEvent: return TEXT("no hay ningún evento con ese identificador (SbEvents los lista)");
		case EWardenEventStart::NotBetweenTurns: return TEXT("los eventos van entre turnos: con SbStepMode 1, cuando la partida espera el siguiente turno");
		case EWardenEventStart::EventInProgress: return TEXT("ya hay un evento en curso");
		case EWardenEventStart::MatchOver: return TEXT("la partida ha terminado");
		case EWardenEventStart::NotImplemented: return TEXT("usa un efecto que llega en otra story");
		}
		return TEXT("rechazado");
	}

	FString ChoiceKindText(int32 Kind)
	{
		switch (static_cast<EChoiceKind>(Kind))
		{
		case EChoiceKind::Dilemma: return TEXT("el dilema");
		case EChoiceKind::ChosenRival: return TEXT("a quién señala: el efecto cae sobre él");
		case EChoiceKind::GiftRune: return TEXT("qué runa recibe: el regalo se pierde");
		case EChoiceKind::LoseRune: return TEXT("qué runa pierde: una al azar");
		default: return TEXT("una elección");
		}
	}

	FString TurnPhaseText(int32 Phase)
	{
		switch (static_cast<ETurnPhase>(Phase))
		{
		case ETurnPhase::Start: return TEXT("al empezar (Start)");
		case ETurnPhase::PreRoll: return TEXT("antes de tirar (PreRoll)");
		case ETurnPhase::RollAndChoose: return TEXT("mientras elige destino (RollAndChoose)");
		case ETurnPhase::Moving: return TEXT("a medio camino (Moving)");
		case ETurnPhase::MoveAndResolve: return TEXT("tras moverse (MoveAndResolve)");
		default: return TEXT("en una fase que no se corta");
		}
	}

	FString InsolencePunishmentText(int32 Kind)
	{
		switch (static_cast<EInsolencePunishment>(Kind))
		{
		case EInsolencePunishment::Light: return TEXT("castigo leve");
		case EInsolencePunishment::Medium: return TEXT("castigo medio");
		case EInsolencePunishment::Severe: return TEXT("castigo grave");
		case EInsolencePunishment::Fury: return TEXT("la FURIA");
		}
		return TEXT("castigo");
	}

	FString TurnCutLossText(int32 Phase)
	{
		// R-TUR-11: lo que pierde depende de por dónde iba: antes de mover, la tirada; tras mover, la tirada ya se usó.
		switch (static_cast<ETurnPhase>(Phase))
		{
		case ETurnPhase::Start:
		case ETurnPhase::PreRoll: return TEXT("pierde la tirada");
		case ETurnPhase::RollAndChoose: return TEXT("pierde la tirada sin moverse");
		// R-TAB-26 (S-020): la ficha se para en la última casilla en la que entró, que no resuelve.
		case ETurnPhase::Moving: return TEXT("la ficha se para en la casilla en la que estaba, que no resuelve, y pierde el resto del camino");
		case ETurnPhase::MoveAndResolve: return TEXT("pierde el salto o la acción que le quedaban; la ficha se queda donde llegó");
		default: return TEXT("pierde lo que le quedaba del turno");
		}
	}

	FString DescribeWardenDecision(const FMatchEvent& Decided, const TArray<FMatchEvent>& Triggers, bool bViaOracle, const FWardenEventTable* WardenEvents)
	{
		const EWardenDecisionKind Kind = static_cast<EWardenDecisionKind>(Decided.Detail);
		if (Decided.Type != EMatchEventType::WardenDecided || (Kind != EWardenDecisionKind::Strike && Kind != EWardenDecisionKind::Scene))
		{
			return FString();
		}
		const FWardenEventDef* Def = WardenEvents && WardenEvents->Events.IsValidIndex(Decided.Value) ? &WardenEvents->Events[Decided.Value] : nullptr;
		const FString Name = Def ? Def->DisplayName() : FString::Printf(TEXT("evento %d"), Decided.Value);
		const FString Causes = FString::JoinBy(Triggers, TEXT("; "), [](const FMatchEvent& Trigger)
		{
			return Trigger.Seat == INDEX_NONE ? TriggerName(Trigger.Value) : FString::Printf(TEXT("%s: asiento %d"), *TriggerName(Trigger.Value), Trigger.Seat);
		});
		return FString::Printf(TEXT("el Warden decide: %s \"%s\"%s%s"), Kind == EWardenDecisionKind::Strike ? TEXT("golpe") : TEXT("escena"), *Name,
			bViaOracle ? TEXT(" por el Oráculo") : TEXT(""), Causes.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" (%s)"), *Causes));
	}

	FString OracleOriginText(int32 Origin)
	{
		switch (static_cast<EOracleOrigin>(Origin))
		{
		case EOracleOrigin::Node: return TEXT("acabó en un Nodo del Oráculo");
		case EOracleOrigin::Forced: return TEXT("Consulta forzada");
		case EOracleOrigin::Strike: return TEXT("golpe del Warden por el Oráculo");
		}
		return TEXT("origen desconocido");
	}

	FString TriggerName(int32 Trigger)
	{
		switch (static_cast<EWardenTrigger>(Trigger))
		{
		case EWardenTrigger::ClearLeader: return TEXT("Líder claro");
		case EWardenTrigger::ImminentVictory: return TEXT("Victoria inminente");
		case EWardenTrigger::IgnoredPlayer: return TEXT("Jugador ignorado");
		case EWardenTrigger::StalledMatch: return TEXT("Partida estancada");
		case EWardenTrigger::Hoarder: return TEXT("Acaparador");
		}
		return TEXT("disparador desconocido");
	}

	FString DifficultyText(int32 Difficulty)
	{
		switch (static_cast<ESkillDifficulty>(Difficulty))
		{
		case ESkillDifficulty::Easy: return TEXT("fácil");
		case ESkillDifficulty::Normal: return TEXT("normal");
		case ESkillDifficulty::Hard: return TEXT("difícil");
		}
		return TEXT("dificultad desconocida");
	}

	FString VoidExitText(int32 Exit)
	{
		switch (static_cast<EVoidExit>(Exit))
		{
		case EVoidExit::Escaped: return TEXT("por el escape");
		case EVoidExit::Deflected: return TEXT("con Desvío, sacrificando la runa, y juega el turno");
		case EVoidExit::TurnLimit: return TEXT("sin jugar, al acabar su escalera");
		case EVoidExit::Echo: return TEXT("sin jugar, por ser Eco");
		case EVoidExit::Released: return TEXT("por la Liberación");
		}
		return TEXT("por un motivo desconocido");
	}

	FString DescribeEffectResult(EEffectResult Result)
	{
		switch (Result)
		{
		case EEffectResult::Applied: return TEXT("aplicado");
		case EEffectResult::InvalidSeat: return TEXT("ese asiento no existe (0 a 3)");
		case EEffectResult::InVoid: return TEXT("ese asiento está en el Vacío: solo la Liberación lo saca");
		case EEffectResult::InPlay: return TEXT("ese asiento está en juego (su turno, o un duelo o trilero en curso): hazlo entre turnos, con SbStepMode 1");
		case EEffectResult::AlreadyHome: return TEXT("ese asiento ya está en su hogar");
		case EEffectResult::NobodyInVoid: return TEXT("no hay nadie en el Vacío que pueda salir (a quien está en el Vacío de la Furia no le saca una Liberación)");
		case EEffectResult::BondRace: return TEXT("durante la carrera nadie va al Vacío (solo por la Furia)");
		case EEffectResult::NoRunes: return TEXT("ese asiento no tiene runas");
		case EEffectResult::NoReserve: return TEXT("ninguna Fuente tiene reserva");
		case EEffectResult::NoEchoes: return TEXT("no hay Ecos a los que aplicarlo");
		case EEffectResult::NoValidTile: return TEXT("no hay casilla que sellar");
		case EEffectResult::SelfTarget: return TEXT("nadie se marca a sí mismo");
		case EEffectResult::InvalidRune: return TEXT("ese tipo de runa no es una opción");
		case EEffectResult::NotImplemented: return TEXT("ese efecto llega en otra story");
		case EEffectResult::AlreadyActive: return TEXT("ya es de noche");
		case EEffectResult::AlreadyPending: return TEXT("ese asiento ya tiene una Prueba del Warden o una consulta del Oráculo pendiente");
		case EEffectResult::NoTurnToCut: return TEXT("el Alto solo vale cortando un turno, en un golpe");
		}
		return TEXT("rechazado");
	}

	FString DescribeDebugResult(EDebugEditResult Result)
	{
		switch (Result)
		{
		case EDebugEditResult::Applied: return TEXT("aplicada");
		case EDebugEditResult::InvalidSeat: return TEXT("ese asiento no existe (0 a 3)");
		case EDebugEditResult::InvalidTile: return TEXT("esa casilla no existe");
		case EDebugEditResult::InvalidDomain: return TEXT("ese Dominio o tipo de runa no existe (0 a 5)");
		case EDebugEditResult::InvalidAmount: return TEXT("la cantidad debe ser 1 o más");
		case EDebugEditResult::BoardPrimordial: return TEXT("el tablero es Primordial: aún no hay reservas de runas");
		case EDebugEditResult::NoSource: return TEXT("ese Dominio no tiene Fuente de Poder");
		case EDebugEditResult::NotEnoughReserve: return TEXT("la reserva de la Fuente no llega a esa cantidad");
		case EDebugEditResult::NotEnoughRunes: return TEXT("el asiento no tiene tantas runas de ese tipo");
		case EDebugEditResult::NotInBondRace: return TEXT("la partida no está en la carrera por el vínculo");
		case EDebugEditResult::NotLostEcho: return TEXT("ese asiento ya es Elegido");
		case EDebugEditResult::NicheNotFree: return TEXT("el Nicho de ese Dominio no está Libre");
		case EDebugEditResult::NotSealed: return TEXT("esa casilla no está sellada");
		case EDebugEditResult::InvalidRoll: return FString::Printf(TEXT("la tirada debe estar entre 1 y %d"), Soulbound::Debug::MaxFixedRoll());
		}
		return TEXT("rechazada");
	}

	FString DescribeSealResult(ESealResult Result)
	{
		switch (Result)
		{
		case ESealResult::Accepted: return TEXT("aceptado");
		case ESealResult::InvalidTile: return TEXT("esa casilla no existe");
		case ESealResult::NoSealsAllowed: return TEXT("este tablero no admite Sellos (máximo 0)");
		case ESealResult::AlreadySealed: return TEXT("esa casilla ya está sellada");
		case ESealResult::Altar: return TEXT("no se sella el Altar");
		case ESealResult::Niche: return TEXT("no se sella un Nicho");
		case ESealResult::PowerSource: return TEXT("no se sella una Fuente de Poder");
		case ESealResult::Occupied: return TEXT("la casilla está ocupada");
		case ESealResult::CutsOffAltar: return TEXT("dejaría casillas sin camino al Altar");
		}
		return TEXT("rechazado");
	}

	EEventSource EventSource(EMatchEventType Type)
	{
		switch (Type)
		{
		case EMatchEventType::DebugEdit:
			return EEventSource::Debug;
		case EMatchEventType::SealPlaced:
		case EMatchEventType::SealReleased:
		// S-013: desterrar y mandar a casa son efectos del Warden (hoy los piden los comandos).
		case EMatchEventType::Banished:
		case EMatchEventType::SentHome:
		// S-014: los eventos del Warden y lo que dejan sus efectos.
		case EMatchEventType::WardenEventStarted:
		case EMatchEventType::DilemmaChosen:
		case EMatchEventType::WardenIgnored:
		case EMatchEventType::EffectApplied:
		case EMatchEventType::WardenTaunt:
		case EMatchEventType::Marked:
		case EMatchEventType::MarkCollected:
		case EMatchEventType::MarkExpired:
		case EMatchEventType::BracerRevealed:
		case EMatchEventType::BracerHidden:
		case EMatchEventType::EchoNightBegan:
		case EMatchEventType::EchoNightEnded:
		// S-015: la Prueba pendiente o anulada, el duelo contra el Warden y la runa que el Warden se lleva.
		case EMatchEventType::WardenTrialPending:
		case EMatchEventType::WardenTrialCancelled:
		case EMatchEventType::WardenDuelStarted:
		case EMatchEventType::WardenDuelWon:
		case EMatchEventType::WardenDuelLost:
		case EMatchEventType::RuneTakenByWarden:
		// S-016: el corte de un golpe y el comentario.
		case EMatchEventType::TurnInterrupted:
		case EMatchEventType::WardenRemark:
		// S-016 (B): los castigos de insolencia, la Furia y sus negativas.
		case EMatchEventType::InsolencePunished:
		case EMatchEventType::FuryUnleashed:
		case EMatchEventType::FuryRefusal:
		// S-017: el Oráculo y los disparadores.
		case EMatchEventType::OracleOpened:
		case EMatchEventType::OracleIgnored:
		case EMatchEventType::OracleRefused:
		case EMatchEventType::OracleSentence:
		case EMatchEventType::OracleEmpty:
		case EMatchEventType::WardenTriggered:
		case EMatchEventType::VoteStarted:
		case EMatchEventType::VoteResolved:
			return EEventSource::Warden;
		case EMatchEventType::Bonded:
		case EMatchEventType::BoardAwakening:
		case EMatchEventType::HomeAssigned:
		case EMatchEventType::ReturnedHome:
		case EMatchEventType::WardenVictory:
		case EMatchEventType::BondRaceEnded:
		// S-018: el final de la partida, como el de la carrera.
		case EMatchEventType::AspirantAnnounced:
		case EMatchEventType::MatchWon:
			return EEventSource::Race;
		default:
			return EEventSource::Seat;
		}
	}
}
