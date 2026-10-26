#include "Warden/WardenParams.h"

#include "Data/JsonFields.h"
#include "Dom/JsonObject.h"
#include "Match/MatchState.h"
#include "Match/WardenEvents.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Class.h"

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

	/** Lee una entrada de la tabla de un disparador: evento, multiplicador y, opcionales, lo que hace con su dirección. */
	FWardenParamsLoadResult ReadTriggerEntry(const TSharedPtr<FJsonValue>& Value, const FString& Table, FTriggerEventEntry& Out)
	{
		const TSharedPtr<FJsonObject>* Object = nullptr;
		if (!Value.IsValid() || !Value->TryGetObject(Object) || !Object || !(*Object)->TryGetStringField(TEXT("event"), Out.EventId)
			|| !Soulbound::JsonFields::ReadNumberField(**Object, TEXT("multiplier"), Out.Multiplier))
		{
			return Reject(EWardenParamsError::MalformedData, FString::Printf(TEXT("La tabla de %s: cada evento es un objeto con \"event\" y \"multiplier\" numérico."), *Table));
		}
		(*Object)->TryGetBoolField(TEXT("whenActiveIsCause"), Out.bWhenActiveIsCause);
		(*Object)->TryGetBoolField(TEXT("suggestCause"), Out.bSuggestCause);
		(*Object)->TryGetBoolField(TEXT("sealTowardsAltar"), Out.bSealTowardsAltar);
		FString Target;
		if ((*Object)->TryGetStringField(TEXT("target"), Target))
		{
			Out.bHasTargetOverride = true;
			if (!Soulbound::Rules::ParseEffectTargetCode(Target, Out.TargetOverride))
			{
				return Reject(EWardenParamsError::InvalidTrigger, FString::Printf(TEXT("R-DIS-2: la tabla de %s da a \"%s\" la dirección \"%s\", que no existe."), *Table, *Out.EventId, *Target));
			}
		}
		return FWardenParamsLoadResult();
	}

	/** Lee `triggers`: los umbrales y las tablas de los disparadores contextuales (S-017). */
	FWardenParamsLoadResult ReadTriggers(const FJsonObject& Root, FWardenTriggerParams& Out)
	{
		using Soulbound::JsonFields::ReadIntegerField;
		using Soulbound::JsonFields::ReadNumberField;
		const TSharedPtr<FJsonObject>* Object = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* Tables = nullptr;
		if (!Root.TryGetObjectField(TEXT("triggers"), Object) || !Object || !(*Object)->TryGetArrayField(TEXT("tables"), Tables) || !Tables)
		{
			return Reject(EWardenParamsError::MalformedData, TEXT("Falta el objeto \"triggers\" con la lista \"tables\"."));
		}
		const TPair<const TCHAR*, int32*> Integers[] = {
			{TEXT("clearLeaderGap"), &Out.ClearLeaderGap},
			{TEXT("imminentVictoryTypes"), &Out.ImminentVictoryTypes},
			{TEXT("hoarderCount"), &Out.HoarderCount},
		};
		for (const TPair<const TCHAR*, int32*>& Field : Integers)
		{
			if (!ReadIntegerField(**Object, Field.Key, *Field.Value))
			{
				return Reject(EWardenParamsError::MalformedData, FString::Printf(TEXT("\"triggers\": falta el entero \"%s\"."), Field.Key));
			}
		}
		const TPair<const TCHAR*, double*> Numbers[] = {
			{TEXT("imminentVictoryInterveneMultiplier"), &Out.ImminentVictoryInterveneMultiplier},
			{TEXT("ignoredMinutes"), &Out.IgnoredMinutes},
			{TEXT("ignoredRandomMultiplier"), &Out.IgnoredRandomMultiplier},
			{TEXT("ignoredSuggestMultiplier"), &Out.IgnoredSuggestMultiplier},
			{TEXT("stalledMinutes"), &Out.StalledMinutes},
		};
		for (const TPair<const TCHAR*, double*>& Field : Numbers)
		{
			if (!ReadNumberField(**Object, Field.Key, *Field.Value))
			{
				return Reject(EWardenParamsError::MalformedData, FString::Printf(TEXT("\"triggers\": falta el número \"%s\"."), Field.Key));
			}
		}
		for (int32 Index = 0; Index < Tables->Num(); ++Index)
		{
			const TSharedPtr<FJsonObject>* TableObject = nullptr;
			const TArray<TSharedPtr<FJsonValue>>* Events = nullptr;
			FString Code;
			if (!(*Tables)[Index].IsValid() || !(*Tables)[Index]->TryGetObject(TableObject) || !TableObject || !(*TableObject)->TryGetStringField(TEXT("trigger"), Code)
				|| !(*TableObject)->TryGetArrayField(TEXT("events"), Events) || !Events)
			{
				return Reject(EWardenParamsError::MalformedData, FString::Printf(TEXT("La tabla %d de \"triggers\" no es un objeto con \"trigger\" y la lista \"events\"."), Index + 1));
			}
			FTriggerTable& Table = Out.Tables.AddDefaulted_GetRef();
			// El código es el nombre del valor de la enumeración, como los efectos (R-DIS-2).
			const int64 Trigger = StaticEnum<EWardenTrigger>()->GetValueByNameString(Code, EGetByNameFlags::CaseSensitive);
			if (Trigger == INDEX_NONE)
			{
				return Reject(EWardenParamsError::InvalidTrigger, FString::Printf(TEXT("R-DIS-1: la tabla %d nombra el disparador \"%s\", que no existe."), Index + 1, *Code));
			}
			Table.Trigger = static_cast<EWardenTrigger>(Trigger);
			if ((*TableObject)->HasField(TEXT("strikesAgainstActive")) && !ReadNumberField(**TableObject, TEXT("strikesAgainstActive"), Table.StrikesAgainstActiveMultiplier))
			{
				return Reject(EWardenParamsError::MalformedData, FString::Printf(TEXT("La tabla de %s: \"strikesAgainstActive\" es un número."), *Code));
			}
			for (const TSharedPtr<FJsonValue>& Entry : *Events)
			{
				FWardenParamsLoadResult Read = ReadTriggerEntry(Entry, Code, Table.Events.AddDefaulted_GetRef());
				if (!Read.IsSuccess())
				{
					return Read;
				}
			}
		}
		return FWardenParamsLoadResult();
	}

	/** Comprueba los disparadores (CA-DIS-5): umbrales y multiplicadores positivos, una tabla por disparador como mucho. */
	FWardenParamsLoadResult ValidateTriggers(const FWardenTriggerParams& Triggers)
	{
		const bool bThresholds = Triggers.ClearLeaderGap > 0 && Triggers.ImminentVictoryTypes > 0 && Triggers.HoarderCount > 0 && Triggers.IgnoredMinutes > 0.0
			&& Triggers.StalledMinutes > 0.0;
		const bool bMultipliers = Triggers.ImminentVictoryInterveneMultiplier > 0.0 && Triggers.IgnoredRandomMultiplier > 0.0 && Triggers.IgnoredSuggestMultiplier > 0.0;
		if (!bThresholds || !bMultipliers)
		{
			return Reject(EWardenParamsError::InvalidTrigger, TEXT("R-DIS-1: los umbrales y los multiplicadores de \"triggers\" son mayores que cero."));
		}
		TSet<EWardenTrigger> Seen;
		for (const FTriggerTable& Table : Triggers.Tables)
		{
			const FString Name = StaticEnum<EWardenTrigger>()->GetNameStringByValue(static_cast<int64>(Table.Trigger));
			bool bAlreadySeen = false;
			Seen.Add(Table.Trigger, &bAlreadySeen);
			if (bAlreadySeen)
			{
				return Reject(EWardenParamsError::InvalidTrigger, FString::Printf(TEXT("R-DIS-2: el disparador %s tiene dos tablas."), *Name));
			}
			if (Table.StrikesAgainstActiveMultiplier <= 0.0)
			{
				return Reject(EWardenParamsError::InvalidTrigger, FString::Printf(TEXT("R-DIS-2: la tabla de %s: \"strikesAgainstActive\" es mayor que cero."), *Name));
			}
			for (const FTriggerEventEntry& Entry : Table.Events)
			{
				if (Entry.Multiplier <= 0.0)
				{
					return Reject(EWardenParamsError::InvalidTrigger, FString::Printf(TEXT("R-DIS-2: la tabla de %s multiplica \"%s\" por %.2f; el multiplicador es mayor que cero."),
						*Name, *Entry.EventId, Entry.Multiplier));
				}
				// La dirección preferida sustituye sin pedir nada a nadie: nunca un rival señalado (decisión del autor del 23/10/2026).
				const bool bChoosing = Entry.TargetOverride == EEffectTarget::ChosenRival || Entry.TargetOverride == EEffectTarget::TwoChosenRivals
					|| Entry.TargetOverride == EEffectTarget::None;
				if (Entry.bHasTargetOverride && bChoosing)
				{
					return Reject(EWardenParamsError::InvalidTrigger, FString::Printf(TEXT("R-DIS-2: la tabla de %s da a \"%s\" una dirección preferida que pide elegir."), *Name, *Entry.EventId));
				}
			}
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
		// R-ORA-3, R-ORA-4 (S-017): el plazo de la consulta avanza; el sesgo mezcla dos repartos, de 0 a 1.
		if (Params.OracleSeconds <= 0.0)
		{
			return Reject(EWardenParamsError::NonPositiveDuration, FString::Printf(TEXT("R-ORA-3: el plazo del Oráculo (%.1f s) es mayor que cero."), Params.OracleSeconds));
		}
		// R-VOT-1 (S-017): el plazo de una votación avanza.
		if (Params.VoteSeconds <= 0.0)
		{
			return Reject(EWardenParamsError::NonPositiveDuration, FString::Printf(TEXT("R-VOT-1: el plazo de una votación (%.1f s) es mayor que cero."), Params.VoteSeconds));
		}
		if (Params.OracleBias < 0.0 || Params.OracleBias > 1.0)
		{
			return Reject(EWardenParamsError::ProbabilityOutOfRange, FString::Printf(TEXT("R-ORA-4: el sesgo del Oráculo (%.2f) va de 0 a 1."), Params.OracleBias));
		}
		return ValidateTriggers(Params.Triggers);
	}

	FWardenParamsLoadResult BindWardenTriggerEvents(FWardenParams& Params, const FWardenEventTable& Events)
	{
		for (FTriggerTable& Table : Params.Triggers.Tables)
		{
			for (FTriggerEventEntry& Entry : Table.Events)
			{
				Entry.EventIndex = Events.IndexOf(Entry.EventId);
				if (Entry.EventIndex == INDEX_NONE)
				{
					return Reject(EWardenParamsError::UnknownTriggerEvent, FString::Printf(TEXT("R-DIS-2: la tabla de %s nombra el evento \"%s\", que no está en los eventos del Warden."),
						*StaticEnum<EWardenTrigger>()->GetNameStringByValue(static_cast<int64>(Table.Trigger)), *Entry.EventId));
				}
			}
		}
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
			// S-017: el Oráculo.
			{TEXT("oracleSeconds"), &Params.OracleSeconds},
			{TEXT("oracleBias"), &Params.OracleBias},
			// S-017 (PR B): las votaciones.
			{TEXT("voteSeconds"), &Params.VoteSeconds},
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
		FWardenParamsLoadResult Triggers = ReadTriggers(*Root, Params.Triggers);
		if (!Triggers.IsSuccess())
		{
			return Triggers;
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
