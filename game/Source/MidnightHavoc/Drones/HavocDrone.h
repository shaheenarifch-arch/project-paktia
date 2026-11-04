#pragma once

// Drone pawn. Ports src/gameplay/drones/DroneBase.ts with DroneFlightController.ts, DroneBattery.ts,
// DroneSignalManager.ts, DroneSensor.ts and DroneCamera.ts (see HavocDroneModel.h for the pure-data ports).
// Subclasses AHavocRavenDrone, AHavocWardenDrone and AHavocTalonDrone set the TS parameters.
//
// The server owns state. Each tick it computes signal from the relay, runs the TS update (battery drain,
// lost-link fallback), flies the drone toward its target, scans for detectables and publishes
// drone:battery / drone:signal / drone:detected on UHavocEventSubsystem. Clients republish the first two
// from OnRep_Core so a local HUD still updates.
// Distances inside the model are metres as in the TS; actor locations are converted (1 m = 100 uu).

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Drones/HavocDroneTypes.h"
#include "Drones/HavocDroneModel.h"
#include "HavocDrone.generated.h"

class UCameraComponent;
class USphereComponent;
class UStaticMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FHavocDroneStateDelegate, EHavocDroneState, OldState, EHavocDroneState, NewState);

UCLASS()
class MIDNIGHTHAVOC_API AHavocDrone : public APawn
{
    GENERATED_BODY()

public:
    AHavocDrone();

    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Havoc|Drone")
    TObjectPtr<USphereComponent> Collision;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Havoc|Drone")
    TObjectPtr<UStaticMeshComponent> Mesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Havoc|Drone")
    TObjectPtr<UCameraComponent> Camera;

    /** Optional signal relay. Without one the launch point is the relay. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Drone")
    TObjectPtr<AActor> RelayActor;

    /** 0..1, subtracted from the range-based signal (DroneSignalManager interference). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Drone", meta = (ClampMin = "0", ClampMax = "1"))
    double Interference = 0.0;

    /** Seconds between sensor scans. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Havoc|Drone")
    float SensorInterval = 0.5f;

    /** Field of view at zero and full camera zoom. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Havoc|Drone")
    float WideFov = 90.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Havoc|Drone")
    float ZoomedFov = 30.f;

    UPROPERTY(ReplicatedUsing = OnRep_Core, BlueprintReadOnly, Category = "Havoc|Drone")
    FHavocDroneCore Core;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Havoc|Drone")
    FHavocDroneCamera CameraState;

    UPROPERTY(BlueprintAssignable, Category = "Havoc|Drone")
    FHavocDroneStateDelegate OnDroneStateChanged;

    // ---- Queries ----
    UFUNCTION(BlueprintPure, Category = "Havoc|Drone")
    FHavocDroneTelemetry GetTelemetry() const;

    UFUNCTION(BlueprintPure, Category = "Havoc|Drone")
    EHavocDroneState GetDroneState() const { return Core.State; }

    UFUNCTION(BlueprintPure, Category = "Havoc|Drone")
    FString GetDroneId() const { return Core.Definition.Id; }

    UFUNCTION(BlueprintPure, Category = "Havoc|Drone")
    EHavocSignalState GetSignalState() const { return FHavocDroneSignalManager::Classify(Core.Signal); }

    UFUNCTION(BlueprintPure, Category = "Havoc|Drone")
    bool IsBatteryLow() const { return Core.Battery.IsLow(); }

    UFUNCTION(BlueprintPure, Category = "Havoc|Drone")
    bool IsBatteryCritical() const { return Core.Battery.IsCritical(); }

    // ---- Commands (server only; return values match DroneBase.ts) ----
    /** DroneController.launch: launch from HomeLocation (world uu) and activate. False if not stowed or recovered. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Drone")
    bool Launch(const FVector& HomeLocation);

    UFUNCTION(BlueprintCallable, Category = "Havoc|Drone")
    bool MoveTo(const FVector& WorldLocation, const FString& Task = TEXT("MOVE"));

    UFUNCTION(BlueprintCallable, Category = "Havoc|Drone")
    bool ReturnHome();

    /** Only valid while RETURNING. The drone also does this itself on arriving home. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Drone")
    void Recover();

    UFUNCTION(BlueprintCallable, Category = "Havoc|Drone")
    void ApplyDroneDamage(double Amount);

    // ---- Camera (DroneCamera.ts) ----
    UFUNCTION(BlueprintCallable, Category = "Havoc|Drone")
    void SetCameraActive(bool bActive);

    UFUNCTION(BlueprintCallable, Category = "Havoc|Drone")
    void SetCameraZoom(double NormalizedZoom);

protected:
    virtual void BeginPlay() override;

    /** Subclasses call this from their constructor to apply their TS definition. */
    void SetDefinition(const FHavocDroneDefinition& InDefinition);

    UFUNCTION()
    void OnRep_Core();

private:
    void UpdateServer(float DeltaSeconds);
    void FlyToward(float DeltaSeconds);
    double ComputeSignal() const;
    void ScanForDetectables();
    void ApplyCamera();
    void NotifyStateChange(EHavocDroneState OldState);
    void PublishTelemetryChanges();
    void PublishEvent(const TCHAR* EventName, const FString& Payload) const;

    /** Server: first detection of each detectable by this drone. */
    TSet<FName> DetectedIds;

    float SensorTimer = 0.f;

    /** Last published values, to avoid spamming the event bus every frame. */
    int32 LastBatteryPercent = -1;
    int32 LastSignalPercent = -1;
    EHavocDroneState LastKnownState = EHavocDroneState::Stowed;
};

UCLASS()
class MIDNIGHTHAVOC_API AHavocRavenDrone : public AHavocDrone
{
    GENERATED_BODY()

public:
    AHavocRavenDrone();
};

UCLASS()
class MIDNIGHTHAVOC_API AHavocWardenDrone : public AHavocDrone
{
    GENERATED_BODY()

public:
    AHavocWardenDrone();

    /** WardenDrone.surveilArea: MoveTo with task SURVEIL_AREA. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Drone")
    bool SurveilArea(const FVector& WorldCenter);
};

UCLASS()
class MIDNIGHTHAVOC_API AHavocTalonDrone : public AHavocDrone
{
    GENERATED_BODY()

public:
    AHavocTalonDrone();

    UFUNCTION(BlueprintCallable, Category = "Havoc|Drone")
    void AuthorizeMission();

    /** Clears the authorization and returns home (TalonDrone.abortMission). */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Drone")
    void AbortMission();

    UFUNCTION(BlueprintPure, Category = "Havoc|Drone")
    bool IsMissionAuthorized() const { return bMissionAuthorized; }

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
    UPROPERTY(Replicated)
    bool bMissionAuthorized = false;
};
