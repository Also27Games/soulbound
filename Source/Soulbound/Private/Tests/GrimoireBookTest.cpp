// Tests del Grimorio como libro del juego (S-044, R-HUD-10 a R-HUD-12): sus pestañas, lo que lleva cada una, cuándo se cierra solo y su botón.
// Funciones puras sobre un FMatchState armado a mano con los Elegidos de elegidos.json: sin mundo, sin actor y sin Slate. Desde S-048 (R-HUD-18 y
// R-HUD-20), también el libro de "Cómo se juega" y la zona de contenido del libro; de la vista solo se lee su objeto por defecto (las teclas).
// Filtro: "Soulbound.Game.Presentation.Grimorio".

#include "../../Presentation/BoardViewActor.h"
#include "../../Presentation/BoardViewState.h"
#include "../../Presentation/ChronicleLines.h"
#include "../../Presentation/EventText.h"
#include "../../Presentation/GrimoireBook.h"
#include "../../Presentation/GrimoireFont.h"
#include "Engine/Font.h"
#include "Styling/CoreStyle.h"
#include "UObject/Package.h"
#include "Elects/ElectsLoader.h"
#include "Match/MatchState.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags GrimoireBookTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter;

	using Soulbound::Presentation::EGrimoireTab;
	using Soulbound::Presentation::FGrimoireTexts;

	constexpr int32 GbHuman = 0;
	constexpr int32 GbOther = 2;
	constexpr int32 GbAspirant = 3;
	/** Fenrir es el Elegido del Dominio 1 en elegidos.json; la runa de su propio Dominio le da Celeridad. */
	constexpr int32 GbFenrirDomain = 1;
	constexpr int32 GbAltarTile = 0;
	constexpr int32 GbPlainTile = 1;

	const TArray<FLinearColor> GbSeatColors{FLinearColor::Red, FLinearColor::Green, FLinearColor::Blue, FLinearColor::Yellow};
	const TArray<FLinearColor> GbDomainColors{FLinearColor(0.1f, 0.f, 0.f), FLinearColor(0.2f, 0.f, 0.f), FLinearColor(0.3f, 0.f, 0.f),
		FLinearColor(0.4f, 0.f, 0.f), FLinearColor(0.5f, 0.f, 0.f), FLinearColor(0.6f, 0.f, 0.f)};

	/**
	 * Cuatro asientos con los Elegidos del repositorio sobre un tablero de dos casillas (0, el Altar; 1, normal) y seis Dominios con nombre
	 * ("Dominio N"). Todos los asientos, en la casilla normal y Ecos durante la carrera.
	 */
	bool GbMakeState(FAutomationTestBase& Test, FMatchState& OutState)
	{
		const FElectsLoadResult Elects = Soulbound::Rules::LoadElectsFromFile(FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Data/Elects/elegidos.json")));
		if (!Test.TestTrue(FString::Printf(TEXT("elegidos.json se carga (%s)"), *Elects.Message), Elects.IsSuccess()))
		{
			return false;
		}
		OutState.Elects = Elects.Table;
		OutState.Board.Tiles.SetNum(2);
		OutState.Board.Tiles[GbAltarTile].TileId = GbAltarTile;
		OutState.Board.Tiles[GbAltarTile].Type = ETileType::Altar;
		OutState.Board.Tiles[GbPlainTile].TileId = GbPlainTile;
		OutState.Board.Domains.SetNum(Soulbound::Board::DomainCount);
		for (int32 Id = 0; Id < Soulbound::Board::DomainCount; ++Id)
		{
			OutState.Board.Domains[Id].DomainId = Id;
			OutState.Board.Domains[Id].Name = FString::Printf(TEXT("Dominio %d"), Id);
		}
		OutState.Seats.SetNum(4);
		for (FSeatState& Seat : OutState.Seats)
		{
			Seat.TileId = GbPlainTile;
		}
		return true;
	}

	/** Un FDecisionView del humano o de la espera, para el seguimiento del cierre. */
	TOptional<Soulbound::Presentation::FDecisionView> GbView(Soulbound::Presentation::EDecisionInput Input, const FString& Identity, bool bBlocked = false)
	{
		Soulbound::Presentation::FDecisionView View;
		View.Input = Input;
		View.Identity = Identity;
		View.bBlocked = bBlocked;
		return View;
	}

	FMatchEvent GbEvent(EMatchEventType Type, int32 Seat, int32 Value, double Clock)
	{
		FMatchEvent Event;
		Event.Type = Type;
		Event.Seat = Seat;
		Event.Value = Value;
		Event.ClockSeconds = Clock;
		return Event;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGrimoireTabsTest, "Soulbound.Game.Presentation.Grimorio.PestanasOrdenYAbreEnTu", GrimoireBookTestFlags)

bool FGrimoireTabsTest::RunTest(const FString& Parameters)
{
	// CA-HUD-18 (R-HUD-10): la lista de pestañas es, en este orden, Tú, La mesa, El tablero, Cómo se juega y Crónica. Al abrir el Grimorio la
	// activa es Tú, también si antes se cerró en otra. Con las flechas, cíclico (aceptado por el autor el 17/11/2026): la siguiente de Crónica es
	// Tú y la anterior de Tú es Crónica.
	using namespace Soulbound::Presentation;
	const TArray<EGrimoireTab> Expected{EGrimoireTab::You, EGrimoireTab::Table, EGrimoireTab::Board, EGrimoireTab::HowToPlay, EGrimoireTab::Chronicle};
	TestTrue(TEXT("Cinco pestañas, en su orden"), GrimoireTabs() == Expected);
	const TArray<FString> Titles{TEXT("Tú"), TEXT("La mesa"), TEXT("El tablero"), TEXT("Cómo se juega"), TEXT("Crónica")};
	for (int32 Index = 0; Index < Expected.Num(); ++Index)
	{
		TestEqual(FString::Printf(TEXT("El nombre de la pestaña %d"), Index), GrimoireTabTitle(Expected[Index]), Titles[Index]);
	}

	FGrimoireTabState Tab;
	TestEqual(TEXT("Empieza en Tú"), static_cast<int32>(Tab.Active), static_cast<int32>(EGrimoireTab::You));
	Tab.Select(EGrimoireTab::HowToPlay);
	TestEqual(TEXT("Un clic en su nombre la activa"), static_cast<int32>(Tab.Active), static_cast<int32>(EGrimoireTab::HowToPlay));
	Tab.Open();
	TestEqual(TEXT("Cerrado en Cómo se juega, se abre en Tú"), static_cast<int32>(Tab.Active), static_cast<int32>(EGrimoireTab::You));

	// La siguiente recorre el orden y vuelve a Tú.
	for (int32 Index = 1; Index <= Expected.Num(); ++Index)
	{
		Tab.Next();
		TestEqual(FString::Printf(TEXT("Siguiente %d"), Index), static_cast<int32>(Tab.Active), static_cast<int32>(Expected[Index % Expected.Num()]));
	}
	Tab.Previous();
	TestEqual(TEXT("La anterior de Tú es Crónica"), static_cast<int32>(Tab.Active), static_cast<int32>(EGrimoireTab::Chronicle));
	Tab.Previous();
	TestEqual(TEXT("Y la de Crónica, Cómo se juega"), static_cast<int32>(Tab.Active), static_cast<int32>(EGrimoireTab::HowToPlay));
	Tab.Select(EGrimoireTab::Chronicle);
	Tab.Next();
	TestEqual(TEXT("La siguiente de Crónica es Tú"), static_cast<int32>(Tab.Active), static_cast<int32>(EGrimoireTab::You));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGrimoireYouTabTest, "Soulbound.Game.Presentation.Grimorio.PestanaTu", GrimoireBookTestFlags)

bool FGrimoireYouTabTest::RunTest(const FString& Parameters)
{
	// CA-HUD-19 (R-HUD-10): para un Elegido, la pestaña lleva lo que llevaba el Grimorio (su fila de GrimoireRows y las seis líneas de poderes al
	// pie) y su insolencia. Para un Eco tras la carrera con hogar en el Dominio 4, la línea de la runa de su hogar con Celeridad, con el número y el
	// nombre de ese Dominio; durante la carrera, sin hogar, no la lleva.
	using namespace Soulbound::Presentation;
	FMatchState State;
	if (!GbMakeState(*this, State))
	{
		return false;
	}
	FGrimoireTexts Texts;
	Texts.PowerTexts.Add(EPower::Haste, TEXT("Frase de prueba de Celeridad"));
	State.Seats[GbHuman].Role = ESeatRole::Elect;
	State.Seats[GbHuman].DomainId = GbFenrirDomain;
	State.Seats[GbHuman].Runes[GbFenrirDomain] = 1;
	State.Seats[GbHuman].Insolence = 2;

	const FGrimoireYouView Elect = GrimoireYouView(State, GbHuman, Texts, GbSeatColors);
	const TArray<FGrimoireRow> Rows = GrimoireRows(State, GbHuman, Texts, GbSeatColors);
	TestTrue(TEXT("Elegido: su tarjeta es su fila del Grimorio"), Rows.IsValidIndex(GbHuman) && Elect.Row == Rows[GbHuman]);
	TestEqual(TEXT("Elegido: con su insolencia"), Elect.InsolenceLine, FString(TEXT("Insolencia: 2 de 5")));
	TestTrue(TEXT("Elegido: sin línea de hogar"), Elect.HomeRuneLine.IsEmpty());
	TestEqual(TEXT("Al pie, las seis líneas de poderes"), Elect.PowerLines, GrimoirePowerLines(Texts));

	// Un Eco tras la carrera, con hogar en el Dominio 4.
	constexpr int32 EchoSeat = 1;
	constexpr int32 Home = 4;
	State.MatchPhase = EMatchPhase::RuneHunt;
	State.Seats[EchoSeat].DomainId = Home;
	const FGrimoireYouView Echo = GrimoireYouView(State, EchoSeat, Texts, GbSeatColors);
	TestFalse(TEXT("Eco: su tarjeta es la de un Eco"), Echo.Row.bElect);
	TestEqual(TEXT("Eco: la tarjeta es la suya"), Echo.Row.Seat, EchoSeat);
	// Corrección del autor del 17/11/2026: Celeridad deja elegir, así que "puedes tirar dos dados".
	TestEqual(TEXT("Eco tras la carrera: la runa de su hogar"), Echo.HomeRuneLine, FString(TEXT("La runa de tu hogar (D4, Dominio 4) te da Celeridad: puedes tirar dos dados")));
	TestEqual(TEXT("Eco: su insolencia, también"), Echo.InsolenceLine, FString(TEXT("Insolencia: 0 de 5")));

	// Durante la carrera, sin hogar.
	State.MatchPhase = EMatchPhase::BondRace;
	State.Seats[EchoSeat].DomainId = INDEX_NONE;
	TestTrue(TEXT("Eco durante la carrera: sin la línea"), GrimoireYouView(State, EchoSeat, Texts, GbSeatColors).HomeRuneLine.IsEmpty());

	// El panel solo se rehace si cambia (R-VIS-21): la misma vista dos veces es igual; con otra insolencia, no.
	TestTrue(TEXT("Dos veces el mismo estado: iguales"), Elect == GrimoireYouView(State, GbHuman, Texts, GbSeatColors));
	FMatchState Angrier = State;
	Angrier.Seats[GbHuman].Insolence = 3;
	TestFalse(TEXT("Con otra insolencia: distintas"), Elect == GrimoireYouView(Angrier, GbHuman, Texts, GbSeatColors));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGrimoireTableTabTest, "Soulbound.Game.Presentation.Grimorio.PestanaLaMesa", GrimoireBookTestFlags)

bool FGrimoireTableTabTest::RunTest(const FString& Parameters)
{
	// CA-HUD-20 (R-HUD-10, corrección del autor del 17/11/2026): una entrada por asiento, con su rol, sus runas por Dominio y sus poderes activos (los
	// de otro asiento, como "N runas" salvo con su Brazal revelado), la insolencia solo en la del humano, y las marcas de Prueba del Warden pendiente
	// y de aspirante cuando tocan; y una entrada por Dominio con su dueño u "Olvidado". De cada asiento, lo mismo que la leyenda, ni más ni menos.
	using namespace Soulbound::Presentation;
	FMatchState State;
	if (!GbMakeState(*this, State))
	{
		return false;
	}
	State.MatchPhase = EMatchPhase::RuneHunt;
	// El humano: Elegido de Fenrir, con la runa de su Dominio (Celeridad), insolencia 3 y la Prueba del Warden pendiente.
	State.Seats[GbHuman].Role = ESeatRole::Elect;
	State.Seats[GbHuman].DomainId = GbFenrirDomain;
	State.Seats[GbHuman].Runes[GbFenrirDomain] = 1;
	State.Seats[GbHuman].Insolence = 3;
	State.Seats[GbHuman].bWardenTrialPending = true;
	State.Board.Domains[GbFenrirDomain].OwnerSeat = GbHuman;
	// Otro asiento, con dos runas del Dominio 0 y su insolencia, que no se enseña.
	State.Seats[GbOther].Runes[0] = 2;
	State.Seats[GbOther].Insolence = 4;
	// Un aspirante: en el Altar con los seis tipos.
	State.Seats[GbAspirant].TileId = GbAltarTile;
	State.Seats[GbAspirant].Runes.Init(1, Soulbound::Board::DomainCount);
	const TArray<bool> Forgotten{false, false, true, false, false, false};

	const TArray<FLegendRow> Rows = GrimoireTableRows(State, GbHuman, Forgotten, GbSeatColors, GbDomainColors);
	if (!TestEqual(TEXT("Dos filas: jugadores y Dominios"), Rows.Num(), 2))
	{
		return false;
	}
	const FLegendRow& Players = Rows[0];
	const FLegendRow& Domains = Rows[1];
	TestTrue(TEXT("La de jugadores se titula como en la leyenda"), Players.Title.StartsWith(TEXT("Jugadores")));
	TestEqual(TEXT("La de Dominios, también"), Domains.Title, FString(TEXT("Los seis Dominios y su Elegido")));
	if (!TestEqual(TEXT("Una entrada por asiento"), Players.Chips.Num(), 4) || !TestEqual(TEXT("Una entrada por Dominio"), Domains.Chips.Num(), 6))
	{
		return false;
	}
	for (int32 Seat = 0; Seat < 4; ++Seat)
	{
		const FLegendChip& Chip = Players.Chips[Seat];
		TestEqual(FString::Printf(TEXT("Asiento %d: su color"), Seat), Chip.Color, GbSeatColors[Seat]);
		TestTrue(FString::Printf(TEXT("Asiento %d: el texto ya va puesto"), Seat), !Chip.TextGetter);
		TestTrue(FString::Printf(TEXT("Asiento %d: empieza por la línea de la leyenda"), Seat), Chip.Text.StartsWith(SeatLegendText(State, Seat, GbHuman)));
		TestTrue(FString::Printf(TEXT("Asiento %d: lleva su Brazal como la leyenda"), Seat), Chip.Text.Contains(TEXT("Runas: ") + BracerSummary(State, Seat, GbHuman)));
		TestEqual(FString::Printf(TEXT("Asiento %d: aspirante solo si lo es"), Seat), Chip.Text.Contains(TEXT("ASPIRANTE")), Seat == GbAspirant);
		TestEqual(FString::Printf(TEXT("Asiento %d: insolencia solo la del humano"), Seat), Chip.Text.Contains(TEXT("insolencia")), Seat == GbHuman);
	}
	const FString& Mine = Players.Chips[GbHuman].Text;
	TestTrue(TEXT("El humano: su rol"), Mine.Contains(TEXT("Elegido, Fenrir")));
	TestTrue(TEXT("El humano: su insolencia"), Mine.Contains(TEXT("insolencia 3 de 5")));
	TestTrue(TEXT("El humano: la Prueba del Warden pendiente"), Mine.Contains(TEXT("PRUEBA DEL WARDEN")));
	TestTrue(TEXT("El humano: sus runas por Dominio"), Mine.Contains(TEXT("D1×1")));
	TestTrue(TEXT("El humano: sus poderes activos"), Mine.Contains(TEXT("Celeridad")));
	const FString& Theirs = Players.Chips[GbOther].Text;
	TestTrue(TEXT("Otro asiento: sus runas, como total"), Theirs.Contains(TEXT("Runas: 2 runas")));
	TestFalse(TEXT("Otro asiento: sin sus tipos"), Theirs.Contains(TEXT("D0×2")));
	TestFalse(TEXT("Nadie más lleva la Prueba del Warden"), Theirs.Contains(TEXT("PRUEBA DEL WARDEN")));

	// Con el Brazal del otro revelado, sus tipos.
	FMatchState Revealed = State;
	Revealed.Round = 3;
	Revealed.Seats[GbOther].RevealedUntilRound = 5;
	const TArray<FLegendRow> Shown = GrimoireTableRows(Revealed, GbHuman, Forgotten, GbSeatColors, GbDomainColors);
	TestTrue(TEXT("Brazal revelado: sus tipos"), Shown.Num() == 2 && Shown[0].Chips.IsValidIndex(GbOther) && Shown[0].Chips[GbOther].Text.Contains(TEXT("D0×2")));

	for (int32 Id = 0; Id < Soulbound::Board::DomainCount; ++Id)
	{
		TestEqual(FString::Printf(TEXT("Dominio %d: su color"), Id), Domains.Chips[Id].Color, GbDomainColors[Id]);
		TestEqual(FString::Printf(TEXT("Dominio %d: la línea de la leyenda"), Id), Domains.Chips[Id].Text, DomainLegendText(State, Id, Forgotten[Id]));
	}
	TestTrue(TEXT("El Dominio del humano dice su dueño"), Domains.Chips[GbFenrirDomain].Text.Contains(TEXT("del asiento 0")));
	TestTrue(TEXT("El Olvidado lo dice"), Domains.Chips[2].Text.Contains(TEXT("Olvidado")));
	TestTrue(TEXT("Un libre lo dice"), Domains.Chips[3].Text.Contains(TEXT("libre")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGrimoireOtherTabsTest, "Soulbound.Game.Presentation.Grimorio.TableroComoSeJuegaYCronica", GrimoireBookTestFlags)

bool FGrimoireOtherTabsTest::RunTest(const FString& Parameters)
{
	// CA-HUD-21 (R-HUD-10): El tablero lleva las filas de casillas, Nichos, marcas y destellos, y ninguna de jugadores ni de runas. Cómo se juega
	// lleva el texto del archivo y, si el archivo falta, el mensaje del cargador en vez de quedarse vacía; al final, las teclas. Crónica lleva las
	// mismas líneas y en el mismo orden que la crónica de hoy (una sola función para las dos: ChronicleLines).
	using namespace Soulbound::Presentation;

	// El tablero.
	FBoardLegendLook Look;
	Look.bKit = true;
	Look.bForgottenFloor = true;
	Look.Altar = FLinearColor(0.11f, 0.f, 0.f);
	Look.NicheFree = FLinearColor(0.12f, 0.f, 0.f);
	Look.Source = FLinearColor(0.13f, 0.f, 0.f);
	Look.Node = FLinearColor(0.14f, 0.f, 0.f);
	Look.NicheClaimed = FLinearColor(0.15f, 0.f, 0.f);
	Look.NicheClosed = FLinearColor(0.16f, 0.f, 0.f);
	Look.Reachable = FLinearColor(0.17f, 0.f, 0.f);
	Look.SealMark = FLinearColor(0.18f, 0.f, 0.f);
	Look.ForgottenRing = FLinearColor(0.19f, 0.f, 0.f);
	const TArray<FLegendRow> Board = BoardLegendRows(Look);
	if (TestEqual(TEXT("El tablero: cuatro filas"), Board.Num(), 4))
	{
		TestEqual(TEXT("Casillas"), Board[0].Title, FString(TEXT("Casillas")));
		TestEqual(TEXT("Nichos"), Board[1].Title, FString(TEXT("Nichos")));
		TestEqual(TEXT("Tablero (las marcas)"), Board[2].Title, FString(TEXT("Tablero")));
		TestEqual(TEXT("Los destellos"), Board[3].Title, FString(FlashLegendText));
		const TArray<FLinearColor> TileColors{Look.Altar, Look.NicheFree, Look.Source, Look.Node};
		if (TestEqual(TEXT("Cuatro tipos de casilla"), Board[0].Chips.Num(), 4))
		{
			for (int32 Index = 0; Index < 4; ++Index)
			{
				TestEqual(FString::Printf(TEXT("Casilla %d: el color de la vista"), Index), Board[0].Chips[Index].Color, TileColors[Index]);
			}
			TestTrue(TEXT("Con kit, el Altar es la plataforma"), Board[0].Chips[0].Text.Contains(TEXT("plataforma")));
		}
		const TArray<FLinearColor> NicheColors{Look.NicheFree, Look.NicheClaimed, Look.NicheClosed};
		if (TestEqual(TEXT("Tres estados de Nicho"), Board[1].Chips.Num(), 3))
		{
			for (int32 Index = 0; Index < 3; ++Index)
			{
				TestEqual(FString::Printf(TEXT("Nicho %d: su color"), Index), Board[1].Chips[Index].Color, NicheColors[Index]);
			}
		}
		const TArray<FLinearColor> MarkColors{Look.Reachable, Look.SealMark, Look.ForgottenRing};
		if (TestEqual(TEXT("Tres marcas"), Board[2].Chips.Num(), 3))
		{
			for (int32 Index = 0; Index < 3; ++Index)
			{
				TestEqual(FString::Printf(TEXT("Marca %d: su color"), Index), Board[2].Chips[Index].Color, MarkColors[Index]);
			}
			TestTrue(TEXT("Con el suelo cargado, el agrietado"), Board[2].Chips[2].Text.Contains(TEXT("agrietado")));
		}
		for (const FLegendRow& Row : Board)
		{
			TestFalse(FString::Printf(TEXT("%s: no es de jugadores"), *Row.Title), Row.Title.Contains(TEXT("Jugadores")));
			TestFalse(FString::Printf(TEXT("%s: no es de runas"), *Row.Title), Row.Title.Contains(TEXT("Runas")));
		}
	}
	FBoardLegendLook Plain = Look;
	Plain.bKit = false;
	Plain.bForgottenFloor = false;
	const TArray<FLegendRow> PlainBoard = BoardLegendRows(Plain);
	TestTrue(TEXT("Sin kit, el Altar es la forma básica"), PlainBoard.Num() == 4 && PlainBoard[0].Chips.Num() == 4 && PlainBoard[0].Chips[0].Text.Contains(TEXT("casilla grande")));
	TestTrue(TEXT("Sin suelo, casillas apagadas"), PlainBoard.Num() == 4 && PlainBoard[2].Chips.Num() == 3 && !PlainBoard[2].Chips[2].Text.Contains(TEXT("agrietado")));

	// Cómo se juega. Desde S-048 (R-HUD-18) es un libro: las páginas del archivo y, al final, la de teclas (sus líneas, en
	// Grimorio.LibroTeclasYRecuerdo); si el archivo falta, ninguna página y el mensaje del cargador, para que la pestaña nunca quede vacía.
	FGrimoireKeyNames Keys;
	Keys.Grimoire = TEXT("G");
	Keys.PreviousTab = TEXT("Left");
	Keys.NextTab = TEXT("Right");
	Keys.View = TEXT("C");
	Keys.Overlay = TEXT("L");
	Keys.RingStart = TEXT("Enter");
	Keys.ReturnToMenu = TEXT("Enter");
	Keys.PreviousPage = TEXT("Up");
	Keys.NextPage = TEXT("Down");
	const FHowToPlayLoadResult Loaded = ParseHowToPlayText(TEXT("## Qué es esto\nUn texto.\n\n## Cómo se gana\nOtro.\n\n## El Warden\nEl último."));
	TestTrue(TEXT("El texto de prueba carga"), Loaded.IsSuccess());
	const FHowToPlayBook HowTo = HowToPlayBook(Loaded, Keys);
	TestTrue(TEXT("Cómo se juega: las páginas del archivo, como en el menú"), HowTo.Pages.Num() == Loaded.Pages.Num() + 1
		&& TArray<FHowToPlayPage>(HowTo.Pages.GetData(), Loaded.Pages.Num()) == Loaded.Pages);
	TestTrue(TEXT("Y al final, las teclas"), HowTo.Pages.Num() > 0 && HowTo.Pages.Last().Title == HowToPlayKeysTitle);
	const FHowToPlayLoadResult Missing = LoadHowToPlayText(TEXT("no/existe/como-se-juega.md"));
	TestFalse(TEXT("Sin archivo, el cargador falla"), Missing.IsSuccess());
	const FHowToPlayBook MissingBook = HowToPlayBook(Missing, Keys);
	TestTrue(TEXT("Sin archivo: el mensaje del cargador, no vacía"), !Missing.Message.IsEmpty() && MissingBook.ErrorMessage == Missing.Message && !MissingBook.IsLoaded());
	TestEqual(TEXT("Y sin páginas"), MissingBook.Pages.Num(), 0);

	// Crónica.
	TestEqual(TEXT("La marca de tiempo"), ChronicleStamp(65.4), FString(TEXT("1:05")));
	TestEqual(TEXT("Un tiempo negativo, 0:00"), ChronicleStamp(-3.0), FString(TEXT("0:00")));
	FMatchState State;
	State.Seats.SetNum(2);
	State.EventLog.Add(GbEvent(EMatchEventType::TurnStarted, 0, 0, 5.0));
	State.EventLog.Add(GbEvent(EMatchEventType::Rolled, 0, 3, 6.0));
	State.EventLog.Add(GbEvent(EMatchEventType::TurnStarted, 1, 0, 70.0));
	for (int32 Index = 0; Index < State.EventLog.Num(); ++Index)
	{
		TestFalse(FString::Printf(TEXT("El evento %d tiene línea"), Index), DescribeChronicleEvent(State, Index).IsEmpty());
	}
	FChronicleLook Chronicle;
	Chronicle.SeatColors = {FLinearColor::Red, FLinearColor::Green};
	Chronicle.WardenColor = FLinearColor(1.f, 0.5f, 0.f);
	Chronicle.MaxEvents = 200;
	const TArray<FInstructionNote> Notes{{1, 6.0, TEXT("Una instrucción")}};
	const TArray<FChronicleLine> Lines = ChronicleLines(State, Notes, Chronicle);
	const TArray<FString> Expected{
		TEXT("[1:10] ") + DescribeChronicleEvent(State, 2),
		TEXT("[0:06] Una instrucción"),
		TEXT("[0:06] ") + DescribeChronicleEvent(State, 1),
		TEXT("[0:05] ") + DescribeChronicleEvent(State, 0),
	};
	if (TestEqual(TEXT("Crónica: una línea por evento y la nota"), Lines.Num(), Expected.Num()))
	{
		for (int32 Index = 0; Index < Expected.Num(); ++Index)
		{
			TestEqual(FString::Printf(TEXT("Crónica, línea %d: la más reciente arriba, la nota encima de su evento"), Index), Lines[Index].Text, Expected[Index]);
		}
		TestEqual(TEXT("La del asiento 1, de su color"), Lines[0].Color, FLinearColor::Green);
		TestEqual(TEXT("La nota, del color del Warden"), Lines[1].Color, Chronicle.WardenColor);
		TestEqual(TEXT("La del asiento 0, del suyo"), Lines[2].Color, FLinearColor::Red);
	}
	Chronicle.MaxEvents = 1;
	const TArray<FChronicleLine> Last = ChronicleLines(State, Notes, Chronicle);
	TestTrue(TEXT("Con el límite, solo los últimos eventos"), Last.Num() == 1 && Last[0].Text == Expected[0]);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGrimoireCloseTest, "Soulbound.Game.Presentation.Grimorio.SeCierraConDecisionPropia", GrimoireBookTestFlags)

bool FGrimoireCloseTest::RunTest(const FString& Parameters)
{
	// CA-HUD-22 (R-HUD-11, precisiones del autor del 17/11/2026): se cierra al abrirse una decisión nueva del humano (también bloqueada porque habla
	// el Warden) o un anillo suyo; sigue abierto con una decisión de otro asiento, con el Warden hablando sin decisión propia, con un resultado y con
	// la decisión propia que ya estaba abierta al abrirlo. Durante un anillo del humano, el Grimorio no se abre.
	using namespace Soulbound::Presentation;
	FGrimoireCloseWatch Watch;
	TestFalse(TEXT("Sin nada: sigue"), Watch.Observe({}, false));
	TestFalse(TEXT("La espera a otro asiento: sigue"), Watch.Observe(GbView(EDecisionInput::Wait, TEXT("Wait")), false));
	TestFalse(TEXT("El Warden hablando sin decisión propia (el panel no enseña nada): sigue"), Watch.Observe({}, false));
	TestFalse(TEXT("Un resultado: sigue"), Watch.Observe(GbView(EDecisionInput::Result, TEXT("Result")), false));
	TestTrue(TEXT("Una decisión propia nueva: se cierra"), Watch.Observe(GbView(EDecisionInput::Buttons, TEXT("Buttons|Dilemma")), false));
	TestFalse(TEXT("La misma decisión, que ya estaba (se abrió el Grimorio para consultar): sigue"), Watch.Observe(GbView(EDecisionInput::Buttons, TEXT("Buttons|Dilemma")), false));
	TestFalse(TEXT("La misma, bloqueada porque habla el Warden: sigue"), Watch.Observe(GbView(EDecisionInput::Buttons, TEXT("Buttons|Dilemma"), true), false));
	TestTrue(TEXT("Otra decisión propia, bloqueada porque habla el Warden: se cierra"), Watch.Observe(GbView(EDecisionInput::Orb, TEXT("Orb"), true), false));
	TestFalse(TEXT("Se contesta y no queda nada: sigue"), Watch.Observe({}, false));
	TestTrue(TEXT("El destino de su turno: se cierra"), Watch.Observe(GbView(EDecisionInput::Tile, TEXT("Tile")), false));
	TestFalse(TEXT("Otra vez la espera: sigue"), Watch.Observe(GbView(EDecisionInput::Wait, TEXT("Wait")), false));
	TestTrue(TEXT("El voto del humano: se cierra"), Watch.Observe(GbView(EDecisionInput::Token, TEXT("Token")), false));
	TestTrue(TEXT("Se abre su anillo: se cierra"), Watch.Observe({}, true));
	TestFalse(TEXT("El anillo sigue abierto: nada nuevo"), Watch.Observe({}, true));
	TestFalse(TEXT("Se cierra el anillo: sigue"), Watch.Observe({}, false));
	TestTrue(TEXT("Un anillo nuevo: se cierra"), Watch.Observe({}, true));
	TestFalse(TEXT("Durante un anillo del humano, el Grimorio no se abre"), CanOpenGrimoire(true));
	TestTrue(TEXT("Sin anillo, sí"), CanOpenGrimoire(false));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGrimoireButtonTest, "Soulbound.Game.Presentation.Grimorio.BotonTextoYVisibilidad", GrimoireBookTestFlags)

bool FGrimoireButtonTest::RunTest(const FString& Parameters)
{
	// CA-HUD-23 (R-HUD-12, corrección del autor del 17/11/2026): el texto del botón es "Grimorio (G)" con la tecla por defecto y lleva el nombre de
	// otra tecla si se configura. Se ve salvo en el prólogo, con la partida terminada o con una frase del Warden en pantalla. Las teclas de ocultar
	// paneles no cuentan: la función no las recibe, así que da lo mismo con cualquier combinación de ellas.
	using namespace Soulbound::Presentation;
	TestEqual(TEXT("Con la tecla por defecto"), GrimoireButtonLabel(TEXT("G")), FString(TEXT("Grimorio (G)")));
	TestEqual(TEXT("Con otra tecla"), GrimoireButtonLabel(TEXT("F1")), FString(TEXT("Grimorio (F1)")));
	TestEqual(TEXT("Con una tecla que tiene nombre en español"), GrimoireButtonLabel(TEXT("Enter")), FString(TEXT("Grimorio (Intro)")));
	TestTrue(TEXT("En partida, sin frase: se ve"), IsGrimoireButtonShown(false, false, false));
	TestFalse(TEXT("En el prólogo, no"), IsGrimoireButtonShown(true, false, false));
	TestFalse(TEXT("Con la partida terminada, no"), IsGrimoireButtonShown(false, true, false));
	TestFalse(TEXT("Con una frase del Warden en pantalla, no"), IsGrimoireButtonShown(false, false, true));
	TestFalse(TEXT("Con todo a la vez, no"), IsGrimoireButtonShown(true, true, true));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGrimoireKeyNamesTest, "Soulbound.Game.Presentation.Grimorio.TeclasEnEspanol", GrimoireBookTestFlags)

bool FGrimoireKeyNamesTest::RunTest(const FString& Parameters)
{
	// Corrección del autor del 17/11/2026 tras PIE (R-HUD-10): el título del Grimorio decía "Left y Right, pestañas", los nombres que da Unreal. Dice
	// "flechas izquierda y derecha"; con otras teclas, su nombre en español donde lo haya, como ya se hacía con Enter (Intro). Lo mismo en el bloque
	// "Teclas" de "Cómo se juega".
	using namespace Soulbound::Presentation;
	TestEqual(TEXT("Enter es Intro"), SpanishKeyName(TEXT("Enter")), FString(TEXT("Intro")));
	TestEqual(TEXT("Left es flecha izquierda"), SpanishKeyName(TEXT("Left")), FString(TEXT("flecha izquierda")));
	TestEqual(TEXT("Right es flecha derecha"), SpanishKeyName(TEXT("Right")), FString(TEXT("flecha derecha")));
	TestEqual(TEXT("Up es flecha arriba"), SpanishKeyName(TEXT("Up")), FString(TEXT("flecha arriba")));
	TestEqual(TEXT("Down es flecha abajo"), SpanishKeyName(TEXT("Down")), FString(TEXT("flecha abajo")));
	TestEqual(TEXT("Space Bar es Espacio"), SpanishKeyName(TEXT("Space Bar")), FString(TEXT("Espacio")));
	TestEqual(TEXT("Una letra, tal cual"), SpanishKeyName(TEXT("G")), FString(TEXT("G")));
	TestEqual(TEXT("Una tecla de función, tal cual"), SpanishKeyName(TEXT("F1")), FString(TEXT("F1")));
	TestEqual(TEXT("La línea bajo un botón sigue diciendo Intro"), KeyHintText(TEXT("Enter")), FString(TEXT("o pulsa Intro")));

	// S-045 (R-HUD-16): el Grimorio deja de anunciar H para cerrarlo.
	TestEqual(TEXT("Título con las teclas por defecto"), GrimoireKeysHint(TEXT("G"), TEXT("Left"), TEXT("Right")),
		FString(TEXT("G para cerrar; flechas izquierda y derecha, pestañas")));
	TestEqual(TEXT("Título con otras teclas de pestaña"), GrimoireKeysHint(TEXT("G"), TEXT("Q"), TEXT("E")),
		FString(TEXT("G para cerrar; Q y E, pestañas")));
	TestEqual(TEXT("Título con teclas que tienen nombre en español"), GrimoireKeysHint(TEXT("Enter"), TEXT("Up"), TEXT("Down")),
		FString(TEXT("Intro para cerrar; flecha arriba y flecha abajo, pestañas")));

	FGrimoireKeyNames Keys;
	Keys.Grimoire = TEXT("G");
	Keys.PreviousTab = TEXT("Left");
	Keys.NextTab = TEXT("Right");
	Keys.View = TEXT("C");
	Keys.Overlay = TEXT("L");
	Keys.RingStart = TEXT("Enter");
	Keys.ReturnToMenu = TEXT("Enter");
	Keys.PreviousPage = TEXT("Up");
	Keys.NextPage = TEXT("Down");
	// Desde S-048 (R-HUD-18), las teclas son la última página del libro.
	const FString HowTo = HowToPlayKeysPage(Keys).Body;
	TestTrue(TEXT("Cómo se juega: las flechas, en español"), HowTo.Contains(TEXT("flechas izquierda y derecha")));
	TestTrue(TEXT("Cómo se juega: Intro"), HowTo.Contains(TEXT("Intro:")));
	TestFalse(TEXT("Cómo se juega: sin Left"), HowTo.Contains(TEXT("Left")));
	TestFalse(TEXT("Cómo se juega: sin Enter"), HowTo.Contains(TEXT("Enter")));
	return true;
}

namespace
{
	/** Un libro de cuatro páginas del autor, con las teclas de siempre. */
	Soulbound::Presentation::FHowToPlayBook GbFourPageBook(FAutomationTestBase& Test, Soulbound::Presentation::FGrimoireKeyNames& OutKeys)
	{
		using namespace Soulbound::Presentation;
		OutKeys = FGrimoireKeyNames();
		OutKeys.Grimoire = TEXT("G");
		OutKeys.PreviousTab = TEXT("Left");
		OutKeys.NextTab = TEXT("Right");
		OutKeys.View = TEXT("C");
		OutKeys.Overlay = TEXT("L");
		OutKeys.RingStart = TEXT("Enter");
		OutKeys.ReturnToMenu = TEXT("Enter");
		OutKeys.PreviousPage = TEXT("Up");
		OutKeys.NextPage = TEXT("Down");
		const FHowToPlayLoadResult Loaded = ParseHowToPlayText(TEXT("## Uno\nA.\n\n## Dos\nB.\n\n## Tres\nC.\n\n## Cuatro\nD.\n"), TEXT("prueba.md"));
		Test.TestTrue(FString::Printf(TEXT("Las cuatro páginas cargan (%s)"), *Loaded.Message), Loaded.IsSuccess());
		return HowToPlayBook(Loaded, OutKeys);
	}

	bool GbBoxInside(const FBox2f& Inner, const FBox2f& Outer)
	{
		constexpr float Slack = 0.01f;
		return Inner.Min.X >= Outer.Min.X - Slack && Inner.Min.Y >= Outer.Min.Y - Slack && Inner.Max.X <= Outer.Max.X + Slack && Inner.Max.Y <= Outer.Max.Y + Slack;
	}

	bool GbBoxValid(const FBox2f& Box)
	{
		return Box.Max.X >= Box.Min.X && Box.Max.Y >= Box.Min.Y;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGrimoireBookPagesTest, "Soulbound.Game.Presentation.Grimorio.LibroPasarPagina", GrimoireBookTestFlags)

bool FGrimoireBookPagesTest::RunTest(const FString& Parameters)
{
	// CA-HUD-33 (R-HUD-18): con cuatro páginas del autor hay cinco en el libro, la última "Teclas". Se empieza en la primera; "Siguiente" avanza una y
	// se queda en la última; "Anterior" retrocede una y se queda en la primera (no es cíclico); el pie dice "N de M" con la página y el total.
	using namespace Soulbound::Presentation;
	FGrimoireKeyNames Keys;
	const FHowToPlayBook Book = GbFourPageBook(*this, Keys);
	if (!TestEqual(TEXT("Cinco páginas en el libro"), Book.Pages.Num(), 5))
	{
		return false;
	}
	const TArray<FString> Titles{TEXT("Uno"), TEXT("Dos"), TEXT("Tres"), TEXT("Cuatro"), FString(HowToPlayKeysTitle)};
	for (int32 Index = 0; Index < Titles.Num(); ++Index)
	{
		TestEqual(FString::Printf(TEXT("La página %d, en su orden"), Index), Book.Pages[Index].Title, Titles[Index]);
	}
	TestTrue(TEXT("El libro cargó"), Book.IsLoaded());
	const int32 Count = Book.Pages.Num();

	FGrimoireTabState Tab;
	TestEqual(TEXT("Se empieza en la primera"), Tab.Page, 0);
	TestEqual(TEXT("El pie de la primera"), HowToPlayPageFooter(Tab.Page, Count), FString(TEXT("1 de 5")));
	Tab.PreviousPage(Count);
	TestEqual(TEXT("Anterior en la primera: se queda"), Tab.Page, 0);
	Tab.NextPage(Count);
	TestEqual(TEXT("Siguiente: la segunda"), Tab.Page, 1);
	TestEqual(TEXT("El pie de la segunda"), HowToPlayPageFooter(Tab.Page, Count), FString(TEXT("2 de 5")));
	for (int32 Step = 0; Step < 10; ++Step)
	{
		Tab.NextPage(Count);
	}
	TestEqual(TEXT("Siguiente muchas veces: se queda en la última"), Tab.Page, Count - 1);
	TestEqual(TEXT("El pie de la última"), HowToPlayPageFooter(Tab.Page, Count), FString(TEXT("5 de 5")));
	Tab.PreviousPage(Count);
	TestEqual(TEXT("Anterior: retrocede una"), Tab.Page, Count - 2);
	TestEqual(TEXT("El pie de la penúltima"), HowToPlayPageFooter(Tab.Page, Count), FString(TEXT("4 de 5")));
	for (int32 Step = 0; Step < 10; ++Step)
	{
		Tab.PreviousPage(Count);
	}
	TestEqual(TEXT("Anterior muchas veces: se queda en la primera"), Tab.Page, 0);
	TestTrue(TEXT("Sin páginas, el pie está vacío"), HowToPlayPageFooter(0, 0).IsEmpty());
	Tab.NextPage(0);
	TestEqual(TEXT("Sin páginas, Siguiente no se mueve"), Tab.Page, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGrimoireBookKeysTest, "Soulbound.Game.Presentation.Grimorio.LibroTeclasYRecuerdo", GrimoireBookTestFlags)

bool FGrimoireBookKeysTest::RunTest(const FString& Parameters)
{
	// CA-HUD-34 (R-HUD-18): la página "Teclas" lleva las teclas de la vista con el nombre de las configuradas, en español donde lo haya: las del
	// Grimorio, sus pestañas y sus páginas (precisión del autor del 17/11/2026: flechas arriba y abajo), 1 a 9 y 0, Intro, la vista y las etiquetas;
	// sin H, P, T ni D (S-045). Al cerrar y abrir el Grimorio y volver a la pestaña, la página es la que se dejó.
	using namespace Soulbound::Presentation;
	FGrimoireKeyNames Keys;
	const FHowToPlayBook Book = GbFourPageBook(*this, Keys);
	const FHowToPlayPage KeysPage = HowToPlayKeysPage(Keys);
	TestEqual(TEXT("Su título"), KeysPage.Title, FString(HowToPlayKeysTitle));
	TestTrue(TEXT("Es la última del libro"), Book.Pages.Num() > 0 && Book.Pages.Last() == KeysPage);
	TestTrue(TEXT("Sin comentario ni imagen"), KeysPage.WardenComment.IsEmpty() && KeysPage.ImageName.IsEmpty());
	const FString& Body = KeysPage.Body;
	for (const TCHAR* Expected : {TEXT("G:"), TEXT("flechas izquierda y derecha:"), TEXT("flechas arriba y abajo:"), TEXT("1 a 9 y 0:"), TEXT("Intro:"), TEXT("C:"), TEXT("L:")})
	{
		TestTrue(FString::Printf(TEXT("Nombra %s"), Expected), Body.Contains(Expected));
	}
	TestTrue(TEXT("Las de página dicen lo que hacen"), Body.Contains(TEXT("página anterior y siguiente")));
	TestTrue(TEXT("Intro también vuelve al menú al acabar"), Body.Contains(TEXT("vuelve al menú")));
	for (const TCHAR* Raw : {TEXT("Left"), TEXT("Right"), TEXT("Up"), TEXT("Down"), TEXT("Enter")})
	{
		TestFalse(FString::Printf(TEXT("Sin el nombre de Unreal %s"), Raw), Body.Contains(Raw));
	}
	TArray<FString> Lines;
	Body.ParseIntoArrayLines(Lines);
	for (const TCHAR* Gone : {TEXT("H"), TEXT("P"), TEXT("T"), TEXT("D")})
	{
		TestFalse(FString::Printf(TEXT("Ninguna línea empieza por %s"), Gone),
			Lines.ContainsByPredicate([Gone](const FString& Line) { return Line.StartsWith(FString(Gone) + TEXT(":")); }));
	}
	TestFalse(TEXT("Ni columna ni paneles que ocultar"), Body.Contains(TEXT("columna")) || Body.Contains(TEXT("paneles")));

	// Con otras teclas configuradas, sus nombres.
	FGrimoireKeyNames Other = Keys;
	Other.PreviousPage = TEXT("Q");
	Other.NextPage = TEXT("E");
	Other.Grimoire = TEXT("F1");
	Other.ReturnToMenu = TEXT("Space Bar");
	const FString OtherBody = HowToPlayKeysPage(Other).Body;
	TestTrue(TEXT("Otras teclas de página: Q y E"), OtherBody.Contains(TEXT("Q y E:")));
	TestTrue(TEXT("Otra tecla del Grimorio: F1"), OtherBody.Contains(TEXT("F1:")));
	TestTrue(TEXT("Volver al menú con otra tecla: su línea, en español"), OtherBody.Contains(TEXT("Espacio:")));

	// Las de la vista por defecto (Config): flecha arriba y flecha abajo.
	const ABoardViewActor* View = GetDefault<ABoardViewActor>();
	if (TestNotNull(TEXT("La vista por defecto"), View))
	{
		TestTrue(TEXT("Página anterior: flecha arriba"), View->GrimoirePreviousPageKey == EKeys::Up);
		TestTrue(TEXT("Página siguiente: flecha abajo"), View->GrimoireNextPageKey == EKeys::Down);
		TestTrue(TEXT("La página de teclas de la vista nombra las flechas arriba y abajo"), HowToPlayKeysPage(View->GrimoireKeyNames()).Body.Contains(TEXT("flechas arriba y abajo")));
	}

	// El recuerdo: la página se queda al cerrar, abrir y volver; una partida nueva (un estado nuevo) empieza en la primera.
	const int32 Count = Book.Pages.Num();
	FGrimoireTabState Tab;
	Tab.Select(EGrimoireTab::HowToPlay);
	Tab.NextPage(Count);
	Tab.NextPage(Count);
	TestEqual(TEXT("En la tercera"), Tab.Page, 2);
	Tab.Open();
	TestEqual(TEXT("Al abrir, la pestaña es Tú"), static_cast<int32>(Tab.Active), static_cast<int32>(EGrimoireTab::You));
	Tab.Next();
	Tab.Select(EGrimoireTab::HowToPlay);
	TestEqual(TEXT("Al volver a Cómo se juega, la página que se dejó"), Tab.Page, 2);
	TestEqual(TEXT("Una partida nueva, la primera"), FGrimoireTabState().Page, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGrimoireBookLayoutTest, "Soulbound.Game.Presentation.Grimorio.ZonaDeContenidoDelLibro", GrimoireBookTestFlags)

bool FGrimoireBookLayoutTest::RunTest(const FString& Parameters)
{
	// CA-HUD-36 (R-HUD-20): la zona de contenido dentro del libro queda, para un panel dado y unos márgenes de borde, dentro del panel y fuera de los
	// bordes; ningún texto cruza el lomo (las dos páginas quedan a cada lado del centro, con su hueco); con la textura ausente, la zona es la de hoy.
	using namespace Soulbound::Presentation;
	const FMargin Border(0.08f, 0.1f, 0.06f, 0.12f);
	constexpr float Inset = 12.f;
	constexpr float Spine = 0.06f;

	// Un caso a mano: la imagen, el doble del panel; escala 0,5 en los dos ejes.
	const FGrimoireBookLayout Exact = GrimoireBookLayout(FVector2f(1000.f, 700.f), FVector2f(2000.f, 1400.f), FMargin(0.08f), Inset, Spine);
	TestTrue(TEXT("Con imagen, hay libro"), Exact.bBook);
	TestTrue(TEXT("La imagen se pinta al tamaño del panel"), Exact.BrushSize.Equals(FVector2f(1000.f, 700.f), 0.01f));
	TestTrue(TEXT("El contenido, dentro de los bordes y del margen"), Exact.Content.Min.Equals(FVector2f(92.f, 68.f), 0.01f) && Exact.Content.Max.Equals(FVector2f(908.f, 632.f), 0.01f));

	const TArray<FVector2f> Panels{FVector2f(1000.f, 700.f), FVector2f(1100.f, 830.f), FVector2f(992.f, 582.f), FVector2f(600.f, 900.f), FVector2f(300.f, 200.f)};
	const TArray<FVector2f> Images{FVector2f(2048.f, 1365.f), FVector2f(1024.f, 1024.f), FVector2f(512.f, 300.f)};
	for (const FVector2f& Panel : Panels)
	{
		for (const FVector2f& Image : Images)
		{
			const FString Case = FString::Printf(TEXT("Panel %.0fx%.0f, imagen %.0fx%.0f"), Panel.X, Panel.Y, Image.X, Image.Y);
			const FGrimoireBookLayout Layout = GrimoireBookLayout(Panel, Image, Border, Inset, Spine);
			const FBox2f Whole(FVector2f::ZeroVector, Panel);
			TestTrue(Case + TEXT(": hay libro"), Layout.bBook);
			TestTrue(Case + TEXT(": zonas válidas"), GbBoxValid(Layout.Content) && GbBoxValid(Layout.LeftPage) && GbBoxValid(Layout.RightPage));
			TestTrue(Case + TEXT(": el contenido, dentro del panel"), GbBoxInside(Layout.Content, Whole));
			// Escala uniforme: la imagen pintada guarda la proporción de la imagen y no se sale del panel.
			TestTrue(Case + TEXT(": la imagen, con su proporción"), FMath::IsNearlyEqual(Layout.BrushSize.X * Image.Y, Layout.BrushSize.Y * Image.X, 1.f));
			TestTrue(Case + TEXT(": la imagen cabe en el panel"), Layout.BrushSize.X <= Panel.X + 0.01f && Layout.BrushSize.Y <= Panel.Y + 0.01f);
			// Fuera de los bordes: cada borde mide su fracción de la imagen pintada.
			const float Left = Border.Left * Layout.BrushSize.X;
			const float Top = Border.Top * Layout.BrushSize.Y;
			const float Right = Border.Right * Layout.BrushSize.X;
			const float Bottom = Border.Bottom * Layout.BrushSize.Y;
			const FBox2f Inside(FVector2f(Left, Top), FVector2f(Panel.X - Right, Panel.Y - Bottom));
			if (GbBoxValid(Inside))
			{
				TestTrue(Case + TEXT(": el contenido no pisa los bordes"), GbBoxInside(Layout.Content, Inside));
			}
			// El lomo, en el centro del panel: la página izquierda acaba antes y la derecha empieza después, y las dos están en el contenido.
			const float Center = Panel.X * 0.5f;
			TestTrue(Case + TEXT(": la página izquierda, antes del lomo"), Layout.LeftPage.Max.X <= Center + 0.01f);
			TestTrue(Case + TEXT(": la derecha, después"), Layout.RightPage.Min.X >= Center - 0.01f);
			TestTrue(Case + TEXT(": las dos, dentro del contenido"), GbBoxInside(Layout.LeftPage, Layout.Content) && GbBoxInside(Layout.RightPage, Layout.Content));
			if (Layout.Content.Max.X - Layout.Content.Min.X > 1.f && Layout.Content.Min.X < Center && Layout.Content.Max.X > Center)
			{
				TestTrue(Case + TEXT(": con hueco en el lomo"), Layout.RightPage.Min.X - Layout.LeftPage.Max.X >= Spine * (Layout.Content.Max.X - Layout.Content.Min.X) - 0.01f);
			}
		}
	}

	// Sin textura: la zona de hoy, de una columna.
	const FVector2f Panel(1100.f, 830.f);
	const FGrimoireBookLayout Flat = GrimoireBookLayout(Panel, FVector2f::ZeroVector, Border, Inset, Spine);
	TestFalse(TEXT("Sin textura, sin libro"), Flat.bBook);
	TestTrue(TEXT("Sin textura, la zona de hoy"), Flat.Content.Min.Equals(GrimoireFlatPadding, 0.01f) && Flat.Content.Max.Equals(Panel - GrimoireFlatPadding, 0.01f));
	TestTrue(TEXT("Sin textura, una sola columna"), Flat.LeftPage == Flat.Content && Flat.RightPage == Flat.Content);
	TestTrue(TEXT("Sin textura, nada que pintar"), Flat.BrushSize.IsNearlyZero());

	// R-HUD-18: la imagen al lado del texto si cabe; si no, debajo.
	TestTrue(TEXT("Ancho de sobra: la imagen al lado"), HowToPlayImageBeside(800.f, 300.f));
	TestTrue(TEXT("Justo: al lado"), HowToPlayImageBeside(600.f, 300.f));
	TestFalse(TEXT("Estrecho: la imagen debajo"), HowToPlayImageBeside(500.f, 300.f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGrimoireColumnTextWidthTest, "Soulbound.Game.Presentation.Grimorio.TextoAlAnchoDeSuColumna", GrimoireBookTestFlags)

bool FGrimoireColumnTextWidthTest::RunTest(const FString& Parameters)
{
	// CA-HUD-39 (R-HUD-20, corrección 1 del autor del 17/11/2026 tras PIE): en "Tú" los poderes de la página derecha se cortaban por el borde, porque
	// se partían a un ancho mayor que el de su columna. Para cada columna del libro (y la única sin él) y cada sangría, el ancho de partir más la
	// sangría y la reserva de la barra de desplazamiento no pasa del ancho de la columna; y cada página usa el suyo, no el de la otra.
	using namespace Soulbound::Presentation;
	TestTrue(TEXT("La reserva cubre la barra, sus márgenes y el hueco"),
		GrimoireScrollRoom >= GrimoireScrollBarThickness + 2.f * GrimoireScrollBarPadding + GrimoireScrollGap - 0.01f && GrimoireScrollBarThickness > 0.f);

	// Márgenes distintos a cada lado, para que las dos páginas no midan lo mismo.
	const FMargin Border(0.12f, 0.08f, 0.05f, 0.08f);
	const TArray<FVector2f> Panels{FVector2f(1100.f, 830.f), FVector2f(992.f, 582.f), FVector2f(600.f, 900.f)};
	// Las sangrías de las líneas del panel: ninguna, la de la insolencia (4 y 4), la tarjeta (8 y 8) y la de cada poder (12 y 4).
	const TArray<float> Indents{0.f, 8.f, 16.f, 16.f, 30.f};
	for (const FVector2f& Panel : Panels)
	{
		for (const bool bBook : {true, false})
		{
			const FGrimoireBookLayout Layout = GrimoireBookLayout(Panel, bBook ? FVector2f(2048.f, 1365.f) : FVector2f::ZeroVector, Border, 12.f, 0.06f);
			const float Left = Layout.LeftPage.Max.X - Layout.LeftPage.Min.X;
			const float Right = Layout.RightPage.Max.X - Layout.RightPage.Min.X;
			if (bBook)
			{
				TestFalse(FString::Printf(TEXT("Panel %.0f: las dos páginas miden distinto"), Panel.X), FMath::IsNearlyEqual(Left, Right, 0.5f));
			}
			for (const float Column : {Left, Right})
			{
				for (const float Indent : Indents)
				{
					const FString Case = FString::Printf(TEXT("Panel %.0fx%.0f, %s, columna %.0f, sangría %.0f"), Panel.X, Panel.Y, bBook ? TEXT("libro") : TEXT("sin libro"), Column, Indent);
					const float Width = GrimoireColumnTextWidth(Column, Indent);
					TestTrue(Case + TEXT(": nunca menos de 1"), Width >= 1.f);
					TestTrue(Case + TEXT(": cabe en su columna con la sangría y la barra"), Width + Indent + GrimoireScrollRoom <= Column + 0.01f);
					TestTrue(Case + TEXT(": usa todo el ancho de su columna, no el de la otra"), FMath::IsNearlyEqual(Width, Column - Indent - GrimoireScrollRoom, 0.01f));
				}
			}
		}
	}
	TestEqual(TEXT("Una columna sin sitio: 1"), GrimoireColumnTextWidth(10.f, 30.f), 1.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGrimoireTitleAndFootTest, "Soulbound.Game.Presentation.Grimorio.TituloYTeclasAlPie", GrimoireBookTestFlags)

bool FGrimoireTitleAndFootTest::RunTest(const FString& Parameters)
{
	// CA-HUD-40 (R-HUD-20, corrección 2 del autor del 17/11/2026 tras PIE): el título ocupaba tres líneas. Dice solo "Grimorio"; las teclas van en una
	// línea pequeña al pie, una mitad por página: a la izquierda, cerrar y pestañas; a la derecha, páginas. Con los nombres de las teclas configuradas.
	// Ajustado por la corrección 5 (segunda prueba en PIE): la parte de páginas, solo en "Cómo se juega", la única pestaña con páginas; en las demás, el
	// pie de la derecha queda vacío, y la línea única sin el libro lleva solo cerrar y pestañas.
	using namespace Soulbound::Presentation;
	TestEqual(TEXT("El título"), FString(GrimoireTitleText), FString(TEXT("Grimorio")));
	FGrimoireKeyNames Keys;
	Keys.Grimoire = TEXT("G");
	Keys.PreviousTab = TEXT("Left");
	Keys.NextTab = TEXT("Right");
	Keys.PreviousPage = TEXT("Up");
	Keys.NextPage = TEXT("Down");
	const FString Left(TEXT("G para cerrar; flechas izquierda y derecha, pestañas"));
	const FString Pages(TEXT("flechas arriba y abajo, páginas"));
	for (const EGrimoireTab Tab : GrimoireTabs())
	{
		const bool bPages = Tab == EGrimoireTab::HowToPlay;
		const FString Name = GrimoireTabTitle(Tab);
		const FGrimoireFootHints Foot = GrimoireFootHints(Keys, Tab);
		TestEqual(TEXT("Al pie de la izquierda, cerrar y pestañas, en ") + Name, Foot.Left, Left);
		TestEqual(TEXT("Al pie de la izquierda, lo mismo que GrimoireKeysHint, en ") + Name, Foot.Left, GrimoireKeysHint(Keys.Grimoire, Keys.PreviousTab, Keys.NextTab));
		TestEqual(TEXT("Al pie de la derecha, páginas solo en Cómo se juega: ") + Name, Foot.Right, bPages ? Pages : FString());
		TestEqual(TEXT("Sin el libro, la línea única: ") + Name, Foot.OneLine(), bPages ? Left + TEXT("; ") + Pages : Left);
	}

	FGrimoireKeyNames Other = Keys;
	Other.Grimoire = TEXT("F1");
	Other.PreviousTab = TEXT("Q");
	Other.NextTab = TEXT("E");
	Other.PreviousPage = TEXT("Z");
	Other.NextPage = TEXT("Space Bar");
	const FGrimoireFootHints OtherFoot = GrimoireFootHints(Other, EGrimoireTab::HowToPlay);
	TestEqual(TEXT("Otras teclas, a la izquierda"), OtherFoot.Left, FString(TEXT("F1 para cerrar; Q y E, pestañas")));
	TestEqual(TEXT("Otras teclas, a la derecha, en español donde lo haya"), OtherFoot.Right, FString(TEXT("Z y Espacio, páginas")));
	TestEqual(TEXT("Otras teclas, la línea única"), OtherFoot.OneLine(), FString(TEXT("F1 para cerrar; Q y E, pestañas; Z y Espacio, páginas")));
	TestEqual(TEXT("Otras teclas, sin páginas en la Crónica"), GrimoireFootHints(Other, EGrimoireTab::Chronicle).OneLine(), FString(TEXT("F1 para cerrar; Q y E, pestañas")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGrimoireChronicleHeadingTest, "Soulbound.Game.Presentation.Grimorio.CabeceraDeLaCronica", GrimoireBookTestFlags)

bool FGrimoireChronicleHeadingTest::RunTest(const FString& Parameters)
{
	// CA-HUD-42 (R-HUD-20, corrección 6 del autor del 17/11/2026): el encabezado de la Crónica, con sus tildes.
	TestEqual(TEXT("El encabezado de la Crónica"), FString(Soulbound::Presentation::ChronicleHeadingText), FString(TEXT("CRÓNICA (la última arriba; rueda para ir al inicio)")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGrimoireFontTest, "Soulbound.Game.Presentation.Grimorio.FuenteDelLibro", GrimoireBookTestFlags)

bool FGrimoireFontTest::RunTest(const FString& Parameters)
{
	// CA-HUD-41 (R-HUD-20, corrección 4 del autor del 17/11/2026): sin fuente, la letra es la de hoy, con el tipo y el tamaño pedidos; con fuente, esa
	// fuente, con su tipo normal o negrita y el tamaño pedido.
	using namespace Soulbound::Presentation;
	const FGrimoireFont None;
	for (const bool bBold : {false, true})
	{
		const FSlateFontInfo Expected = FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", 17);
		const FSlateFontInfo Got = GrimoireFontInfo(None, bBold, 17);
		const FString Kind = bBold ? TEXT("negrita") : TEXT("normal");
		TestTrue(TEXT("Sin fuente, la de hoy: ") + Kind, Got.FontObject == Expected.FontObject && Got.TypefaceFontName == Expected.TypefaceFontName
			&& Got.CompositeFont == Expected.CompositeFont);
		TestTrue(TEXT("Sin fuente, el tamaño pedido: ") + Kind, FMath::IsNearlyEqual(static_cast<float>(Got.Size), static_cast<float>(Expected.Size)));
	}
	TestTrue(TEXT("Un tamaño de 0 se queda en 1"), FMath::IsNearlyEqual(static_cast<float>(GrimoireFontInfo(None, false, 0).Size), 1.f));

	FGrimoireFont Own;
	Own.Font = NewObject<UFont>(GetTransientPackage());
	Own.RegularTypeface = TEXT("Normal");
	Own.BoldTypeface = TEXT("Negrita");
	const FSlateFontInfo Regular = GrimoireFontInfo(Own, false, 14);
	const FSlateFontInfo Bold = GrimoireFontInfo(Own, true, 20);
	TestTrue(TEXT("Con fuente, la normal es esa fuente"), Regular.FontObject == Own.Font);
	TestTrue(TEXT("Con fuente, su tipo normal"), Regular.TypefaceFontName == Own.RegularTypeface);
	TestTrue(TEXT("Con fuente, el tamaño de la normal"), FMath::IsNearlyEqual(static_cast<float>(Regular.Size), 14.f));
	TestTrue(TEXT("Con fuente, la negrita es esa fuente"), Bold.FontObject == Own.Font);
	TestTrue(TEXT("Con fuente, su tipo negrita"), Bold.TypefaceFontName == Own.BoldTypeface);
	TestTrue(TEXT("Con fuente, el tamaño de la negrita"), FMath::IsNearlyEqual(static_cast<float>(Bold.Size), 20.f));
	return true;
}

#endif
