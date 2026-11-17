#include "BoardViewState.h"

#include "Match/MatchState.h"
#include "Misc/PackageName.h"
#include "Warden/WardenLines.h"

FBoardViewState Soulbound::Presentation::ComputeBoardViewState(const FMatchState& State, bool bNicheThroneConfigured)
{
	FBoardViewState View;
	const FBoardState& Board = State.Board;
	View.Tiles.SetNum(Board.Tiles.Num());
	for (const FTileState& Tile : Board.Tiles)
	{
		FTileViewState& Shown = View.Tiles[Tile.TileId];
		Shown.Structure = StructureFor(Tile);
		// R-TAB-8: antes del Despertar, el Nodo no enseña su efecto; tampoco su estructura.
		if (Tile.Type == ETileType::PowerNode && !Board.AreNodesRevealed())
		{
			Shown.Structure = EBoardStructure::None;
		}
		if (Shown.Structure == EBoardStructure::NicheThrone && !bNicheThroneConfigured)
		{
			Shown.Structure = EBoardStructure::None;
		}
		// La reserva es 0 hasta el Despertar (R-RUN-4): antes no hay runa flotante.
		Shown.bFloatingRune = Tile.Type == ETileType::PowerSource && Tile.RuneReserve > 0;
		Shown.bSealChains = Tile.bSealed;
	}
	View.TokensOnBoard.Reserve(State.Seats.Num());
	for (const FSeatState& Seat : State.Seats)
	{
		View.TokensOnBoard.Add(!Seat.IsInVoid());
	}
	return View;
}

FLinearColor Soulbound::Presentation::WedgeFloorTint(const FLinearColor& DomainColor, bool bForgotten, float Strength, float ForgottenDim)
{
	const float Mix = FMath::Clamp(Strength, 0.f, 1.f);
	const FLinearColor Mixed = FLinearColor::White + (DomainColor - FLinearColor::White) * Mix;
	if (!bForgotten)
	{
		return Mixed;
	}
	return FLinearColor(Mixed.R * ForgottenDim, Mixed.G * ForgottenDim, Mixed.B * ForgottenDim, Mixed.A);
}

float Soulbound::Presentation::WedgeFloorDim(bool bForgotten, bool bForgottenMaterial, float ForgottenFloorDim, float ForgottenDim)
{
	// R-VIS-27: el material propio ya dice "Olvidado" con su textura; se apaga menos. Sin él, como antes de S-040.
	if (!bForgotten)
	{
		return 1.f;
	}
	return bForgottenMaterial ? ForgottenFloorDim : ForgottenDim;
}

FString Soulbound::Presentation::MenuMapPackage(const FString& ReturnToMenuMap, bool bReturnMapExists, const FString& GameDefaultMap)
{
	// R-MEN-1: el menú principal si está configurado y existe; si no, el de arranque, como en S-022.
	const FString& Map = !ReturnToMenuMap.IsEmpty() && bReturnMapExists ? ReturnToMenuMap : GameDefaultMap;
	return FPackageName::ObjectPathToPackageName(Map);
}

FString Soulbound::Presentation::ViewKeysText(bool bSeated, const FString& ViewKey, const FString& OverlayKey, const FString& GrimoireKey, const FString& ReturnToMenuKey,
	const FString& DebugTextKey, const FString& DebugPanelKey)
{
	// S-045 (precisión de R-CAM): C, L, G e Intro; T y D, aparte, de depuración. Ni H ni P, que se fueron con la columna (R-HUD-16).
	return FString::Printf(TEXT("vista %s (%s: %s) | %s etiquetas | %s Grimorio | %s volver al menú al acabar | depuración: %s panel de estado, %s panel de depuración"),
		bSeated ? TEXT("sentada") : TEXT("cenital"), *SpanishKeyName(ViewKey), bSeated ? TEXT("cenital") : TEXT("sentada"), *SpanishKeyName(OverlayKey),
		*SpanishKeyName(GrimoireKey), *SpanishKeyName(ReturnToMenuKey), *SpanishKeyName(DebugTextKey), *SpanishKeyName(DebugPanelKey));
}

Soulbound::Presentation::FTopCameraFraming Soulbound::Presentation::TopCameraFraming(const FVector2D& BoardHalf, const FVector2D& Viewport, float CameraMargin)
{
	FTopCameraFraming Framing;
	if (Viewport.X < 1.0 || Viewport.Y < 1.0)
	{
		return Framing;
	}
	// Con FOV horizontal de 60 grados, a la altura H se ve media anchura H*tan(30); la media altura es esa entre el aspecto. Hasta S-045 la cámara
	// dejaba a la derecha el hueco de la columna y se corría hacia +Y; sin columna (R-HUD-16), el tablero usa todo el ancho y la cámara va centrada.
	constexpr float Fov = 60.f;
	const float Aspect = static_cast<float>(Viewport.X / Viewport.Y);
	const float HalfTan = FMath::Tan(FMath::DegreesToRadians(Fov * 0.5f));
	Framing.Height = CameraMargin * FMath::Max(static_cast<float>(BoardHalf.Y) / HalfTan, static_cast<float>(BoardHalf.X) * Aspect / HalfTan);
	Framing.ShiftY = 0.f;
	return Framing;
}

FString Soulbound::Presentation::MatchResultText(const FMatchState& State)
{
	if (!State.IsMatchOver())
	{
		return FString();
	}
	return State.Outcome == EMatchOutcome::PlayerVictory ? FString::Printf(TEXT("GANA EL ASIENTO %d"), State.WinnerSeat)
		: State.EndReason == EMatchEndReason::TimeOut ? FString(TEXT("GANA EL WARDEN: SE ACABÓ EL TIEMPO")) : FString(TEXT("GANA EL WARDEN: NADIE SE VINCULÓ"));
}

