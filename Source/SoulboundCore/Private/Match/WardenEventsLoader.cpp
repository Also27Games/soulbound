#include "Match/WardenEvents.h"

#include "Data/JsonFields.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	FWardenEventsLoadResult Reject(EWardenEventsError Error, const FString& Message)
	{
		FWardenEventsLoadResult Result;
		Result.Error = Error;
		Result.Message = Message;
		return Result;
	}

	/** "evento \"id\", opción n, aplicación m" (contando desde 1): dónde está lo que se rechaza. */
	FString Where(const FString& Id, int32 Option, int32 Application)
	{
		return FString::Printf(TEXT("evento \"%s\", opción %d, aplicación %d"), *Id, Option + 1, Application + 1);
	}

	/** Comprueba una aplicación contra el catálogo (R-EFE-0): dirección, cantidad y variante. */
	FWardenEventsLoadResult ValidateApplication(const FEffectApplication& Application, const FString& Context)
	{
		if (static_cast<int32>(Application.Effect) >= Soulbound::Effects::EffectCount)
		{
			return Reject(EWardenEventsError::UnknownEffect, FString::Printf(TEXT("R-EFE-0: %s: el efecto no está en el catálogo."), *Context));
		}
		const FEffectDef& Def = Soulbound::Rules::EffectDefOf(Application.Effect);
		const FString Code = Soulbound::Rules::EffectCode(Application.Effect);
		if (!Def.AllowedTargets.Contains(Application.Target))
		{
			const FString Allowed = FString::JoinBy(Def.AllowedTargets, TEXT(", "), [](EEffectTarget Target)
			{
				return Target == EEffectTarget::None ? FString(TEXT("sin dirección")) : Soulbound::Rules::EffectTargetCode(Target);
			});
			const FString Given = Application.Target == EEffectTarget::None ? FString(TEXT("sin dirección")) : Soulbound::Rules::EffectTargetCode(Application.Target);
			return Reject(EWardenEventsError::TargetNotAllowed, FString::Printf(TEXT("R-EFE-0: %s: %s no admite la dirección %s (admite: %s)."), *Context, *Code, *Given, *Allowed));
		}
		const int32 MaxAmount = Def.bTakesAmount ? Soulbound::Effects::MaxAmount : 1;
		if (Application.Amount < 1 || Application.Amount > MaxAmount)
		{
			return Reject(EWardenEventsError::InvalidAmount, FString::Printf(TEXT("R-EFE-5, R-EFE-15: %s: %s admite una cantidad de 1 a %d (es %d)."),
				*Context, *Code, MaxAmount, Application.Amount));
		}
		// None en un efecto con variantes es la de por defecto (la primera).
		if (Application.Variant != EEffectVariant::None && !Def.Variants.Contains(Application.Variant))
		{
			return Reject(EWardenEventsError::VariantNotAllowed, FString::Printf(TEXT("%s: %s no admite esa variante."), *Context, *Code));
		}
		return FWardenEventsLoadResult();
	}

	/**
	 * Lee una aplicación del JSON. Los errores de forma son MalformedData; los de contenido, el suyo: efecto desconocido,
	 * dirección que no existe, cantidad que no es un entero o que el efecto no admite, variante que no existe o que no admite.
	 * La regla (dirección permitida, rango de la cantidad) la comprueba después ValidateApplication.
	 */
	FWardenEventsLoadResult ReadApplication(const TSharedPtr<FJsonValue>& Value, const FString& Context, FEffectApplication& Out)
	{
		const TSharedPtr<FJsonObject>* Object = nullptr;
		FString EffectName;
		if (!Value.IsValid() || !Value->TryGetObject(Object) || !Object || !(*Object)->TryGetStringField(TEXT("effect"), EffectName))
		{
			return Reject(EWardenEventsError::MalformedData, FString::Printf(TEXT("%s: no es un objeto con \"effect\" de texto."), *Context));
		}
		if (!Soulbound::Rules::ParseEffectCode(EffectName, Out.Effect))
		{
			return Reject(EWardenEventsError::UnknownEffect, FString::Printf(TEXT("R-EFE-0: %s: \"%s\" no es un efecto del catálogo."), *Context, *EffectName));
		}
		const FEffectDef& Def = Soulbound::Rules::EffectDefOf(Out.Effect);

		// Sin "target", sin dirección: solo vale en el Sello (ValidateApplication lo comprueba).
		Out.Target = EEffectTarget::None;
		if (const TSharedPtr<FJsonValue> Target = (*Object)->TryGetField(TEXT("target")))
		{
			FString TargetName;
			if (!Target->TryGetString(TargetName) || !Soulbound::Rules::ParseEffectTargetCode(TargetName, Out.Target))
			{
				return Reject(EWardenEventsError::TargetNotAllowed, FString::Printf(TEXT("R-DIR-1: %s: la dirección no es una de las ocho."), *Context));
			}
		}

		Out.Amount = 1;
		if (const TSharedPtr<FJsonValue> Amount = (*Object)->TryGetField(TEXT("amount")))
		{
			if (!Def.bTakesAmount || !Soulbound::JsonFields::ReadIntegerValue(Amount, Out.Amount))
			{
				return Reject(EWardenEventsError::InvalidAmount, FString::Printf(TEXT("%s: %s no lleva \"amount\", o no es un entero."), *Context, *EffectName));
			}
		}

		Out.Variant = Def.Variants.IsEmpty() ? EEffectVariant::None : Def.Variants[0];
		if (const TSharedPtr<FJsonValue> Variant = (*Object)->TryGetField(TEXT("variant")))
		{
			FString VariantName;
			if (!Variant->TryGetString(VariantName) || !Soulbound::Rules::ParseEffectVariantCode(VariantName, Out.Variant) || !Def.Variants.Contains(Out.Variant))
			{
				return Reject(EWardenEventsError::VariantNotAllowed, FString::Printf(TEXT("%s: %s no admite esa \"variant\"."), *Context, *EffectName));
			}
		}
		return FWardenEventsLoadResult();
	}

	FWardenEventsLoadResult BagError(const FString& Id, const FString& What)
	{
		return Reject(EWardenEventsError::InvalidBag, FString::Printf(TEXT("R-SAC-2: evento \"%s\": %s."), *Id, *What));
	}

	/**
	 * Lee el saco de un evento (S-016, R-SAC-2), todo opcional salvo el canal de un evento con acto: `name`, `act` ("I", "II" o
	 * "III"), `channel` ("strike" o "scene"), `weights` (tres números), `maxUses` (entero de 1 en adelante), `window` (`from` y
	 * `to`, segundos) y `cutsTurn`. Por defecto, peso 1 en su acto y los siguientes, sin límite de usos, sin ventana y el corte
	 * del canal (golpe sí, escena no). La regla (pesos no negativos, ventana no vacía) la comprueba ValidateWardenEvents.
	 */
	FWardenEventsLoadResult ReadBag(const FJsonObject& Object, FWardenEventDef& Out)
	{
		Object.TryGetStringField(TEXT("name"), Out.Name);
		if (const TSharedPtr<FJsonValue> Act = Object.TryGetField(TEXT("act")))
		{
			const TCHAR* const Acts[] = {TEXT("I"), TEXT("II"), TEXT("III")};
			FString Name;
			int32 Found = INDEX_NONE;
			if (Act->TryGetString(Name))
			{
				for (int32 Candidate = 0; Candidate < UE_ARRAY_COUNT(Acts); ++Candidate)
				{
					Found = Name.Equals(Acts[Candidate], ESearchCase::CaseSensitive) ? Candidate : Found;
				}
			}
			if (Found == INDEX_NONE)
			{
				return BagError(Out.Id, TEXT("\"act\" debe ser \"I\", \"II\" o \"III\""));
			}
			Out.bInBag = true;
			Out.Act = static_cast<EAct>(Found);
		}
		FString Channel;
		if (Object.TryGetStringField(TEXT("channel"), Channel))
		{
			if (Channel != TEXT("strike") && Channel != TEXT("scene"))
			{
				return BagError(Out.Id, TEXT("\"channel\" debe ser \"strike\" o \"scene\" (el comentario no lleva evento)"));
			}
			Out.Channel = Channel == TEXT("scene") ? EWardenChannel::Scene : EWardenChannel::Strike;
		}
		else if (Out.bInBag)
		{
			return BagError(Out.Id, TEXT("un evento con acto necesita \"channel\""));
		}
		if (const TSharedPtr<FJsonValue> Weights = Object.TryGetField(TEXT("weights")))
		{
			const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
			if (!Weights->TryGetArray(List) || !List)
			{
				return BagError(Out.Id, TEXT("\"weights\" debe ser una lista de tres números"));
			}
			for (const TSharedPtr<FJsonValue>& Weight : *List)
			{
				double Number = 0.0;
				if (!Soulbound::JsonFields::ReadNumberValue(Weight, Number))
				{
					return BagError(Out.Id, TEXT("\"weights\" debe ser una lista de tres números"));
				}
				Out.Weights.Add(Number);
			}
		}
		else if (Out.bInBag)
		{
			// Lo que dice la story donde el GDD no dice nada: peso 1 en su acto y en los siguientes.
			for (int32 Act = 0; Act < Soulbound::Warden::ActCount; ++Act)
			{
				Out.Weights.Add(Act >= static_cast<int32>(Out.Act) ? 1.0 : 0.0);
			}
		}
		if (const TSharedPtr<FJsonValue> MaxUses = Object.TryGetField(TEXT("maxUses")))
		{
			if (!Soulbound::JsonFields::ReadIntegerValue(MaxUses, Out.MaxUses) || Out.MaxUses < 1)
			{
				return BagError(Out.Id, TEXT("\"maxUses\" debe ser un entero de 1 en adelante (sin campo, sin límite)"));
			}
		}
		if (const TSharedPtr<FJsonValue> Window = Object.TryGetField(TEXT("window")))
		{
			const TSharedPtr<FJsonObject>* Bounds = nullptr;
			if (!Window->TryGetObject(Bounds) || !Bounds)
			{
				return BagError(Out.Id, TEXT("\"window\" debe ser un objeto con \"from\" y \"to\""));
			}
			const TSharedPtr<FJsonValue> From = (*Bounds)->TryGetField(TEXT("from"));
			const TSharedPtr<FJsonValue> To = (*Bounds)->TryGetField(TEXT("to"));
			if ((From && !Soulbound::JsonFields::ReadNumberValue(From, Out.WindowFromSeconds)) || (To && (!Soulbound::JsonFields::ReadNumberValue(To, Out.WindowToSeconds) || Out.WindowToSeconds < 0.0)))
			{
				return BagError(Out.Id, TEXT("\"from\" y \"to\" de \"window\" son segundos, de 0 en adelante"));
			}
		}
		Out.bCutsTurn = Out.Channel == EWardenChannel::Strike;
		bool bCuts = false;
		if (Object.TryGetBoolField(TEXT("cutsTurn"), bCuts))
		{
			Out.bCutsTurn = bCuts;
		}
		return FWardenEventsLoadResult();
	}

	/** Lee un evento del JSON: id, texto, entrega, saco y opciones. */
	FWardenEventsLoadResult ReadEvent(const TSharedPtr<FJsonValue>& Value, int32 Index, FWardenEventDef& Out)
	{
		const TSharedPtr<FJsonObject>* Object = nullptr;
		if (!Value.IsValid() || !Value->TryGetObject(Object) || !Object || !(*Object)->TryGetStringField(TEXT("id"), Out.Id)
			|| !(*Object)->TryGetStringField(TEXT("text"), Out.Text))
		{
			return Reject(EWardenEventsError::MalformedData, FString::Printf(TEXT("El evento %d no es un objeto con \"id\" y \"text\" de texto."), Index + 1));
		}
		FString Delivery;
		if (!(*Object)->TryGetStringField(TEXT("delivery"), Delivery) || !(Delivery == TEXT("direct") || Delivery == TEXT("oracle") || Delivery == TEXT("any")))
		{
			return Reject(EWardenEventsError::MalformedData, FString::Printf(TEXT("Evento \"%s\": \"delivery\" debe ser \"direct\", \"oracle\" o \"any\"."), *Out.Id));
		}
		Out.Delivery = Delivery == TEXT("oracle") ? EEventDelivery::Oracle : Delivery == TEXT("any") ? EEventDelivery::Any : EEventDelivery::Direct;
		FWardenEventsLoadResult Bag = ReadBag(**Object, Out);
		if (!Bag.IsSuccess())
		{
			return Bag;
		}

		const TArray<TSharedPtr<FJsonValue>>* Options = nullptr;
		if (!(*Object)->TryGetArrayField(TEXT("options"), Options) || !Options)
		{
			return Reject(EWardenEventsError::MalformedData, FString::Printf(TEXT("Evento \"%s\": \"options\" falta o no es una lista."), *Out.Id));
		}
		for (int32 OptionIndex = 0; OptionIndex < Options->Num(); ++OptionIndex)
		{
			const TArray<TSharedPtr<FJsonValue>>* Applications = nullptr;
			if (!(*Options)[OptionIndex].IsValid() || !(*Options)[OptionIndex]->TryGetArray(Applications) || !Applications)
			{
				return Reject(EWardenEventsError::MalformedData, FString::Printf(TEXT("Evento \"%s\": la opción %d no es una lista de aplicaciones."), *Out.Id, OptionIndex + 1));
			}
			FWardenEventOption& Option = Out.Options.AddDefaulted_GetRef();
			for (int32 ApplicationIndex = 0; ApplicationIndex < Applications->Num(); ++ApplicationIndex)
			{
				FWardenEventsLoadResult Read = ReadApplication((*Applications)[ApplicationIndex], Where(Out.Id, OptionIndex, ApplicationIndex), Option.Applications.AddDefaulted_GetRef());
				if (!Read.IsSuccess())
				{
					return Read;
				}
			}
		}
		return FWardenEventsLoadResult();
	}
}

