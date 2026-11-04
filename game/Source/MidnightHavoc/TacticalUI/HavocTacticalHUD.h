#pragma once

// C++ base for the tactical HUD. Make a UMG Blueprint child (see game/docs/hud-guide.md) and
// implement the events below; no widget assets live in the repo.
//
// Subscribes on UHavocEventSubsystem in NativeConstruct and unsubscribes in NativeDestruct:
//   mission:state, mission:blocked, mission:checkpoint, mission:complete   (UHavocMissionRunner)
//   drone:battery, drone:signal                                           (AHavocDrone)
//   helicopter:damage                                                     (AHavocFalconHelicopter)
// Payloads are the JSON objects documented in game/docs/hud-guide.md.

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Aircraft/HavocAircraftTypes.h"
#include "Drones/HavocDroneTypes.h"
#include "HavocTacticalHUD.generated.h"

class UHavocMissionSubsystem;

UCLASS(Abstract, Blueprintable)
class MIDNIGHTHAVOC_API UHavocTacticalHUD : public UUserWidget
{
    GENERATED_BODY()

public:
    /** Re-reads the active mission and fires OnObjectiveChanged / OnChoicesAvailable. Called from NativeConstruct, so a HUD created mid-mission starts in sync. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|HUD")
    void RefreshFromMission();

    // ---- Designer events (implement in the Blueprint child) ----
    UFUNCTION(BlueprintImplementableEvent, Category = "Havoc|HUD")
    void OnObjectiveChanged(const FString& Objective, const FString& StateId);

    /** Fired when the current state has named branches (the player must choose). */
    UFUNCTION(BlueprintImplementableEvent, Category = "Havoc|HUD")
    void OnChoicesAvailable(const TArray<FString>& Choices);

    UFUNCTION(BlueprintImplementableEvent, Category = "Havoc|HUD")
    void OnCheckpoint(const FString& StateId);

    UFUNCTION(BlueprintImplementableEvent, Category = "Havoc|HUD")
    void OnMissionComplete(const FString& Outcome, const FString& StateId);

    /** mission:blocked: the state needs flags that are not set yet. */
    UFUNCTION(BlueprintImplementableEvent, Category = "Havoc|HUD")
    void OnMissionBlocked(const FString& StateId, const TArray<FString>& Missing);

    UFUNCTION(BlueprintImplementableEvent, Category = "Havoc|HUD")
    void OnDroneBattery(const FString& DroneId, double Battery, bool bLow, bool bCritical);

    UFUNCTION(BlueprintImplementableEvent, Category = "Havoc|HUD")
    void OnDroneSignal(const FString& DroneId, double Signal, EHavocSignalState SignalState);

    UFUNCTION(BlueprintImplementableEvent, Category = "Havoc|HUD")
    void OnHelicopterDamage(const FString& Callsign, EHavocHelicopterDamageLevel Level, const FHavocHelicopterDamageSnapshot& Snapshot);

    // ---- Episode 04 getters (0 when Episode 04 is not the active mission) ----
    UFUNCTION(BlueprintPure, Category = "Havoc|HUD")
    int32 GetReinforcements() const;

    /** 0..1 */
    UFUNCTION(BlueprintPure, Category = "Havoc|HUD")
    double GetTerminalIntegrity() const;

    UFUNCTION(BlueprintPure, Category = "Havoc|HUD")
    double GetSupplyFlow() const;

    UFUNCTION(BlueprintPure, Category = "Havoc|HUD")
    bool HasEpisode04Data() const;

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    /** Bound to every event above; routes on EventName. */
    UFUNCTION()
    void HandleHavocEvent(FName EventName, const FString& Payload);

private:
    UHavocMissionSubsystem* GetMissionSubsystem() const;
};
