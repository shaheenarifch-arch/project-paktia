#pragma once

// Shared face of the playable episode loops (AHavocEpisode04Flow, AHavocEpisode02Flow). The player
// controller, HUD and menus talk to whichever flow is active through this class, so each episode only
// has to implement its own states. Nothing here is episode specific.
// Help: https://dev.epicgames.com/documentation/en-us/unreal-engine/actors-in-unreal-engine

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HavocEpisodeFlowBase.generated.h"

UENUM(BlueprintType)
enum class EHavocFlowPhase : uint8
{
    Menu,
    Playing,
    Results
};

UCLASS(Abstract)
class MIDNIGHTHAVOC_API AHavocEpisodeFlowBase : public AActor
{
    GENERATED_BODY()

public:
    // ---- Player actions (called by AHavocPlayerController and the menus); defaults do nothing ----
    UFUNCTION(BlueprintCallable, Category = "Havoc|Flow") virtual void StartEpisode() {}
    UFUNCTION(BlueprintCallable, Category = "Havoc|Flow") virtual void RestartFromCheckpoint() {}
    /** Keys 1 and 2: the first or second branch of a choice (Episode 04: north / west beach; Episode 02: riverbed / drainage). */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Flow") virtual void ChooseBranch(bool bFirst) {}
    UFUNCTION(BlueprintCallable, Category = "Havoc|Flow") virtual void HandleInteract() {}
    UFUNCTION(BlueprintCallable, Category = "Havoc|Flow") virtual void DeployRaven() {}
    UFUNCTION(BlueprintCallable, Category = "Havoc|Flow") virtual void CallFalcon() {}
    UFUNCTION(BlueprintCallable, Category = "Havoc|Flow") virtual void ReturnToMarine() {}

    // ---- Queries for the HUD and menus ----
    UFUNCTION(BlueprintPure, Category = "Havoc|Flow") EHavocFlowPhase GetPhase() const { return Phase; }
    UFUNCTION(BlueprintPure, Category = "Havoc|Flow") FString GetHint() const { return Hint; }
    UFUNCTION(BlueprintPure, Category = "Havoc|Flow") FString GetToast() const { return ToastTime > 0.f ? Toast : FString(); }
    UFUNCTION(BlueprintPure, Category = "Havoc|Flow") FString GetResultSummary() const { return ResultSummary; }
    UFUNCTION(BlueprintPure, Category = "Havoc|Flow") FString GetOutcome() const { return Outcome; }
    UFUNCTION(BlueprintPure, Category = "Havoc|Flow") virtual FString GetSupportStatus() const { return FString(); }
    UFUNCTION(BlueprintPure, Category = "Havoc|Flow") virtual bool IsPiloting() const { return false; }

    /** HUD text for flows that do not use the Episode 04 mission snapshot. Empty means "use the snapshot". */
    virtual FString GetObjectiveText() const { return FString(); }
    virtual FString GetTrackerText() const { return FString(); }
    virtual FString GetStatusText() const { return FString(); }

    /** Results screen headline for the current outcome ("SUCCESS" / "PARTIAL" / "FAILED"). */
    virtual FString GetResultTitle() const { return Outcome == TEXT("FAILED") ? TEXT("OPERATION FAILED") : TEXT("MISSION COMPLETE"); }
    virtual FString GetReplayLabel() const { return TEXT("Play again"); }

protected:
    void SetHint(const FString& Text) { Hint = Text; }
    void ShowToast(const FString& Text, float Seconds = 5.f)
    {
        Toast = Text;
        ToastTime = Seconds;
    }

    EHavocFlowPhase Phase = EHavocFlowPhase::Menu;
    FString Hint;
    FString Toast;
    float ToastTime = 0.f;
    FString ResultSummary;
    FString Outcome;
};
