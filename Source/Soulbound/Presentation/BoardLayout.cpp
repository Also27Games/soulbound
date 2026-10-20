#include "BoardLayout.h"

#include "Board/BoardState.h"

float FBoardLayoutParams::StructureDistance(EBoardStructure Structure) const
{
	return 0.f;
}

EBoardStructure Soulbound::Presentation::StructureFor(const FTileState& Tile)
{
	return EBoardStructure::None;
}

FBoardLayout Soulbound::Presentation::ComputeBoardLayout(const FBoardState& Board, const FBoardLayoutParams& Params)
{
	// TODO(R-VIS-1): firma sin implementar, para ver los tests en rojo.
	return FBoardLayout();
}
