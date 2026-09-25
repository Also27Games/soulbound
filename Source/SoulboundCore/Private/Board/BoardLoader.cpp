#include "Board/BoardLoader.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	FBoardLoadResult Reject(EBoardError Error, const FString& Message)
	{
		FBoardLoadResult Result;
		Result.Error = Error;
		Result.Message = Message;
		return Result;
	}

	/** Lee un número entero de un valor JSON. Devuelve false si no es un número o tiene decimales. */
	bool ReadInteger(const TSharedPtr<FJsonValue>& Value, int32& Out)
	{
		double Number = 0.0;
		if (!Value.IsValid() || !Value->TryGetNumber(Number) || Number != FMath::FloorToDouble(Number) || Number < MIN_int32 || Number > MAX_int32)
		{
			return false;
		}
		Out = static_cast<int32>(Number);
		return true;
	}

	/** Lee el campo entero Field de Object. Devuelve false si falta o no es entero. */
	bool ReadIntegerField(const FJsonObject& Object, const TCHAR* Field, int32& Out)
	{
		return ReadInteger(Object.TryGetField(Field), Out);
	}

	bool ParseTileType(const FString& Text, ETileType& Out)
	{
		static const TPair<const TCHAR*, ETileType> Types[] = {
			{TEXT("normal"), ETileType::Normal},
			{TEXT("altar"), ETileType::Altar},
			{TEXT("niche"), ETileType::Niche},
			{TEXT("powerSource"), ETileType::PowerSource},
			{TEXT("powerNode"), ETileType::PowerNode},
		};
		for (const TPair<const TCHAR*, ETileType>& Type : Types)
		{
			if (Text.Equals(Type.Key, ESearchCase::CaseSensitive))
			{
				Out = Type.Value;
				return true;
			}
		}
		return false;
	}

	FBoardLoadResult ParseParams(const FJsonObject& Root, FBoardParams& Out)
	{
		const TSharedPtr<FJsonObject>* Params = nullptr;
		const TSharedPtr<FJsonObject>* Effects = nullptr;
		if (!Root.TryGetObjectField(TEXT("parameters"), Params) || !(*Params)->TryGetObjectField(TEXT("powerNodeEffects"), Effects))
		{
			return Reject(EBoardError::MalformedData, TEXT("Falta el bloque \"parameters\" o su \"powerNodeEffects\"."));
		}

		// Todos los parámetros son cantidades: enteros mayores o iguales que cero.
		struct FCountField
		{
			const FJsonObject& Object;
			const TCHAR* Name;
			int32& Value;
		};
		const FCountField Fields[] = {
			{**Params, TEXT("powerSourcesPerDomain"), Out.PowerSourcesPerDomain},
			{**Params, TEXT("maxSealedTiles"), Out.MaxSealedTiles},
			{**Params, TEXT("runeReservePerSource"), Out.RuneReservePerSource},
			{**Effects, TEXT("duel"), Out.DuelNodeCount},
			{**Effects, TEXT("oracle"), Out.OracleNodeCount},
			{**Effects, TEXT("planar"), Out.PlanarNodeCount},
		};
		for (const FCountField& Field : Fields)
		{
			if (!ReadIntegerField(Field.Object, Field.Name, Field.Value) || Field.Value < 0)
			{
				return Reject(EBoardError::MalformedData, FString::Printf(TEXT("El parámetro \"%s\" falta o no es un entero mayor o igual que cero."), Field.Name));
			}
		}
		return FBoardLoadResult();
	}

	FBoardLoadResult ParseDomains(const FJsonObject& Root, TArray<FDomainState>& Out)
	{
		const TArray<TSharedPtr<FJsonValue>>* Domains = nullptr;
		if (!Root.TryGetArrayField(TEXT("domains"), Domains))
		{
			return Reject(EBoardError::MalformedData, TEXT("Falta la lista \"domains\"."));
		}
		for (int32 Index = 0; Index < Domains->Num(); ++Index)
		{
			const TSharedPtr<FJsonObject>* Domain = nullptr;
			FDomainState& Parsed = Out.AddDefaulted_GetRef();
			if (!(*Domains)[Index]->TryGetObject(Domain) || !ReadIntegerField(**Domain, TEXT("id"), Parsed.DomainId)
				|| !(*Domain)->TryGetStringField(TEXT("name"), Parsed.Name))
			{
				return Reject(EBoardError::MalformedData, FString::Printf(TEXT("El Dominio en la posición %d de \"domains\" necesita \"id\" entero y \"name\"."), Index));
			}
		}
		// Indexados por identificador; si hay repetidos o fuera de rango, ValidateBoard los rechaza con DomainCount.
		Out.StableSort([](const FDomainState& A, const FDomainState& B) { return A.DomainId < B.DomainId; });
		return FBoardLoadResult();
	}

	FBoardLoadResult ParseTiles(const FJsonObject& Root, FBoardState& Board)
	{
		const TArray<TSharedPtr<FJsonValue>>* Tiles = nullptr;
		if (!Root.TryGetArrayField(TEXT("tiles"), Tiles))
		{
			return Reject(EBoardError::MalformedData, TEXT("Falta la lista \"tiles\"."));
		}
		// Los identificadores son el índice en FBoardState::Tiles: deben cubrir de 0 a n-1 sin repetirse.
		Board.Tiles.SetNum(Tiles->Num());
		TBitArray<> Seen(false, Tiles->Num());
		for (int32 Index = 0; Index < Tiles->Num(); ++Index)
		{
			const TSharedPtr<FJsonObject>* Tile = nullptr;
			FString TypeText;
			int32 Id = INDEX_NONE;
			if (!(*Tiles)[Index]->TryGetObject(Tile) || !ReadIntegerField(**Tile, TEXT("id"), Id)
				|| !(*Tile)->TryGetStringField(TEXT("type"), TypeText))
			{
				return Reject(EBoardError::MalformedData, FString::Printf(TEXT("La casilla en la posición %d de \"tiles\" necesita \"id\" entero y \"type\"."), Index));
			}
			if (!Board.Tiles.IsValidIndex(Id) || Seen[Id])
			{
				return Reject(EBoardError::InvalidGraph, FString::Printf(TEXT("Identificador de casilla %d repetido o fuera de 0 a %d: los identificadores van de 0 a n-1 sin huecos."), Id, Tiles->Num() - 1));
			}
			Seen[Id] = true;

			FTileState& Parsed = Board.Tiles[Id];
			Parsed.TileId = Id;
			if (!ParseTileType(TypeText, Parsed.Type))
			{
				return Reject(EBoardError::MalformedData, FString::Printf(TEXT("La casilla %d tiene un tipo desconocido \"%s\" (normal, altar, niche, powerSource o powerNode)."), Id, *TypeText));
			}
			// Sin "domain", la casilla no pertenece a ninguno (el Altar); ValidateBoard decide si eso es válido.
			if ((*Tile)->HasField(TEXT("domain")))
			{
				if (!ReadIntegerField(**Tile, TEXT("domain"), Parsed.DomainId))
				{
					return Reject(EBoardError::MalformedData, FString::Printf(TEXT("La casilla %d tiene un \"domain\" que no es un entero."), Id));
				}
			}
		}
		return FBoardLoadResult();
	}

	FBoardLoadResult ParseEdges(const FJsonObject& Root, FBoardState& Board)
	{
		const TArray<TSharedPtr<FJsonValue>>* Edges = nullptr;
		if (!Root.TryGetArrayField(TEXT("edges"), Edges))
		{
			return Reject(EBoardError::MalformedData, TEXT("Falta la lista \"edges\"."));
		}
		for (int32 Index = 0; Index < Edges->Num(); ++Index)
		{
			const TArray<TSharedPtr<FJsonValue>>* Pair = nullptr;
			int32 A = INDEX_NONE;
			int32 B = INDEX_NONE;
			if (!(*Edges)[Index]->TryGetArray(Pair) || Pair->Num() != 2 || !ReadInteger((*Pair)[0], A) || !ReadInteger((*Pair)[1], B))
			{
				return Reject(EBoardError::MalformedData, FString::Printf(TEXT("La conexión en la posición %d de \"edges\" no es un par de identificadores."), Index));
			}
			if (!Board.IsValidTile(A) || !Board.IsValidTile(B) || A == B)
			{
				return Reject(EBoardError::InvalidGraph, FString::Printf(TEXT("La conexión %d-%d une una casilla inexistente o una casilla consigo misma."), A, B));
			}
			Board.Connect(A, B);
		}
		return FBoardLoadResult();
	}

	int32 CountDomainTiles(const FBoardState& Board, int32 DomainId, ETileType Type)
	{
		int32 Count = 0;
		for (const FTileState& Tile : Board.Tiles)
		{
			Count += (Tile.DomainId == DomainId && Tile.Type == Type) ? 1 : 0;
		}
		return Count;
	}

	/** Si alguna casilla del Dominio DomainId tiene una conexión con una casilla que cumple Pred. */
	template <typename PredType>
	bool DomainTouches(const FBoardState& Board, int32 DomainId, PredType Pred)
	{
		for (const FTileState& Tile : Board.Tiles)
		{
			if (Tile.DomainId == DomainId && Tile.Neighbours.ContainsByPredicate([&](int32 Other) { return Pred(Board.Tiles[Other]); }))
			{
				return true;
			}
		}
		return false;
	}
}

