#include "BoardViewState.h"

#include "Match/MatchState.h"
#include "Match/SeatGoalRules.h"
#include "Misc/PackageName.h"
#include "Warden/WardenLines.h"

FBoardViewState Soulbound::Presentation::ComputeBoardViewState(const FMatchState& State, bool bNicheThroneConfigured)
{
	FBoardViewState View;
	const FBoardState& Board = State.Board;
	View.Tiles.SetNum(Board.Tiles.Num());
	for (const FTileState& Tile : Board.Tiles)
	{
		FTileViewState& Shown = View.Tiles[Tile.TileId];
		Shown.Structure = StructureFor(Tile);
		// R-TAB-8: antes del Despertar, el Nodo no enseña su efecto; tampoco su estructura.
		if (Tile.Type == ETileType::PowerNode && !Board.AreNodesRevealed())
		{
			Shown.Structure = EBoardStructure::None;
		}
		if (Shown.Structure == EBoardStructure::NicheThrone && !bNicheThroneConfigured)
		{
			Shown.Structure = EBoardStructure::None;
		}
		// La reserva es 0 hasta el Despertar (R-RUN-4): antes no hay runa flotante.
		Shown.bFloatingRune = Tile.Type == ETileType::PowerSource && Tile.RuneReserve > 0;
		Shown.bSealChains = Tile.bSealed;
	}
	View.TokensOnBoard.Reserve(State.Seats.Num());
	for (const FSeatState& Seat : State.Seats)
	{
		View.TokensOnBoard.Add(!Seat.IsInVoid());
	}
	return View;
}

FLinearColor Soulbound::Presentation::WedgeFloorTint(const FLinearColor& DomainColor, bool bForgotten, float Strength, float ForgottenDim)
{
	const float Mix = FMath::Clamp(Strength, 0.f, 1.f);
	const FLinearColor Mixed = FLinearColor::White + (DomainColor - FLinearColor::White) * Mix;
	if (!bForgotten)
	{
		return Mixed;
	}
	return FLinearColor(Mixed.R * ForgottenDim, Mixed.G * ForgottenDim, Mixed.B * ForgottenDim, Mixed.A);
}

float Soulbound::Presentation::WedgeFloorDim(bool bForgotten, bool bForgottenMaterial, float ForgottenFloorDim, float ForgottenDim)
{
	// R-VIS-27: el material propio ya dice "Olvidado" con su textura; se apaga menos. Sin él, como antes de S-040.
	if (!bForgotten)
	{
		return 1.f;
	}
	return bForgottenMaterial ? ForgottenFloorDim : ForgottenDim;
}

TArray<int32> Soulbound::Presentation::GoalLightTiles(const FMatchState& State, int32 HumanSeat, bool bInPrologue)
{
	// R-VIS-28: solo la meta del humano; en el prólogo, ninguna (la partida aún no cuenta). SeatGoal ya da None sin asiento o con la partida terminada.
	if (bInPrologue)
	{
		return TArray<int32>();
	}
	return Soulbound::Rules::SeatGoal(State, HumanSeat).Tiles;
}

FString Soulbound::Presentation::MenuMapPackage(const FString& ReturnToMenuMap, bool bReturnMapExists, const FString& GameDefaultMap)
{
	// R-MEN-1: el menú principal si está configurado y existe; si no, el de arranque, como en S-022.
	const FString& Map = !ReturnToMenuMap.IsEmpty() && bReturnMapExists ? ReturnToMenuMap : GameDefaultMap;
	return FPackageName::ObjectPathToPackageName(Map);
}

FString Soulbound::Presentation::ViewKeysText(bool bSeated, const FString& ViewKey, const FString& OverlayKey, const FString& PanelsKey, const FString& DebugTextKey,
	const FString& GrimoireKey, const FString& DebugPanelKey)
{
	// S-027: T oculta el panel de estado; G abre el Grimorio y D el panel de depuración.
	return FString::Printf(TEXT("vista %s (%s: %s) | %s etiquetas | %s paneles | %s panel de estado | %s Grimorio | %s depuración"), bSeated ? TEXT("sentada") : TEXT("cenital"),
		*ViewKey, bSeated ? TEXT("cenital") : TEXT("sentada"), *OverlayKey, *PanelsKey, *DebugTextKey, *GrimoireKey, *DebugPanelKey);
}

FString Soulbound::Presentation::MatchResultText(const FMatchState& State, const FString& ReturnKey)
{
	if (!State.IsMatchOver())
	{
		return FString();
	}
	const FString Winner = State.Outcome == EMatchOutcome::PlayerVictory ? FString::Printf(TEXT("GANA EL ASIENTO %d"), State.WinnerSeat)
		: State.EndReason == EMatchEndReason::TimeOut ? FString(TEXT("GANA EL WARDEN: SE ACABÓ EL TIEMPO")) : FString(TEXT("GANA EL WARDEN: NADIE SE VINCULÓ"));
	return FString::Printf(TEXT("%s\n%s o el botón: volver al menú"), *Winner, *ReturnKey);
}

TArray<bool> Soulbound::Presentation::ForgottenDomainsShown(const FMatchState& State, bool bInPrologue, int32 OpeningLinesShown)
{
	TArray<bool> Shown;
	Shown.Init(false, State.Board.Domains.Num());
	if (!bInPrologue)
	{
		for (int32 Domain = 0; Domain < Shown.Num(); ++Domain)
		{
			Shown[Domain] = State.Board.Domains[Domain].State == EDomainState::Forgotten;
		}
		return Shown;
	}
	// En el prólogo, la frase de apertura número n (en el orden del registro, las del instante cero) se ha enseñado si n < OpeningLinesShown;
	// la de corrupción apaga el Dominio del DomainForgotten que la precede.
	int32 Opening = 0;
	for (int32 Index = 0; Index < State.EventLog.Num() && Opening < OpeningLinesShown; ++Index)
	{
		const FMatchEvent& Event = State.EventLog[Index];
		if (Event.Type != EMatchEventType::WardenSpoke || Event.ClockSeconds != 0.0)
		{
			continue;
		}
		++Opening;
		const FMatchEvent* Previous = Index > 0 ? &State.EventLog[Index - 1] : nullptr;
		if (Previous && Previous->Type == EMatchEventType::DomainForgotten && Shown.IsValidIndex(Previous->Value)
			&& State.WardenLines.Entries.IsValidIndex(Event.Value) && State.WardenLines.Entries[Event.Value].Id == Soulbound::Warden::LineCorruption)
		{
			Shown[Previous->Value] = true;
		}
	}
	return Shown;
}

TArray<int32> Soulbound::Presentation::SealMarkedTiles(const FMatchState& State)
{
	// R-VIS-24: la vista no decide nada, lee el orden de los Sellos tal como lo guarda el tablero.
	return State.Board.SealedTiles;
}
