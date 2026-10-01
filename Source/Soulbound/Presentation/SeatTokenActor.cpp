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
	/** Segmentos del aro: cubos finos en círculo (las formas básicas del motor no traen un anillo). */
	constexpr int32 RingSegmentCount = 20;
}

ASeatTokenActor::ASeatTokenActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	// La raíz es solo un punto: la vista la coloca y la escala; el giro y la elevación son del pivote.
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

void ASeatTokenActor::Configure(float InLabelSize, float InLift, float InSpinDegreesPerSecond, float InRingDiameter, float InRingWidth, float InActiveScale)
{
	Lift = InLift;
	ActiveScale = InActiveScale;
	SpinDegreesPerSecond = InSpinDegreesPerSecond;
	RingDiameter = InRingDiameter;
	RingWidth = InRingWidth;
	// La raíz lleva la escala de la ficha: el texto y el aro se miden en cm del mundo, así que se compensa.
	const float RootScale = FMath::Max(GetActorScale3D().X, 0.01f);
	Label->SetWorldSize(InLabelSize / RootScale);
	Label->SetRelativeLocation(FVector(0.f, 0.f, 62.f));
	BuildRing();
	SetActive(false);
}

void ASeatTokenActor::BuildRing()
{
	const float RootScale = FMath::Max(GetActorScale3D().X, 0.01f);
	const float Radius = RingDiameter * 0.5f;
	const float SegmentLength = 2.f * PI * Radius / RingSegmentCount * 1.15f;
	for (int32 Index = 0; Index < RingSegmentCount; ++Index)
	{
		UStaticMeshComponent* Segment = NewObject<UStaticMeshComponent>(this);
		Segment->SetStaticMesh(CubeMesh);
		Segment->SetupAttachment(RingRoot);
		Segment->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Segment->SetCastShadow(false);
		Segment->RegisterComponent();
		const float Angle = 2.f * PI * Index / RingSegmentCount;
		// El aro apoya en la casilla: a la altura de la base de la ficha (50 unidades locales bajo su centro).
		Segment->SetRelativeLocation(FVector(FMath::Cos(Angle) * Radius / RootScale, FMath::Sin(Angle) * Radius / RootScale, -47.f));
		Segment->SetRelativeRotation(FRotator(0.f, FMath::RadiansToDegrees(Angle) + 90.f, 0.f));
		Segment->SetRelativeScale3D(FVector(SegmentLength, RingWidth, 5.f) / (100.f * RootScale));
		RingSegments.Add(Segment);
	}
}

void ASeatTokenActor::SetColor(const FLinearColor& Color)
{
	RingColor = Color;
	Soulbound::Presentation::SetComponentColor(*Mesh, BaseMaterial, Color);
	for (UStaticMeshComponent* Segment : RingSegments)
	{
		Soulbound::Presentation::SetComponentColor(*Segment, BaseMaterial, Color);
	}
	Label->SetText(FText::AsNumber(Seat));
	Label->SetTextRenderColor(FColor::Black);
}

void ASeatTokenActor::SetElectShape(bool bElect)
{
	UStaticMesh* Wanted = bElect ? ElectMesh : EchoMesh;
	// La instancia dinámica del material va en la ranura del componente, así que el color se conserva al cambiar de malla.
	if (Wanted && Mesh->GetStaticMesh() != Wanted)
	{
		Mesh->SetStaticMesh(Wanted);
	}
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
	if (Location.Equals(SlideTarget, 0.5))
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
	const float RootScale = FMath::Max(GetActorScale3D().X, 0.01f);
	Pivot->SetRelativeLocation(FVector(0.f, 0.f, CurrentLift / RootScale));
	CurrentScale = FMath::FInterpTo(CurrentScale, bActive ? ActiveScale : 1.f, DeltaSeconds, 8.f);
	Pivot->SetRelativeScale3D(FVector(CurrentScale));
	if (bActive)
	{
		Pivot->AddRelativeRotation(FRotator(0.f, SpinDegreesPerSecond * DeltaSeconds, 0.f));
	}
}