namespace Soulbound::Rules
{
	FBoardLoadResult ValidateBoard(const FBoardState& Board)
	{
		using Soulbound::Board::DomainCount;

		// R-TAB-1: grafo coherente. Identificador igual a la posición; conexiones válidas, sin bucles y en los dos sentidos.
		for (int32 Index = 0; Index < Board.Tiles.Num(); ++Index)
		{
			const FTileState& Tile = Board.Tiles[Index];
			if (Tile.TileId != Index)
			{
				return Reject(EBoardError::InvalidGraph, FString::Printf(TEXT("La casilla en la posición %d tiene el identificador %d."), Index, Tile.TileId));
			}
			for (const int32 Other : Tile.Neighbours)
			{
				if (!Board.IsValidTile(Other) || Other == Index || !Board.Tiles[Other].Neighbours.Contains(Index))
				{
					return Reject(EBoardError::InvalidGraph, FString::Printf(TEXT("La conexión %d-%d apunta a una casilla inexistente, a sí misma o va en un solo sentido."), Index, Other));
				}
			}
		}

		// R-TAB-2: una única casilla Altar.
		const int32 Altars = Board.Tiles.FilterByPredicate([](const FTileState& Tile) { return Tile.Type == ETileType::Altar; }).Num();
		if (Altars != 1)
		{
			return Reject(EBoardError::AltarCount, FString::Printf(TEXT("R-TAB-2: hay %d casillas Altar y debe haber exactamente una."), Altars));
		}

		// R-TAB-3: exactamente seis Dominios, con identificadores de 0 a 5 en el orden del anillo.
		if (Board.Domains.Num() != DomainCount)
		{
			return Reject(EBoardError::DomainCount, FString::Printf(TEXT("R-TAB-3: hay %d Dominios y debe haber %d."), Board.Domains.Num(), DomainCount));
		}
		for (int32 D = 0; D < DomainCount; ++D)
		{
			if (Board.Domains[D].DomainId != D)
			{
				return Reject(EBoardError::DomainCount, FString::Printf(TEXT("R-TAB-3: los Dominios deben tener los identificadores de 0 a %d sin repetir; falta el %d."), DomainCount - 1, D));
			}
		}

		// Toda casilla que no es el Altar pertenece a un Dominio; el Altar a ninguno (decisión del autor, 25/09/2026).
		for (const FTileState& Tile : Board.Tiles)
		{
			const bool bAltar = Tile.Type == ETileType::Altar;
			const bool bInDomain = Tile.DomainId >= 0 && Tile.DomainId < DomainCount;
			if (bAltar ? Tile.DomainId != INDEX_NONE : !bInDomain)
			{
				return Reject(EBoardError::TileWithoutDomain, bAltar
					? FString::Printf(TEXT("El Altar (casilla %d) no pertenece a ningún Dominio."), Tile.TileId)
					: FString::Printf(TEXT("La casilla %d no pertenece a un Dominio de 0 a %d (tiene %d)."), Tile.TileId, DomainCount - 1, Tile.DomainId));
			}
		}

		const int32 PowerSources = Board.Params.PowerSourcesPerDomain;
		for (int32 D = 0; D < DomainCount; ++D)
		{
			// R-TAB-3: al menos una conexión al Altar y a cada uno de los dos vecinos del anillo.
			if (!DomainTouches(Board, D, [](const FTileState& Other) { return Other.Type == ETileType::Altar; }))
			{
				return Reject(EBoardError::DomainNotConnectedToAltar, FString::Printf(TEXT("R-TAB-3: el Dominio %d no tiene ninguna conexión con el Altar."), D));
			}
			// Basta con comprobar el vecino siguiente: las conexiones van en los dos sentidos, así que el anterior
			// queda comprobado al revisar el Dominio anterior.
			const int32 Next = (D + 1) % DomainCount;
			if (!DomainTouches(Board, D, [Next](const FTileState& Other) { return Other.DomainId == Next; }))
			{
				return Reject(EBoardError::BrokenRing, FString::Printf(TEXT("R-TAB-3: anillo roto, el Dominio %d no tiene conexión con su vecino %d."), D, Next));
			}

			// R-TAB-4 y 4c: exactamente un Nicho, con una sola conexión.
			const int32 Niches = CountDomainTiles(Board, D, ETileType::Niche);
			if (Niches != 1)
			{
				return Reject(EBoardError::NicheCount, FString::Printf(TEXT("R-TAB-4: el Dominio %d tiene %d Nichos y debe tener exactamente uno."), D, Niches));
			}
			const int32 Niche = Board.FindNiche(D);
			if (Board.Tiles[Niche].Neighbours.Num() != 1)
			{
				return Reject(EBoardError::NicheNotDeadEnd, FString::Printf(TEXT("R-TAB-4c: el Nicho del Dominio %d (casilla %d) tiene %d conexiones y debe tener una."), D, Niche, Board.Tiles[Niche].Neighbours.Num()));
			}

			// R-TAB-4: tantas Fuentes como dicen los datos.
			const int32 Sources = CountDomainTiles(Board, D, ETileType::PowerSource);
			if (Sources != PowerSources)
			{
				return Reject(EBoardError::PowerSourceCount, FString::Printf(TEXT("R-TAB-4: el Dominio %d tiene %d Fuentes y los datos piden %d."), D, Sources, PowerSources));
			}
		}

		// R-TAB-5: tantos Nodos como la suma de las cantidades por efecto, para que el sorteo las pueda respetar.
		const int32 Nodes = Board.Tiles.FilterByPredicate([](const FTileState& Tile) { return Tile.Type == ETileType::PowerNode; }).Num();
		if (Nodes != Board.Params.TotalPowerNodeCount())
		{
			return Reject(EBoardError::PowerNodeCount, FString::Printf(TEXT("R-TAB-5: hay %d Nodos de Poder y las cantidades por efecto suman %d."), Nodes, Board.Params.TotalPowerNodeCount()));
		}

		return FBoardLoadResult();
	}

