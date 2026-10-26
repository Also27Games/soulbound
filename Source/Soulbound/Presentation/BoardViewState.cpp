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
