#include "WardenLineQueue.h"

#include "EventText.h"
#include "Match/MatchState.h"
#include "Warden/WardenLines.h"

namespace Soulbound::Presentation
{
	void FWardenLineQueue::Advance(double NowReal)
	{
		// Sin empezar (nadie la ha pedido aún, o el telón está bajado), la primera no cuenta: nada caduca.
		if (ShownSince < 0.0)
		{
			return;
		}
		// Pasa a la siguiente cada vez que la actual cumple su tiempo; sin siguiente, la cola se vacía. La siguiente empieza donde acabó la anterior.
		while (!Pending.IsEmpty() && NowReal - ShownSince >= Pending[0].Seconds)
		{
			ShownSince += Pending[0].Seconds;
			Pending.RemoveAt(0);
			if (Pending.IsEmpty())
			{
				ShownSince = -1.0;
				return;
			}
			++Shown;
		}
	}

	void FWardenLineQueue::Push(const FWardenLineItem& Item, double NowReal)
	{
		// Lo que ya cumplió su tiempo no cuenta para lo que sigue: una burla dicha cuando la frase ya terminó se enseña (R-VOZ-7).
		Advance(NowReal);
		// R-VOZ-7 (S-041): una burla no tapa una frase protegida, ni la que se enseña ni las que esperan; se descarta para siempre.
		if (Item.bRemark && Pending.ContainsByPredicate([](const FWardenLineItem& Waiting) { return Waiting.bShielded; }))
		{
			return;
		}
		// Una frase dicha más tarde que las que esperan las sustituye ("hasta la siguiente línea"); en el mismo instante, hace cola.
		if (!Pending.IsEmpty() && Item.GameClock > LastGameClock)
		{
			Pending.Reset();
			ShownSince = -1.0;
		}
		LastGameClock = Item.GameClock;
		Pending.Add(Item);
	}

	FWardenLineItem FWardenLineQueue::Current(double NowReal)
	{
		if (Pending.IsEmpty())
		{
			return FWardenLineItem();
		}
		if (ShownSince < 0.0)
		{
			ShownSince = NowReal;
			++Shown;
		}
		Advance(NowReal);
		return Pending.IsEmpty() ? FWardenLineItem() : Pending[0];
	}

	double WardenLineDuration(const FString& Text, const FString& CutNote, double MinSeconds, double SecondsPerChar)
	{
		// R-VOZ-6: el mayor entre el mínimo y lo que se tarda en leer; en un corte se leen la frase y la línea pequeña.
		const int32 Characters = Text.Len() + CutNote.Len();
		return FMath::Max(MinSeconds, SecondsPerChar > 0.0 ? Characters * SecondsPerChar : 0.0);
	}

	bool LineHoldsTheTable(const FString& Key)
	{
		// Los comentarios son el Warden sin materializarse (R-COM-1); las instrucciones son de cada máquina y en red el anfitrión no
		// para la mesa por lo que ve un cliente.
		if (Key.StartsWith(TEXT("comentario:")) || Key.StartsWith(TEXT("instruccion:")))
		{
			return false;
		}
		// Los avisos del final no paran el reloj que anuncian; con la victoria, el fin por tiempo y nadie vinculado la partida ya terminó.
		return Key != Soulbound::Warden::LineLastMinute && Key != Soulbound::Warden::LineTenSeconds && Key != Soulbound::Warden::LineVictory
			&& Key != Soulbound::Warden::LineTimeOut && Key != Soulbound::Warden::LineNobodyBonded;
	}

	namespace
	{
		/** Un corte de turno emparejado (o no) con su frase (R-VOZ-8). */
		struct FTurnCut
		{
			/** El asiento cortado. */
			int32 Seat = INDEX_NONE;
			/** Si lo corta el Warden (golpe o escena): palabra de corte y oscurecido. Si no, es el fin de la carrera. */
			bool bWarden = false;
			/** La línea pequeña. */
			FString Note;
		};

