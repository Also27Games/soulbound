#include "Warden/WardenLines.h"

#include "Dom/JsonObject.h"
#include "Match/MatchState.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Warden/WardenVoice.h"

namespace
{
	FWardenLinesLoadResult Reject(EWardenLinesError Error, const FString& Message)
	{
		FWardenLinesLoadResult Result;
		Result.Error = Error;
		Result.Message = Message;
		return Result;
	}

	/** Nombre en la clave de cada tipo de comentario, en el orden de EWardenRemarkKind. */
	const TCHAR* const RemarkNames[] = {
		TEXT("burla-al-ultimo"), TEXT("cinco-runas"), TEXT("prisa"), TEXT("acaparador"), TEXT("mendigo"), TEXT("pardillo"),
		TEXT("lento"), TEXT("desagradecido"), TEXT("apuesta-fallida"), TEXT("presentacion"), TEXT("risa-al-cortar"), TEXT("esta-vez-no"),
	};

	/** Nombre en la clave de cada momento de instrucción, en el orden de EInstructionMoment. */
	const TCHAR* const InstructionNames[] = {
		TEXT("tirar"), TEXT("elegir-casilla"), TEXT("nicho"), TEXT("fuente"), TEXT("dilema"), TEXT("duelo"), TEXT("orbe"), TEXT("voto"),
		TEXT("seis-runas"), TEXT("insolencia"),
	};

	constexpr int32 RemarkCount = UE_ARRAY_COUNT(RemarkNames);
	constexpr int32 InstructionCount = UE_ARRAY_COUNT(InstructionNames);
	static_assert(RemarkCount == static_cast<int32>(EWardenRemarkKind::NotThisTime) + 1, "Un nombre por tipo de comentario");
	static_assert(InstructionCount == static_cast<int32>(EInstructionMoment::Insolence) + 1, "Un nombre por momento de instrucción");
}

namespace Soulbound::Rules
{
	FString WardenEventKey(const FString& EventId)
	{
		return TEXT("evento:") + EventId;
	}

	FString WardenRemarkKey(EWardenRemarkKind Kind)
	{
		const int32 Index = static_cast<int32>(Kind);
		return FString(TEXT("comentario:")) + (Index >= 0 && Index < RemarkCount ? RemarkNames[Index] : TEXT("desconocido"));
	}

	FString InstructionKey(EInstructionMoment Moment)
	{
		const int32 Index = static_cast<int32>(Moment);
		return FString(TEXT("instruccion:")) + (Index >= 0 && Index < InstructionCount ? InstructionNames[Index] : TEXT("desconocido"));
	}

	FString ScheduledOrderKey(int32 Position)
	{
		return FString::Printf(TEXT("programado:orden-%d"), Position);
	}

	FString IntroductionKey(int32 DomainId)
	{
		return FString::Printf(TEXT("presentacion:%d"), DomainId);
	}

	TArray<FString> RequiredWardenLineKeys(const FWardenEventTable& Events, const FElectsTable& Elects)
	{
		TArray<FString> Keys;
		for (const FWardenEventDef& Event : Events.Events)
		{
			Keys.Add(WardenEventKey(Event.Id));
		}
		for (int32 Kind = 0; Kind < RemarkCount; ++Kind)
		{
			Keys.Add(WardenRemarkKey(static_cast<EWardenRemarkKind>(Kind)));
		}
		for (int32 Moment = 0; Moment < InstructionCount; ++Moment)
		{
			Keys.Add(InstructionKey(static_cast<EInstructionMoment>(Moment)));
		}
		for (const EInsolencePunishment Punishment : {EInsolencePunishment::Light, EInsolencePunishment::Medium, EInsolencePunishment::Severe, EInsolencePunishment::Fury})
		{
			Keys.Add(InsolenceLineKey(Punishment));
		}
		Keys.Add(Soulbound::Warden::LineStart);
		Keys.Add(Soulbound::Warden::LineCorruption);
		for (int32 Position = 1; Position <= Soulbound::Warden::OrderLineCount; ++Position)
		{
			Keys.Add(ScheduledOrderKey(Position));
		}
		Keys.Add(Soulbound::Warden::LineRaceEnd);
		Keys.Add(Soulbound::Warden::LineAspirant);
		Keys.Add(Soulbound::Warden::LineLastMinute);
		Keys.Add(Soulbound::Warden::LineTenSeconds);
		Keys.Add(Soulbound::Warden::LineTimeOut);
		Keys.Add(Soulbound::Warden::LineVictory);
		for (int32 Domain = 0; Domain < Elects.Elects.Num(); ++Domain)
		{
			Keys.Add(IntroductionKey(Domain));
		}
		return Keys;
	}