FString Soulbound::Presentation::ReturnToMenuHint(const FString& KeyDisplayName)
{
	return KeyHintText(KeyDisplayName);
}

FString Soulbound::Presentation::KeyHintText(const FString& KeyDisplayName)
{
	if (KeyDisplayName.IsEmpty())
	{
		return FString();
	}
	// Unreal llama "Enter" a la tecla Intro; el texto del juego va en español (SpanishKeyName, desde S-044).
	return TEXT("o pulsa ") + SpanishKeyName(KeyDisplayName);
}

FString Soulbound::Presentation::SpanishKeyName(const FString& KeyDisplayName)
{
	// S-044 (corrección del autor del 17/11/2026): los nombres de Unreal de las teclas que no son letras, números ni de función, en español.
	static const TMap<FString, FString> Names{
		{TEXT("Enter"), TEXT("Intro")},
		{TEXT("Left"), TEXT("flecha izquierda")},
		{TEXT("Right"), TEXT("flecha derecha")},
		{TEXT("Up"), TEXT("flecha arriba")},
		{TEXT("Down"), TEXT("flecha abajo")},
		{TEXT("Space Bar"), TEXT("Espacio")},
		{TEXT("Tab"), TEXT("Tabulador")},
		{TEXT("Backspace"), TEXT("Retroceso")},
		{TEXT("Delete"), TEXT("Supr")},
		{TEXT("Insert"), TEXT("Insert")},
		{TEXT("Home"), TEXT("Inicio")},
		{TEXT("End"), TEXT("Fin")},
		{TEXT("Page Up"), TEXT("Re Pág")},
		{TEXT("Page Down"), TEXT("Av Pág")},
		{TEXT("Escape"), TEXT("Esc")},
	};
	const FString* Spanish = Names.Find(KeyDisplayName);
	return Spanish ? *Spanish : KeyDisplayName;
}

TArray<bool> Soulbound::Presentation::ForgottenDomainsShown(const FMatchState& State, bool bInPrologue, int32 OpeningLinesShown)
{
	TArray<bool> Shown;
	Shown.Init(false, State.Board.Domains.Num());
	if (!bInPrologue)
	{
		for (int32 Domain = 0; Domain < Shown.Num(); ++Domain)
		{
			Shown[Domain] = State.Board.Domains[Domain].State == EDomainState::Forgotten;
		}
		return Shown;
	}
	// En el prólogo, la frase de apertura número n (en el orden del registro, las del instante cero) se ha enseñado si n < OpeningLinesShown;
	// la de corrupción apaga el Dominio del DomainForgotten que la precede.
	int32 Opening = 0;
	for (int32 Index = 0; Index < State.EventLog.Num() && Opening < OpeningLinesShown; ++Index)
	{
		const FMatchEvent& Event = State.EventLog[Index];
		if (Event.Type != EMatchEventType::WardenSpoke || Event.ClockSeconds != 0.0)
		{
			continue;
		}
		++Opening;
		const FMatchEvent* Previous = Index > 0 ? &State.EventLog[Index - 1] : nullptr;
		if (Previous && Previous->Type == EMatchEventType::DomainForgotten && Shown.IsValidIndex(Previous->Value)
			&& State.WardenLines.Entries.IsValidIndex(Event.Value) && State.WardenLines.Entries[Event.Value].Id == Soulbound::Warden::LineCorruption)
		{
			Shown[Previous->Value] = true;
		}
	}
	return Shown;
}

TArray<int32> Soulbound::Presentation::SealMarkedTiles(const FMatchState& State)
{
	// R-VIS-24: la vista no decide nada, lee el orden de los Sellos tal como lo guarda el tablero.
	return State.Board.SealedTiles;
}

bool Soulbound::Presentation::EchoNightDimShown(const FMatchState& State)
{
	// R-VIS-31: con la partida terminada no hay penumbra, aunque la Noche siguiera activa.
	return State.IsEchoNight() && !State.IsMatchOver();
}

float Soulbound::Presentation::EchoNightDimOpacity(bool bEchoNight, float FromOpacity, double SecondsSinceChange, float MaxOpacity, double FadeSeconds)
{
	// R-VIS-31: una transición corta a velocidad constante, desde donde estaba al cambiar: de cero a la configurada (o al revés) en FadeSeconds;
	// si cambia a medias, tarda solo lo que le queda.
	if (FadeSeconds <= 0.0)
	{
		return bEchoNight ? MaxOpacity : 0.f;
	}
	const float Step = static_cast<float>(MaxOpacity * FMath::Max(SecondsSinceChange, 0.0) / FadeSeconds);
	return bEchoNight ? FMath::Min(FromOpacity + Step, MaxOpacity) : FMath::Max(FromOpacity - Step, 0.f);
}

FLinearColor Soulbound::Presentation::BoardDimColor(const FLinearColor& NightColor, float NightOpacity, float CutOpacity)
{
	// R-VIS-31: el más oscuro de los dos, no la suma; el pulso del corte gana los empates, que es el aviso más urgente.
	if (CutOpacity >= NightOpacity)
	{
		return FLinearColor(0.f, 0.f, 0.f, CutOpacity);
	}
	return FLinearColor(NightColor.R, NightColor.G, NightColor.B, NightOpacity);
}

Soulbound::Presentation::ETokenArrival Soulbound::Presentation::TokenArrival(int32 StateTile, int32 TokenTile, const TArray<int32>& RemainingWalk)
{
	if (StateTile == TokenTile)
	{
		return ETokenArrival::Stay;
	}
	return !RemainingWalk.IsEmpty() && RemainingWalk[0] == StateTile ? ETokenArrival::Walk : ETokenArrival::Slide;
}
