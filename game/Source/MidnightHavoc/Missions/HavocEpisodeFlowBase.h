#pragma once

// Common base for episode flows that share hint/toast/result bookkeeping (AHavocEpisode02Flow). It
// implements IHavocEpisodeFlow (Missions/HavocEpisodeFlow.h), which is what the player controller, HUD
// and menus talk to, so a flow built on this base plugs in beside AHavocEpisode04Flow.
// Help: https://dev.epicgames.com/documentation/en-us/unreal-engine/interfaces-in-unreal-engine

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "Missions/HavocEpisodeFlow.h"
#include "HavocEpisodeFlowBase.generated.h"

class AHavocCutscene;
class AHavocPlayerController;

// EHavocFlowPhase is declared in Missions/HavocEpisodeFlow.h.
UCLASS(Abstract)
class MIDNIGHTHAVOC_API AHavocEpisodeFlowBase : public AActor, public IHavocEpisodeFlow
{
    GENERATED_BODY()

public:
    // ---- Player actions (called by AHavocPlayerController and the menus); defaults do nothing ----
    UFUNCTION(BlueprintCallable, Category = "Havoc|Flow") virtual void StartEpisode() override {}
    UFUNCTION(BlueprintCallable, Category = "Havoc|Flow") virtual void RestartFromCheckpoint() override {}
    /** Keys 1 and 2: the first or second branch of a choice (Episode 04: north / west beach; Episode 02: riverbed / drainage). */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Flow") virtual void ChooseBranch(bool bFirst) {}
    UFUNCTION(BlueprintCallable, Category = "Havoc|Flow") virtual void HandleInteract() override {}
    UFUNCTION(BlueprintCallable, Category = "Havoc|Flow") virtual void DeployRaven() {}
    UFUNCTION(BlueprintCallable, Category = "Havoc|Flow") virtual void CallFalcon() {}
    UFUNCTION(BlueprintCallable, Category = "Havoc|Flow") virtual void ReturnToMarine() {}

    // ---- Queries for the HUD and menus ----
    UFUNCTION(BlueprintPure, Category = "Havoc|Flow") virtual EHavocFlowPhase GetPhase() const override { return Phase; }
    UFUNCTION(BlueprintPure, Category = "Havoc|Flow") virtual FString GetHint() const override { return Hint; }
    UFUNCTION(BlueprintPure, Category = "Havoc|Flow") virtual FString GetToast() const override { return ToastTime > 0.f ? Toast : FString(); }
    UFUNCTION(BlueprintPure, Category = "Havoc|Flow") virtual FString GetResultSummary() const override { return ResultSummary; }
    UFUNCTION(BlueprintPure, Category = "Havoc|Flow") virtual FString GetOutcome() const override { return Outcome; }
    UFUNCTION(BlueprintPure, Category = "Havoc|Flow") virtual FString GetSupportStatus() const override { return FString(); }
    UFUNCTION(BlueprintPure, Category = "Havoc|Flow") virtual bool IsPiloting() const { return false; }

    // ---- IHavocEpisodeFlow (pause, objective) ----
    virtual void PauseEpisode() override
    {
        if (Phase == EHavocFlowPhase::Playing)
        {
            UGameplayStatics::SetGamePaused(this, true);
        }
    }
    virtual void ResumeEpisode() override { UGameplayStatics::SetGamePaused(this, false); }
    virtual FString GetCurrentObjective() const override { return Phase == EHavocFlowPhase::Playing ? GetObjectiveText() : FString(); }

    /** HUD text for flows that do not use the Episode 04 mission snapshot. Empty means "use the snapshot". */
    virtual FString GetObjectiveText() const { return FString(); }
    virtual FString GetTrackerText() const { return FString(); }
    virtual FString GetStatusText() const { return FString(); }

    /** Results screen headline for the current outcome ("SUCCESS" / "PARTIAL" / "FAILED"). */
    virtual FString GetResultTitle() const { return Outcome == TEXT("FAILED") ? TEXT("OPERATION FAILED") : TEXT("MISSION COMPLETE"); }
    virtual FString GetReplayLabel() const { return TEXT("Play again"); }

    // ---- Cutscenes (skippable fly-over with subtitles; Cutscene/HavocCutscene.h) ----
    /** Plays the intro or outro by itself from StartEpisode / the end of the episode; also used by the smoke test. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Cutscene") void PlayIntro() { PlayCutscene(true, nullptr); }
    UFUNCTION(BlueprintCallable, Category = "Havoc|Cutscene") void PlayOutro() { PlayCutscene(false, nullptr); }
    UFUNCTION(BlueprintCallable, Category = "Havoc|Cutscene") void SkipCutscene();
    UFUNCTION(BlueprintPure, Category = "Havoc|Cutscene") bool IsCutsceneActive() const;
    UFUNCTION(BlueprintPure, Category = "Havoc|Cutscene") FString GetCutsceneSubtitle() const;

protected:
    /**
     * Plays the intro (bIntro) or outro as a fly-over of CutsceneTargets(bIntro). Then runs when it finishes or
     * is skipped (or straight away if it cannot start). Logs "HavocCutscene: <CutsceneLabel()> intro|outro ...".
     */
    void PlayCutscene(bool bIntro, TFunction<void()> Then);
    /** World positions (cm) the camera visits: one 5 s shot each. An episode's sector volumes, with a fallback. */
    virtual TArray<FVector> CutsceneTargets(bool bIntro) { return TArray<FVector>(); }
    /** Episode number as logged ("01".."03"). */
    virtual FString CutsceneLabel() const { return FString(); }

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

    UPROPERTY() TObjectPtr<AHavocCutscene> Cutscene;
};
