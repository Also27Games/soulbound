#pragma once

#include "CoreMinimal.h"

/**
 * La cara de un dado en el panel de decisión (S-039, R-DEC-15): dónde van los puntos si se dibuja por código y qué trozo de la imagen de
 * caras corresponde a un valor si hay imagen. Funciones puras, sin actor ni mundo, con tests.
 */
namespace Soulbound::Presentation
{
	/** El diámetro de un punto de la cara por el lado de la cara (R-DEC-15). Las proporciones, no números sueltos en píxeles. */
	inline constexpr float DicePipDiameterRatio = 0.13f;

	/** El radio de las esquinas de la cara dibujada por código, por el lado de la cara. */
	inline constexpr float DiceFaceCornerRatio = 0.15f;

	/**
	 * Dónde van los puntos de la cara de un dado dibujada por código (R-DEC-15), en coordenadas de la cara de 0 a 1 en cada eje, con el origen
	 * arriba a la izquierda, como las pintan los dados de siempre: 1 al centro, 2 en una diagonal, 3 en la diagonal con el del centro, 4 en las
	 * esquinas, 5 las esquinas y el centro, 6 dos columnas de tres. Todos dentro de la cara y sin repetir posición.
	 * @param Value El valor del dado, de 1 a Dice::Faces.
	 * @return Tantos puntos como el valor; ninguno si el valor no está entre 1 y 6.
	 */
	SOULBOUND_API TArray<FVector2f> DiceFacePips(int32 Value);

	/**
	 * El trozo de la imagen de caras que corresponde a un valor (R-DEC-15): la imagen lleva las seis caras en una fila, del 1 al 6 de izquierda
	 * a derecha, y cada una ocupa un sexto del ancho y todo el alto. Coordenadas UV de 0 a 1.
	 * @param Value El valor del dado, de 1 a Dice::Faces.
	 * @return El rectángulo UV de la cara (la 1 empieza en 0, la 6 acaba en 1); un rectángulo sin validar (IsValid falso) si el valor no está entre 1 y 6.
	 */
	SOULBOUND_API FBox2f DiceFaceUV(int32 Value);
}
