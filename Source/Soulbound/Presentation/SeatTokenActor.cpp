#include "SeatTokenActor.h"

#include "PresentationShapes.h"
#include "TokenLook.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	/** Segmentos de un aro: cubos finos en círculo (las formas básicas del motor no traen un anillo). */
	constexpr int32 RingSegmentCount = 20;
	/** Alto de los segmentos de un aro y altura a la que apoyan sobre la losa, en cm. */
	constexpr float RingHeight = 0.18f;
	constexpr float RingZ = 0.06f;
	/** Grosor del disco de rol, en cm. */
	constexpr float RoleDiscThickness = 0.1f;
	/** Holgura del número del asiento sobre lo alto de la ficha, en cm. */
	constexpr float LabelClearance = 0.15f;
}

ASeatTokenActor::ASeatTokenActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	// La raíz es la base de la ficha, sin escala: la vista la coloca sobre la losa; el giro y la elevación son del pivote.
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	Pivot = CreateDefaultSubobject<USceneComponent>(TEXT("Pivot"));
	Pivot->SetupAttachment(GetRootComponent());
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Pivot);
	RingRoot = CreateDefaultSubobject<USceneComponent>(TEXT("RingRoot"));
	RingRoot->SetupAttachment(GetRootComponent());
	FlashRoot = CreateDefaultSubobject<USceneComponent>(TEXT("FlashRoot"));
	FlashRoot->SetupAttachment(GetRootComponent());
	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(Pivot); // gira con la ficha: el número girando es lo que se ve desde arriba

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(Soulbound::Presentation::SpherePath);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cone(Soulbound::Presentation::ConePath);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(Soulbound::Presentation::CubePath);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(Soulbound::Presentation::CylinderPath);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Material(Soulbound::Presentation::BasicMaterialPath);
	EchoMesh = Sphere.Succeeded() ? Sphere.Object : nullptr;
	ElectMesh = Cone.Succeeded() ? Cone.Object : nullptr;
	CubeMesh = Cube.Succeeded() ? Cube.Object : nullptr;
	CylinderMesh = Cylinder.Succeeded() ? Cylinder.Object : nullptr;
	Mesh->SetStaticMesh(EchoMesh);
	BaseMaterial = Material.Succeeded() ? Material.Object : nullptr;

	// S-017: la esfera del clic para votar, invisible y sin colisión hasta que haya una votación (SetClickable).
	ClickTarget = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ClickTarget"));
	ClickTarget->SetupAttachment(GetRootComponent());
	ClickTarget->SetStaticMesh(EchoMesh);
	ClickTarget->SetVisibility(false);
	ClickTarget->SetCastShadow(false);
	ClickTarget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ClickTarget->SetCollisionResponseToAllChannels(ECR_Ignore);
	ClickTarget->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	// S-029: el disco de rol, bajo la ficha y colgado de la raíz (no sube ni gira con ella); lo mide y lo coloca SetRoleDisc.
	RoleDisc = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RoleDisc"));
	RoleDisc->SetupAttachment(GetRootComponent());
	RoleDisc->SetStaticMesh(CylinderMesh);
	RoleDisc->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RoleDisc->SetCastShadow(false);

	// La ficha no recibe clics ni choca con nada: el clic va a la casilla que hay debajo.
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);

	// Número del asiento, plano sobre la ficha y legible desde la cámara cenital (el texto del motor se ve desde +X).
	Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetVerticalAlignment(EVRTA_TextCenter);
	Label->SetRelativeRotation(FRotator(90.f, 180.f, 0.f));
	Label->SetCastShadow(false);
}

