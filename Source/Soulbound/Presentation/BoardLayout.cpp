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
		auto Half = [&Board, &Params, &Layout](int32 TileId) { return Board.IsAltar(TileId) ? Layout.AltarApothem : Params.SlabHalfSize; };
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
					Segment.Location = FVector(A + Along * (Half(Tile.TileId) + (Index + 0.5f) * Gap / Count), Params.KitFloorHeight);
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
	// El Altar crece con el kit (R-VIS-1): su apotema escalado es el borde interior de las casillas.
	Layout.KitScale = Params.KitTableRadius > 0.f ? Params.TableRadius / Params.KitTableRadius : 1.f;
	Layout.AltarApothem = Params.AltarApothem * Layout.KitScale;
	const float Inner = Layout.AltarApothem + Params.AltarGap;
	Layout.Scale = Farthest > 0.f ? FMath::Max(0.f, (Params.TableRadius - Inner - Params.SlabHalfSize) / Farthest) : 0.f;
	Layout.Tiles.SetNum(Board.Tiles.Num());
	for (const FTileState& Tile : Board.Tiles)
	{
		const FVector2D Offset = Offsets[Tile.TileId];
		const float Distance = Offset.Size();
		// R-VIS-3: las losas se posan sobre el suelo del kit; el Altar, con su base en la mesa, no sube.
		const float Floor = Board.IsAltar(Tile.TileId) ? 0.f : Params.KitFloorHeight;
		Layout.Tiles[Tile.TileId].Top = Board.IsAltar(Tile.TileId) ? Params.AltarTop * Layout.KitScale : Floor + Params.SlabTop;
		Layout.Tiles[Tile.TileId].Location = Distance > UE_KINDA_SMALL_NUMBER ? FVector(Offset / Distance * (Inner + Distance * Layout.Scale), Floor) : FVector(0.f, 0.f, Floor);
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
			Placed.StructureLocation = FVector(Location + Placed.OutwardDirection * (Params.SlabHalfSize + Params.StructureDistance(Placed.Structure)), Params.KitFloorHeight);
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

float Soulbound::Presentation::HaloDiameter(float TileDiameter, float HaloScale, float PieceSizeX, float PieceSizeY, float Margin)
{
	// El margen sobre la mayor medida: un hexágono es más ancho en las esquinas que en las caras, y un halo redondo tiene que cubrir las esquinas.
	return FMath::Max(TileDiameter * HaloScale, FMath::Max(PieceSizeX, PieceSizeY) * FMath::Max(Margin, 1.f));
}

namespace
{
	/** "Olvidado", la segunda línea del rótulo (S-047, R-VIS-34): su ancho cuenta en el rectángulo, que la reserva siempre. */
	const FString NameplateForgottenWord = TEXT("Olvidado");

	/** Iteraciones de la búsqueda de la letra mayor que cabe (bisección entre 0 y el tope): con el tope de 7,7 cm, una precisión de 0,01 cm. */
	constexpr int32 NameplateLetterIterations = 10;

	/** Un rectángulo girado del plano (S-047): centro, sus dos ejes unitarios y su medio tamaño a lo largo de cada uno. */
	struct FPlaneBox
	{
		FVector2D Center = FVector2D::ZeroVector;
		FVector2D AxisX = FVector2D(1.f, 0.f);
		FVector2D AxisY = FVector2D(0.f, 1.f);
		FVector2D Half = FVector2D::ZeroVector;
	};

	FVector2D UnitAt(float Degrees)
	{
		const float Radians = FMath::DegreesToRadians(Degrees);
		return FVector2D(FMath::Cos(Radians), FMath::Sin(Radians));
	}

	/** Medio ancho de la sombra de una caja sobre un eje unitario. */
	float ProjectedRadius(const FPlaneBox& Box, const FVector2D& Axis)
	{
		return Box.Half.X * FMath::Abs(FVector2D::DotProduct(Box.AxisX, Axis)) + Box.Half.Y * FMath::Abs(FVector2D::DotProduct(Box.AxisY, Axis));
	}

	/** Si dos cajas se tocan: no se tocan solo si las separa alguno de sus cuatro ejes (teorema del eje separador). */
	bool BoxesOverlap(const FPlaneBox& A, const FPlaneBox& B)
	{
		const FVector2D Delta = B.Center - A.Center;
		for (const FVector2D& Axis : {A.AxisX, A.AxisY, B.AxisX, B.AxisY})
		{
			if (FMath::Abs(FVector2D::DotProduct(Delta, Axis)) > ProjectedRadius(A, Axis) + ProjectedRadius(B, Axis))
			{
				return false;
			}
		}
		return true;
	}

	/** Distancia del centro de la mesa (el del Altar) a la caja; 0 si lo contiene. */
	float DistanceFromOrigin(const FPlaneBox& Box)
	{
		const FVector2D Local(FVector2D::DotProduct(-Box.Center, Box.AxisX), FVector2D::DotProduct(-Box.Center, Box.AxisY));
		return FVector2D(FMath::Max(FMath::Abs(Local.X) - Box.Half.X, 0.f), FMath::Max(FMath::Abs(Local.Y) - Box.Half.Y, 0.f)).Size();
	}

	/**
	 * Lo que hace falta para buscar el sitio del rótulo en una cuña (S-047): los obstáculos (cada losa, girada, y cada conexión, como una franja de su
	 * medio ancho) y los sitios candidatos dentro del sector, del más cercano al preferido al más lejano. Se arma una vez por cuña y sirve para todas las
	 * letras que prueba la bisección.
	 */
	struct FNameplateSearch
	{
		TArray<FPlaneBox> Obstacles;
		TArray<float> ObstacleRadii;
		TArray<FVector2D> Candidates;
		FVector2D Right = FVector2D::ZeroVector;
		FVector2D Up = FVector2D::ZeroVector;
		float Yaw = 0.f;
		float WedgeYaw = 0.f;
		float HalfSector = 0.f;
		float TableRadius = 0.f;
		float AltarRadius = 0.f;
		float Margin = 0.f;
	};

	FNameplateSearch MakeNameplateSearch(const FBoardState& Board, const FBoardLayout& Layout, const FBoardLayoutParams& LayoutParams, int32 DomainId,
		float ViewYawDegrees, const Soulbound::Presentation::FDomainNameplateParams& Params)
	{
		FNameplateSearch Search;
		// El texto se lee derecho desde la vista sentada (R-VIS-34): sube en la dirección en que mira la cámara (del asiento hacia el centro) y se lee
		// hacia su derecha. Con la vista en 180 (en -X), sube hacia +X y se lee hacia +Y, como las etiquetas de casilla.
		Search.Up = -UnitAt(ViewYawDegrees);
		Search.Right = FVector2D(-Search.Up.Y, Search.Up.X);
		Search.Yaw = ViewYawDegrees;
		Search.WedgeYaw = Layout.Wedges.IsValidIndex(DomainId) ? Layout.Wedges[DomainId].Yaw : DomainId * SectorDegrees;
		Search.HalfSector = SectorDegrees / 2.f;
		Search.TableRadius = LayoutParams.TableRadius;
		// El Altar es un hexágono: su radio circunscrito lo cubre entero.
		Search.AltarRadius = Layout.AltarApothem * 2.f / FMath::Sqrt(3.f);
		const float Depth = FMath::Max(LayoutParams.TableRadius - Layout.AltarApothem, 0.f);
		Search.Margin = FMath::Max(Params.MarginFraction, 0.f) * Depth;

		// Los obstáculos son las losas, giradas, de todos los Dominios. Los senderos no: el rótulo puede cruzarlos (cambio de regla del autor, 19/11/2026).
		for (const FTileState& Tile : Board.Tiles)
		{
			if (!Layout.Tiles.IsValidIndex(Tile.TileId) || Board.IsAltar(Tile.TileId))
			{
				continue;
			}
			const FTileLayout& Slab = Layout.Tiles[Tile.TileId];
			FPlaneBox& Box = Search.Obstacles.AddDefaulted_GetRef();
			Box.Center = FVector2D(Slab.Location);
			Box.AxisX = UnitAt(Slab.Yaw);
			Box.AxisY = UnitAt(Slab.Yaw + 90.f);
			Box.Half = FVector2D(LayoutParams.SlabHalfSize, LayoutParams.SlabHalfSize);
		}
		Search.ObstacleRadii.Reserve(Search.Obstacles.Num());
		for (const FPlaneBox& Box : Search.Obstacles)
		{
			Search.ObstacleRadii.Add(Box.Half.Size());
		}

		// Candidatos: una rejilla con el paso de la búsqueda, alineada con el sitio preferido, solo los centros que caen dentro del sector y a no más de
		// MaxOffsetFraction del preferido (ronda del 19/11/2026: los seis rótulos, en la misma zona de su cuña).
		const float Inner = Search.AltarRadius;
		const FVector2D Preferred = UnitAt(Search.WedgeYaw + Params.AngleFraction * Search.HalfSector)
			* (Inner + Params.RadiusFraction * (LayoutParams.TableRadius - Inner));
		const float Step = FMath::Max(Params.SearchStepFraction * Depth, 0.1f);
		const float MaxOffset = FMath::Max(Params.MaxOffsetFraction, 0.f) * Depth;
		const int32 Reach = FMath::CeilToInt(FMath::Min(MaxOffset, 2.f * LayoutParams.TableRadius) / Step);
		for (int32 IndexX = -Reach; IndexX <= Reach; ++IndexX)
		{
			for (int32 IndexY = -Reach; IndexY <= Reach; ++IndexY)
			{
				const FVector2D Candidate = Preferred + FVector2D(IndexX * Step, IndexY * Step);
				const float Radius = Candidate.Size();
				if (Radius <= Inner || Radius >= LayoutParams.TableRadius || FVector2D::Distance(Candidate, Preferred) > MaxOffset)
				{
					continue;
				}
				const float Angle = FMath::RadiansToDegrees(FMath::Atan2(Candidate.Y, Candidate.X));
				if (FMath::Abs(FMath::UnwindDegrees(Angle - Search.WedgeYaw)) < Search.HalfSector)
				{
					Search.Candidates.Add(Candidate);
				}
			}
		}
		Search.Candidates.Sort([&Preferred](const FVector2D& A, const FVector2D& B)
		{
			return FVector2D::DistSquared(A, Preferred) < FVector2D::DistSquared(B, Preferred);
		});
		return Search;
	}

	/** Si el rectángulo de medio tamaño Half, con su holgura, cabe con su centro en Center: dentro del sector y de la mesa, fuera del Altar y sin tocar nada. */
	bool NameplateFits(const FNameplateSearch& Search, const FVector2D& Center, const FVector2D& Half)
	{
		FPlaneBox Box;
		Box.Center = Center;
		Box.AxisX = Search.Right;
		Box.AxisY = Search.Up;
		Box.Half = Half + FVector2D(Search.Margin, Search.Margin);
		// El sector es convexo (60°): si las cuatro esquinas están dentro, el rectángulo entero lo está.
		for (const FVector2D& Corner : {FVector2D(1.f, 1.f), FVector2D(1.f, -1.f), FVector2D(-1.f, 1.f), FVector2D(-1.f, -1.f)})
		{
			const FVector2D Point = Center + Box.AxisX * (Corner.X * Box.Half.X) + Box.AxisY * (Corner.Y * Box.Half.Y);
			if (Point.Size() > Search.TableRadius)
			{
				return false;
			}
			const float Angle = FMath::RadiansToDegrees(FMath::Atan2(Point.Y, Point.X));
			if (FMath::Abs(FMath::UnwindDegrees(Angle - Search.WedgeYaw)) > Search.HalfSector)
			{
				return false;
			}
		}
		if (DistanceFromOrigin(Box) < Search.AltarRadius)
		{
			return false;
		}
		const float BoxRadius = Box.Half.Size();
		for (int32 Index = 0; Index < Search.Obstacles.Num(); ++Index)
		{
			const FPlaneBox& Obstacle = Search.Obstacles[Index];
			if (FVector2D::Distance(Obstacle.Center, Center) <= BoxRadius + Search.ObstacleRadii[Index] && BoxesOverlap(Box, Obstacle))
			{
				return false;
			}
		}
		return true;
	}

	/** El candidato más cercano al preferido en el que cabe el rectángulo de tamaño Size; el sitio sin bFound si no cabe en ninguno. */
	Soulbound::Presentation::FDomainNameplateSpot FindNameplateSpot(const FNameplateSearch& Search, const FVector2D& Size)
	{
		Soulbound::Presentation::FDomainNameplateSpot Spot;
		Spot.HalfExtent = Size / 2.f;
		Spot.Right = Search.Right;
		Spot.Up = Search.Up;
		Spot.Yaw = Search.Yaw;
		for (const FVector2D& Candidate : Search.Candidates)
		{
			if (NameplateFits(Search, Candidate, Spot.HalfExtent))
			{
				Spot.bFound = true;
				Spot.Center = Candidate;
				break;
			}
		}
		return Spot;
	}

	/** La letra mayor que cabe en la cuña, entre 0 y el tope, para un rectángulo que mide UnitSize por cm de letra. */
	float MaxNameplateLetter(const FNameplateSearch& Search, const FVector2D& UnitSize, float MaxLetter)
	{
		if (MaxLetter <= 0.f)
		{
			return 0.f;
		}
		if (FindNameplateSpot(Search, UnitSize * MaxLetter).bFound)
		{
			return MaxLetter;
		}
		float Low = 0.f;
		float High = MaxLetter;
		for (int32 Iteration = 0; Iteration < NameplateLetterIterations; ++Iteration)
		{
			const float Mid = (Low + High) / 2.f;
			(FindNameplateSpot(Search, UnitSize * Mid).bFound ? Low : High) = Mid;
		}
		return Low;
	}
}

TArray<FString> Soulbound::Presentation::SplitNameplateName(const FString& Name, int32 MinLineChars)
{
	if (Name.IsEmpty())
	{
		return {};
	}
	// Las dos líneas más parejas: el espacio más cercano a (Len - 1) / 2, donde las dos mitades miden lo mismo; a igual distancia, el primero.
	const float Middle = (Name.Len() - 1) / 2.f;
	int32 Best = INDEX_NONE;
	for (int32 Index = 0; Index < Name.Len(); ++Index)
	{
		if (Name[Index] == TEXT(' ') && (Best == INDEX_NONE || FMath::Abs(Index - Middle) < FMath::Abs(Best - Middle)))
		{
			Best = Index;
		}
	}
	// Ronda del 19/11/2026: sin espacio, o si una de las dos líneas se queda corta ("El" de "El Banquete"), no se parte. El espacio más cercano a la
	// mitad es el que deja la línea más corta más larga: si con él una se queda corta, con cualquier otro también.
	if (Best == INDEX_NONE || Best < MinLineChars || Name.Len() - Best - 1 < MinLineChars)
	{
		return {Name};
	}
	return {Name.Left(Best), Name.Mid(Best + 1)};
}

FVector2D Soulbound::Presentation::NameplateSize(const TArray<FString>& NameLines, float LetterSize, const FDomainNameplateParams& Params)
{
	int32 LongestLine = 0;
	for (const FString& Line : NameLines)
	{
		LongestLine = FMath::Max(LongestLine, Line.Len());
	}
	const float CharWidth = LetterSize * Params.CharWidthFraction;
	const float Width = FMath::Max(LongestLine * CharWidth, NameplateForgottenWord.Len() * CharWidth * Params.ForgottenScale);
	// La línea de "Olvidado" se reserva siempre: el nombre no se mueve cuando aparece.
	const float Height = (NameLines.Num() + Params.ForgottenScale) * LetterSize * Params.LineHeightFraction;
	return FVector2D(Width, Height);
}

Soulbound::Presentation::FDomainNameplateSpot Soulbound::Presentation::DomainNameplateSpot(const FBoardState& Board, const FBoardLayout& Layout,
	const FBoardLayoutParams& LayoutParams, int32 DomainId, const FVector2D& Size, float ViewYawDegrees, const FDomainNameplateParams& Params)
{
	return FindNameplateSpot(MakeNameplateSearch(Board, Layout, LayoutParams, DomainId, ViewYawDegrees, Params), Size);
}

Soulbound::Presentation::FDomainNameplateLayout Soulbound::Presentation::ComputeDomainNameplates(const FBoardState& Board, const FBoardLayout& Layout,
	const FBoardLayoutParams& LayoutParams, float ViewYawDegrees, const FDomainNameplateParams& Params)
{
	FDomainNameplateLayout Result;
	const int32 DomainCount = Board.Domains.Num();
	if (DomainCount == 0)
	{
		return Result;
	}
	TArray<FNameplateSearch> Searches;
	for (int32 DomainId = 0; DomainId < DomainCount; ++DomainId)
	{
		Searches.Add(MakeNameplateSearch(Board, Layout, LayoutParams, DomainId, ViewYawDegrees, Params));
	}
	const float MaxLetter = FMath::Max(Params.MaxLetterSizeFraction, 0.f) * FMath::Max(LayoutParams.TableRadius - Layout.AltarApothem, 0.f);

	// Las dos formas: todos en una línea, o cada nombre partido donde se puede (SplitNameplateName). Ronda del 19/11/2026: cada rótulo mide su propio
	// rectángulo (sus líneas y la de "Olvidado"), y la letra común es la mayor que cabe en las seis cuñas.
	float Common[2] = {0.f, 0.f};
	TArray<TArray<FString>> Lines[2];
	TArray<FVector2D> UnitSizes[2];
	for (int32 Form = 0; Form < 2; ++Form)
	{
		const bool bSplit = Form == 1;
		TArray<float>& PerWedge = bSplit ? Result.MaxLetterSplit : Result.MaxLetterOneLine;
		Common[Form] = MaxLetter;
		for (int32 DomainId = 0; DomainId < DomainCount; ++DomainId)
		{
			const FString& Name = Board.Domains[DomainId].Name;
			Lines[Form].Add(bSplit ? SplitNameplateName(Name, Params.MinSplitLineChars) : TArray<FString>({Name}));
			UnitSizes[Form].Add(NameplateSize(Lines[Form].Last(), 1.f, Params));
			PerWedge.Add(MaxNameplateLetter(Searches[DomainId], UnitSizes[Form].Last(), MaxLetter));
			Common[Form] = FMath::Min(Common[Form], PerWedge.Last());
		}
	}
	// Se queda la forma de letra mayor; a igualdad, en una línea.
	const int32 Chosen = Common[1] > Common[0] ? 1 : 0;
	Result.bSplit = Chosen == 1;
	Result.LetterSize = Common[Chosen];
	Result.NameLines = Lines[Chosen];

	const float LineHeight = Result.LetterSize * Params.LineHeightFraction;
	const float ForgottenLineHeight = LineHeight * Params.ForgottenScale;
	for (int32 DomainId = 0; DomainId < DomainCount; ++DomainId)
	{
		FDomainNameplateSpot Spot = FindNameplateSpot(Searches[DomainId], UnitSizes[Chosen][DomainId] * Result.LetterSize);
		// De arriba abajo: las líneas del nombre y, debajo, la de "Olvidado".
		const FVector2D Top = Spot.Center + Spot.Up * Spot.HalfExtent.Y;
		const int32 Count = Result.NameLines[DomainId].Num();
		for (int32 Line = 0; Line < Count; ++Line)
		{
			Spot.NameLineCenters.Add(Top - Spot.Up * ((Line + 0.5f) * LineHeight));
		}
		Spot.ForgottenLineCenter = Top - Spot.Up * (Count * LineHeight + ForgottenLineHeight / 2.f);
		Result.Spots.Add(Spot);
	}
	return Result;
}