		/** "Tu turno se corta" o "Se corta el turno del asiento N", con el número de asiento de WardenLinePrefix. */
		FString CutOpening(int32 Seat, int32 HumanSeat)
		{
			return Seat == HumanSeat ? FString(TEXT("Tu turno se corta")) : FString::Printf(TEXT("Se corta el turno del asiento %d"), Seat);
		}

		/** La palabra de corte de la tabla (su primera variante, sin acotaciones, sin sorteo); vacía si la tabla no la trae. */
		FString CutWord(const FMatchState& State)
		{
			const int32 Entry = State.WardenLines.IndexOf(CutWordKey);
			if (!State.WardenLines.Entries.IsValidIndex(Entry) || State.WardenLines.Entries[Entry].Lines.IsEmpty())
			{
				return FString();
			}
			return Soulbound::Rules::StripLineCues(State.WardenLines.Entries[Entry].Lines[0]);
		}

		/** La clave de la frase de un WardenSpoke; vacía si la entrada no existe. */
		FString SpokenKey(const FMatchState& State, const FMatchEvent& Spoke)
		{
			return State.WardenLines.Entries.IsValidIndex(Spoke.Value) ? State.WardenLines.Entries[Spoke.Value].Id : FString();
		}

		/**
		 * Busca la frase del corte de la posición CutIndex (R-VOZ-8), entre lo que el motor deja en el mismo instante y antes del siguiente corte. Del
		 * Warden: el evento que entra en el hueco (WardenEventStarted) y la frase de ese evento (`evento:<id>`), no la primera frase que haya (al pasar
		 * el turno se puede decir un castigo de insolencia); o la consulta del cortado abierta por golpe, que no trae frase. Del fin de la carrera: su
		 * frase. Deja en OutCut el corte con su línea pequeña y devuelve la posición de la frase, o INDEX_NONE si no la hay.
		 */
		int32 PairCut(const FMatchState& State, int32 CutIndex, int32 HumanSeat, FTurnCut& OutCut)
		{
			const TArray<FMatchEvent>& Log = State.EventLog;
			const FMatchEvent& Cut = Log[CutIndex];
			OutCut.Seat = Cut.Seat;
			OutCut.bWarden = Cut.Detail == INDEX_NONE;
			const FString Opening = CutOpening(Cut.Seat, HumanSeat);
			if (!OutCut.bWarden)
			{
				OutCut.Note = Opening + TEXT(": la carrera ha terminado.");
			}
			else
			{
				OutCut.Note = Opening + TEXT(".");
			}
			int32 EventIndex = INDEX_NONE;
			for (int32 Index = CutIndex + 1; Index < Log.Num() && Log[Index].ClockSeconds == Cut.ClockSeconds; ++Index)
			{
				const FMatchEvent& Event = Log[Index];
				if (Event.Type == EMatchEventType::TurnInterrupted)
				{
					break;
				}
				if (!OutCut.bWarden)
				{
					const FString Key = Event.Type == EMatchEventType::WardenSpoke ? SpokenKey(State, Event) : FString();
					if (Key == Soulbound::Warden::LineRaceEnd || Key == Soulbound::Warden::LineRaceEndAll)
					{
						return Index;
					}
					continue;
				}
				if (Event.Type == EMatchEventType::OracleOpened && Event.Value == static_cast<int32>(EOracleOrigin::Strike) && Event.Seat == Cut.Seat)
				{
					// El Oráculo como golpe: sin frase; la línea es la palabra de corte sola.
					OutCut.Note = Opening + (Cut.Seat == HumanSeat ? TEXT(". El Oráculo te llama.") : TEXT(". El Oráculo le llama."));
					return INDEX_NONE;
				}
				if (Event.Type == EMatchEventType::WardenEventStarted && EventIndex == INDEX_NONE && State.WardenEvents.Events.IsValidIndex(Event.Value))
				{
					// Lo decretado: al humano, el texto del evento (escrito para quien lo recibe). A otro asiento, solo que se le corta el turno
					// (corrección del autor del 16/11/2026 tras PIE: el nombre del evento, suelto, no explicaba nada).
					EventIndex = Event.Value;
					const FString What = Cut.Seat == HumanSeat ? State.WardenEvents.Events[EventIndex].Text : FString();
					OutCut.Note = What.IsEmpty() ? Opening + TEXT(".") : Opening + TEXT(". ") + What;
					continue;
				}
				if (Event.Type == EMatchEventType::WardenSpoke && EventIndex != INDEX_NONE
					&& SpokenKey(State, Event) == Soulbound::Rules::WardenEventKey(State.WardenEvents.Events[EventIndex].Id))
				{
					return Index;
				}
			}
			return INDEX_NONE;
		}