void ASeatTokenActor::Configure(float InLabelSize, float InLift, float InSpinDegreesPerSecond, float InRingDiameter, float InRingWidth, float InActiveScale, float InTokenDiameter)
{
	Lift = InLift;
	ActiveScale = InActiveScale;
	SpinDegreesPerSecond = InSpinDegreesPerSecond;
	RingDiameter = InRingDiameter;
	RingWidth = InRingWidth;
	TokenDiameter = InTokenDiameter;
	Footprint = InTokenDiameter;
	// Las formas básicas del motor miden 100 cm y tienen el origen en su centro: la ficha se escala y se posa en la base.
	Mesh->SetRelativeScale3D(FVector(TokenDiameter / 100.f));
	Mesh->SetRelativeLocation(FVector(0.f, 0.f, TokenDiameter * 0.5f));
	Label->SetWorldSize(InLabelSize);
	Label->SetRelativeLocation(FVector(0.f, 0.f, TokenDiameter + LabelClearance));
	BuildRing(RingRoot, RingSegments, RingDiameter, RingWidth, RingZ);
	// La esfera del clic, algo mayor que la ficha para acertar con el ratón; con la carta, la vuelve a medir UseCard.
	ClickTarget->SetRelativeScale3D(FVector(TokenDiameter * 1.5f / 100.f));
	ClickTarget->SetRelativeLocation(FVector(0.f, 0.f, TokenDiameter * 0.5f));
	SetActive(false);
}

void ASeatTokenActor::ConfigureFlash(float DiameterScale, float Width, float Growth)
{
	FlashGrowth = Growth;
	// El aro del destello, mayor y más grueso que el del turno para que se vea también sobre él; oculto hasta que haya un destello. Se arma de una vez:
	// con otra llamada, los segmentos viejos se quedarían.
	for (UStaticMeshComponent* Segment : FlashSegments)
	{
		if (Segment)
		{
			Segment->DestroyComponent();
		}
	}
	FlashSegments.Reset();
	BuildRing(FlashRoot, FlashSegments, RingDiameter * DiameterScale, Width, RingZ + 0.03f);
	FlashRoot->SetVisibility(false, true);
}

void ASeatTokenActor::SetClickable(bool bClickable)
{
	ClickTarget->SetCollisionEnabled(bClickable ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
}

UStaticMeshComponent* ASeatTokenActor::UseCard(UStaticMesh* Card)
{
	if (!Card)
	{
		return nullptr;
	}
	bCard = true;
	// Las piezas del kit no se escalan: origen en la base, un metro del kit son 100 cm.
	Mesh->SetStaticMesh(Card);
	Mesh->SetRelativeScale3D(FVector::OneVector);
	Mesh->SetRelativeLocation(FVector::ZeroVector);
	const FBoxSphereBounds Bounds = Card->GetBounds();
	Footprint = 2.f * FMath::Max(Bounds.BoxExtent.X, Bounds.BoxExtent.Y);
	// S-017: la esfera del clic, del alto de la carta.
	ClickTarget->SetRelativeScale3D(FVector(2.f * FMath::Max(Bounds.BoxExtent.Z, Bounds.BoxExtent.X) / 100.f));
	ClickTarget->SetRelativeLocation(FVector(0.f, 0.f, Bounds.Origin.Z));
	Label->SetRelativeLocation(FVector(0.f, 0.f, Bounds.Origin.Z + Bounds.BoxExtent.Z + LabelClearance));
	// Sobre la carta oscura, el número en blanco.
	Label->SetTextRenderColor(FColor::White);
	PaintRoleDisc();
	return Mesh;
}

void ASeatTokenActor::SetRoleDisc(float Diameter, float DiscLift, float Brightness)
{
	RoleDiscBrightness = FMath::Clamp(Brightness, 0.f, 1.f);
	// El cilindro del motor mide 100 cm de diámetro y 100 de alto y tiene el origen en su centro: se aplasta a un disco y se sube su mitad.
	RoleDisc->SetRelativeScale3D(FVector(Diameter / 100.f, Diameter / 100.f, RoleDiscThickness / 100.f));
	RoleDisc->SetRelativeLocation(FVector(0.f, 0.f, DiscLift + RoleDiscThickness * 0.5f));
	PaintRoleDisc();
}

void ASeatTokenActor::BuildRing(USceneComponent* Parent, TArray<TObjectPtr<UStaticMeshComponent>>& OutSegments, float Diameter, float Width, float Z)
{
	const float Radius = Diameter * 0.5f;
	const float SegmentLength = 2.f * PI * Radius / RingSegmentCount * 1.15f;
	for (int32 Index = 0; Index < RingSegmentCount; ++Index)
	{
		UStaticMeshComponent* Segment = NewObject<UStaticMeshComponent>(this);
		Segment->SetStaticMesh(CubeMesh);
		Segment->SetupAttachment(Parent);
		Segment->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Segment->SetCastShadow(false);
		const float Angle = 2.f * PI * Index / RingSegmentCount;
		// El aro apoya en la losa, a la altura de la base de la ficha.
		Segment->SetRelativeLocation(FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, Z));
		Segment->SetRelativeRotation(FRotator(0.f, FMath::RadiansToDegrees(Angle) + 90.f, 0.f));
		Segment->SetRelativeScale3D(FVector(SegmentLength, Width, RingHeight) / 100.f);
		Segment->RegisterComponent();
		OutSegments.Add(Segment);
	}
}

