#include "SeatTokenActor.h"

#include "PresentationShapes.h"
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
	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(Pivot); // gira con la ficha: el número girando es lo que se ve desde arriba

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(Soulbound::Presentation::SpherePath);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cone(Soulbound::Presentation::ConePath);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(Soulbound::Presentation::CubePath);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Material(Soulbound::Presentation::BasicMaterialPath);
	EchoMesh = Sphere.Succeeded() ? Sphere.Object : nullptr;
	ElectMesh = Cone.Succeeded() ? Cone.Object : nullptr;
	CubeMesh = Cube.Succeeded() ? Cube.Object : nullptr;
	Mesh->SetStaticMesh(EchoMesh);
	BaseMaterial = Material.Succeeded() ? Material.Object : nullptr;

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
	// Las formas básicas del motor miden 100 cm y tienen el origen en su centro: la ficha se escala y se posa en la base.
	Mesh->SetRelativeScale3D(FVector(TokenDiameter / 100.f));
	Mesh->SetRelativeLocation(FVector(0.f, 0.f, TokenDiameter * 0.5f));
	Label->SetWorldSize(InLabelSize);
	Label->SetRelativeLocation(FVector(0.f, 0.f, TokenDiameter + LabelClearance));
	BuildRing(RingRoot, RingSegments, RingDiameter, RingWidth, RingZ);
	SetActive(false);
}

UStaticMeshComponent* ASeatTokenActor::UseCard(UStaticMesh* Card, UMaterialInterface* InRoleRingMaterial, float RoleRingScale, float RoleRingWidth, float InEchoBrightness)
{
	if (!Card)
	{
		return nullptr;
	}
	bCard = true;
	EchoBrightness = InEchoBrightness;
	RoleRingMaterial = InRoleRingMaterial ? InRoleRingMaterial : BaseMaterial.Get();
	// Las piezas del kit no se escalan: origen en la base, un metro del kit son 100 cm.
	Mesh->SetStaticMesh(Card);
	Mesh->SetRelativeScale3D(FVector::OneVector);
	Mesh->SetRelativeLocation(FVector::ZeroVector);
	const FBoxSphereBounds Bounds = Card->GetBounds();
	const float Footprint = 2.f * FMath::Max(Bounds.BoxExtent.X, Bounds.BoxExtent.Y);
	Label->SetRelativeLocation(FVector(0.f, 0.f, Bounds.Origin.Z + Bounds.BoxExtent.Z + LabelClearance));
	// Sobre la carta oscura, el número en blanco.
	Label->SetTextRenderColor(FColor::White);
	BuildRing(Pivot, RoleRingSegments, Footprint * RoleRingScale, RoleRingWidth, RingZ);
	PaintRoleRing();
	return Mesh;
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

void ASeatTokenActor::PaintRoleRing()
{
	// S-019, decisión del autor del 20/10/2026: el rol se ve en el brillo del aro, Eco a medias y Elegido entero.
	const float Brightness = bElect ? 1.f : EchoBrightness;
	const FLinearColor Color(RingColor.R * Brightness, RingColor.G * Brightness, RingColor.B * Brightness);
	for (UStaticMeshComponent* Segment : RoleRingSegments)
	{
		Soulbound::Presentation::SetComponentColor(*Segment, RoleRingMaterial, Color);
	}
}

void ASeatTokenActor::SetColor(const FLinearColor& Color)
{
	RingColor = Color;
	if (!bCard)
	{
		Soulbound::Presentation::SetComponentColor(*Mesh, BaseMaterial, Color);
		Label->SetTextRenderColor(FColor::Black);
	}
	for (UStaticMeshComponent* Segment : RingSegments)
	{
		Soulbound::Presentation::SetComponentColor(*Segment, BaseMaterial, Color);
	}
	PaintRoleRing();
	Label->SetText(FText::AsNumber(Seat));
}

void ASeatTokenActor::SetElectShape(bool bInElect)
{
	if (bCard)
	{
		if (bElect != bInElect)
		{
			bElect = bInElect;
			PaintRoleRing();
		}
		return;
	}
	bElect = bInElect;
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

void ASeatTokenActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bPlaced && SlideElapsed < SlideDuration)
	{
		SlideElapsed = FMath::Min(SlideElapsed + DeltaSeconds, SlideDuration);
		SetActorLocation(FMath::Lerp(SlideStart, SlideTarget, SlideElapsed / SlideDuration));
	}
	// Sube y baja suave hacia la altura que toca, sin saltos cuando cambia el asiento activo.
	const float TargetLift = bActive ? Lift : 0.f;
	CurrentLift = FMath::FInterpTo(CurrentLift, TargetLift, DeltaSeconds, 8.f);
	Pivot->SetRelativeLocation(FVector(0.f, 0.f, CurrentLift));
	CurrentScale = FMath::FInterpTo(CurrentScale, bActive ? ActiveScale : 1.f, DeltaSeconds, 8.f);
	Pivot->SetRelativeScale3D(FVector(CurrentScale));
	if (bActive)
	{
		Pivot->AddRelativeRotation(FRotator(0.f, SpinDegreesPerSecond * DeltaSeconds, 0.f));
	}
}
