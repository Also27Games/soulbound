// Test de guarda del Grimorio construido (S-049, R-HUD-21): qué textos enseña cada una de sus tres pestañas, con la imagen del libro y sin ella. Se
// escribe antes de sacar "Cómo se juega" de SGrimoirePanel a su pieza propia y nace en verde a propósito: fija lo que el panel enseña hoy, y tiene que
// seguir pasando igual, sin tocarlo, después del cambio. Sin mundo, sin editor, sin FSlateApplication y sin pintar: se construye el panel y se recorren
// sus widgets. De cada cambiador de pestañas se lee la ranura de la pestaña que se mira (GetWidget), no la activa, que depende de un atributo que solo
// se actualiza al pintar. Los textos que se leen al pintar (las teclas del pie, las muestras de la mesa) no se comprueban. Filtro:
// "Soulbound.Game.Presentation.Grimorio".

#include "../../Presentation/ChronicleLines.h"
#include "../../Presentation/GrimoireBook.h"
#include "../../Presentation/GrimoirePanel.h"
#include "../../Presentation/HowToPlay.h"
#include "Misc/AutomationTest.h"
#include "Styling/SlateBrush.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Text/SRichTextBlock.h"
#include "Widgets/Text/STextBlock.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	using Soulbound::Presentation::EGrimoireTab;

	/** La marca con que se anota una imagen entre los textos. */
	const FString GtImageMark(TEXT("[imagen]"));

	/**
	 * Los textos de un widget y de sus hijos, en su orden, sin pintarlo: los de STextBlock y SRichTextBlock, y GtImageMark por cada SImage. Lo que no se
	 * ve (Hidden o Collapsed) no cuenta, con sus hijos. De un SWidgetSwitcher, solo la ranura Slot.
	 */
	void GtCollect(const TSharedRef<SWidget>& Widget, int32 Slot, TArray<FString>& Out)
	{
		if (!Widget->GetVisibility().IsVisible())
		{
			return;
		}
		const FName Type = Widget->GetType();
		if (Type == TEXT("STextBlock"))
		{
			Out.Add(StaticCastSharedRef<STextBlock>(Widget)->GetText().ToString());
		}
		else if (Type == TEXT("SRichTextBlock"))
		{
			Out.Add(StaticCastSharedRef<SRichTextBlock>(Widget)->GetText().ToString());
		}
		else if (Type == TEXT("SImage"))
		{
			Out.Add(GtImageMark);
		}
		if (Type == TEXT("SWidgetSwitcher"))
		{
			const TSharedPtr<SWidget> Active = StaticCastSharedRef<SWidgetSwitcher>(Widget)->GetWidget(Slot);
			if (Active.IsValid())
			{
				GtCollect(Active.ToSharedRef(), Slot, Out);
			}
			return;
		}
		FChildren* Children = Widget->GetChildren();
		for (int32 Index = 0; Children && Index < Children->Num(); ++Index)
		{
			GtCollect(Children->GetChildAt(Index), Slot, Out);
		}
	}

	/** Los textos de una pestaña del panel. */
	TArray<FString> GtTabTexts(const TSharedRef<SGrimoirePanel>& Panel, EGrimoireTab Tab)
	{
		TArray<FString> Out;
		GtCollect(Panel, Soulbound::Presentation::GrimoireTabs().Find(Tab), Out);
		return Out;
	}

	/** Si Expected está en Texts en ese orden, aunque haya otros textos entre medias. */
	bool GtInOrder(const TArray<FString>& Texts, const TArray<FString>& Expected)
	{
		int32 Next = 0;
		for (const FString& Text : Texts)
		{
			if (Next < Expected.Num() && Text == Expected[Next])
			{
				++Next;
			}
		}
		return Next == Expected.Num();
	}

	/** Un libro de prueba de cinco hojas y la de teclas: párrafo con negrita, lámina dentro del texto, frase del Warden, lista, lámina a hoja entera y subtítulo. */
	const TCHAR* const GtBookText =
		TEXT("# Libro de prueba\n\n> Portada.\n\n")
		TEXT("## Uno\n\nTexto **uno**.\n\n![](T_Uno)\n\n")
		TEXT("## Dos\n\n> Frase del Warden.\n\n- Primero\n- Segundo\n\n")
		TEXT("##\n\n![](T_Hoja)\n\n")
		TEXT("## Cuatro\n\n### Sub\n\nTexto cuatro.\n\n")
		TEXT("## Cinco\n\nTexto cinco.\n");

	/** Los textos de una hoja tal como los pinta el panel: su título, si lo tiene, y cada bloque (la marca de la lista delante de su elemento). */
	TArray<FString> GtSheetTexts(const FHowToPlayPage& Page)
	{
		TArray<FString> Out;
		if (!Page.Title.IsEmpty())
		{
			Out.Add(Page.Title);
		}
		for (const FHowToPlayBlock& Block : Page.Blocks)
		{
			if (Block.Kind == EHowToPlayBlockKind::Image)
			{
				Out.Add(GtImageMark);
				continue;
			}
			if (Block.Kind == EHowToPlayBlockKind::ListItem)
			{
				Out.Add(Soulbound::Presentation::HowToPlayListMark);
			}
			Out.Add(Soulbound::Presentation::HowToPlayBlockRichText(Block));
		}
		return Out;
	}

	/** Si algún texto de la lista está entre los de la pestaña. */
	bool GtAnyOf(const TArray<FString>& Texts, const TArray<FString>& Some)
	{
		return Some.ContainsByPredicate([&Texts](const FString& Text) { return Texts.Contains(Text); });
	}
}

