#pragma once

// Ports: src/gameplay/missions/episode04/OpenStrait.ts (Episode 04, "Operation Open Strait").
// A hypothetical scenario on real geography (data/missions/episode-04-open-strait.json,
// setting real_world_hypothetical). The state graph lives in the JSON; this class owns the
// island assault, the terminal, the lanes and the tankers. It registers the Episode 04 effect
// handlers and implements IHavocMissionSystems so checkpoints capture and restore them.
// Abstract game rules only: no real equipment or targeting data. Aegis stays abstract and is
// not ported here (see GetProtectedZones for the zones it must honour).

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Data/HavocMissionDef.h"
#include "Missions/HavocMissionSystems.h"
#include "HavocOpenStraitMission.generated.h"

class UHavocEventSubsystem;
class UHavocMissionRunner;
class UHavocOilTerminal;
class UHavocShippingLanes;
class UHavocTankerEscort;

/** Ports Episode04Rules (read from the mission JSON). */
USTRUCT(BlueprintType)
struct MIDNIGHTHAVOC_API FHavocEpisode04Rules
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Episode04")
    int32 BaseReinforcements = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Episode04")
    int32 BonusIfEpisode3Succeeded = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Episode04")
    int32 Tankers = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Episode04")
    double TerminalIntactThreshold = 0.0;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Episode04")
    double SupplyFlowTarget = 0.0;
};

/** Ports E04Result. */
USTRUCT(BlueprintType)
struct MIDNIGHTHAVOC_API FHavocEpisode04Result
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Episode04")
    int32 Episode = 4;

    /** "SUCCESS" / "PARTIAL" / "FAILED" or empty while running. */
    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Episode04")
    FString Outcome;

    /** "NORTH", "WEST" or empty if no landing has happened. */
    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Episode04")
    FString Beach;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Episode04")
    double TerminalIntegrity = 0.0;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Episode04")
    bool bCanExport = false;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Episode04")
    int32 TankersThrough = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Episode04")
    int32 TankersLost = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Episode04")
    double SupplyFlow = 0.0;
};

UCLASS()
class MIDNIGHTHAVOC_API UHavocOpenStraitMission : public UObject, public IHavocMissionSystems
{
    GENERATED_BODY()

public:
    /**
     * Builds the terminal, lanes, escort and runner. Fails (OutError set) unless the
     * definition's setting is real_world_hypothetical, or the runner cannot start.
     * @param Episode3Outcome "SUCCESS" adds BonusIfEpisode3Succeeded reinforcements; any other value (or empty) adds none.
     */
    bool Initialize(const FHavocMissionDef& Definition, const FString& Episode3Outcome, UHavocEventSubsystem* Events, FString& OutError);

    // ---- IHavocMissionSystems ----
    virtual FString SnapshotSystems() const override;
    virtual void RestoreSystems(const FString& SnapshotJson) override;

    // ---- Accessors ----
    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode04")
    UHavocMissionRunner* GetRunner() const { return Runner; }

    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode04")
    UHavocOilTerminal* GetTerminal() const { return Terminal; }

    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode04")
    UHavocShippingLanes* GetLanes() const { return Lanes; }

    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode04")
    UHavocTankerEscort* GetEscort() const { return Escort; }

    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode04")
    FString GetState() const;

    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode04")
    int32 GetReinforcements() const { return Reinforcements; }

    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode04")
    bool HasAirSupport() const { return bAirSupport; }

    const FHavocEpisode04Rules& GetRules() const { return Rules; }

    // ---- Mission flow (same names and rules as the TS) ----
    /** Advances until the runner stops moving (blocked or a branch/terminal state). */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode04")
    FString Progress();

    /** Raven passes over both beaches; marked defences cost reinforcements on landing. Only valid in E04_BEACH_RECON. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode04")
    bool ScoutBeaches(int32 NorthDefences, int32 WestDefences);

    /** BeachBranch is "north_beach" or "west_beach". */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode04")
    FString ChooseBeach(const FString& BeachBranch);

    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode04")
    void SecureBeachhead();

    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode04")
    void SecureAirstrip();

    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode04")
    void SecureTerminal();

    /** World input: damage to a terminal facility (stray fire, a fight inside it). */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode04")
    bool DamageTerminal(FName FacilityId, double Amount);

    /** World input: squads lost. At zero (terminal not yet secured) the assault fails. Returns reinforcements left. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode04")
    int32 SpendReinforcements(int32 Count);

    /** World input: a drone pass reveals hazards (LaneId "inbound"/"outbound"). Only during E04_LANE_CLEARANCE. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode04")
    int32 RevealHazards(FName LaneId, int32 Count);

    /** World input: a clearance team removes one revealed hazard. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode04")
    bool ClearHazard(FName LaneId);

    /** Only in E04_TANKER_ESCORT, and only on an open lane. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode04")
    bool StartTransit(const FString& TankerId, FName LaneId);

    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode04")
    void TankerArrived(const FString& TankerId);

    /** World input: damage to a tanker. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode04")
    void DamageTanker(const FString& TankerId, double Amount);

    /** Closes the episode: SUCCESS needs terminal integrity, export ability and supply flow at their thresholds; otherwise PARTIAL. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode04")
    FString Finish();

    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode04")
    FHavocEpisode04Result GetResult() const;

    /** Ports snapshot(): a JSON summary for HUDs and telemetry. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode04")
    FString GetSummaryJson() const;

    // ---- Protected zones (ports E04_PROTECTED) ----
    /** Terminal and town. Targeting/support systems must never select anything inside these. */
    static const TArray<FHavocProtectedZoneDef>& GetProtectedZones();

    /** True (and OutZoneId set) if the game-space position (metres) is inside a protected zone. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode04")
    static bool IsInProtectedZone(double X, double Y, double Z, FString& OutZoneId);

private:
    void CheckEscort();

    UPROPERTY()
    TObjectPtr<UHavocOilTerminal> Terminal;

    UPROPERTY()
    TObjectPtr<UHavocShippingLanes> Lanes;

    UPROPERTY()
    TObjectPtr<UHavocTankerEscort> Escort;

    UPROPERTY()
    TObjectPtr<UHavocMissionRunner> Runner;

    UPROPERTY()
    FHavocEpisode04Rules Rules;

    UPROPERTY()
    int32 Reinforcements = 0;

    UPROPERTY()
    bool bAirSupport = false;

    /** Marked defences from ScoutBeaches (not part of the checkpoint snapshot, as in the TS). */
    int32 NorthDefences = 0;
    int32 WestDefences = 0;
};
