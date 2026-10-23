#include "Warden/WardenParams.h"

#include "Data/JsonFields.h"
#include "Dom/JsonObject.h"
#include "Match/MatchState.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	FWardenParamsLoadResult Reject(EWardenParamsError Error, const FString& Message)
	{
		FWardenParamsLoadResult Result;
		Result.Error = Error;
		Result.Message = Message;
		return Result;
	}

	/** Nombres de los canales en los datos (`channelWeights`), en el orden de EWardenChannel. */
	const TCHAR* const ChannelFields[] = {TEXT("strike"), TEXT("scene"), TEXT("remark")};

	/** Nombres de las gravedades en la tabla de insolencia (`insolence`), en el orden de la tabla (leve, media, grave). */
	const TCHAR* const SeverityFields[] = {TEXT("light"), TEXT("medium"), TEXT("severe")};

	/** Lee un acto: inicio, probabilidad de intervenir, pesos de canal y objetivo de cortes. */
	FWardenParamsLoadResult ReadAct(const TSharedPtr<FJsonValue>& Value, int32 Index, FWardenActParams& Out)
	{
		using Soulbound::JsonFields::ReadNumberField;
		const TSharedPtr<FJsonObject>* Object = nullptr;
		const TSharedPtr<FJsonObject>* Weights = nullptr;
		if (!Value.IsValid() || !Value->TryGetObject(Object) || !Object || !ReadNumberField(**Object, TEXT("startSeconds"), Out.StartSeconds)
			|| !ReadNumberField(**Object, TEXT("interveneProbability"), Out.InterveneProbability) || !ReadNumberField(**Object, TEXT("cutTarget"), Out.CutTarget)
			|| !(*Object)->TryGetObjectField(TEXT("channelWeights"), Weights) || !Weights)
		{
			return Reject(EWardenParamsError::MalformedData, FString::Printf(TEXT("El acto %d no es un objeto con \"startSeconds\", \"interveneProbability\", \"cutTarget\" y \"channelWeights\" numéricos."), Index + 1));
		}
		for (const TCHAR* Field : ChannelFields)
		{
			double Weight = 0.0;
			if (!ReadNumberField(**Weights, Field, Weight))
			{
				return Reject(EWardenParamsError::MalformedData, FString::Printf(TEXT("El acto %d: falta el peso numérico \"%s\" en \"channelWeights\"."), Index + 1, Field));
			}
			Out.ChannelWeights.Add(Weight);
		}
		return FWardenParamsLoadResult();
	}

	/** Lee una fila de la tabla de insolencia: niveles y, o los tres porcentajes, o la Furia. */
	FWardenParamsLoadResult ReadInsolenceRow(const TSharedPtr<FJsonValue>& Value, int32 Index, FInsolenceRow& Out)
	{
		const TSharedPtr<FJsonObject>* Object = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* Levels = nullptr;
		if (!Value.IsValid() || !Value->TryGetObject(Object) || !Object || !(*Object)->TryGetArrayField(TEXT("levels"), Levels) || !Levels)
		{
			return Reject(EWardenParamsError::MalformedData, FString::Printf(TEXT("La fila %d de \"insolence\" no es un objeto con la lista \"levels\"."), Index + 1));
		}
		for (const TSharedPtr<FJsonValue>& Level : *Levels)
		{
			int32 Number = 0;
			if (!Soulbound::JsonFields::ReadIntegerValue(Level, Number))
			{
				return Reject(EWardenParamsError::MalformedData, FString::Printf(TEXT("La fila %d de \"insolence\": los niveles son enteros."), Index + 1));
			}
			Out.Levels.Add(Number);
		}
		bool bFury = false;
		if ((*Object)->TryGetBoolField(TEXT("fury"), bFury) && bFury)
		{
			Out.bFury = true;
			return FWardenParamsLoadResult();
		}
		for (const TCHAR* Field : SeverityFields)
		{
			int32 Percent = 0;
			if (!Soulbound::JsonFields::ReadIntegerField(**Object, Field, Percent))
			{
				return Reject(EWardenParamsError::MalformedData, FString::Printf(TEXT("La fila %d de \"insolence\": falta el porcentaje entero \"%s\" (o \"fury\": true)."), Index + 1, Field));
			}
			Out.SeverityPercents.Add(Percent);
		}
		return FWardenParamsLoadResult();
	}
}

