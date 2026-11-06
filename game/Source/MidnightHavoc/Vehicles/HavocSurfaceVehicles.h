#pragma once

// Ground vehicle and boat: both are surface vehicles (throttle + steering, no lift).
//   AHavocGroundVehicle  data/vehicles/warden_tank.json   role Vehicle.Tank.Coalition  follows the terrain
//   AHavocNomadVehicle   data/vehicles/nomad_transport.json  role Vehicle.Truck.Coalition  wheeled truck, follows the terrain
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

    /** Episode 02 convoy: when nobody drives this vehicle it follows ConvoyLeader (straight pursuit on the ground plane). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Convoy")
    bool bConvoyFollow = false;

    /** Vehicle or actor to follow. Empty with bConvoyFollow: the nearest Warden is used. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Convoy")
    TObjectPtr<AActor> ConvoyLeader;

    /** Distance in cm to keep behind the leader. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Convoy")
    float FollowGap = 2500.f;

protected:
    virtual void BeginPlay() override;

    /** True: pivot turn on the spot (tracked tank). False: steering needs speed (wheels, boat). */
    bool bTrackedSteer = false;

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

/** Nomad light transport: wheeled truck (driver + passenger). Chaos Vehicles needs a rigged mesh, see docs/vehicles.md. */
UCLASS()
class MIDNIGHTHAVOC_API AHavocNomadVehicle : public AHavocSurfaceVehicle
{
    GENERATED_BODY()

public:
    AHavocNomadVehicle();

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
