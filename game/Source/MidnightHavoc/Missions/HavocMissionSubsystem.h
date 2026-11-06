#pragma once

// Ports: src/gameplay/missions/framework/MissionRunner.ts (entry point for Blueprints)
// Starts a mission from the data loaded by UHavocDataSubsystem and forwards the runner calls.
// Episode 03 (episode_03_broken_citadel) and Episode 04 (episode_04_open_strait) get their own mission
// objects with their rules; any other mission runs as a bare runner and fails to start if it uses effects (no handlers yet).

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Data/HavocMissionDef.h"
#include "Missions/Episode03/HavocBrokenCitadelMission.h"
#include "HavocMissionSubsystem.generated.h"

class UHavocMissionRunner;
class UHavocOpenStraitMission;
class UHavocBrokenCitadelMission;

UCLASS()
class MIDNIGHTHAVOC_API UHavocMissionSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    /** Looks a mission definition up in UHavocDataSubsystem. Returns false if unknown. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Missions")
    bool GetMissionDef(FName Id, FHavocMissionDef& OutMission) const;

    /** Outcome of Episode 03 ("SUCCESS", "PARTIAL", "FAILED" or empty). Used by StartMission for Episode 04's bonus. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Missions")
    void SetEpisode3Outcome(const FString& Outcome) { Episode3Outcome = Outcome; }

    /** What Episode 03 reads from Episode 02 (crew delivered, Nomads left). Defaults: none. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Missions")
    void SetEpisode2Carryover(const FHavocEpisode02Carryover& Carryover) { Episode2Carryover = Carryover; }

    /** Seed that picks Episode 03's surprise event. Same seed, same event. Empty (default) = a fresh seed per StartMission. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Missions")
    void SetSurpriseSeed(const FString& Seed) { SurpriseSeed = Seed; }

    /** The seed the active Episode 03 mission was started with. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Missions")
    FString GetActiveSurpriseSeed() const { return ActiveSeed; }

    /** Replaces any active mission. Returns false (and logs) if the mission is unknown or cannot start. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Missions")
    bool StartMission(FName MissionId);

    UFUNCTION(BlueprintCallable, Category = "Havoc|Missions")
    UHavocMissionRunner* GetActiveRunner() const { return ActiveRunner; }

    /** The Episode 04 mission object while it is the active mission (for world-driven calls), else null. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Missions")
    UHavocOpenStraitMission* GetOpenStrait() const { return ActiveOpenStrait; }

    /** The Episode 03 mission object while it is the active mission (for world-driven calls), else null. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Missions")
    UHavocBrokenCitadelMission* GetBrokenCitadel() const { return ActiveBrokenCitadel; }

    // Pass-throughs to the active runner. All are safe no-ops (empty result) without one.
    UFUNCTION(BlueprintCallable, Category = "Havoc|Missions")
    FString Advance();

    UFUNCTION(BlueprintCallable, Category = "Havoc|Missions")
    FString Choose(const FString& Branch);

    UFUNCTION(BlueprintCallable, Category = "Havoc|Missions")
    FString Fail();

    UFUNCTION(BlueprintCallable, Category = "Havoc|Missions")
    bool RestoreCheckpoint();

    UFUNCTION(BlueprintCallable, Category = "Havoc|Missions")
    FString GetObjective() const;

    UFUNCTION(BlueprintCallable, Category = "Havoc|Missions")
    TArray<FString> GetChoices() const;

private:
    UPROPERTY()
    TObjectPtr<UHavocMissionRunner> ActiveRunner;

    UPROPERTY()
    TObjectPtr<UHavocOpenStraitMission> ActiveOpenStrait;

    UPROPERTY()
    TObjectPtr<UHavocBrokenCitadelMission> ActiveBrokenCitadel;

    FString Episode3Outcome;
    FHavocEpisode02Carryover Episode2Carryover;
    FString SurpriseSeed;
    FString ActiveSeed;
};