namespace Soulbound::Rules
{
	FWardenEventsLoadResult ValidateWardenEvents(const FWardenEventTable& Table)
	{
		TSet<FString> Ids;
		for (const FWardenEventDef& Event : Table.Events)
		{
			if (Event.Id.IsEmpty())
			{
				return Reject(EWardenEventsError::MalformedData, TEXT("Un evento sin identificador."));
			}
			bool bRepeated = false;
			Ids.Add(Event.Id, &bRepeated);
			if (bRepeated)
			{
				return Reject(EWardenEventsError::DuplicateId, FString::Printf(TEXT("El identificador \"%s\" está repetido."), *Event.Id));
			}
			// R-SAC-2 (S-016): un saco con canal de evento, tres pesos no negativos y usos no negativos (0, sin límite); una
			// ventana que no empieza antes de la partida ni acaba antes de empezar.
			if (Event.bInBag)
			{
				const bool bNegativeWeight = Event.Weights.ContainsByPredicate([](double Weight) { return Weight < 0.0; });
				if (Event.Channel == EWardenChannel::Remark || Event.Weights.Num() != Soulbound::Warden::ActCount || bNegativeWeight || Event.MaxUses < 0)
				{
					return Reject(EWardenEventsError::InvalidBag, FString::Printf(TEXT("R-SAC-2: evento \"%s\": canal golpe o escena, %d pesos no negativos y usos no negativos."),
						*Event.Id, Soulbound::Warden::ActCount));
				}
			}
			if (Event.WindowFromSeconds < 0.0 || (Event.WindowToSeconds >= 0.0 && Event.WindowToSeconds < Event.WindowFromSeconds))
			{
				return Reject(EWardenEventsError::InvalidBag, FString::Printf(TEXT("R-SAC-2: evento \"%s\": la ventana va de %.0f a %.0f s."), *Event.Id, Event.WindowFromSeconds, Event.WindowToSeconds));
			}
			// R-COM-1: de una a cuatro opciones, cada una con al menos una aplicación.
			if (Event.Options.Num() < 1 || Event.Options.Num() > Soulbound::Match::MaxEventOptions)
			{
				return Reject(EWardenEventsError::InvalidOptionCount, FString::Printf(TEXT("R-COM-1: evento \"%s\": debe tener de 1 a %d opciones (tiene %d)."),
					*Event.Id, Soulbound::Match::MaxEventOptions, Event.Options.Num()));
			}
			for (int32 OptionIndex = 0; OptionIndex < Event.Options.Num(); ++OptionIndex)
			{
				const TArray<FEffectApplication>& Applications = Event.Options[OptionIndex].Applications;
				if (Applications.IsEmpty())
				{
					return Reject(EWardenEventsError::InvalidOptionCount, FString::Printf(TEXT("R-COM-1: evento \"%s\": la opción %d no tiene aplicaciones."), *Event.Id, OptionIndex + 1));
				}
				for (int32 ApplicationIndex = 0; ApplicationIndex < Applications.Num(); ++ApplicationIndex)
				{
					FWardenEventsLoadResult Check = ValidateApplication(Applications[ApplicationIndex], Where(Event.Id, OptionIndex, ApplicationIndex));
					if (!Check.IsSuccess())
					{
						return Check;
					}
				}
			}
			// R-ORA-1 (S-017): el Oráculo solo entrega decretos; un dilema nunca llega por él, ni con "oracle" ni con "any".
			if (Event.IsDilemma() && Event.Delivery != EEventDelivery::Direct)
			{
				return Reject(EWardenEventsError::OracleDilemma, FString::Printf(TEXT("R-ORA-1: evento \"%s\": un dilema solo se entrega directo; el Oráculo entrega decretos."), *Event.Id));
			}
		}
		return FWardenEventsLoadResult();
	}

