#include "GrimoireBook.h"

#include "Match/MatchState.h"
#include "Match/VictoryRules.h"

namespace Soulbound::Presentation
{
	const TArray<EGrimoireTab>& GrimoireTabs()
	{
		static const TArray<EGrimoireTab> Tabs{EGrimoireTab::You, EGrimoireTab::Table, EGrimoireTab::Board, EGrimoireTab::HowToPlay, EGrimoireTab::Chronicle};
		return Tabs;
	}

	FString GrimoireTabTitle(EGrimoireTab Tab)
	{
		switch (Tab)
		{
		case EGrimoireTab::You: return TEXT("Tú");
		case EGrimoireTab::Table: return TEXT("La mesa");
		case EGrimoireTab::Board: return TEXT("El tablero");
		case EGrimoireTab::HowToPlay: return TEXT("Cómo se juega");
		case EGrimoireTab::Chronicle: return TEXT("Crónica");
		}
		return FString();
	}

	void FGrimoireTabState::Open()
	{
		// Decisión del autor del 16/11/2026: siempre en "Tú", no recuerda la última.
		Active = EGrimoireTab::You;
	}

	void FGrimoireTabState::Next()
	{
		const TArray<EGrimoireTab>& Tabs = GrimoireTabs();
		const int32 Index = Tabs.Find(Active);
		Active = Tabs[(FMath::Max(Index, 0) + 1) % Tabs.Num()];
	}

	void FGrimoireTabState::Previous()
	{
		const TArray<EGrimoireTab>& Tabs = GrimoireTabs();
		const int32 Index = Tabs.Find(Active);
		Active = Tabs[(FMath::Max(Index, 0) + Tabs.Num() - 1) % Tabs.Num()];
	}

	void FGrimoireTabState::Select(EGrimoireTab Tab)
	{
		Active = Tab;
	}

	FGrimoireYouView GrimoireYouView(const FMatchState& State, int32 HumanSeat, const FGrimoireTexts& Texts, const TArray<FLinearColor>& SeatColors)
	{
		FGrimoireYouView View;
		View.PowerLines = GrimoirePowerLines(Texts);
		if (!State.Seats.IsValidIndex(HumanSeat))
		{
			return View;
		}
		// La tarjeta es la misma fila que pintaba el Grimorio de S-027 (se reutiliza, no se reescribe).
		const TArray<FGrimoireRow> Rows = GrimoireRows(State, HumanSeat, Texts, SeatColors);
		if (Rows.IsValidIndex(HumanSeat))
		{
			View.Row = Rows[HumanSeat];
		}
		const FSeatState& Seat = State.Seats[HumanSeat];
		View.InsolenceLine = FString::Printf(TEXT("Insolencia: %d de %d"), Seat.Insolence, Soulbound::Match::MaxInsolence);
		// R-ECO-7: un Eco tras la carrera tiene hogar, y la runa de su hogar es lo único que le da un poder.
		if (Seat.Role != ESeatRole::Elect && Seat.DomainId != INDEX_NONE)
		{
			const FString Name = State.Board.Domains.IsValidIndex(Seat.DomainId) ? State.Board.Domains[Seat.DomainId].Name : FString();
			const FString Domain = Name.IsEmpty() ? FString::Printf(TEXT("D%d"), Seat.DomainId) : FString::Printf(TEXT("D%d, %s"), Seat.DomainId, *Name);
			View.HomeRuneLine = FString::Printf(TEXT("La runa de tu hogar (%s) te da Celeridad: tiras dos dados"), *Domain);
		}
		return View;
	}