// CA-HUD-70 (R-HUD-21, guarda; S-049): la pestaña "Cómo se juega" del Grimorio enseña para una vista los textos de sus hojas en su orden, "N de M" y los
// botones que tocan (en la primera vista no se ve "Anterior"; en la última, "Siguiente"), con el libro (dos hojas por vista) y sin él (una). "La mesa"
// enseña la hoja del jugador y los títulos de la mesa, y "Crónica" sus líneas; ninguna pestaña enseña lo de otra. Escrito antes de sacar la pieza de
// SGrimoirePanel, en verde a propósito, y no se toca después: es lo que asegura que el Grimorio enseña lo mismo construido con la pieza.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGrimoireTabsTextsTest, "Soulbound.Game.Presentation.Grimorio.PestanasConstruidasMismosTextos",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FGrimoireTabsTextsTest::RunTest(const FString& Parameters)
{
	using namespace Soulbound::Presentation;
	Soulbound::Presentation::FGrimoireKeyNames Keys;
	Keys.Grimoire = TEXT("G");
	Keys.PreviousTab = TEXT("Left");
	Keys.NextTab = TEXT("Right");
	Keys.PreviousPage = TEXT("Up");
	Keys.NextPage = TEXT("Down");
	const FHowToPlayLoadResult Loaded = ParseHowToPlayText(GtBookText);
	if (!TestTrue(TEXT("El libro de prueba carga"), Loaded.IsSuccess()))
	{
		return false;
	}
	const FHowToPlayBook Book = HowToPlayBook(Loaded, Keys);
	if (!TestEqual(TEXT("Cinco hojas y la de teclas"), Book.Pages.Num(), 6))
	{
		return false;
	}

	// La hoja del jugador, la mesa y la crónica.
	FGrimoirePlayerSheet Sheet;
	Sheet.Row.Seat = 0;
	Sheet.Row.bHuman = true;
	Sheet.Row.bElect = true;
	Sheet.Row.ElectName = TEXT("Tifón");
	Sheet.Row.DomainId = 4;
	Sheet.Row.DomainName = TEXT("La Ruina Tormentosa");
	Sheet.Row.AffinityName = TEXT("Afinidad");
	Sheet.Row.AffinityText = TEXT("Lo que hace.");
	Sheet.InsolenceLine = TEXT("Insolencia: 1 de 5");
	const FString SheetHeader = GrimoireCardHeader(Sheet.Row, true);
	TArray<FLegendRow> Table;
	Table.AddDefaulted_GetRef().Title = TEXT("Jugadores");
	Table[0].Chips.AddDefaulted_GetRef().Text = TEXT("Asiento 1");
	Table.AddDefaulted_GetRef().Title = TEXT("Los seis Dominios y su Elegido");
	const TArray<FChronicleLine> Chronicle{{TEXT("Línea dos"), FLinearColor::White}, {TEXT("Línea uno"), FLinearColor::White}};
	const TArray<FString> TableTexts{SheetHeader, Sheet.InsolenceLine, TEXT("Jugadores"), TEXT("Los seis Dominios y su Elegido")};
	const TArray<FString> ChronicleTexts{ChronicleHeadingText, TEXT("Línea dos"), TEXT("Línea uno")};
	TArray<FString> AllBookTexts;
	for (const FHowToPlayPage& Page : Book.Pages)
	{
		AllBookTexts.Append(GtSheetTexts(Page));
	}
	AllBookTexts.RemoveAll([](const FString& Text) { return Text == GtImageMark || Text == HowToPlayListMark; });

	// Las láminas: un pincel con tamaño por nombre (la vista los carga; aquí no hace falta la textura).
	TMap<FString, FSlateBrush> Images;
	for (const TCHAR* Name : {TEXT("T_Uno"), TEXT("T_Hoja")})
	{
		FSlateBrush& Brush = Images.Add(Name);
		Brush.ImageSize = FVector2D(200.0, 300.0);
	}
	const TArray<FLinearColor> Colors{FLinearColor::Red, FLinearColor::Green, FLinearColor::Blue, FLinearColor::Yellow, FLinearColor::White, FLinearColor::Black};
	const TArray<FString> Header{GrimoireTitleText, GrimoireTabTitle(EGrimoireTab::Table), GrimoireTabTitle(EGrimoireTab::HowToPlay),
		GrimoireTabTitle(EGrimoireTab::Chronicle)};

	for (const bool bBook : {false, true})
	{
		const FString Case = bBook ? TEXT("Con el libro") : TEXT("Sin el libro");
		TOptional<FSlateBrush> BookBrush;
		if (bBook)
		{
			FSlateBrush Brush;
			Brush.ImageSize = FVector2D(1600.0, 1000.0);
			Brush.Margin = FMargin(0.08f);
			BookBrush = Brush;
		}
		const TSharedRef<SGrimoirePanel> Panel = SNew(SGrimoirePanel)
			.FontSize(14)
			.DomainColors(Colors)
			.KeyNames(Keys)
			.PanelWidth(1000.f)
			.PanelHeight(700.f)
			.ActiveTab(EGrimoireTab::Table)
			.BookBrush(BookBrush)
			.ImageForPage([&Images](const FString& ImageName) -> const FSlateBrush* { return Images.Find(ImageName); });
		Panel->SetPlayer(Sheet);
		Panel->SetTable(Table);
		Panel->SetChronicle(Chronicle);

		// "La mesa" y "Crónica": lo suyo, tras la cabecera, y nada de las otras.
		Panel->SetHowToPlay(Book, 0);
		const TArray<FString> TableTab = GtTabTexts(Panel, EGrimoireTab::Table);
		TestTrue(Case + TEXT(", La mesa: la cabecera y la hoja del jugador y la mesa, en su orden"), GtInOrder(TableTab, Header) && GtInOrder(TableTab, TableTexts));
		TestFalse(Case + TEXT(", La mesa: nada de la crónica ni del libro"), GtAnyOf(TableTab, ChronicleTexts) || GtAnyOf(TableTab, AllBookTexts));
		const TArray<FString> ChronicleTab = GtTabTexts(Panel, EGrimoireTab::Chronicle);
		TestTrue(Case + TEXT(", Crónica: la cabecera y sus líneas, la más reciente arriba"), GtInOrder(ChronicleTab, Header) && GtInOrder(ChronicleTab, ChronicleTexts));
		TestFalse(Case + TEXT(", Crónica: nada de la mesa ni del libro"), GtAnyOf(ChronicleTab, TableTexts) || GtAnyOf(ChronicleTab, AllBookTexts));

		// "Cómo se juega": una vista al principio, una en medio y la última. Con el libro, "Anterior" al pie de la hoja izquierda y "N de M" y
		// "Siguiente" al pie de la derecha; sin él, los tres al pie de la única hoja.
		struct FViewCase
		{
			int32 Page;
			TArray<int32> Sheets;
			FString Footer;
			bool bPrevious;
			bool bNext;
		};
		const TArray<FViewCase> Views = bBook
			? TArray<FViewCase>{{0, {0, 1}, TEXT("1-2 de 6"), false, true}, {3, {2, 3}, TEXT("3-4 de 6"), true, true}, {5, {4, 5}, TEXT("5-6 de 6"), true, false}}
			: TArray<FViewCase>{{0, {0}, TEXT("1 de 6"), false, true}, {3, {3}, TEXT("4 de 6"), true, true}, {5, {5}, TEXT("6 de 6"), true, false}};
		for (const FViewCase& View : Views)
		{
			Panel->SetHowToPlay(Book, View.Page);
			const FString ViewCase = FString::Printf(TEXT("%s, Cómo se juega en la página %d"), *Case, View.Page + 1);
			const TArray<FString> Texts = GtTabTexts(Panel, EGrimoireTab::HowToPlay);
			TArray<FString> Expected = GtSheetTexts(Book.Pages[View.Sheets[0]]);
			if (bBook)
			{
				if (View.bPrevious)
				{
					Expected.Add(TEXT("Anterior"));
				}
				Expected.Append(GtSheetTexts(Book.Pages[View.Sheets[1]]));
				Expected.Add(View.Footer);
			}
			else
			{
				if (View.bPrevious)
				{
					Expected.Add(TEXT("Anterior"));
				}
				Expected.Add(View.Footer);
			}
			if (View.bNext)
			{
				Expected.Add(TEXT("Siguiente"));
			}
			TestTrue(ViewCase + TEXT(": la cabecera"), GtInOrder(Texts, Header));
			TestTrue(ViewCase + TEXT(": sus hojas, el pie y sus botones, en su orden"), GtInOrder(Texts, Expected));
			TestEqual(ViewCase + TEXT(": Anterior, solo si hay una vista antes"), Texts.Contains(TEXT("Anterior")), View.bPrevious);
			TestEqual(ViewCase + TEXT(": Siguiente, solo si hay una después"), Texts.Contains(TEXT("Siguiente")), View.bNext);
			TestFalse(ViewCase + TEXT(": nada de la mesa ni de la crónica"), GtAnyOf(Texts, TableTexts) || GtAnyOf(Texts, ChronicleTexts));
			// Ninguna hoja de otra vista.
			for (int32 Other = 0; Other < Book.Pages.Num(); ++Other)
			{
				if (View.Sheets.Contains(Other))
				{
					continue;
				}
				TArray<FString> OtherTexts = GtSheetTexts(Book.Pages[Other]);
				OtherTexts.RemoveAll([](const FString& Text) { return Text == GtImageMark || Text == HowToPlayListMark; });
				TestFalse(FString::Printf(TEXT("%s: nada de la hoja %d"), *ViewCase, Other + 1), GtAnyOf(Texts, OtherTexts));
			}
		}
	}
	return true;
}

#endif
