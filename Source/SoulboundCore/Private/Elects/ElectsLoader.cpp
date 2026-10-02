#include "Elects/ElectsLoader.h"

#include "Board/BoardState.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	/** Número de poderes y de runas: seis, uno por Dominio (R-RUN-1, R-ELE-3). */
	constexpr int32 PowerCount = 6;

	/** Identificadores de poder en el archivo, en el orden de EPower. */
	const TCHAR* const PowerNames[PowerCount] = {TEXT("duel"), TEXT("haste"), TEXT("freePassage"), TEXT("expel"), TEXT("planarFlight"), TEXT("deflection")};

	/** Identificadores de afinidad en el archivo, en el orden de EAffinity. */
	const TCHAR* const AffinityNames[6] = {TEXT("duelAdvantage"), TEXT("freePassage"), TEXT("rerollOnce"), TEXT("voidEscape"), TEXT("refuseOracle"), TEXT("safeRune")};

	FElectsLoadResult Reject(EElectsError Error, const FString& Message)
	{
		FElectsLoadResult Result;
		Result.Error = Error;
		Result.Message = Message;
		return Result;
	}

	/** Índice de Name en Names, o INDEX_NONE si no es ninguno. */
	template <int32 Count>
	int32 IndexOfName(const TCHAR* const (&Names)[Count], const FString& Name)
	{
		for (int32 Index = 0; Index < Count; ++Index)
		{
			if (Name == Names[Index])
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}

	/** Lee el campo entero Field de Object. Devuelve false si falta, no es un número o tiene decimales. */
	bool ReadIntegerField(const FJsonObject& Object, const TCHAR* Field, int32& Out)
	{
		double Number = 0.0;
		const TSharedPtr<FJsonValue> Value = Object.TryGetField(Field);
		if (!Value.IsValid() || Value->Type != EJson::Number || !Value->TryGetNumber(Number) || Number != FMath::FloorToDouble(Number)
			|| Number < MIN_int32 || Number > MAX_int32)
		{
			return false;
		}
		Out = static_cast<int32>(Number);
		return true;
	}

	/** Lee un Elegido de su objeto JSON. Devuelve false, con el motivo en Error, si el objeto no cumple el esquema. */
	bool ReadElect(const FJsonObject& Object, int32& OutDomain, FElectDef& OutElect, FString& Error)
	{
		if (!ReadIntegerField(Object, TEXT("domain"), OutDomain) || OutDomain < 0 || OutDomain >= Soulbound::Board::DomainCount)
		{
			Error = FString::Printf(TEXT("Un Elegido no tiene \"domain\" entero entre 0 y %d."), Soulbound::Board::DomainCount - 1);
			return false;
		}
		const FString Where = FString::Printf(TEXT("Elegido del Dominio %d"), OutDomain);
		if (!Object.TryGetStringField(TEXT("name"), OutElect.Name) || OutElect.Name.IsEmpty())
		{
			Error = FString::Printf(TEXT("%s: falta \"name\" o está vacío."), *Where);
			return false;
		}
		FString AffinityName;
		const int32 Affinity = Object.TryGetStringField(TEXT("affinity"), AffinityName) ? IndexOfName(AffinityNames, AffinityName) : INDEX_NONE;
		if (Affinity == INDEX_NONE)
		{
			Error = FString::Printf(TEXT("%s: \"affinity\" falta o no es una afinidad conocida (%s)."), *Where, *AffinityName);
			return false;
		}
		OutElect.Affinity = static_cast<EAffinity>(Affinity);

		const TSharedPtr<FJsonObject>* Powers = nullptr;
		if (!Object.TryGetObjectField(TEXT("powers"), Powers) || !Powers)
		{
			Error = FString::Printf(TEXT("%s: falta el mapa \"powers\"."), *Where);
			return false;
		}
		OutElect.RuneToPower.Reset();
		for (int32 Rune = 0; Rune < Soulbound::Board::DomainCount; ++Rune)
		{
			FString PowerName;
			const int32 Power = (*Powers)->TryGetStringField(FString::FromInt(Rune), PowerName) ? IndexOfName(PowerNames, PowerName) : INDEX_NONE;
			if (Power == INDEX_NONE)
			{
				Error = FString::Printf(TEXT("%s: la runa %d falta en \"powers\" o su poder no es conocido (%s)."), *Where, Rune, *PowerName);
				return false;
			}
			OutElect.RuneToPower.Add(static_cast<EPower>(Power));
		}
		return true;
	}
}

