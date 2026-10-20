#include "BoardLayout.h"

#include "Board/BoardState.h"
#include "Math/RandomStream.h"

namespace
{
	/** Grados de un sector: los seis Dominios se reparten la vuelta alrededor del Altar (R-TAB-3, R-VIS-2). */
	constexpr float SectorDegrees = 360.f / Soulbound::Board::DomainCount;

	/** Dos conexiones cuyos huecos se separan menos que esto de 180° están en línea recta. */
	constexpr float StraightToleranceDegrees = 0.01f;

	/** Reparto en anillo de un tablero sin posiciones (heredado de S-005): casillas por fila y grados entre vecinas. */
	constexpr int32 RingPerRow = 5;
	constexpr float RingStepDegrees = 12.f;

	float AngleOf(const FVector2D& Vector)
	{
		return FMath::RadiansToDegrees(FMath::Atan2(Vector.Y, Vector.X));
	}

	FVector2D DirectionOf(float Degrees)
	{
		const float Radians = FMath::DegreesToRadians(Degrees);
		return FVector2D(FMath::Cos(Radians), FMath::Sin(Radians));
	}

	/**
	 * Posición de cada casilla relativa al Altar, en los ejes de la mesa y en unidades del editor. El editor dibuja con la Y
	 * hacia abajo; la cámara cenital tiene el +X de la mesa hacia arriba de la pantalla. Sin posiciones en el archivo, cada
	 * Dominio en su sector, en filas de cinco.
	 */
	TArray<FVector2D> EditorOffsets(const FBoardState& Board)
	{
		TArray<FVector2D> Offsets;
		Offsets.Init(FVector2D::ZeroVector, Board.Tiles.Num());
		const bool bAllPositioned = Board.Tiles.FindByPredicate([](const FTileState& Tile) { return !Tile.bHasLayoutPosition; }) == nullptr;
		if (bAllPositioned)
		{
			FVector2D Center = FVector2D::ZeroVector;
			const int32 Altar = Board.FindAltar();
			if (Altar != INDEX_NONE)
			{
				Center = Board.Tiles[Altar].LayoutPosition;
			}
			else
			{
				FBox2D Bounds(ForceInit);
				for (const FTileState& Tile : Board.Tiles)
				{
					Bounds += Tile.LayoutPosition;
				}
				Center = Bounds.GetCenter();
			}
			for (const FTileState& Tile : Board.Tiles)
			{
				Offsets[Tile.TileId] = FVector2D(-(Tile.LayoutPosition.Y - Center.Y), Tile.LayoutPosition.X - Center.X);
			}
			return Offsets;
		}
		TArray<int32> IndexInDomain;
		IndexInDomain.Init(0, Soulbound::Board::DomainCount);
		for (const FTileState& Tile : Board.Tiles)
		{
			if (!IndexInDomain.IsValidIndex(Tile.DomainId))
			{
				continue; // el Altar, en el centro
			}
			const int32 K = IndexInDomain[Tile.DomainId]++;
			const float Angle = Tile.DomainId * SectorDegrees + ((K % RingPerRow) - RingPerRow / 2) * RingStepDegrees;
			Offsets[Tile.TileId] = DirectionOf(Angle) * (1.f + K / RingPerRow);
		}
		return Offsets;
	}

