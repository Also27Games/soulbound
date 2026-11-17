#include "GrimoireBook.h"

#include "BoardViewState.h"

#include "Match/MatchState.h"
#include "Match/VictoryRules.h"

namespace Soulbound::Presentation
{
	namespace
	{
		/** Las dos teclas de pestaña: "flechas izquierda y derecha" con las de por defecto; si no, cada una con su nombre en español donde lo haya. */
		FString TabKeysText(const FString& PreviousTabKey, const FString& NextTabKey)
		{
			if (PreviousTabKey == TEXT("Left") && NextTabKey == TEXT("Right"))
			{
				return TEXT("flechas izquierda y derecha");
			}
			return FString::Printf(TEXT("%s y %s"), *SpanishKeyName(PreviousTabKey), *SpanishKeyName(NextTabKey));
		}

		/** S-048 (R-HUD-18): las dos teclas de página: "flechas arriba y abajo" con las de por defecto; si no, cada una con su nombre en español. */
		FString PageKeysText(const FString& PreviousPageKey, const FString& NextPageKey)
		{
			if (PreviousPageKey == TEXT("Up") && NextPageKey == TEXT("Down"))
			{
				return TEXT("flechas arriba y abajo");
			}
			return FString::Printf(TEXT("%s y %s"), *SpanishKeyName(PreviousPageKey), *SpanishKeyName(NextPageKey));
		}
	}
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
			View.HomeRuneLine = FString::Printf(TEXT("La runa de tu hogar (%s) te da Celeridad: puedes tirar dos dados"), *Domain);
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

	FHowToPlayPage HowToPlayKeysPage(const FGrimoireKeyNames& Keys)
	{
		TArray<FString> Lines;
		// Los nombres de las teclas, en español donde lo haya (corrección del autor del 17/11/2026).
		Lines.Add(FString::Printf(TEXT("%s: abre y cierra el Grimorio (también su botón, abajo a la derecha)"), *SpanishKeyName(Keys.Grimoire)));
		Lines.Add(FString::Printf(TEXT("%s: pestaña anterior y siguiente del Grimorio"), *TabKeysText(Keys.PreviousTab, Keys.NextTab)));
		// S-048 (R-HUD-18, precisión del autor del 17/11/2026): las de página, flechas arriba y abajo por defecto.
		Lines.Add(FString::Printf(TEXT("%s: página anterior y siguiente de Cómo se juega"), *PageKeysText(Keys.PreviousPage, Keys.NextPage)));
		Lines.Add(TEXT("1 a 9 y 0: las opciones del panel de decisión, como sus botones"));
		// S-045 (precisión de R-CAM, decisión del autor del 17/11/2026): Intro empieza la prueba y vuelve al menú al acabar; con la misma tecla, una línea.
		if (SpanishKeyName(Keys.RingStart) == SpanishKeyName(Keys.ReturnToMenu))
		{
			Lines.Add(FString::Printf(TEXT("%s: empieza la prueba del anillo cuando estás listo y, al acabar la partida, vuelve al menú"), *SpanishKeyName(Keys.RingStart)));
		}
		else
		{
			Lines.Add(FString::Printf(TEXT("%s: empieza la prueba del anillo cuando estás listo"), *SpanishKeyName(Keys.RingStart)));
			Lines.Add(FString::Printf(TEXT("%s: al acabar la partida, vuelve al menú"), *SpanishKeyName(Keys.ReturnToMenu)));
		}
		Lines.Add(FString::Printf(TEXT("%s: cambia la vista"), *SpanishKeyName(Keys.View)));
		Lines.Add(FString::Printf(TEXT("%s: oculta y enseña las etiquetas y los números de las casillas"), *SpanishKeyName(Keys.Overlay)));
		// Precisión del autor al aprobar el plan de la ampliación (17/11/2026): una lista, una tecla por elemento.
		FHowToPlayPage Page;
		Page.Title = HowToPlayKeysTitle;
		for (const FString& Line : Lines)
		{
			FHowToPlayBlock Item;
			Item.Kind = EHowToPlayBlockKind::ListItem;
			Item.Text = Line;
			Item.Spans = HowToPlaySpans(Line);
			Page.Blocks.Add(Item);
		}
		Page.Body = HowToPlayBodyText(Page.Blocks);
		return Page;
	}