	FWardenEventsLoadResult LoadWardenEventsFromJsonString(const FString& Json)
	{
		TSharedPtr<FJsonObject> Root;
		const TArray<TSharedPtr<FJsonValue>>* Events = nullptr;
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root.IsValid() || !Root->TryGetArrayField(TEXT("events"), Events) || !Events)
		{
			return Reject(EWardenEventsError::MalformedData, TEXT("El texto no es un objeto JSON con la lista \"events\"."));
		}
		// Se leen enteros o se rechazan enteros: el primer evento que falla devuelve su motivo.
		FWardenEventTable Table;
		for (int32 Index = 0; Index < Events->Num(); ++Index)
		{
			FWardenEventsLoadResult Read = ReadEvent((*Events)[Index], Index, Table.Events.AddDefaulted_GetRef());
			if (!Read.IsSuccess())
			{
				return Read;
			}
		}
		FWardenEventsLoadResult Result = ValidateWardenEvents(Table);
		if (Result.IsSuccess())
		{
			Result.Table = MoveTemp(Table);
		}
		return Result;
	}

	FWardenEventsLoadResult LoadWardenEventsFromFile(const FString& FilePath)
	{
		FString Json;
		if (!FFileHelper::LoadFileToString(Json, *FilePath))
		{
			return Reject(EWardenEventsError::FileNotFound, FString::Printf(TEXT("No se pudo leer el archivo de eventos del Warden: %s"), *FilePath));
		}
		return LoadWardenEventsFromJsonString(Json);
	}
}