	/**
	 * Lado de la estructura de una casilla (R-VIS-5): hacia fuera del camino. Sin conexiones, hacia fuera del Altar; con una,
	 * la contraria; con dos o más, el centro del mayor hueco entre ellas (con dos, fuera de la bisectriz); con dos en línea
	 * recta, en perpendicular hacia el lado contrario al Altar (decisión del autor del 20/10/2026).
	 */
	FVector2D OutwardDirection(const FVector2D& Location, const TArray<FVector2D>& Neighbours)
	{
		const FVector2D FromAltar = Location.IsNearlyZero() ? FVector2D(1.f, 0.f) : Location.GetSafeNormal();
		TArray<float> Angles;
		for (const FVector2D& Neighbour : Neighbours)
		{
			if (!(Neighbour - Location).IsNearlyZero())
			{
				Angles.Add(AngleOf(Neighbour - Location));
			}
		}
		if (Angles.IsEmpty())
		{
			return FromAltar;
		}
		if (Angles.Num() == 1)
		{
			return -DirectionOf(Angles[0]);
		}
		Angles.Sort();
		int32 Widest = 0;
		float WidestGap = -1.f;
		for (int32 Index = 0; Index < Angles.Num(); ++Index)
		{
			const float Next = Index + 1 < Angles.Num() ? Angles[Index + 1] : Angles[0] + 360.f;
			if (Next - Angles[Index] > WidestGap)
			{
				WidestGap = Next - Angles[Index];
				Widest = Index;
			}
		}
		if (Angles.Num() == 2 && FMath::Abs(WidestGap - 180.f) < StraightToleranceDegrees)
		{
			const FVector2D Across = DirectionOf(Angles[0] + 90.f);
			return (Across | FromAltar) >= 0.f ? Across : -Across;
		}
		return DirectionOf(Angles[Widest] + WidestGap * 0.5f);
	}

