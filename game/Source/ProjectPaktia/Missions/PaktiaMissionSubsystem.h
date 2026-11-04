#pragma once

// Ports: src/gameplay/missions/framework/MissionRunner.ts (entry point for Blueprints)
// Starts a mission from the data loaded by UPaktiaDataSubsystem and forwards the runner calls.
// Episode 04 (episode_04_open_strait) gets its own mission object with the Episode 04 rules;
// any other mission runs as a bare runner and fails to start if it uses effects (no handlers yet).

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Data/PaktiaMissionDef.h"
#include "PaktiaMissionSubsystem.generated.h"

class UPaktiaMissionRunner;
class UPaktiaOpenStraitMission;

UCLASS()
class PROJECTPAKTIA_API UPaktiaMissionSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    /** Looks a mission definition up in UPaktiaDataSubsystem. Returns false if unknown. */
    UFUNCTION(BlueprintCallable, Category = "Paktia|Missions")
    bool GetMissionDef(FName Id, FPaktiaMissionDef& OutMission) const;

    /** Outcome of Episode 03 ("SUCCESS", "PARTIAL", "FAILED" or empty). Used by StartMission for Episode 04's bonus. */
    UFUNCTION(BlueprintCallable, Category = "Paktia|Missions")
    void SetEpisode3Outcome(const FString& Outcome) { Episode3Outcome = Outcome; }

    /** Replaces any active mission. Returns false (and logs) if the mission is unknown or cannot start. */
    UFUNCTION(BlueprintCallable, Category = "Paktia|Missions")
    bool StartMission(FName MissionId);

    UFUNCTION(BlueprintCallable, Category = "Paktia|Missions")
    UPaktiaMissionRunner* GetActiveRunner() const { return ActiveRunner; }

    /** The Episode 04 mission object while it is the active mission (for world-driven calls), else null. */
    UFUNCTION(BlueprintCallable, Category = "Paktia|Missions")
    UPaktiaOpenStraitMission* GetOpenStrait() const { return ActiveOpenStrait; }

    // Pass-throughs to the active runner. All are safe no-ops (empty result) without one.
    UFUNCTION(BlueprintCallable, Category = "Paktia|Missions")
    FString Advance();

    UFUNCTION(BlueprintCallable, Category = "Paktia|Missions")
    FString Choose(const FString& Branch);

    UFUNCTION(BlueprintCallable, Category = "Paktia|Missions")
    FString Fail();

    UFUNCTION(BlueprintCallable, Category = "Paktia|Missions")
    bool RestoreCheckpoint();

    UFUNCTION(BlueprintCallable, Category = "Paktia|Missions")
    FString GetObjective() const;

    UFUNCTION(BlueprintCallable, Category = "Paktia|Missions")
    TArray<FString> GetChoices() const;

private:
    UPROPERTY()
    TObjectPtr<UPaktiaMissionRunner> ActiveRunner;

    UPROPERTY()
    TObjectPtr<UPaktiaOpenStraitMission> ActiveOpenStrait;

    FString Episode3Outcome;
};