	FHowToPlayBook HowToPlayBook(const FHowToPlayLoadResult& Loaded, const FGrimoireKeyNames& Keys)
	{
		FHowToPlayBook Book;
		if (!Loaded.IsSuccess())
		{
			// Sin páginas y con el mensaje del cargador: la pestaña lo enseña y el menú lo puede enseñar (R-HUD-18, R-HUD-19).
			Book.ErrorMessage = Loaded.Message.IsEmpty() ? FString(TEXT("No se pudo cargar el texto de cómo se juega")) : Loaded.Message;
			return Book;
		}
		Book.Title = Loaded.BookTitle;
		Book.CoverLine = Loaded.CoverLine;
		Book.Pages = Loaded.Pages;
		Book.Pages.Add(HowToPlayKeysPage(Keys));
		return Book;
	}

	// TODO(R-HUD-18): stubs del commit de los tests en rojo de la doble página; la implementación va en el commit siguiente.
	int32 HowToPlayPagesPerView(bool bBook)
	{
		return 1;
	}

	FVector2f HowToPlayImageSize(float SheetWidth, float MaxHeight, const FVector2f& ImageSize)
	{
		return FVector2f::ZeroVector;
	}

	FGrimoireWatermarks GrimoireWatermarks(const FGrimoireBookLayout& Book, const FVector2f& TextureSize, float SizeFraction)
	{
		return FGrimoireWatermarks();
	}

	FString HowToPlayPageFooter(int32 Page, int32 PageCount, int32 PagesPerView)
	{
		if (PageCount <= 0)
		{
			return FString();
		}
		return FString::Printf(TEXT("%d de %d"), FMath::Clamp(Page, 0, PageCount - 1) + 1, PageCount);
	}

	FString HowToPlayFlatText(const FHowToPlayBook& Book)
	{
		TArray<FString> Blocks;
		for (const FHowToPlayPage& Page : Book.Pages)
		{
			Blocks.Add(Page.Title + TEXT("\n\n") + Page.Body);
		}
		return FString::Join(Blocks, TEXT("\n\n"));
	}

	FGrimoireBookLayout GrimoireBookLayout(const FVector2f& PanelSize, const FVector2f& ImageSize, const FMargin& BorderFraction, float PageInset, float SpineGapFraction)
	{
		FGrimoireBookLayout Layout;
		const FVector2f Panel(FMath::Max(PanelSize.X, 0.f), FMath::Max(PanelSize.Y, 0.f));
		// Una zona que nunca da la vuelta: si los márgenes no caben, se queda en una línea en el centro del hueco.
		const auto Inset = [](const FVector2f& Size, float Left, float Top, float Right, float Bottom)
		{
			FVector2f Min(Left, Top);
			FVector2f Max(Size.X - Right, Size.Y - Bottom);
			if (Max.X < Min.X)
			{
				Min.X = Max.X = (Min.X + Max.X) * 0.5f;
			}
			if (Max.Y < Min.Y)
			{
				Min.Y = Max.Y = (Min.Y + Max.Y) * 0.5f;
			}
			Min = FVector2f(FMath::Clamp(Min.X, 0.f, Size.X), FMath::Clamp(Min.Y, 0.f, Size.Y));
			Max = FVector2f(FMath::Clamp(Max.X, Min.X, Size.X), FMath::Clamp(Max.Y, Min.Y, Size.Y));
			return FBox2f(Min, Max);
		};

		if (ImageSize.X <= KINDA_SMALL_NUMBER || ImageSize.Y <= KINDA_SMALL_NUMBER)
		{
			// Sin la imagen del libro: la zona de hoy, de una columna (S-044).
			Layout.Content = Inset(Panel, GrimoireFlatPadding.X, GrimoireFlatPadding.Y, GrimoireFlatPadding.X, GrimoireFlatPadding.Y);
			Layout.LeftPage = Layout.Content;
			Layout.RightPage = Layout.Content;
			return Layout;
		}

		// Escala uniforme: la menor de las dos que llevan la imagen al panel, para que las esquinas y el lomo no se deformen.
		const float Scale = FMath::Min(Panel.X / ImageSize.X, Panel.Y / ImageSize.Y);
		Layout.bBook = true;
		Layout.BrushSize = ImageSize * Scale;
		const float Margin = FMath::Max(PageInset, 0.f);
		Layout.Content = Inset(Panel, FMath::Max(BorderFraction.Left, 0.f) * Layout.BrushSize.X + Margin, FMath::Max(BorderFraction.Top, 0.f) * Layout.BrushSize.Y + Margin,
			FMath::Max(BorderFraction.Right, 0.f) * Layout.BrushSize.X + Margin, FMath::Max(BorderFraction.Bottom, 0.f) * Layout.BrushSize.Y + Margin);

		// El lomo, en el centro del panel; su hueco, una fracción del ancho del contenido. Ninguna página lo pisa.
		const float Center = Panel.X * 0.5f;
		const float HalfGap = FMath::Clamp(SpineGapFraction, 0.f, 1.f) * (Layout.Content.Max.X - Layout.Content.Min.X) * 0.5f;
		const float LeftEnd = FMath::Clamp(Center - HalfGap, Layout.Content.Min.X, Layout.Content.Max.X);
		const float RightStart = FMath::Clamp(Center + HalfGap, Layout.Content.Min.X, Layout.Content.Max.X);
		Layout.LeftPage = FBox2f(Layout.Content.Min, FVector2f(LeftEnd, Layout.Content.Max.Y));
		Layout.RightPage = FBox2f(FVector2f(RightStart, Layout.Content.Min.Y), Layout.Content.Max);
		return Layout;
	}

