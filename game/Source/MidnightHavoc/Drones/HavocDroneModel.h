#pragma once

// Pure-data drone model (no world needed, so the automation tests can drive it).
//   FHavocDroneBattery          -> src/gameplay/drones/DroneBattery.ts
//   FHavocDroneSignalManager    -> DroneSignalManager.ts
//   FHavocDroneFlightController -> DroneFlightController.ts
//   FHavocDroneSensor           -> DroneSensor.ts
//   FHavocDroneCamera           -> DroneCamera.ts
//   FHavocDroneCore             -> DroneBase.ts (state machine, battery drain, lost-link fallback)

#include "CoreMinimal.h"
#include "Drones/HavocDroneTypes.h"
#include "HavocDroneModel.generated.h"

USTRUCT(BlueprintType)
struct MIDNIGHTHAVOC_API FHavocDroneBattery
{
    GENERATED_BODY()

    /** 0..1 */
    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Drone")
    double Charge = 1.0;

    /** Returns the new charge. Negative amounts are ignored. */
    double Consume(double Amount);
    double Recharge(double Amount);

    bool IsLow() const { return Charge <= 0.25; }
    bool IsCritical() const { return Charge <= 0.1; }
};

/** Static helpers, not reflected. */
struct MIDNIGHTHAVOC_API FHavocDroneSignalManager
{
    /** clamp01(1 - distance / max(maxRange, 1) - clamp01(interference)). */
    static double Calculate(const FVector& DronePosition, const FVector& RelayPosition, double MaxRange, double Interference = 0.0);
    static EHavocSignalState Classify(double Signal);
};

struct MIDNIGHTHAVOC_API FHavocDroneFlightController
{
    /** Moves Current toward Target by at most Speed * DeltaSeconds; snaps to Target when within reach. */
    static FVector Step(const FVector& Current, const FVector& Target, double Speed, double DeltaSeconds);
};

struct MIDNIGHTHAVOC_API FHavocDroneSensor
{
    /** Visible candidates within Range of Origin. */
    static TArray<FHavocSensorCandidate> Scan(const FVector& Origin, double Range, const TArray<FHavocSensorCandidate>& Candidates);
};

USTRUCT(BlueprintType)
struct MIDNIGHTHAVOC_API FHavocDroneCamera
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Drone")
    bool bActive = false;

    /** 0..1 */
    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Drone")
    double Zoom = 0.0;

    void SetActive(bool bInActive) { bActive = bInActive; }
    void SetZoom(double NormalizedZoom) { Zoom = FMath::Clamp(NormalizedZoom, 0.0, 1.0); }
};

USTRUCT(BlueprintType)
struct MIDNIGHTHAVOC_API FHavocDroneCore
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Drone")
    FHavocDroneDefinition Definition;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Drone")
    EHavocDroneState State = EHavocDroneState::Stowed;

    /** Metres. */
    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Drone")
    FVector Position = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Drone")
    FVector Home = FVector::ZeroVector;

    /** Destination of the last MoveTo. */
    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Drone")
    FVector Target = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Drone")
    FHavocDroneBattery Battery;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Drone")
    double Signal = 1.0;

    /** Empty when the TS task is undefined. */
    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Drone")
    FString Task;

    bool Launch(const FVector& HomePosition);
    void Activate();
    /**
     * Same state rules as the TS. Deviation: the TS teleports Position to the target; here the target
     * is stored in Target and Position is advanced by the caller (see FHavocDroneFlightController).
     */
    bool MoveTo(const FVector& InTarget, const FString& InTask = TEXT("MOVE"));
    bool ReturnHome();
    void Recover();
    void ApplyDamage(double Amount);
    /** Drains the battery, applies SignalStrength, then lost-link / degraded / low-battery rules. No-op unless airborne. */
    void Update(double DeltaSeconds, double SignalStrength);
    FHavocDroneTelemetry Telemetry() const;

    bool IsControllable() const;
    bool IsAirborne() const;

private:
    void HandleSignalLost();
};
