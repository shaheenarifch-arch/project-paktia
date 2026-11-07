#pragma once

// Ports: src/gameplay/missions/framework/MissionRunner.ts (entry point for Blueprints)
// Starts a mission from the data loaded by UHavocDataSubsystem and forwards the runner calls.
// Episode 04 (episode_04_open_strait) gets its own mission object with the Episode 04 rules;
// Episode 01 (mission_01_mountain_shield) gets UHavocMountainShieldMission;
// Episode 02 (episode_02_silent_corridor) gets UHavocSilentCorridorMission (convoy, relays, ambush);
// Episode 03 (episode_03_broken_citadel) gets UHavocBrokenCitadelMission (phases, surprise event);
// any other mission runs as a bare runner and fails to start if it uses effects (no handlers yet).

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Data/HavocMissionDef.h"
#include "Missions/Episode03/HavocBrokenCitadelMission.h"
#include "HavocMissionSubsystem.generated.h"

class UHavocMissionRunner;
class UHavocMountainShieldMission;
class UHavocOpenStraitMission;
class UHavocSilentCorridorMission;
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

    /** Mission 01 result for Episode 02: was the Falcon-2 crew recovered? (default true). Used by StartMission. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Missions")
    void SetEpisode1CrewRecovered(bool bRecovered) { bEpisode1CrewRecovered = bRecovered; }

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

    /** The Episode 01 mission object while it is the active mission, else null. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Missions")
    UHavocMountainShieldMission* GetMountainShield() const { return ActiveMountainShield; }

    /** The Episode 02 mission object while it is the active mission (for world-driven calls), else null. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Missions")
    UHavocSilentCorridorMission* GetSilentCorridor() const { return ActiveSilentCorridor; }

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
    TObjectPtr<UHavocMountainShieldMission> ActiveMountainShield;

    UPROPERTY()
    TObjectPtr<UHavocSilentCorridorMission> ActiveSilentCorridor;

    UPROPERTY()
    TObjectPtr<UHavocBrokenCitadelMission> ActiveBrokenCitadel;

    FString Episode3Outcome;
    bool bEpisode1CrewRecovered = true;
    FHavocEpisode02Carryover Episode2Carryover;
    FString SurpriseSeed;
    FString ActiveSeed;
};