void ASeatTokenActor::PaintRoleDisc()
{
	// S-029 (R-VIS-10, corrección del autor del 13/11/2026): el rol se ve en el disco plano bajo la ficha, gris de datos para un Eco y el color del Dominio
	// para un Elegido (el aro fino de la base no se distinguía desde la cámara). La carta del kit no se tiñe; sin carta, la ficha toma el color. El
	// destello de la ficha enciende el disco.
	FLinearColor Base = RoleColor;
	FLinearColor FlashColor;
	float Intensity = 0.f;
	float Progress = 0.f;
	bool bInward = true;
	if (CurrentFlash(GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0, FlashColor, Intensity, Progress, bInward))
	{
		Base = FMath::Lerp(RoleColor, FlashColor, Intensity);
	}
	// El material básico (BaseMaterial), el mismo del aro del turno y de los halos: tiene el parámetro "Color". El emisivo del kit (M_Emisivo) no lo tiene
	// (es vértice por multiplicación, sin parámetro) y el disco salía siempre blanco.
	Soulbound::Presentation::SetComponentColor(*RoleDisc, BaseMaterial,
		FLinearColor(Base.R * RoleDiscBrightness, Base.G * RoleDiscBrightness, Base.B * RoleDiscBrightness));
	if (!bCard)
	{
		Soulbound::Presentation::SetComponentColor(*Mesh, BaseMaterial, Base);
	}
}

void ASeatTokenActor::PaintTurnRing(float Intensity)
{
	const FLinearColor Color(RingColor.R * Intensity, RingColor.G * Intensity, RingColor.B * Intensity);
	for (UStaticMeshComponent* Segment : RingSegments)
	{
		Soulbound::Presentation::SetComponentColor(*Segment, BaseMaterial, Color);
	}
}

bool ASeatTokenActor::CurrentFlash(double Now, FLinearColor& OutColor, float& OutIntensity, float& OutProgress, bool& bOutInward)
{
	FlashSlots.RemoveAll([Now](const FFlashSlot& Slot) { return Slot.End <= Now; });
	for (const FFlashSlot& Slot : FlashSlots)
	{
		if (Slot.Start <= Now)
		{
			OutColor = Slot.Color;
			bOutInward = Slot.bInward;
			OutProgress = static_cast<float>((Now - Slot.Start) / FMath::Max(Slot.End - Slot.Start, static_cast<double>(KINDA_SMALL_NUMBER)));
			// Subida y bajada suaves: seno de medio periodo, de 0 a 1 y a 0.
			OutIntensity = static_cast<float>(FMath::Sin(UE_DOUBLE_PI * (Now - Slot.Start) / FMath::Max(Slot.End - Slot.Start, KINDA_SMALL_NUMBER)));
			return true;
		}
	}
	return false;
}

