#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

// Cabecera privada del motor: lectura estricta de números JSON, compartida por los cargadores de datos (tablero,
// partida y Elegidos). Un texto "30" no es un número. No es API del módulo.
namespace Soulbound::JsonFields
{
	/** Lee un número de un valor JSON. Devuelve false si falta o no es un número JSON. */
	inline bool ReadNumberValue(const TSharedPtr<FJsonValue>& Value, double& Out)
	{
		return Value.IsValid() && Value->Type == EJson::Number && Value->TryGetNumber(Out);
	}

	/** Lee el campo numérico Field de Object. Devuelve false si falta o no es un número JSON. */
	inline bool ReadNumberField(const FJsonObject& Object, const TCHAR* Field, double& Out)
	{
		return ReadNumberValue(Object.TryGetField(Field), Out);
	}

	/** Lee un entero de un valor JSON. Devuelve false si falta, no es un número, tiene decimales o no cabe en int32. */
	inline bool ReadIntegerValue(const TSharedPtr<FJsonValue>& Value, int32& Out)
	{
		double Number = 0.0;
		if (!ReadNumberValue(Value, Number) || Number != FMath::FloorToDouble(Number) || Number < MIN_int32 || Number > MAX_int32)
		{
			return false;
		}
		Out = static_cast<int32>(Number);
		return true;
	}

	/** Lee el campo entero Field de Object. Devuelve false si falta, no es un número, tiene decimales o no cabe en int32. */
	inline bool ReadIntegerField(const FJsonObject& Object, const TCHAR* Field, int32& Out)
	{
		return ReadIntegerValue(Object.TryGetField(Field), Out);
	}
}
