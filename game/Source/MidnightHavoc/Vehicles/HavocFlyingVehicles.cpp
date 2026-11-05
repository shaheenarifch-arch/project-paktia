#include "Vehicles/HavocFlyingVehicles.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/SpringArmComponent.h"

void AHavocFlyingVehicle::StepMovement(float Dt)
{
    const double MaxSpeedCm = Stats.MaxSpeed * 100.0;
    const double AccelCm = Stats.Acceleration * 100.0;
    Speed = HavocVehicleMath::StepSpeed(Speed, Throttle, MaxSpeedCm, AccelCm, AccelCm * DragFactor, Dt);

    FRotator Rotation = GetActorRotation();
    Rotation.Yaw += Steer * Stats.TurnRate * Dt;
    Rotation.Pitch = 0.0;
    Rotation.Roll = 0.0;

    const FVector Forward = Rotation.Vector();
    const FVector Up = FVector::UpVector * Lift * Stats.ClimbRate * 100.0;
    // Bank the whole pawn into the turn / nose down with speed. The camera boom follows the controller, not this.
    FRotator Visual = Rotation;
    const double SpeedFraction = MaxSpeedCm > 0.0 ? FMath::Clamp(Speed / MaxSpeedCm, -1.0, 1.0) : 0.0;
    Visual.Pitch = -SpeedFraction * BankDegrees;
    Visual.Roll = Steer * BankDegrees;

    MoveSwept(Forward * Speed * Dt + Up * Dt, Visual.Quaternion());
}

AHavocHelicopterVehicle::AHavocHelicopterVehicle()
{
    VehicleId = FName(TEXT("kestrel_scout_helicopter"));
    AssetRole = FName(TEXT("Aircraft.Falcon"));
    Collision->SetBoxExtent(FVector(450.0, 150.0, 150.0));
    CameraBoom->TargetArmLength = 1600.f;
}

AHavocDroneVehicle::AHavocDroneVehicle()
{
    VehicleId = FName(TEXT("wisp_recon_drone"));
    AssetRole = FName(TEXT("Drone.Raven"));
    Collision->SetBoxExtent(FVector(60.0, 60.0, 25.0));
    CameraBoom->TargetArmLength = 350.f;
    BankDegrees = 20.f;
    DragFactor = 1.5; // stops quickly
}