void ASeatTokenActor::Flash(const FLinearColor& Color, float Seconds, float DelaySeconds, bool bInward)
{
	if (!GetWorld() || Seconds <= 0.f)
	{
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	FFlashSlot Slot;
	Slot.Start = FMath::Max(Now + FMath::Max(DelaySeconds, 0.f), FlashBusyUntil);
	Slot.End = Slot.Start + Seconds;
	Slot.Color = Color;
	Slot.bInward = bInward;
	FlashBusyUntil = Slot.End;
	FlashSlots.Add(Slot);
}

void ASeatTokenActor::SetRingPulse(float PeriodSeconds, float MinIntensity)
{
	RingPulsePeriod = PeriodSeconds;
	RingPulseMin = MinIntensity;
}

void ASeatTokenActor::SetRoleColor(const FLinearColor& Color)
{
	if (bRoleColorSet && RoleColor.Equals(Color, 0.0001f))
	{
		return;
	}
	bRoleColorSet = true;
	RoleColor = Color;
	PaintRoleDisc();
}

void ASeatTokenActor::SetColor(const FLinearColor& Color)
{
	RingColor = Color;
	if (!bRoleColorSet)
	{
		// Hasta que la vista diga el rol, la ficha lleva el color del asiento.
		RoleColor = Color;
	}
	if (!bCard)
	{
		Label->SetTextRenderColor(FColor::Black);
	}
	PaintTurnRing(1.f);
	PaintRoleDisc();
	Label->SetText(FText::AsNumber(Seat));
}

void ASeatTokenActor::SetElectShape(bool bInElect)
{
	bElect = bInElect;
	if (bCard)
	{
		return;
	}
	UStaticMesh* Wanted = bInElect ? ElectMesh : EchoMesh;
	// La instancia dinámica del material va en la ranura del componente, así que el color se conserva al cambiar de malla.
	if (Wanted && Mesh->GetStaticMesh() != Wanted)
	{
		Mesh->SetStaticMesh(Wanted);
	}
}

void ASeatTokenActor::SetLabelVisible(bool bVisible)
{
	Label->SetVisibility(bVisible);
}

void ASeatTokenActor::SetActive(bool bInActive)
{
	bActive = bInActive;
	RingRoot->SetVisibility(bInActive, true);
	if (!bInActive)
	{
		Pivot->SetRelativeRotation(FRotator::ZeroRotator);
	}
}

void ASeatTokenActor::MoveTo(const FVector& Location, float SlideSeconds)
{
	if (!bPlaced || SlideSeconds <= 0.f)
	{
		SetActorLocation(Location);
		SlideTarget = Location;
		SlideElapsed = SlideDuration = SlideSeconds;
		bPlaced = true;
		return;
	}
	// El mismo destino no reinicia el deslizamiento (la vista repinta a cada cambio del estado).
	if (Location.Equals(SlideTarget, 0.01))
	{
		return;
	}
	SlideStart = GetActorLocation();
	SlideTarget = Location;
	SlideDuration = SlideSeconds;
	SlideElapsed = 0.f;
}

void ASeatTokenActor::WalkPath(const TArray<int32>& Tiles, const TArray<FVector>& Points, float Speed)
{
	++WalkGeneration;
	WalkTiles = Tiles;
	WalkPoints = Points;
	WalkPoints.SetNum(WalkTiles.Num());
	WalkIndex = 0;
	WalkElapsed = 0.f;
	WalkSpeed = FMath::Max(Speed, KINDA_SMALL_NUMBER);
	WalkSegmentStart = GetActorLocation();
	// El recorrido manda: el deslizamiento que hubiera se da por terminado donde esté.
	SlideTarget = WalkPoints.IsEmpty() ? SlideTarget : WalkPoints.Last();
	SlideElapsed = SlideDuration;
	bPlaced = true;
}

TArray<int32> ASeatTokenActor::RemainingWalk() const
{
	// Los puntos de paso (casilla INDEX_NONE) no son casillas del recorrido.
	TArray<int32> Remaining;
	for (int32 Index = WalkIndex; Index < WalkTiles.Num(); ++Index)
	{
		if (WalkTiles[Index] != INDEX_NONE)
		{
			Remaining.Add(WalkTiles[Index]);
		}
	}
	return Remaining;
}

float ASeatTokenActor::WalkSegmentSeconds() const
{
	return IsWalking() ? FVector::Dist(WalkSegmentStart, WalkPoints[WalkIndex]) / WalkSpeed : 0.f;
}

void ASeatTokenActor::StopWalk()
{
	++WalkGeneration;
	WalkTiles.Reset();
	WalkPoints.Reset();
	WalkIndex = 0;
}

void ASeatTokenActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// S-020: el recorrido, tramo a tramo y a velocidad constante. Llegar a una casilla avisa, y el aviso puede cambiar o cortar
	// el recorrido (la vista entrega la intención, el motor avanza y la vista se refresca): entonces este Tick no sigue con el viejo.
	if (IsWalking())
	{
		WalkElapsed += DeltaSeconds;
		while (IsWalking() && WalkElapsed >= WalkSegmentSeconds())
		{
			WalkElapsed -= WalkSegmentSeconds();
			const int32 Generation = WalkGeneration;
			const int32 Tile = WalkTiles[WalkIndex];
			const bool bLast = WalkIndex == WalkTiles.Num() - 1;
			SetActorLocation(WalkPoints[WalkIndex]);
			WalkSegmentStart = WalkPoints[WalkIndex];
			++WalkIndex;
			if (bLast)
			{
				StopWalk();
			}
			if (Tile != INDEX_NONE)
			{
				OnTileEntered.ExecuteIfBound(Seat, Tile, bLast);
			}
			if (Generation != WalkGeneration && !bLast)
			{
				break;
			}
		}
		if (IsWalking())
		{
			SetActorLocation(FMath::Lerp(WalkSegmentStart, WalkPoints[WalkIndex], WalkElapsed / WalkSegmentSeconds()));
		}
	}
	else if (bPlaced && SlideElapsed < SlideDuration)
	{
		SlideElapsed = FMath::Min(SlideElapsed + DeltaSeconds, SlideDuration);
		SetActorLocation(FMath::Lerp(SlideStart, SlideTarget, SlideElapsed / SlideDuration));
	}
	// S-029: el pulso lento del aro del turno y el destello en curso (los dos solo repintan mientras cambian).
	if (bActive)
	{
		PaintTurnRing(Soulbound::Presentation::RingPulse(GetWorld()->GetTimeSeconds(), RingPulsePeriod, RingPulseMin));
	}
	{
		FLinearColor FlashColor;
		float Intensity = 0.f;
		float Progress = 0.f;
		bool bInward = true;
		const bool bFlashing = CurrentFlash(GetWorld()->GetTimeSeconds(), FlashColor, Intensity, Progress, bInward);
		if (bFlashing || bFlashShown)
		{
			bFlashShown = bFlashing;
			FlashRoot->SetVisibility(bFlashing, true);
			if (bFlashing)
			{
				// Hacia dentro (recibir) se cierra y hacia fuera (perder) se abre mientras dura (FlashScale): el aro entero, así también se hace más grueso.
				const float Grown = Soulbound::Presentation::FlashScale(Progress, FlashGrowth, bInward);
				FlashRoot->SetRelativeScale3D(FVector(Grown, Grown, 1.f));
				for (UStaticMeshComponent* Segment : FlashSegments)
				{
					Soulbound::Presentation::SetComponentColor(*Segment, BaseMaterial, FLinearColor(FlashColor.R * Intensity, FlashColor.G * Intensity, FlashColor.B * Intensity));
				}
			}
			PaintRoleDisc();
		}
	}
	// Sube y baja suave hacia la altura que toca, sin saltos cuando cambia el asiento activo.
	const float TargetLift = bActive ? Lift : 0.f;
	CurrentLift = FMath::FInterpTo(CurrentLift, TargetLift, DeltaSeconds, 8.f);
	Pivot->SetRelativeLocation(FVector(0.f, 0.f, CurrentLift));
	CurrentScale = FMath::FInterpTo(CurrentScale, bActive ? ActiveScale : 1.f, DeltaSeconds, 8.f);
	Pivot->SetRelativeScale3D(FVector(CurrentScale));
	if (IsWalking())
	{
		// R-VIS-6 (decisión del autor del 23/10/2026): mientras recorre su camino, la carta mira hacia donde va. Su frente es la
		// normal de la cara con el emblema, el -y local de SM_ficha_prueba (como el de las estructuras del kit): para mirar a una
		// dirección, el giro es su ángulo más 90°. Las formas de respaldo no tienen frente y no se orientan.
		const FVector2D Heading(WalkPoints[WalkIndex] - GetActorLocation());
		if (bCard && !Heading.IsNearlyZero(0.01))
		{
			const float TargetYaw = FMath::RadiansToDegrees(FMath::Atan2(Heading.Y, Heading.X)) + 90.f;
			FRotator Rotation = Pivot->GetRelativeRotation();
			Rotation.Yaw = FMath::FixedTurn(Rotation.Yaw, TargetYaw, TurnSpeedDegreesPerSecond * DeltaSeconds);
			Pivot->SetRelativeRotation(Rotation);
		}
	}
	else if (bActive)
	{
		// La ficha activa gira despacio, desde la orientación con la que llegó. Mientras recorre su camino no gira (decisión del
		// autor del 22/10/2026): la elevación y el aro se mantienen.
		Pivot->AddRelativeRotation(FRotator(0.f, SpinDegreesPerSecond * DeltaSeconds, 0.f));
	}
}
