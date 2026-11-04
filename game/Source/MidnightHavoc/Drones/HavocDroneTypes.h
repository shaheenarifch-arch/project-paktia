#pragma once

// Enums and data structs for drones. Ports the type unions and interfaces in
// src/gameplay/drones/DroneBase.ts, DroneSignalManager.ts and DroneSensor.ts.
// Distances and speeds keep the TypeScript values (metres, m/s); AHavocDrone converts to/from
// Unreal units (1 m = 100 uu) at the actor boundary.

#include "CoreMinimal.h"
#include "HavocDroneTypes.generated.h"

UENUM(BlueprintType)
enum class EHavocDroneRole : uint8
{
    Recon,
    Surveillance,
    PrecisionSupport
};

UENUM(BlueprintType)
enum class EHavocDroneState : uint8
{
    Stowed,
    Launching,
    Active,
    Observing,
    Tracking,
    Moving,
    Orbiting,
    Returning,
    Recovered,
    SignalDegraded,
    SignalLost,
    Disabled,
    Destroyed
};

UENUM(BlueprintType)
enum class EHavocLostLinkBehavior : uint8
{
    HoldPosition,
    ReturnHome,
    Land,
    MissionDefined
};

/** SignalState in DroneSignalManager.ts. */
UENUM(BlueprintType)
enum class EHavocSignalState : uint8
{
    Strong,
    Stable,
    Degraded,
    Critical,
    Lost
};

/** DroneDefinition in DroneBase.ts. */
USTRUCT(BlueprintType)
struct MIDNIGHTHAVOC_API FHavocDroneDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Havoc|Drone")
    FString Id;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Havoc|Drone")
    FString DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Havoc|Drone")
    EHavocDroneRole Role = EHavocDroneRole::Recon;

    /** m/s */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Havoc|Drone")
    double MaxSpeed = 0.0;

    /** metres */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Havoc|Drone")
    double SensorRange = 0.0;

    /** Fraction of full charge per second. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Havoc|Drone")
    double BatteryDrainPerSecond = 0.0;

    /** metres */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Havoc|Drone")
    double SignalRange = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Havoc|Drone")
    TArray<FString> AvailableCommands;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Havoc|Drone")
    EHavocLostLinkBehavior FallbackBehavior = EHavocLostLinkBehavior::HoldPosition;

    // TS parameters of the three drones (RavenDrone.ts, WardenDrone.ts, TalonDrone.ts).
    static FHavocDroneDefinition MakeRaven(const FString& InId = TEXT("raven-1"));
    static FHavocDroneDefinition MakeWarden(const FString& InId = TEXT("warden-1"));
    static FHavocDroneDefinition MakeTalon(const FString& InId = TEXT("talon-1"));
};

/** DroneTelemetry in DroneBase.ts. Task is empty when the TS value is undefined. */
USTRUCT(BlueprintType)
struct MIDNIGHTHAVOC_API FHavocDroneTelemetry
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Drone")
    FString Id;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Drone")
    FString DisplayName;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Drone")
    EHavocDroneRole Role = EHavocDroneRole::Recon;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Drone")
    EHavocDroneState State = EHavocDroneState::Stowed;

    /** Metres. */
    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Drone")
    FVector Position = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Drone")
    double Battery = 1.0;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Drone")
    double Signal = 1.0;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Drone")
    FString Task;
};

/** SensorCandidate in DroneSensor.ts. Position in metres. */
USTRUCT(BlueprintType)
struct MIDNIGHTHAVOC_API FHavocSensorCandidate
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Havoc|Drone")
    FName Id;

    UPROPERTY(BlueprintReadWrite, Category = "Havoc|Drone")
    FVector Position = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "Havoc|Drone")
    FString Classification;

    UPROPERTY(BlueprintReadWrite, Category = "Havoc|Drone")
    bool bVisible = false;
};
