#pragma once

// Ground vehicle and boat: both are surface vehicles (throttle + steering, no lift).
//   AHavocGroundVehicle  data/vehicles/warden_tank.json   role Vehicle.Tank.Coalition  follows the terrain
//   AHavocBoatVehicle    data/vehicles/tern_patrol_boat.json  role Vessel.SmallBoat   holds the water level it spawned at
// Placeholder arcade handling with abstract stats; no weapons. Docs: game/docs/vehicles.md
// Chaos upgrade path: https://dev.epicgames.com/documentation/en-us/unreal-engine/chaos-vehicles-overview-in-unreal-engine

#include "CoreMinimal.h"
#include "Vehicles/HavocVehicleBase.h"
#include "HavocSurfaceVehicles.generated.h"

UCLASS(Abstract)
class MIDNIGHTHAVOC_API AHavocSurfaceVehicle : public AHavocVehicleBase
{
    GENERATED_BODY()

public:
    virtual void StepMovement(float Dt) override;

protected:
    /** Slowing rate with no throttle, as a fraction of Acceleration. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Havoc|Vehicle")
    double DragFactor = 0.6;

    /** Keep the vehicle on the surface under it (ground) or at WaterLevelZ (boat). */
    virtual void SnapToSurface() {}
};

UCLASS()
class MIDNIGHTHAVOC_API AHavocGroundVehicle : public AHavocSurfaceVehicle
{
    GENERATED_BODY()

public:
    AHavocGroundVehicle();

protected:
    virtual void SnapToSurface() override;
};

UCLASS()
class MIDNIGHTHAVOC_API AHavocBoatVehicle : public AHavocSurfaceVehicle
{
    GENERATED_BODY()

public:
    AHavocBoatVehicle();

    /** Z the hull rides at; set from the spawn location in BeginPlay. */
    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Vehicle")
    double WaterLevelZ = 0.0;

protected:
    virtual void BeginPlay() override;
    virtual void SnapToSurface() override;
};
