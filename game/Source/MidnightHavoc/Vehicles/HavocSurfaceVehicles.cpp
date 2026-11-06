#include "Vehicles/HavocSurfaceVehicles.h"

#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/SpringArmComponent.h"

void AHavocSurfaceVehicle::BeginPlay()
{
    Super::BeginPlay();
    if (bConvoyFollow && !ConvoyLeader)
    {
        double BestDist = TNumericLimits<double>::Max();
        for (TActorIterator<AHavocGroundVehicle> It(GetWorld()); It; ++It)
        {
            const double D = FVector::DistSquared(It->GetActorLocation(), GetActorLocation());
            if (*It != this && D < BestDist)
            {
                BestDist = D;
                ConvoyLeader = *It;
            }
        }
    }
}

void AHavocSurfaceVehicle::StepMovement(float Dt)
{
    // Convoy AI: an empty vehicle drives itself behind its leader; a player driver always wins.
    if (bConvoyFollow && !IsOccupied() && ConvoyLeader && !IsDestroyed())
    {
        double FollowThrottle = 0.0, FollowSteer = 0.0;
        HavocVehicleMath::FollowInput(GetActorLocation(), GetActorForwardVector(), ConvoyLeader->GetActorLocation(), FollowGap, FollowThrottle, FollowSteer);
        SetInput(FollowThrottle, FollowSteer, 0.0);
    }

    // Damage: speed, acceleration and turning scale down, and a disabled or destroyed vehicle coasts to a stop.
    const double Mobility = GetMobilityFactor();
    const double MaxSpeedCm = Stats.MaxSpeed * 100.0 * Mobility;
    const double AccelCm = Stats.Acceleration * 100.0;
    const double EffectiveThrottle = Mobility > 0.0 ? Throttle : 0.0;
    Speed = HavocVehicleMath::StepSpeed(Speed, EffectiveThrottle, MaxSpeedCm, AccelCm, AccelCm * DragFactor, Dt);

    const double YawDelta = Mobility <= 0.0 ? 0.0
        : bTrackedSteer ? HavocVehicleMath::TrackedYawDelta(Steer, Speed, Stats.TurnRate * Mobility, Dt)
        : HavocVehicleMath::SurfaceYawDelta(Steer, Speed, MaxSpeedCm, Stats.TurnRate * Mobility, Dt);
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
    bTrackedSteer = true; // tracks pivot on the spot
    ConvoyId = FName(TEXT("warden-1"));
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

// ---------------------------------------------------------------- nomad

AHavocNomadVehicle::AHavocNomadVehicle()
{
    VehicleId = FName(TEXT("nomad_transport"));
    AssetRole = FName(TEXT("Vehicle.Truck.Coalition")); // not mapped yet: greybox until a free truck pack is added (docs/fab-assets.md)
    Collision->SetBoxExtent(FVector(300.0, 130.0, 110.0));
    CameraBoom->TargetArmLength = 1000.f;
    ConvoyId = FName(TEXT("nomad-1"));
}

void AHavocNomadVehicle::SnapToSurface()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }
    const FVector Here = GetActorLocation();
    const double HalfHeight = Collision->GetScaledBoxExtent().Z;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(HavocNomadSnap), false, this);
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