	/** Las losas normales que son la variante partida (R-VIS-3): la fracción de datos, barajadas con la semilla de datos. */
	TSet<int32> CrackedSlabs(const FBoardState& Board, const FBoardLayoutParams& Params)
	{
		TArray<int32> Normals;
		for (const FTileState& Tile : Board.Tiles)
		{
			if (Tile.Type == ETileType::Normal)
			{
				Normals.Add(Tile.TileId);
			}
		}
		FRandomStream Stream(Params.CrackedSlabSeed);
		for (int32 Index = Normals.Num() - 1; Index > 0; --Index)
		{
			Normals.Swap(Index, Stream.RandRange(0, Index));
		}
		const int32 Count = FMath::Clamp(FMath::RoundToInt(Normals.Num() * FMath::Clamp(Params.CrackedSlabFraction, 0.f, 1.f)), 0, Normals.Num());
		TSet<int32> Cracked;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Cracked.Add(Normals[Index]);
		}
		return Cracked;
	}

	EBoardSlab SlabFor(const FTileState& Tile, const TSet<int32>& Cracked)
	{
		switch (Tile.Type)
		{
		case ETileType::Altar: return EBoardSlab::Altar;
		case ETileType::Niche: return EBoardSlab::Niche;
		case ETileType::PowerSource: return EBoardSlab::PowerSource;
		case ETileType::PowerNode: return EBoardSlab::PowerNode;
		case ETileType::Normal:
		default: return Cracked.Contains(Tile.TileId) ? EBoardSlab::Cracked : EBoardSlab::Normal;
		}
	}

	/**
	 * Giro de cada cuña (R-VIS-2): el ángulo medio de las casillas de su Dominio vistas desde el Altar. Si el trazado no es
	 * radial (un Dominio sin casillas o repartido en más de un sector, o dos Dominios a menos de medio sector), de 60 en 60
	 * por identificador, y bRadial a falso.
	 */
	void PlaceWedges(const FBoardState& Board, FBoardLayout& Layout)
	{
		TArray<float> Means;
		for (const FDomainState& Domain : Board.Domains)
		{
			FVector2D Sum = FVector2D::ZeroVector;
			for (const FTileState& Tile : Board.Tiles)
			{
				if (Tile.DomainId == Domain.DomainId)
				{
					Sum += FVector2D(Layout.Tiles[Tile.TileId].Location).GetSafeNormal();
				}
			}
			if (Sum.IsNearlyZero())
			{
				Layout.bRadial = false;
				Means.Add(0.f);
				continue;
			}
			const float Mean = AngleOf(Sum);
			Means.Add(Mean);
			float Low = 0.f;
			float High = 0.f;
			for (const FTileState& Tile : Board.Tiles)
			{
				if (Tile.DomainId == Domain.DomainId)
				{
					const float Offset = FMath::UnwindDegrees(AngleOf(FVector2D(Layout.Tiles[Tile.TileId].Location)) - Mean);
					Low = FMath::Min(Low, Offset);
					High = FMath::Max(High, Offset);
				}
			}
			if (High - Low > SectorDegrees)
			{
				Layout.bRadial = false;
			}
		}
		for (int32 A = 0; A < Means.Num(); ++A)
		{
			for (int32 B = A + 1; B < Means.Num(); ++B)
			{
				if (FMath::Abs(FMath::UnwindDegrees(Means[A] - Means[B])) < SectorDegrees * 0.5f)
				{
					Layout.bRadial = false;
				}
			}
		}
		for (int32 Index = 0; Index < Board.Domains.Num(); ++Index)
		{
			FDomainWedgeLayout& Wedge = Layout.Wedges.AddDefaulted_GetRef();
			Wedge.DomainId = Board.Domains[Index].DomainId;
			Wedge.Yaw = Layout.bRadial ? Means[Index] : FMath::UnwindDegrees(Wedge.DomainId * SectorDegrees);
		}
	}

	/** Tramos de sendero de cada conexión (R-VIS-4): los que caben en el hueco entre las dos piezas, repartidos a partes iguales. */
	void PlacePathSegments(const FBoardState& Board, const FBoardLayoutParams& Params, FBoardLayout& Layout)
	{
		if (Params.PathSegmentLength <= 0.f)
		{
			return;
		}
		auto Half = [&Board, &Params](int32 TileId) { return Board.IsAltar(TileId) ? Params.AltarApothem : Params.SlabHalfSize; };
		for (const FTileState& Tile : Board.Tiles)
		{
			for (const int32 Other : Tile.Neighbours)
			{
				if (Other <= Tile.TileId || !Board.IsValidTile(Other))
				{
					continue; // cada conexión una sola vez
				}
				const FVector2D A(Layout.Tiles[Tile.TileId].Location);
				const FVector2D B(Layout.Tiles[Other].Location);
				const float Gap = FVector2D::Distance(A, B) - Half(Tile.TileId) - Half(Other);
				const int32 Count = Gap > 0.f ? FMath::FloorToInt(Gap / Params.PathSegmentLength) : 0;
				const FVector2D Along = (B - A).GetSafeNormal();
				for (int32 Index = 0; Index < Count; ++Index)
				{
					FPathSegmentLayout& Segment = Layout.PathSegments.AddDefaulted_GetRef();
					Segment.TileA = Tile.TileId;
					Segment.TileB = Other;
					Segment.Location = FVector(A + Along * (Half(Tile.TileId) + (Index + 0.5f) * Gap / Count), 0.f);
					Segment.Yaw = AngleOf(Along);
				}
			}
		}
	}
}

float FBoardLayoutParams::StructureDistance(EBoardStructure Structure) const
{
	switch (Structure)
	{
	case EBoardStructure::NicheThrone: return NicheThroneDistance;
	case EBoardStructure::Pylon: return PylonDistance;
	case EBoardStructure::PlanarArch: return PlanarArchDistance;
	case EBoardStructure::DuelSwords: return DuelSwordsDistance;
	case EBoardStructure::OracleOrb: return OracleOrbDistance;
	case EBoardStructure::None:
	default: return 0.f;
	}
}

EBoardStructure Soulbound::Presentation::StructureFor(const FTileState& Tile)
{
	switch (Tile.Type)
	{
	case ETileType::Niche: return EBoardStructure::NicheThrone;
	case ETileType::PowerSource: return EBoardStructure::Pylon;
	case ETileType::PowerNode:
		switch (Tile.PowerNodeEffect)
		{
		case EPowerNodeEffect::Planar: return EBoardStructure::PlanarArch;
		case EPowerNodeEffect::Duel: return EBoardStructure::DuelSwords;
		case EPowerNodeEffect::Oracle: return EBoardStructure::OracleOrb;
		case EPowerNodeEffect::None:
		default: return EBoardStructure::None;
		}
	case ETileType::Altar:
	case ETileType::Normal:
	default: return EBoardStructure::None;
	}
}

