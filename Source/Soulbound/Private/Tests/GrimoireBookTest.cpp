// Tests del Grimorio como libro del juego (S-044, R-HUD-10 a R-HUD-12): sus pestañas, lo que lleva cada una, cuándo se cierra solo y su botón.
// Funciones puras sobre un FMatchState armado a mano con los Elegidos de elegidos.json: sin mundo, sin actor y sin Slate.
// Filtro: "Soulbound.Game.Presentation.Grimorio".

#include "../../Presentation/ChronicleLines.h"
#include "../../Presentation/EventText.h"
#include "../../Presentation/GrimoireBook.h"
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
	TestEqual(TEXT("Eco tras la carrera: la runa de su hogar"), Echo.HomeRuneLine, FString(TEXT("La runa de tu hogar (D4, Dominio 4) te da Celeridad: tiras dos dados")));
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

	// Cómo se juega.
	FGrimoireKeyNames Keys;
	Keys.Grimoire = TEXT("G");
	Keys.PreviousTab = TEXT("Izquierda");
	Keys.NextTab = TEXT("Derecha");
	Keys.ColumnTab = TEXT("P");
	Keys.Panels = TEXT("H");
	Keys.View = TEXT("C");
	Keys.RingStart = TEXT("Intro");
	const FHowToPlayLoadResult Loaded = ParseHowToPlayText(TEXT("## Qué es esto\nUn texto.\n\n## Cómo se gana\nOtro.\n\n## El Warden\nEl último."));
	TestTrue(TEXT("El texto de prueba carga"), Loaded.IsSuccess());
	const FString HowTo = GrimoireHowToPlayText(Loaded, Keys);
	TestTrue(TEXT("Cómo se juega: empieza por el texto, como en el menú"), HowTo.StartsWith(HowToPlayDisplayText(Loaded.Text)));
	const int32 KeysAt = HowTo.Find(TEXT("Teclas"));
	TestTrue(TEXT("Y al final, las teclas"), KeysAt > HowToPlayDisplayText(Loaded.Text).Len() - 1);
	for (const FString& Name : {Keys.Grimoire, Keys.PreviousTab, Keys.NextTab, Keys.ColumnTab, Keys.Panels, Keys.View, Keys.RingStart})
	{
		TestTrue(FString::Printf(TEXT("Las teclas nombran %s"), *Name), HowTo.Mid(FMath::Max(KeysAt, 0)).Contains(Name));
	}
	const FHowToPlayLoadResult Missing = LoadHowToPlayText(TEXT("no/existe/como-se-juega.md"));
	TestFalse(TEXT("Sin archivo, el cargador falla"), Missing.IsSuccess());
	const FString MissingText = GrimoireHowToPlayText(Missing, Keys);
	TestTrue(TEXT("Sin archivo: el mensaje del cargador, no vacía"), !Missing.Message.IsEmpty() && MissingText.StartsWith(Missing.Message));
	TestTrue(TEXT("Y también las teclas"), MissingText.Contains(TEXT("Teclas")));

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
	TestTrue(TEXT("En partida, sin frase: se ve"), IsGrimoireButtonShown(false, false, false));
	TestFalse(TEXT("En el prólogo, no"), IsGrimoireButtonShown(true, false, false));
	TestFalse(TEXT("Con la partida terminada, no"), IsGrimoireButtonShown(false, true, false));
	TestFalse(TEXT("Con una frase del Warden en pantalla, no"), IsGrimoireButtonShown(false, false, true));
	TestFalse(TEXT("Con todo a la vez, no"), IsGrimoireButtonShown(true, true, true));
	return true;
}

#endif