	bool HowToPlayImageBeside(float ColumnsWidth, float MinTextWidth)
	{
		return ColumnsWidth * 0.5f >= MinTextWidth;
	}

	float GrimoireColumnTextWidth(float ColumnWidth, float Indent)
	{
		// Corrección 1 del autor del 17/11/2026: el ancho de su propia columna, menos la barra de desplazamiento y la sangría de la línea.
		return FMath::Max(ColumnWidth - GrimoireScrollRoom - FMath::Max(Indent, 0.f), 1.f);
	}

	FString FGrimoireFootHints::OneLine() const
	{
		return Right.IsEmpty() ? Left : Left + TEXT("; ") + Right;
	}

	FGrimoireFootHints GrimoireFootHints(const FGrimoireKeyNames& Keys, EGrimoireTab Tab)
	{
		// Corrección 2 del autor del 17/11/2026: cerrar y pestañas al pie de la página izquierda; páginas al pie de la derecha. Corrección 5: las
		// páginas, solo en la pestaña que las tiene.
		FGrimoireFootHints Foot;
		Foot.Left = GrimoireKeysHint(Keys.Grimoire, Keys.PreviousTab, Keys.NextTab);
		if (Tab == EGrimoireTab::HowToPlay)
		{
			Foot.Right = FString::Printf(TEXT("%s, páginas"), *PageKeysText(Keys.PreviousPage, Keys.NextPage));
		}
		return Foot;
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


	FString GrimoireKeysHint(const FString& GrimoireKey, const FString& PreviousTabKey, const FString& NextTabKey)
	{
		// Corrección del autor del 17/11/2026 tras PIE: decía "Left y Right", los nombres que da Unreal. S-045 (R-HUD-16): sin la H, que ya no existe.
		return FString::Printf(TEXT("%s para cerrar; %s, pestañas"), *SpanishKeyName(GrimoireKey), *TabKeysText(PreviousTabKey, NextTabKey));
	}

	FString GrimoireButtonLabel(const FString& KeyName)
	{
		return FString::Printf(TEXT("Grimorio (%s)"), *SpanishKeyName(KeyName));
	}

	bool IsGrimoireButtonShown(bool bPrologue, bool bMatchOver, bool bWardenLineOnScreen)
	{
		return !bPrologue && !bMatchOver && !bWardenLineOnScreen;
	}

	void FGrimoireTabState::NextPage(int32 PageCount, int32 PagesPerView)
	{
		// R-HUD-18: no es cíclico; en la última se queda.
		Page = PageCount <= 0 ? 0 : FMath::Clamp(Page + 1, 0, PageCount - 1);
	}

	void FGrimoireTabState::PreviousPage(int32 PageCount, int32 PagesPerView)
	{
		// R-HUD-18: en la primera se queda.
		Page = PageCount <= 0 ? 0 : FMath::Clamp(Page - 1, 0, PageCount - 1);
	}
}