FBoardLayout Soulbound::Presentation::ComputeBoardLayout(const FBoardState& Board, const FBoardLayoutParams& Params)
{
	FBoardLayout Layout;
	if (Board.Tiles.IsEmpty())
	{
		return Layout;
	}

	// R-VIS-1: el ángulo del editor alrededor del Altar se conserva; la distancia se mide desde el borde del Altar, con la
	// escala que deja la losa más lejana entera dentro de la mesa (decisión del autor del 20/10/2026).
	const TArray<FVector2D> Offsets = EditorOffsets(Board);
	float Farthest = 0.f;
	for (const FVector2D& Offset : Offsets)
	{
		Farthest = FMath::Max(Farthest, static_cast<float>(Offset.Size()));
	}
	const float Inner = Params.AltarApothem + Params.AltarGap;
	Layout.Scale = Farthest > 0.f ? FMath::Max(0.f, (Params.TableRadius - Inner - Params.SlabHalfSize) / Farthest) : 0.f;
	Layout.Tiles.SetNum(Board.Tiles.Num());
	for (const FTileState& Tile : Board.Tiles)
	{
		const FVector2D Offset = Offsets[Tile.TileId];
		const float Distance = Offset.Size();
		Layout.Tiles[Tile.TileId].Location = Distance > UE_KINDA_SMALL_NUMBER ? FVector(Offset / Distance * (Inner + Distance * Layout.Scale), 0.f) : FVector::ZeroVector;
	}

	PlaceWedges(Board, Layout);

	// R-VIS-3 y R-VIS-5: la losa de cada casilla, su giro y, si es especial, su estructura.
	const TSet<int32> Cracked = CrackedSlabs(Board, Params);
	for (const FTileState& Tile : Board.Tiles)
	{
		FTileLayout& Placed = Layout.Tiles[Tile.TileId];
		TArray<FVector2D> Neighbours;
		for (const int32 Other : Tile.Neighbours)
		{
			if (Board.IsValidTile(Other))
			{
				Neighbours.Add(FVector2D(Layout.Tiles[Other].Location));
			}
		}
		const FVector2D Location(Placed.Location);
		Placed.Slab = SlabFor(Tile, Cracked);
		Placed.OutwardDirection = OutwardDirection(Location, Neighbours);
		const float OutwardYaw = AngleOf(Placed.OutwardDirection);
		switch (Placed.Slab)
		{
		case EBoardSlab::Niche: Placed.Yaw = OutwardYaw; break; // la cabecera (+x) hacia su estructura
		case EBoardSlab::Altar: Placed.Yaw = Layout.Wedges.IsEmpty() ? 0.f : Layout.Wedges[0].Yaw; break; // las caras con las cuñas
		default: Placed.Yaw = FMath::UnwindDegrees(OutwardYaw + 90.f); break; // la x a lo largo del camino (las puntas del Nodo)
		}
		Placed.Structure = StructureFor(Tile);
		if (Placed.Structure != EBoardStructure::None)
		{
			Placed.StructureLocation = FVector(Location + Placed.OutwardDirection * (Params.SlabHalfSize + Params.StructureDistance(Placed.Structure)), 0.f);
			// El frente de la estructura (-y local) mira a la losa: su +y apunta hacia fuera.
			Placed.StructureYaw = FMath::UnwindDegrees(OutwardYaw - 90.f);
		}
		else
		{
			Placed.StructureLocation = Placed.Location;
		}
	}

	PlacePathSegments(Board, Params, Layout);
	return Layout;
}
