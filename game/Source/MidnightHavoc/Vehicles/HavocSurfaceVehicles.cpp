#include "Vehicles/HavocSurfaceVehicles.h"

#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "GameFramework/SpringArmComponent.h"

void AHavocSurfaceVehicle::StepMovement(float Dt)
{
    const double MaxSpeedCm = Stats.MaxSpeed * 100.0;
    const double AccelCm = Stats.Acceleration * 100.0;
    Speed = HavocVehicleMath::StepSpeed(Speed, Throttle, MaxSpeedCm, AccelCm, AccelCm * DragFactor, Dt);

    const double YawDelta = HavocVehicleMath::SurfaceYawDelta(Steer, Speed, MaxSpeedCm, Stats.TurnRate, Dt);
    FRotator Rotation = GetActorRotation();
    Rotation.Yaw += YawDelta;
    Rotation.Pitch = 0.0;
    Rotation.Roll = 0.0;

    const FVector Forward = Rotation.Vector();
    MoveSwept(Forward * Speed * Dt, Rotation.Quaternion());
    SnapToSurface();
}

// ---------------------------------------------------------------- ground

AHavocGroundVehicle::AHavocGroundVehicle()
{
    VehicleId = FName(TEXT("warden_tank"));
    AssetRole = FName(TEXT("Vehicle.Tank.Coalition"));
    Collision->SetBoxExtent(FVector(350.0, 180.0, 120.0));
    CameraBoom->TargetArmLength = 1200.f;
}

void AHavocGroundVehicle::SnapToSurface()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }
    const FVector Here = GetActorLocation();
    const double HalfHeight = Collision->GetScaledBoxExtent().Z;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(HavocGroundSnap), false, this);
    FHitResult Hit;
    if (World->LineTraceSingleByChannel(Hit, Here + FVector(0, 0, 300.0), Here - FVector(0, 0, 1000.0), ECC_Visibility, Params))
    {
        SetActorLocation(FVector(Here.X, Here.Y, Hit.ImpactPoint.Z + HalfHeight));
    }
}

// ---------------------------------------------------------------- boat

AHavocBoatVehicle::AHavocBoatVehicle()
{
    VehicleId = FName(TEXT("tern_patrol_boat"));
    AssetRole = FName(TEXT("Vessel.SmallBoat"));
    Collision->SetBoxExtent(FVector(400.0, 130.0, 80.0));
    CameraBoom->TargetArmLength = 1100.f;
    DragFactor = 0.3; // water carries the hull further
}

void AHavocBoatVehicle::BeginPlay()
{
    Super::BeginPlay();
    WaterLevelZ = GetActorLocation().Z;
}

void AHavocBoatVehicle::SnapToSurface()
{
    FVector Here = GetActorLocation();
    Here.Z = WaterLevelZ;
    SetActorLocation(Here);
}