namespace Soulbound::Rules
{
	FWardenParamsLoadResult ValidateWardenParams(const FWardenParams& Params)
	{
		using Soulbound::Warden::ActCount;
		using Soulbound::Warden::ChannelCount;
		if (Params.Acts.Num() != ActCount)
		{
			return Reject(EWardenParamsError::MalformedData, FString::Printf(TEXT("R-WAR-2: debe haber %d actos (hay %d)."), ActCount, Params.Acts.Num()));
		}
		// R-WAR-2: el primero empieza con la partida y cada uno después del anterior, antes del fin de la partida.
		for (int32 Act = 0; Act < ActCount; ++Act)
		{
			const double Start = Params.Acts[Act].StartSeconds;
			const bool bIncreasing = Act == 0 ? Start == 0.0 : Start > Params.Acts[Act - 1].StartSeconds;
			if (!bIncreasing || Start >= Params.MatchEndSeconds)
			{
				return Reject(EWardenParamsError::ActsNotIncreasing, FString::Printf(TEXT("R-WAR-2: el acto %d empieza en %.0f s; el primero en 0 y cada uno después del anterior y antes del fin (%.0f s)."),
					Act + 1, Start, Params.MatchEndSeconds));
			}
		}
		for (int32 Act = 0; Act < ActCount; ++Act)
		{
			const FWardenActParams& Data = Params.Acts[Act];
			// R-WAR-3: probabilidades; el objetivo de cortes es una proporción de turnos.
			if (Data.InterveneProbability < 0.0 || Data.InterveneProbability > 1.0 || Data.CutTarget < 0.0 || Data.CutTarget > 1.0)
			{
				return Reject(EWardenParamsError::ProbabilityOutOfRange, FString::Printf(TEXT("R-WAR-3: acto %d: la probabilidad de intervenir (%.2f) y el objetivo de cortes (%.2f) van de 0 a 1."),
					Act + 1, Data.InterveneProbability, Data.CutTarget));
			}
			if (Data.ChannelWeights.Num() != ChannelCount)
			{
				return Reject(EWardenParamsError::MalformedData, FString::Printf(TEXT("R-WAR-3: acto %d: hacen falta %d pesos de canal."), Act + 1, ChannelCount));
			}
			double Sum = 0.0;
			for (const double Weight : Data.ChannelWeights)
			{
				if (Weight < 0.0)
				{
					return Reject(EWardenParamsError::NegativeWeight, FString::Printf(TEXT("R-WAR-3: acto %d: un peso de canal negativo (%.2f)."), Act + 1, Weight));
				}
				Sum += Weight;
			}
			if (Sum <= 0.0)
			{
				return Reject(EWardenParamsError::NegativeWeight, FString::Printf(TEXT("R-WAR-3: acto %d: los pesos de canal no pueden ser todos cero."), Act + 1));
			}
		}
		// R-WAR-3, R-WAR-4d, R-WAR-5: el intervalo avanza siempre (la variación es menor que él); margen y garantía, positivos.
		if (Params.EvaluationIntervalSeconds <= 0.0 || Params.EvaluationVariationSeconds < 0.0 || Params.EvaluationVariationSeconds >= Params.EvaluationIntervalSeconds
			|| Params.GuaranteeSeconds <= 0.0 || Params.MarginSeconds <= 0.0)
		{
			return Reject(EWardenParamsError::NonPositiveDuration, FString::Printf(TEXT("R-WAR-3, R-WAR-4d, R-WAR-5: intervalo (%.1f), garantía (%.1f) y margen (%.1f) mayores que cero, y variación (%.1f) de cero a menos que el intervalo."),
				Params.EvaluationIntervalSeconds, Params.GuaranteeSeconds, Params.MarginSeconds, Params.EvaluationVariationSeconds));
		}
		// R-INS-2: cada nivel anterior a una subida (0 a MaxInsolence - 1), en una sola fila; cada fila sin Furia suma 100.
		TArray<int32> Covered;
		Covered.Init(0, Soulbound::Match::MaxInsolence);
		for (int32 Row = 0; Row < Params.InsolenceTable.Num(); ++Row)
		{
			const FInsolenceRow& Data = Params.InsolenceTable[Row];
			for (const int32 Level : Data.Levels)
			{
				if (!Covered.IsValidIndex(Level))
				{
					return Reject(EWardenParamsError::InsolenceTableInvalid, FString::Printf(TEXT("R-INS-2: fila %d: el nivel %d no va de 0 a %d."), Row + 1, Level, Soulbound::Match::MaxInsolence - 1));
				}
				++Covered[Level];
			}
			int32 Sum = 0;
			bool bNegative = false;
			for (const int32 Percent : Data.SeverityPercents)
			{
				Sum += Percent;
				bNegative |= Percent < 0;
			}
			const bool bValidRow = Data.bFury ? Data.SeverityPercents.IsEmpty()
				: Data.SeverityPercents.Num() == Soulbound::Warden::InsolenceSeverityCount && !bNegative && Sum == 100;
			if (!bValidRow)
			{
				return Reject(EWardenParamsError::InsolenceTableInvalid, FString::Printf(TEXT("R-INS-2: fila %d: leve, media y grave suman %d, no 100 (o la Furia, sin porcentajes)."), Row + 1, Sum));
			}
		}
		for (int32 Level = 0; Level < Covered.Num(); ++Level)
		{
			if (Covered[Level] != 1)
			{
				return Reject(EWardenParamsError::InsolenceTableInvalid, FString::Printf(TEXT("R-INS-2: el nivel %d aparece en %d filas de la tabla de insolencia; debe estar en una."), Level, Covered[Level]));
			}
		}
		return FWardenParamsLoadResult();
	}