	FBoardLoadResult LoadBoardFromJsonString(const FString& Json)
	{
		TSharedPtr<FJsonObject> Root;
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root.IsValid())
		{
			return Reject(EBoardError::MalformedData, TEXT("El texto no es un objeto JSON válido."));
		}

		// Se lee entero o se rechaza entero: el primer paso que falla devuelve su motivo.
		FBoardState Board;
		FBoardLoadResult Step = ParseParams(*Root, Board.Params);
		if (!Step.IsSuccess())
		{
			return Step;
		}
		Step = ParseDomains(*Root, Board.Domains);
		if (!Step.IsSuccess())
		{
			return Step;
		}
		Step = ParseTiles(*Root, Board);
		if (!Step.IsSuccess())
		{
			return Step;
		}
		Step = ParseEdges(*Root, Board);
		if (!Step.IsSuccess())
		{
			return Step;
		}

		FBoardLoadResult Result = ValidateBoard(Board);
		if (Result.IsSuccess())
		{
			Result.Board = MoveTemp(Board);
		}
		return Result;
	}

	FBoardLoadResult LoadBoardFromFile(const FString& FilePath)
	{
		FString Json;
		if (!FFileHelper::LoadFileToString(Json, *FilePath))
		{
			return Reject(EBoardError::FileNotFound, FString::Printf(TEXT("No se pudo leer el archivo del tablero: %s"), *FilePath));
		}
		return LoadBoardFromJsonString(Json);
	}
}
