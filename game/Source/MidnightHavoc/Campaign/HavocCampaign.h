#pragma once

// Ports: src/gameplay/campaign/Campaign.ts
// The campaign: Episodes 01-04 in order, what each hands to the next, which are unlocked, and
// the save file. Rules (same as the TypeScript):
//   - Episode 01 is unlocked from the start; any later episode unlocks when the one before it
//     ended in SUCCESS or PARTIAL (a FAILED episode is replayed from its checkpoint).
//   - A worse replay never overwrites a better result (SUCCESS > PARTIAL > FAILED).
//   - Carryover: Episode 01 crew -> Episode 02; Episode 02 crew / Nomads / Warden -> Episode 03;
//     Episode 03 SUCCESS -> the Episode 04 reinforcement bonus (data/missions/episode-04-open-strait.json).
// Progress is saved to the "HavocCampaign" slot with USaveGame.
// Help: https://dev.epicgames.com/documentation/en-us/unreal-engine/saving-and-loading-your-game-in-unreal-engine

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "HavocCampaign.generated.h"

/** Everything an episode hands to the next. Only the fields of the episode that wrote it are meaningful. */
USTRUCT(BlueprintType)
struct MIDNIGHTHAVOC_API FHavocCarryover
{
    GENERATED_BODY()

    /** Episode 01: the Falcon-2 crew was recovered. */
    UPROPERTY(BlueprintReadWrite, SaveGame, Category = "Havoc|Campaign") bool bCrewRecovered = true;
    /** Episode 02: crew delivered to the South Gate. */
    UPROPERTY(BlueprintReadWrite, SaveGame, Category = "Havoc|Campaign") bool bCrewDelivered = false;
    UPROPERTY(BlueprintReadWrite, SaveGame, Category = "Havoc|Campaign") int32 NomadsRemaining = 0;
    UPROPERTY(BlueprintReadWrite, SaveGame, Category = "Havoc|Campaign") bool bWardenOperational = false;
    /** RIVERBED, RIVER_DRAINAGE or MAIN_ROAD. */
    UPROPERTY(BlueprintReadWrite, SaveGame, Category = "Havoc|Campaign") FString Route;
    UPROPERTY(BlueprintReadWrite, SaveGame, Category = "Havoc|Campaign") int32 RelaysOnline = 0;
};

USTRUCT(BlueprintType)
struct MIDNIGHTHAVOC_API FHavocEpisodeResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, SaveGame, Category = "Havoc|Campaign") int32 Episode = 0;
    /** "SUCCESS", "PARTIAL" or "FAILED". */
    UPROPERTY(BlueprintReadWrite, SaveGame, Category = "Havoc|Campaign") FString Outcome;
    UPROPERTY(BlueprintReadWrite, SaveGame, Category = "Havoc|Campaign") FHavocCarryover Carryover;
};

/** The save file (SaveGame). */
UCLASS()
class MIDNIGHTHAVOC_API UHavocCampaignSave : public USaveGame
{
    GENERATED_BODY()

public:
    UPROPERTY(SaveGame) int32 Version = 1;
    UPROPERTY(SaveGame) TArray<FHavocEpisodeResult> Results;
    /** Testing aid chosen in the campaign menu: treat every episode as unlocked. */
    UPROPERTY(SaveGame) bool bUnlockAll = false;
};

UCLASS()
class MIDNIGHTHAVOC_API UHavocCampaign : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    static constexpr int32 FirstEpisode = 1;
    static constexpr int32 LastEpisode = 4;

    /** Loads the save slot, if there is one. */
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    /** Where the progress is stored. Tests set their own slot so a real save is never touched. */
    FString SlotName = TEXT("HavocCampaign");

    static FString EpisodeName(int32 Episode);
    /** Rank of an outcome: FAILED 0, PARTIAL 1, SUCCESS 2, anything else -1. */
    static int32 OutcomeRank(const FString& Outcome);

    /**
     * Records an episode result. A worse outcome than the stored one is ignored (returns false);
     * an equal or better one replaces it and its carryover. Unknown episodes or outcomes return false.
     * Saves when `bSave` is true.
     */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Campaign")
    bool Record(int32 Episode, const FString& Outcome, const FHavocCarryover& Carryover, bool bSave = true);

    /** Stored outcome, or empty if never played. */
    UFUNCTION(BlueprintPure, Category = "Havoc|Campaign")
    FString GetOutcome(int32 Episode) const;

    UFUNCTION(BlueprintPure, Category = "Havoc|Campaign")
    bool GetCarryover(int32 Episode, FHavocCarryover& OutCarryover) const;

    /** Episode 01 always; others when the previous ended in SUCCESS or PARTIAL (or "unlock all" is on). */
    UFUNCTION(BlueprintPure, Category = "Havoc|Campaign")
    bool IsUnlocked(int32 Episode) const;

    /** Passed = SUCCESS or PARTIAL. */
    UFUNCTION(BlueprintPure, Category = "Havoc|Campaign")
    bool IsPassed(int32 Episode) const;

    /** First episode not yet passed, or 0 when all four are. */
    UFUNCTION(BlueprintPure, Category = "Havoc|Campaign")
    int32 GetNextEpisode() const;

    /** Menu line for an episode: "Locked", "Unlocked" or "Completed: SUCCESS". */
    FString StatusText(int32 Episode) const;

    // ---- carryover ----
    /** What Episode 02 needs from Episode 01 (true if Episode 01 has no result yet). */
    UFUNCTION(BlueprintPure, Category = "Havoc|Campaign")
    bool WasCrewRecoveredInEpisode01() const;

    /** What Episode 03 needs from Episode 02. False if Episode 02 has no result. */
    UFUNCTION(BlueprintPure, Category = "Havoc|Campaign")
    bool GetEpisode02Carryover(FHavocCarryover& OutCarryover) const { return GetCarryover(2, OutCarryover); }

    /** Episode 03's outcome, passed to the Episode 04 mission: SUCCESS adds its reinforcement bonus. */
    UFUNCTION(BlueprintPure, Category = "Havoc|Campaign")
    FString GetEpisode3Outcome() const { return GetOutcome(3); }

    // ---- save / load ----
    UFUNCTION(BlueprintCallable, Category = "Havoc|Campaign")
    bool SaveProgress() const;

    /** Replaces the in-memory progress with the slot's. False (progress emptied) if there is no valid save. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Campaign")
    bool LoadProgress();

    /** Forgets all progress and deletes the slot. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Campaign")
    void ResetProgress();

    UFUNCTION(BlueprintPure, Category = "Havoc|Campaign")
    bool GetUnlockAll() const { return bUnlockAll; }

    UFUNCTION(BlueprintCallable, Category = "Havoc|Campaign")
    void SetUnlockAll(bool bValue);

private:
    const FHavocEpisodeResult* Find(int32 Episode) const;

    UPROPERTY() TArray<FHavocEpisodeResult> Results;
    bool bUnlockAll = false;
};