		/** Las líneas pequeñas de la Noche de los Ecos (R-VOZ-10, S-043): no son de un corte, no llevan palabra de corte ni oscurecido. */
		const TCHAR* const EchoNightBeginNote = TEXT("Empieza la Noche de los Ecos: los Ecos caminan por cualquier Dominio.");
		const TCHAR* const EchoNightEndNote = TEXT("Termina la Noche de los Ecos: los Ecos vuelven a su hogar.");

		/** La línea pequeña de un EchoNightBegan o un EchoNightEnded; vacía para cualquier otro evento. */
		FString EchoNightNote(const FMatchEvent& Event)
		{
			if (Event.Type == EMatchEventType::EchoNightBegan)
			{
				return EchoNightBeginNote;
			}
			return Event.Type == EMatchEventType::EchoNightEnded ? FString(EchoNightEndNote) : FString();
		}

		/**
		 * Busca la frase con que empieza o termina la Noche del evento de la posición NightIndex (R-VOZ-10), en su mismo instante. Por el tope de
		 * rondas, la que dice el motor justo después (`programado:fin-noche-ecos`, R-VOZ-9). Si no, la abre o la cierra un evento del Warden (la Noche,
		 * la Ley de Sombras, Amanecer o cualquier otro con su efecto): el último WardenEventStarted antes de ella y la frase de ese evento
		 * (`evento:<id>`), que va justo después de anunciarlo o, por el Oráculo, antes. Devuelve la posición de la frase, o INDEX_NONE si no la hay.
		 */
		int32 PairEchoNight(const FMatchState& State, int32 NightIndex)
		{
			const TArray<FMatchEvent>& Log = State.EventLog;
			const FMatchEvent& Night = Log[NightIndex];
			const double Clock = Night.ClockSeconds;
			if (Night.Type == EMatchEventType::EchoNightEnded && Night.Detail == static_cast<int32>(EEchoNightEnd::RoundLimit))
			{
				for (int32 Index = NightIndex + 1; Index < Log.Num() && Log[Index].ClockSeconds == Clock; ++Index)
				{
					if (SpokenKey(State, Log[Index]) == Soulbound::Warden::LineEchoNightEnd)
					{
						return Index;
					}
				}
				return INDEX_NONE;
			}
			int32 Started = INDEX_NONE;
			for (int32 Index = NightIndex - 1; Index >= 0 && Log[Index].ClockSeconds == Clock; --Index)
			{
				if (Log[Index].Type == EMatchEventType::WardenEventStarted)
				{
					Started = Index;
					break;
				}
			}
			if (Started == INDEX_NONE || !State.WardenEvents.Events.IsValidIndex(Log[Started].Value))
			{
				return INDEX_NONE;
			}
			const FString Key = Soulbound::Rules::WardenEventKey(State.WardenEvents.Events[Log[Started].Value].Id);
			for (int32 Index = Started + 1; Index < NightIndex; ++Index)
			{
				if (SpokenKey(State, Log[Index]) == Key)
				{
					return Index;
				}
			}
			// Por el Oráculo, la frase de la sentencia va antes del evento (RevealSentence); sin pasar a otro evento anterior.
			for (int32 Index = Started - 1; Index >= 0 && Log[Index].ClockSeconds == Clock && Log[Index].Type != EMatchEventType::WardenEventStarted; --Index)
			{
				if (SpokenKey(State, Log[Index]) == Key)
				{
					return Index;
				}
			}
			return INDEX_NONE;
		}

		/** Añade la línea pequeña Note debajo de la que ya hubiera (R-VOZ-10: primero la del corte, debajo la de la Noche). */
		void AppendNote(FString& Notes, const FString& Note)
		{
			Notes = Notes.IsEmpty() ? Note : Notes + TEXT("\n") + Note;
		}
	}

