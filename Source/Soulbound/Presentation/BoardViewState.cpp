#include "BoardViewState.h"

#include "Match/MatchState.h"

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

FString Soulbound::Presentation::ViewKeysText(bool bSeated, const FString& ViewKey, const FString& OverlayKey, const FString& PanelsKey, const FString& DebugTextKey)
{
	return FString::Printf(TEXT("vista %s (%s: %s) | %s etiquetas | %s paneles | %s texto de depuración"), bSeated ? TEXT("sentada") : TEXT("cenital"), *ViewKey,
		bSeated ? TEXT("cenital") : TEXT("sentada"), *OverlayKey, *PanelsKey, *DebugTextKey);
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
	return Shown;
}