	FWardenLinesLoadResult ValidateWardenLines(const FWardenLineTable& Table, const TArray<FString>& Required)
	{
		// Primero lo que está mal en lo que hay (repetidas, variantes vacías) y después lo que falta, para que el motivo diga la clave.
		TSet<FString> Seen;
		for (const FWardenLineEntry& Entry : Table.Entries)
		{
			if (Seen.Contains(Entry.Id))
			{
				return Reject(EWardenLinesError::DuplicateKey, FString::Printf(TEXT("R-VOZ-5: la clave \"%s\" está repetida."), *Entry.Id));
			}
			Seen.Add(Entry.Id);
		}
		for (const FWardenLineEntry& Entry : Table.Entries)
		{
			if (Entry.Lines.IsEmpty())
			{
				return Reject(EWardenLinesError::EmptyLine, FString::Printf(TEXT("R-VOZ-5: la clave \"%s\" no tiene ninguna variante."), *Entry.Id));
			}
			for (int32 Variant = 0; Variant < Entry.Lines.Num(); ++Variant)
			{
				if (Entry.Lines[Variant].TrimStartAndEnd().IsEmpty())
				{
					return Reject(EWardenLinesError::EmptyLine, FString::Printf(TEXT("R-VOZ-5: la clave \"%s\", variante %d, está vacía."), *Entry.Id, Variant + 1));
				}
			}
		}
		for (const FString& Key : Required)
		{
			if (!Seen.Contains(Key))
			{
				return Reject(EWardenLinesError::MissingKey, FString::Printf(TEXT("R-VOZ-5: falta la clave \"%s\"."), *Key));
			}
		}
		return FWardenLinesLoadResult();
	}

	FWardenLinesLoadResult LoadWardenLinesFromJsonString(const FString& Json, const TArray<FString>& Required)
	{
		TArray<TSharedPtr<FJsonValue>> Root;
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root))
		{
			return Reject(EWardenLinesError::MalformedData, TEXT("El texto no es una lista JSON de entradas {\"id\", \"lines\"}."));
		}
		// Se leen enteras o se rechazan enteras: la primera entrada que falla devuelve su motivo.
		FWardenLineTable Table;
		for (int32 Index = 0; Index < Root.Num(); ++Index)
		{
			const TSharedPtr<FJsonObject>* Object = nullptr;
			const TArray<TSharedPtr<FJsonValue>>* Variants = nullptr;
			FWardenLineEntry& Entry = Table.Entries.AddDefaulted_GetRef();
			// Un texto "estricto": un 3 no es una clave (TryGetStringField convertiría el número).
			const TSharedPtr<FJsonValue> Id = Root[Index].IsValid() && Root[Index]->TryGetObject(Object) && Object ? (*Object)->TryGetField(TEXT("id")) : nullptr;
			if (!Id.IsValid() || Id->Type != EJson::String || !Id->TryGetString(Entry.Id) || !(*Object)->TryGetArrayField(TEXT("lines"), Variants) || !Variants)
			{
				return Reject(EWardenLinesError::MalformedData, FString::Printf(TEXT("La entrada %d no es {\"id\": texto, \"lines\": lista de textos}."), Index + 1));
			}
			for (const TSharedPtr<FJsonValue>& Variant : *Variants)
			{
				FString Text;
				if (!Variant.IsValid() || Variant->Type != EJson::String || !Variant->TryGetString(Text))
				{
					return Reject(EWardenLinesError::MalformedData, FString::Printf(TEXT("La entrada \"%s\" tiene una variante que no es un texto."), *Entry.Id));
				}
				Entry.Lines.Add(Text);
			}
		}
		FWardenLinesLoadResult Result = ValidateWardenLines(Table, Required);
		if (Result.IsSuccess())
		{
			Result.Table = MoveTemp(Table);
		}
		return Result;
	}

	FWardenLinesLoadResult LoadWardenLinesFromFile(const FString& FilePath, const TArray<FString>& Required)
	{
		FString Json;
		if (!FFileHelper::LoadFileToString(Json, *FilePath))
		{
			return Reject(EWardenLinesError::FileNotFound, FString::Printf(TEXT("No se pudo leer el archivo de frases del Warden: %s"), *FilePath));
		}
		return LoadWardenLinesFromJsonString(Json, Required);
	}

	FString StripLineCues(const FString& Line)
	{
		FString Result;
		int32 Index = 0;
		while (Index < Line.Len())
		{
			// La acotación acaba en el primer corchete de cierre posterior; un corchete que no cierra se deja como está.
			const int32 Close = Line[Index] == TEXT('[') ? Line.Find(TEXT("]"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Index) : INDEX_NONE;
			if (Close != INDEX_NONE)
			{
				Index = Close + 1;
				Result.AppendChar(TEXT(' '));
				continue;
			}
			Result.AppendChar(Line[Index++]);
		}
		// Los espacios que dejan las acotaciones se juntan en uno y no quedan en los extremos.
		while (Result.ReplaceInline(TEXT("  "), TEXT(" ")) > 0)
		{
		}
		return Result.TrimStartAndEnd();
	}
}