	TArray<FWardenLineItem> WardenLineItems(const FMatchState& State, int32 FromIndex, int32 HumanSeat, double MinSeconds, double SecondsPerChar)
	{
		const TArray<FMatchEvent>& Log = State.EventLog;
		const int32 Start = FMath::Max(FromIndex, 0);
		const FString Word = CutWord(State);

		// Primero se empareja cada corte con su frase: la frase lleva el corte, y el corte sin frase es una línea propia en su sitio del registro.
		TMap<int32, FTurnCut> CutOfSpoke;
		TMap<int32, FTurnCut> LoneCuts;
		for (int32 Index = Start; Index < Log.Num(); ++Index)
		{
			const FMatchEvent& Event = Log[Index];
			// El corte por el fin de la partida (Detail 2) no lleva nada (R-VOZ-8).
			if (Event.Type != EMatchEventType::TurnInterrupted || (Event.Detail != INDEX_NONE && Event.Detail != Soulbound::Match::RaceEndCut))
			{
				continue;
			}
			FTurnCut Cut;
			const int32 Spoke = PairCut(State, Index, HumanSeat, Cut);
			if (Spoke != INDEX_NONE && !CutOfSpoke.Contains(Spoke))
			{
				CutOfSpoke.Add(Spoke, Cut);
			}
			else
			{
				LoneCuts.Add(Index, Cut);
			}
		}

		// R-VOZ-10 (S-043): la Noche cuelga su línea pequeña de su principio y su final reales, sea cual sea el evento que la abra o la cierre, debajo
		// de la frase con que empieza o termina; sin frase (un evento sin ella o una partida sin voz), la línea pequeña sale sola en su sitio
		// (decisión del autor del 17/11/2026). Una frase ya mirada en otra pasada no se repite.
		TMap<int32, FString> NightNoteOfSpoke;
		TMap<int32, FString> LoneNightNotes;
		for (int32 Index = Start; Index < Log.Num(); ++Index)
		{
			const FString Note = EchoNightNote(Log[Index]);
			if (Note.IsEmpty())
			{
				continue;
			}
			const int32 Spoke = PairEchoNight(State, Index);
			if (Spoke == INDEX_NONE || SpokenText(State.WardenLines, Log[Spoke]).IsEmpty())
			{
				LoneNightNotes.Add(Index, Note);
			}
			else if (Spoke >= Start)
			{
				AppendNote(NightNoteOfSpoke.FindOrAdd(Spoke), Note);
			}
		}

		TArray<FWardenLineItem> Items;
		for (int32 Index = Start; Index < Log.Num(); ++Index)
		{
			const FMatchEvent& Event = Log[Index];
			if (const FString* LoneNight = LoneNightNotes.Find(Index))
			{
				// Sin frase: la línea pequeña sola, sin prefijo; ni detiene la mesa ni está protegida de las burlas (decisión del autor del 17/11/2026).
				FWardenLineItem Item;
				Item.CutNote = *LoneNight;
				Item.GameClock = Event.ClockSeconds;
				Item.Seconds = WardenLineDuration(Item.Text, Item.CutNote, MinSeconds, SecondsPerChar);
				Items.Add(Item);
				continue;
			}
			if (const FTurnCut* Lone = LoneCuts.Find(Index))
			{
				// Sin frase (el Oráculo como golpe): la palabra de corte sola, a quien se corta. No detiene la mesa (el plazo del Oráculo no se mueve),
				// pero un corte del Warden está protegido de las burlas mientras dura.
				FWardenLineItem Item;
				Item.Text = Lone->bWarden && !Word.IsEmpty() ? WardenLinePrefix(Lone->Seat, HumanSeat) + TEXT(" ") + Word : FString();
				Item.CutNote = Lone->Note;
				Item.GameClock = Event.ClockSeconds;
				Item.bShielded = Lone->bWarden;
				Item.bWardenCut = Lone->bWarden;
				Item.Seconds = WardenLineDuration(Item.Text, Item.CutNote, MinSeconds, SecondsPerChar);
				Items.Add(Item);
				continue;
			}
			if (Event.Type != EMatchEventType::WardenSpoke)
			{
				continue;
			}
			const FString Spoken = SpokenText(State.WardenLines, Event);
			if (Spoken.IsEmpty())
			{
				continue;
			}
			const FString Key = SpokenKey(State, Event);
			const FTurnCut* Cut = CutOfSpoke.Find(Index);
			FWardenLineItem Item;
			// R-VOZ-8: tras el prefijo, la palabra de corte, y la frase ("Warden (a ti): ¡Quietos! …"; respuesta del autor al plan). En un corte del
			// Warden el prefijo es el del asiento cortado, no el de la frase del evento, que el motor dice a la mesa (corrección de un defecto de S-041,
			// autor, 16/11/2026, en S-042); con o sin palabra de corte. Las demás frases, incluida la del fin de la carrera, llevan el suyo.
			const bool bWardenCut = Cut && Cut->bWarden;
			const FString Lead = bWardenCut && !Word.IsEmpty() ? Word + TEXT(" ") : FString();
			Item.Text = WardenLinePrefix(bWardenCut ? Cut->Seat : Event.Seat, HumanSeat) + TEXT(" ") + Lead + Spoken;
			Item.CutNote = Cut ? Cut->Note : FString();
			if (const FString* NightNote = NightNoteOfSpoke.Find(Index))
			{
				// R-VOZ-10: si la frase además corta un turno, primero lo del corte y debajo lo de la Noche (decisión del autor del 17/11/2026).
				AppendNote(Item.CutNote, *NightNote);
			}
			Item.GameClock = Event.ClockSeconds;
			Item.bHoldsTable = LineHoldsTheTable(Key);
			Item.bShielded = Item.bHoldsTable;
			Item.bRemark = Key.StartsWith(TEXT("comentario:"));
			Item.bWardenCut = Cut && Cut->bWarden;
			Item.Seconds = WardenLineDuration(Item.Text, Item.CutNote, MinSeconds, SecondsPerChar);
			Items.Add(Item);
		}
		return Items;
	}

