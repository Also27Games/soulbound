#include "GrimoireBook.h"

#include "BoardViewState.h"

#include "Engine/Texture2D.h"

#include "Match/EffectRules.h"
#include "Match/MatchState.h"
#include "Match/RuneRules.h"
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

		/**
		 * R-ECO-7: un Eco tras la carrera tiene hogar, y la runa de su hogar le da Celeridad. Lo pone en la runa de su hogar de su fila del Grimorio,
		 * con su descripción, como las de un Elegido. Lo usan la hoja del jugador y, desde los retoques de "La mesa" del 18/11/2026, las muestras de un
		 * Brazal revelado. Un Elegido o un Eco sin hogar no cambian.
		 */
		void GiveEchoHomeHaste(const FMatchState& State, int32 Seat, const FGrimoireTexts& Texts, FGrimoireRow& Row)
		{
			if (!State.Seats.IsValidIndex(Seat) || State.Seats[Seat].Role == ESeatRole::Elect || !Row.Runes.IsValidIndex(State.Seats[Seat].DomainId))
			{
				return;
			}
			FGrimoireRune& Home = Row.Runes[State.Seats[Seat].DomainId];
			Home.bHasPower = true;
			Home.Power = EPower::Haste;
			Home.PowerName = PowerName(EPower::Haste);
			Home.PowerText = PowerText(Texts, EPower::Haste);
			Home.bActive = !Row.bTypesHidden && Home.Count > 0 && Soulbound::Rules::IsPowerActive(State, Seat, EPower::Haste);
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
		// Ronda de maquetación de S-048 (18/11/2026): tres, con "La mesa" la primera.
		static const TArray<EGrimoireTab> Tabs{EGrimoireTab::Table, EGrimoireTab::HowToPlay, EGrimoireTab::Chronicle};
		return Tabs;
	}

	FString GrimoireTabTitle(EGrimoireTab Tab)
	{
		switch (Tab)
		{
		case EGrimoireTab::Table: return TEXT("La mesa");
		case EGrimoireTab::HowToPlay: return TEXT("Cómo se juega");
		case EGrimoireTab::Chronicle: return TEXT("Crónica");
		}
		return FString();
	}

	void FGrimoireTabState::Open()
	{
		// Decisión del autor del 16/11/2026: siempre en la primera, no recuerda la última. Desde la ronda de maquetación de S-048, "La mesa".
		Active = EGrimoireTab::Table;
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

	FGrimoirePlayerSheet GrimoirePlayerSheet(const FMatchState& State, int32 HumanSeat, const FGrimoireTexts& Texts, const TArray<FLinearColor>& SeatColors)
	{
		FGrimoirePlayerSheet Sheet;
		if (!State.Seats.IsValidIndex(HumanSeat))
		{
			return Sheet;
		}
		// La hoja es la fila del Grimorio de S-027 (se reutiliza, no se reescribe), que ya lleva qué hace el poder de cada runa (PowerText).
		const TArray<FGrimoireRow> Rows = GrimoireRows(State, HumanSeat, Texts, SeatColors);
		if (Rows.IsValidIndex(HumanSeat))
		{
			Sheet.Row = Rows[HumanSeat];
		}
		const FSeatState& Seat = State.Seats[HumanSeat];
		Sheet.InsolenceLine = FString::Printf(TEXT("Insolencia: %d de %d"), Seat.Insolence, Soulbound::Match::MaxInsolence);
		// R-ECO-7: la runa del hogar de un Eco le da Celeridad (antes, una línea aparte de "Tú"; ahora, en su runa, con su descripción).
		GiveEchoHomeHaste(State, HumanSeat, Texts, Sheet.Row);
		return Sheet;
	}

	FString GrimoireCardHeader(const FGrimoireRow& Row, bool bPlayerSheet)
	{
		const FString Who = Row.bHuman ? FString::Printf(TEXT("Asiento %d (tú)"), Row.Seat) : FString::Printf(TEXT("Asiento %d"), Row.Seat);
		const FString Domain = Row.DomainId == INDEX_NONE ? FString() : FString::Printf(TEXT("D%d %s"), Row.DomainId, *Row.DomainName).TrimEnd();
		if (bPlayerSheet)
		{
			// Retoques de "La mesa" del 18/11/2026: en una línea, sin "ELEGIDO:"; con las seis runas, la hoja no cabía a 1024x768.
			return Row.bElect ? FString::Printf(TEXT("%s · %s · %s"), *Who, *Row.ElectName, *Domain)
				: Domain.IsEmpty() ? FString::Printf(TEXT("%s · Eco"), *Who)
				: FString::Printf(TEXT("%s · Eco · hogar en %s"), *Who, *Domain);
		}
		// La tarjeta del Elegido (S-029), como siempre.
		return Row.bElect ? FString::Printf(TEXT("%s · ELEGIDO: %s · %s"), *Who, *Row.ElectName, *Domain)
			: Domain.IsEmpty() ? FString::Printf(TEXT("%s · ECO, sin Elegido"), *Who)
			: FString::Printf(TEXT("%s · ECO, sin Elegido · hogar en %s"), *Who, *Domain);
	}

	FString GrimoireRuneLabel(const FGrimoireRune& Rune, bool bTypesHidden, bool bWithActive)
	{
		// S-035 (R-RUN-7): con los tipos ocultos no hay cantidad que decir (un guion diría "ninguna"): solo el mapa de runa a poder, que es público.
		const FString Count = bTypesHidden ? FString() : Rune.Count > 0 ? FString::Printf(TEXT("×%d"), Rune.Count) : FString(TEXT("—"));
		const FString Power = !Rune.bHasPower ? FString() : FString::Printf(TEXT(": %s%s"), *Rune.PowerName, bWithActive && Rune.bActive ? TEXT(" (ACTIVO)") : TEXT(""));
		return FString::Printf(TEXT("D%d%s%s"), Rune.RuneType, *(Count.IsEmpty() ? Count : FString(TEXT(" ")) + Count), *Power);
	}

	TArray<FLegendRow> GrimoireTableRows(const FMatchState& State, int32 HumanSeat, const TArray<bool>& ForgottenShown, const TArray<FLinearColor>& SeatColors,
		const TArray<FLinearColor>& DomainColors)
	{
		// Corrección del autor del 17/11/2026: de cada asiento, lo mismo que la leyenda (SeatLegendText y el Brazal, con sus filtros), y la marca de
		// aspirante, que es pública (el halo bajo el Altar). Ronda de maquetación de S-048 (18/11/2026): el encabezado, solo "Jugadores"; cada uno en
		// una línea, uno debajo de otro, con " | " y no "Runas: "; el humano, como los demás.
		FLegendRow Players;
		Players.Title = TablePlayersTitle;
		Players.bOnePerLine = true;
		// Retoques de "La mesa" del 18/11/2026: las runas de un Brazal revelado salen de la fila del Grimorio del asiento (su mapa de runa a poder), sin
		// los textos de los poderes, que aquí no se dicen.
		const TArray<FGrimoireRow> GrimoireSeatRows = GrimoireRows(State, HumanSeat, FGrimoireTexts(), SeatColors);
		for (int32 Seat = 0; Seat < State.Seats.Num(); ++Seat)
		{
			FLegendChip Chip;
			Chip.Color = SeatColors.IsValidIndex(Seat) ? SeatColors[Seat] : FLinearColor::White;
			FString Line;
			FString Runes;
			if (Seat == HumanSeat)
			{
				// Su línea como la ve otro (sin su insolencia ni sus castigos, que van en su hoja), con "(tú)" tras el número, y solo el total de runas.
				const FString Number = FString::FromInt(Seat);
				const FString AsOther = SeatLegendText(State, Seat, INDEX_NONE);
				Line = Number + TEXT(" (tú)") + AsOther.RightChop(Number.Len());
				int32 Total = 0;
				for (const int32 Count : State.Seats[Seat].Runes)
				{
					Total += Count;
				}
				Runes = FString::Printf(TEXT("%d runa%s"), Total, Total == 1 ? TEXT("") : TEXT("s"));
			}
			else if (Soulbound::Rules::IsBracerRevealed(State, Seat) && GrimoireSeatRows.IsValidIndex(Seat))
			{
				// Retoques de "La mesa" del 18/11/2026 (R-HUD-10, R-RUN-7): la línea acaba en "BRAZAL REVELADO hasta la ronda N" y va en negrita; debajo,
				// una muestra por tipo de runa que tiene, con el color de su Dominio y su poder (GrimoireRuneLabel, sin "(ACTIVO)"). Detrás de la línea,
				// lo de hoy en su orden: la marca de aspirante, " | 0 runas" sin runas y Paso Libre por afinidad. Un Eco, Celeridad en su hogar (R-ECO-7).
				FGrimoireRow Row = GrimoireSeatRows[Seat];
				GiveEchoHomeHaste(State, Seat, FGrimoireTexts(), Row);
				for (const FGrimoireRune& Rune : Row.Runes)
				{
					if (Rune.Count > 0)
					{
						FLegendSubChip Sub;
						Sub.Color = DomainColors.IsValidIndex(Rune.RuneType) ? DomainColors[Rune.RuneType] : FLinearColor::Gray;
						Sub.Text = GrimoireRuneLabel(Rune, false, false);
						Chip.SubChips.Add(Sub);
					}
				}
				Chip.bBold = true;
				FString Tail = Chip.SubChips.IsEmpty() ? FString(TEXT(" | 0 runas")) : FString();
				if (FreePassageByAffinity(State, Seat))
				{
					Tail += TEXT(" | poderes: Paso Libre (afinidad)");
				}
				Chip.Text = SeatLegendText(State, Seat, HumanSeat) + (Soulbound::Rules::IsAspirant(State, Seat) ? FString(TableAspirantMark) : FString()) + Tail;
				Players.Chips.Add(Chip);
				continue;
			}
			else
			{
				Line = SeatLegendText(State, Seat, HumanSeat);
				Runes = BracerSummary(State, Seat, HumanSeat);
			}
			Chip.Text = Line + (Soulbound::Rules::IsAspirant(State, Seat) ? FString(TableAspirantMark) : FString()) + TEXT(" | ") + Runes;
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
		// S-047 (R-VIS-33): los números de casilla están ocultos al empezar; L los enseña.
		Lines.Add(FString::Printf(TEXT("%s: enseña y oculta los números de las casillas, ocultos al empezar"), *SpanishKeyName(Keys.Overlay)));
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

	int32 HowToPlayPagesPerView(bool bBook)
	{
		// Segunda ampliación del autor del 17/11/2026: con el libro, una página en cada hoja.
		return bBook ? 2 : 1;
	}

	FHowToPlayPageButtons HowToPlayPageButtons(int32 ViewStart, int32 PageCount, int32 PagesPerView)
	{
		// Retoques del autor del 18/11/2026: un botón solo si lleva a alguna vista; el libro no es cíclico.
		FHowToPlayPageButtons Buttons;
		if (PageCount <= 0 || PagesPerView <= 0)
		{
			return Buttons;
		}
		Buttons.bPrevious = ViewStart > 0;
		Buttons.bNext = ViewStart + PagesPerView < PageCount;
		return Buttons;
	}

	FHowToPlayView HowToPlayView(int32 Page, int32 PageCount, bool bBook)
	{
		// S-049 (R-HUD-21): lo que hacía SGrimoirePanel::RebuildHowToPlay al armar la vista, con las mismas funciones.
		FHowToPlayView View;
		if (PageCount <= 0)
		{
			return View;
		}
		const int32 PerView = HowToPlayPagesPerView(bBook);
		const int32 First = HowToPlayViewStart(FMath::Clamp(Page, 0, PageCount - 1), PerView);
		View.LeftPage = First;
		View.RightPage = bBook && First + 1 < PageCount ? First + 1 : INDEX_NONE;
		View.Footer = HowToPlayPageFooter(First, PageCount, PerView);
		View.Buttons = HowToPlayPageButtons(First, PageCount, PerView);
		return View;
	}

	FString HowToPlayMenuFootHint(const FGrimoireKeyNames& Keys)
	{
		// S-049 (R-HUD-22): la parte de páginas del pie del Grimorio en "Cómo se juega"; la de cerrar y pestañas, no.
		return GrimoireFootHints(Keys, EGrimoireTab::HowToPlay).Right;
	}

	int32 HowToPlayViewStart(int32 Page, int32 PagesPerView)
	{
		const int32 PerView = FMath::Max(PagesPerView, 1);
		const int32 Clamped = FMath::Max(Page, 0);
		return Clamped - Clamped % PerView;
	}

	FVector2f HowToPlayImageSize(float SheetWidth, float MaxHeight, const FVector2f& ImageSize)
	{
		if (SheetWidth <= KINDA_SMALL_NUMBER || MaxHeight <= KINDA_SMALL_NUMBER || ImageSize.X <= KINDA_SMALL_NUMBER || ImageSize.Y <= KINDA_SMALL_NUMBER)
		{
			return FVector2f::ZeroVector;
		}
		// La mayor escala que cabe en los dos límites, también si agranda: sin deformar la imagen (plan aprobado por el autor).
		const float Scale = FMath::Min(SheetWidth / ImageSize.X, MaxHeight / ImageSize.Y);
		return ImageSize * Scale;
	}

	FBox2f HowToPlayFullSheetImage(const FVector2f& SheetSize, const FVector2f& ImageSize)
	{
		// Ronda de maquetación de S-048 (18/11/2026): la mayor que cabe en el ancho y el alto que hay de verdad en la hoja, centrada en los dos ejes.
		const FVector2f Size = HowToPlayImageSize(SheetSize.X, SheetSize.Y, ImageSize);
		if (Size.IsNearlyZero())
		{
			return FBox2f(FVector2f::ZeroVector, FVector2f::ZeroVector);
		}
		const FVector2f Min = (SheetSize - Size) * 0.5f;
		return FBox2f(Min, Min + Size);
	}

	FVector2f GrimoireTextureSize(const UTexture2D* Texture)
	{
		if (!Texture)
		{
			return FVector2f::ZeroVector;
		}
		// La fuente no espera a la compilación de la textura: en el editor es Source.GetLogicalSize(); en una build empaquetada, el tamaño guardado al
		// cocinar. GetSizeX y GetSizeY, en cambio, dan el de la textura de relleno mientras se compila (UTexture2D::IsDefaultTexture).
		const FIntPoint Imported = Texture->GetImportedSize();
		if (Imported.X > 0 && Imported.Y > 0)
		{
			return FVector2f(Imported.X, Imported.Y);
		}
		return FVector2f(Texture->GetSizeX(), Texture->GetSizeY());
	}

	float GrimoireFrameWidth(float Scale)
	{
		// Una unidad, salvo que una unidad sea menos de un píxel: entonces, un píxel.
		return Scale > KINDA_SMALL_NUMBER && Scale < 1.f ? 1.f / Scale : 1.f;
	}

	FGrimoireWatermarks GrimoireWatermarks(const FGrimoireBookLayout& Book, const FVector2f& TextureSize, float SizeFraction)
	{
		FGrimoireWatermarks Marks;
		if (!Book.bBook || TextureSize.X <= KINDA_SMALL_NUMBER || TextureSize.Y <= KINDA_SMALL_NUMBER)
		{
			return Marks;
		}
		// Corrección del autor del 17/11/2026: una por hoja, centrada en la suya, al mayor tamaño que cabe sin deformarse en la fracción de la hoja.
		const float Fraction = FMath::Clamp(SizeFraction, 0.f, 1.f);
		const auto Centered = [&TextureSize, Fraction](const FBox2f& Sheet)
		{
			const FVector2f Room = Sheet.GetSize() * Fraction;
			const float Scale = FMath::Min(Room.X / TextureSize.X, Room.Y / TextureSize.Y);
			const FVector2f Half = TextureSize * Scale * 0.5f;
			const FVector2f Center = Sheet.GetCenter();
			return FBox2f(Center - Half, Center + Half);
		};
		Marks.bShown = true;
		Marks.Left = Centered(Book.LeftPage);
		Marks.Right = Centered(Book.RightPage);
		return Marks;
	}

	FString HowToPlayPageFooter(int32 Page, int32 PageCount, int32 PagesPerView)
	{
		if (PageCount <= 0)
		{
			return FString();
		}
		// Segunda ampliación del autor del 17/11/2026: el pie de la vista, "1-2 de 5"; la última de un total impar, "5 de 5".
		const int32 First = HowToPlayViewStart(FMath::Clamp(Page, 0, PageCount - 1), PagesPerView);
		const int32 Last = FMath::Min(First + FMath::Max(PagesPerView, 1), PageCount) - 1;
		return Last > First ? FString::Printf(TEXT("%d-%d de %d"), First + 1, Last + 1, PageCount) : FString::Printf(TEXT("%d de %d"), First + 1, PageCount);
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
		// R-HUD-18: no es cíclico; en la última vista se queda. Segunda ampliación del autor: de vista en vista, en la primera página de cada una.
		const int32 PerView = FMath::Max(PagesPerView, 1);
		Page = PageCount <= 0 ? 0 : FMath::Clamp(HowToPlayViewStart(Page, PerView) + PerView, 0, HowToPlayViewStart(PageCount - 1, PerView));
	}

	void FGrimoireTabState::PreviousPage(int32 PageCount, int32 PagesPerView)
	{
		// R-HUD-18: en la primera vista se queda.
		const int32 PerView = FMath::Max(PagesPerView, 1);
		Page = PageCount <= 0 ? 0 : FMath::Clamp(HowToPlayViewStart(Page, PerView) - PerView, 0, HowToPlayViewStart(PageCount - 1, PerView));
	}
}