	TArray<FLegendRow> GrimoireTableRows(const FMatchState& State, int32 HumanSeat, const TArray<bool>& ForgottenShown, const TArray<FLinearColor>& SeatColors,
		const TArray<FLinearColor>& DomainColors)
	{
		// Corrección del autor del 17/11/2026: de cada asiento, lo mismo que la leyenda (SeatLegendText y el Brazal, con sus filtros), y la marca de
		// aspirante, que es pública (el halo bajo el Altar).
		FLegendRow Players;
		Players.Title = TEXT("Jugadores. Disco gris: Eco. Disco de color: Elegido de ese Dominio. A quien le toca le late un aro");
		for (int32 Seat = 0; Seat < State.Seats.Num(); ++Seat)
		{
			FLegendChip Chip;
			Chip.Color = SeatColors.IsValidIndex(Seat) ? SeatColors[Seat] : FLinearColor::White;
			Chip.Text = SeatLegendText(State, Seat, HumanSeat) + (Soulbound::Rules::IsAspirant(State, Seat) ? FString(TableAspirantMark) : FString())
				+ TEXT("\nRunas: ") + BracerSummary(State, Seat, HumanSeat);
			Players.Chips.Add(Chip);
		}
		FLegendRow Domains;
		Domains.Title = TEXT("Los seis Dominios y su Elegido");
		for (int32 Id = 0; Id < State.Board.Domains.Num(); ++Id)
		{
			FLegendChip Chip;
			Chip.Color = DomainColors.IsValidIndex(Id) ? DomainColors[Id] : FLinearColor::Gray;
			Chip.Text = DomainLegendText(State, Id, ForgottenShown.IsValidIndex(Id) && ForgottenShown[Id]);
			Domains.Chips.Add(Chip);
		}
		return {Players, Domains};
	}

	FString GrimoireHowToPlayText(const FHowToPlayLoadResult& Loaded, const FGrimoireKeyNames& Keys)
	{
		// Sin archivo (o con un título que falta), el mensaje del cargador: la pestaña nunca queda vacía.
		const FString Body = Loaded.IsSuccess() ? HowToPlayDisplayText(Loaded.Text).TrimEnd() : Loaded.Message;
		TArray<FString> Lines;
		Lines.Add(TEXT("Teclas"));
		Lines.Add(FString::Printf(TEXT("%s: abre y cierra el Grimorio (también su botón, abajo a la derecha)"), *Keys.Grimoire));
		Lines.Add(FString::Printf(TEXT("%s y %s: pestaña anterior y siguiente del Grimorio"), *Keys.PreviousTab, *Keys.NextTab));
		Lines.Add(TEXT("1 a 9 y 0: las opciones del panel de decisión, como sus botones"));
		Lines.Add(FString::Printf(TEXT("%s: empieza la prueba del anillo cuando estás listo"), *Keys.RingStart));
		Lines.Add(FString::Printf(TEXT("%s: leyenda o crónica en la columna"), *Keys.ColumnTab));
		Lines.Add(FString::Printf(TEXT("%s: oculta y enseña los paneles"), *Keys.Panels));
		Lines.Add(FString::Printf(TEXT("%s: cambia la vista"), *Keys.View));
		return Body + TEXT("\n\n") + FString::Join(Lines, TEXT("\n"));
	}

	bool FGrimoireCloseWatch::Observe(const TOptional<FDecisionView>& Panel, bool bRingOpen)
	{
		// Una decisión propia es lo que no es espera ni resultado (IsOwnDecisionShown); bloqueada o no, cuenta (precisión del autor del 17/11/2026).
		const bool bOwn = IsOwnDecisionShown(Panel, bRingOpen);
		// Con un prefijo, para que una identidad vacía también se distinga de "ninguna".
		const FString Own = bOwn ? TEXT("propia:") + Panel->Identity : FString();
		const bool bNewDecision = bOwn && Own != LastOwnDecision;
		const bool bNewRing = bRingOpen && !bLastRingOpen;
		LastOwnDecision = Own;
		bLastRingOpen = bRingOpen;
		return bNewDecision || bNewRing;
	}

	bool CanOpenGrimoire(bool bRingOpen)
	{
		return !bRingOpen;
	}

	FString GrimoireButtonLabel(const FString& KeyName)
	{
		return FString::Printf(TEXT("Grimorio (%s)"), *KeyName);
	}

	bool IsGrimoireButtonShown(bool bPrologue, bool bMatchOver, bool bWardenLineOnScreen)
	{
		return !bPrologue && !bMatchOver && !bWardenLineOnScreen;
	}
}
