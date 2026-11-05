#pragma once

// Helicopter and recon drone: simple flight pawns (no rotor or aerodynamic model).
//   AHavocHelicopterVehicle  data/vehicles/kestrel_scout_helicopter.json  role Aircraft.Falcon  (the mission Falcon stays AHavocFalconHelicopter)
//   AHavocDroneVehicle       data/vehicles/wisp_recon_drone.json          role Drone.Raven      (the mission Raven stays AHavocRavenDrone)
// Controls: W/S forward/back, A/D yaw, Space/Ctrl up/down. Cannot descend through the ground (swept move).
// Docs: game/docs/vehicles.md

#include "CoreMinimal.h"
#include "Vehicles/HavocVehicleBase.h"
#include "HavocFlyingVehicles.generated.h"

UCLASS(Abstract)
class MIDNIGHTHAVOC_API AHavocFlyingVehicle : public AHavocVehicleBase
{
    GENERATED_BODY()

public:
    virtual void StepMovement(float Dt) override;

    /** Pitch / roll of the visual at full speed and full turn, in degrees. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Havoc|Vehicle")
    float BankDegrees = 12.f;

protected:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Havoc|Vehicle")
    double DragFactor = 0.8;
};

UCLASS()
class MIDNIGHTHAVOC_API AHavocHelicopterVehicle : public AHavocFlyingVehicle
{
    GENERATED_BODY()

public:
    AHavocHelicopterVehicle();
};

UCLASS()
class MIDNIGHTHAVOC_API AHavocDroneVehicle : public AHavocFlyingVehicle
{
    GENERATED_BODY()

public:
    AHavocDroneVehicle();
};