	FWardenParamsLoadResult BindWardenTriggerEvents(FWardenParams& Params, const FWardenEventTable& Events)
	{
		return FWardenParamsLoadResult();
	}

	FWardenParamsLoadResult LoadWardenParamsFromJsonString(const FString& Json)
	{
		using Soulbound::JsonFields::ReadNumberField;
		TSharedPtr<FJsonObject> Root;
		const TArray<TSharedPtr<FJsonValue>>* Acts = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* Insolence = nullptr;
		FWardenParams Params;
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root.IsValid() || !Root->TryGetArrayField(TEXT("acts"), Acts) || !Acts
			|| !Root->TryGetArrayField(TEXT("insolence"), Insolence) || !Insolence)
		{
			return Reject(EWardenParamsError::MalformedData, TEXT("El texto no es un objeto JSON con las listas \"acts\" e \"insolence\"."));
		}
		const TPair<const TCHAR*, double*> Numbers[] = {
			{TEXT("matchEndSeconds"), &Params.MatchEndSeconds},
			{TEXT("evaluationIntervalSeconds"), &Params.EvaluationIntervalSeconds},
			{TEXT("evaluationVariationSeconds"), &Params.EvaluationVariationSeconds},
			{TEXT("guaranteeSeconds"), &Params.GuaranteeSeconds},
			{TEXT("marginSeconds"), &Params.MarginSeconds},
		};
		for (const TPair<const TCHAR*, double*>& Number : Numbers)
		{
			if (!ReadNumberField(*Root, Number.Key, *Number.Value))
			{
				return Reject(EWardenParamsError::MalformedData, FString::Printf(TEXT("Falta el campo numérico \"%s\"."), Number.Key));
			}
		}
		for (int32 Index = 0; Index < Acts->Num(); ++Index)
		{
			FWardenParamsLoadResult Read = ReadAct((*Acts)[Index], Index, Params.Acts.AddDefaulted_GetRef());
			if (!Read.IsSuccess())
			{
				return Read;
			}
		}
		for (int32 Index = 0; Index < Insolence->Num(); ++Index)
		{
			FWardenParamsLoadResult Read = ReadInsolenceRow((*Insolence)[Index], Index, Params.InsolenceTable.AddDefaulted_GetRef());
			if (!Read.IsSuccess())
			{
				return Read;
			}
		}
		FWardenParamsLoadResult Result = ValidateWardenParams(Params);
		if (Result.IsSuccess())
		{
			Result.Params = MoveTemp(Params);
		}
		return Result;
	}

	FWardenParamsLoadResult LoadWardenParamsFromFile(const FString& FilePath)
	{
		FString Json;
		if (!FFileHelper::LoadFileToString(Json, *FilePath))
		{
			return Reject(EWardenParamsError::FileNotFound, FString::Printf(TEXT("No se pudo leer el archivo del Warden: %s"), *FilePath));
		}
		return LoadWardenParamsFromJsonString(Json);
	}
}