	double OpeningSeconds(const FMatchState& State, int32 HumanSeat, double MinSeconds, double SecondsPerChar)
	{
		// La apertura es lo que StartMatch dice en el instante cero (inicio, corrupciones y orden); nada más se dice en él.
		double Seconds = 0.0;
		for (const FWardenLineItem& Item : WardenLineItems(State, 0, HumanSeat, MinSeconds, SecondsPerChar))
		{
			if (Item.GameClock == 0.0)
			{
				Seconds += Item.Seconds;
			}
		}
		return Seconds;
	}

	double WardenWaitSeconds(const FMatchState& State, int32 FromIndex, int32 HumanSeat, double MinSeconds, double SecondsPerChar)
	{
		double Seconds = 0.0;
		for (const FWardenLineItem& Item : WardenLineItems(State, FromIndex, HumanSeat, MinSeconds, SecondsPerChar))
		{
			// Las del instante cero son la apertura: las cuenta el prólogo (OpeningSeconds), no la espera.
			if (Item.bHoldsTable && Item.GameClock != 0.0)
			{
				Seconds += Item.Seconds;
			}
		}
		return Seconds;
	}

	float CutDimOpacity(double NowReal, double CutStartReal, float MaxOpacity, int32 PulseCount, double PulseSeconds)
	{
		if (CutStartReal < 0.0 || PulseCount <= 0 || PulseSeconds <= 0.0 || NowReal < CutStartReal)
		{
			return 0.f;
		}
		// Corrección del autor del 16/11/2026: varios pulsos seguidos. Cada uno aparece de golpe con el máximo y baja en línea recta hasta cero.
		const double Elapsed = (NowReal - CutStartReal) / PulseSeconds;
		if (Elapsed >= PulseCount)
		{
			return 0.f;
		}
		const double InPulse = Elapsed - FMath::FloorToDouble(Elapsed);
		return static_cast<float>(MaxOpacity * (1.0 - InPulse));
	}
}