namespace Soulbound::Rules
{
	FElectsLoadResult ValidateElects(const FElectsTable& Table)
	{
		using Soulbound::Board::DomainCount;

		// R-ELE-1: seis Elegidos, uno por Dominio, con nombre y seis runas cada uno.
		if (Table.Elects.Num() != DomainCount)
		{
			return Reject(EElectsError::MalformedData, FString::Printf(TEXT("R-ELE-1: debe haber %d Elegidos, uno por Dominio, y hay %d."), DomainCount, Table.Elects.Num()));
		}
		for (int32 Domain = 0; Domain < DomainCount; ++Domain)
		{
			const FElectDef& Elect = Table.Elects[Domain];
			if (Elect.Name.IsEmpty())
			{
				return Reject(EElectsError::MalformedData, FString::Printf(TEXT("R-ELE-1: el Elegido del Dominio %d no tiene nombre."), Domain));
			}
			if (Elect.RuneToPower.Num() != DomainCount)
			{
				return Reject(EElectsError::MalformedData, FString::Printf(TEXT("R-ELE-3: el mapa del Elegido del Dominio %d debe tener %d runas y tiene %d."), Domain, DomainCount, Elect.RuneToPower.Num()));
			}
		}

		// R-ELE-3: cada fila (un Elegido) y cada columna (una runa) contienen los seis poderes una vez.
		for (int32 Domain = 0; Domain < DomainCount; ++Domain)
		{
			TArray<int32> InRow;
			TArray<int32> InColumn;
			InRow.Init(0, PowerCount);
			InColumn.Init(0, PowerCount);
			for (int32 Other = 0; Other < DomainCount; ++Other)
			{
				++InRow[static_cast<int32>(Table.Elects[Domain].RuneToPower[Other])];
				++InColumn[static_cast<int32>(Table.Elects[Other].RuneToPower[Domain])];
			}
			for (int32 Power = 0; Power < PowerCount; ++Power)
			{
				if (InRow[Power] != 1)
				{
					return Reject(EElectsError::NotLatinSquare, FString::Printf(TEXT("R-ELE-3: en la fila del Elegido del Dominio %d, el poder \"%s\" aparece %d veces."), Domain, PowerNames[Power], InRow[Power]));
				}
				if (InColumn[Power] != 1)
				{
					return Reject(EElectsError::NotLatinSquare, FString::Printf(TEXT("R-ELE-3: en la columna de la runa %d, el poder \"%s\" aparece %d veces."), Domain, PowerNames[Power], InColumn[Power]));
				}
			}
		}

		// R-ELE-3, R-ECO-7: la runa del propio Dominio da Celeridad.
		for (int32 Domain = 0; Domain < DomainCount; ++Domain)
		{
			if (Table.Elects[Domain].RuneToPower[Domain] != EPower::Haste)
			{
				return Reject(EElectsError::DiagonalNotHaste, FString::Printf(TEXT("R-ELE-3: la runa %d del Elegido del Dominio %d debe dar Celeridad (\"haste\")."), Domain, Domain));
			}
		}
		return FElectsLoadResult();
	}

	FElectsLoadResult LoadElectsFromJsonString(const FString& Json)
	{
		TSharedPtr<FJsonObject> Root;
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root.IsValid())
		{
			return Reject(EElectsError::MalformedData, TEXT("El texto no es un objeto JSON válido."));
		}
		const TArray<TSharedPtr<FJsonValue>>* Entries = nullptr;
		if (!Root->TryGetArrayField(TEXT("elects"), Entries) || !Entries)
		{
			return Reject(EElectsError::MalformedData, TEXT("Falta la lista \"elects\"."));
		}
		if (Entries->Num() != Soulbound::Board::DomainCount)
		{
			return Reject(EElectsError::MalformedData, FString::Printf(TEXT("R-ELE-1: \"elects\" debe tener %d Elegidos y tiene %d."), Soulbound::Board::DomainCount, Entries->Num()));
		}

		// Se leen enteros o se rechazan enteros: el primer Elegido que falla devuelve su motivo.
		FElectsTable Table;
		Table.Elects.SetNum(Soulbound::Board::DomainCount);
		TArray<bool> Seen;
		Seen.Init(false, Soulbound::Board::DomainCount);
		for (const TSharedPtr<FJsonValue>& Entry : *Entries)
		{
			const TSharedPtr<FJsonObject>* Object = nullptr;
			if (!Entry.IsValid() || !Entry->TryGetObject(Object) || !Object)
			{
				return Reject(EElectsError::MalformedData, TEXT("Un elemento de \"elects\" no es un objeto."));
			}
			int32 Domain = INDEX_NONE;
			FElectDef Elect;
			FString Error;
			if (!ReadElect(**Object, Domain, Elect, Error))
			{
				return Reject(EElectsError::MalformedData, Error);
			}
			if (Seen[Domain])
			{
				return Reject(EElectsError::MalformedData, FString::Printf(TEXT("R-ELE-1: el Dominio %d tiene más de un Elegido."), Domain));
			}
			Seen[Domain] = true;
			Table.Elects[Domain] = MoveTemp(Elect);
		}

		FElectsLoadResult Result = ValidateElects(Table);
		if (Result.IsSuccess())
		{
			Result.Table = MoveTemp(Table);
		}
		return Result;
	}

	FElectsLoadResult LoadElectsFromFile(const FString& FilePath)
	{
		FString Json;
		if (!FFileHelper::LoadFileToString(Json, *FilePath))
		{
			return Reject(EElectsError::FileNotFound, FString::Printf(TEXT("No se pudo leer el archivo de Elegidos: %s"), *FilePath));
		}
		return LoadElectsFromJsonString(Json);
	}
}
