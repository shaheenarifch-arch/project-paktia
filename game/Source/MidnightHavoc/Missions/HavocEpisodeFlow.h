#pragma once

// Shared interface for the playable loop of an episode. AHavocPlayerController, the HUD and the menus
// talk to this instead of to a concrete episode actor, so Episodes 01-03 can plug in beside
// AHavocEpisode04Flow. Episode-specific inputs (beach choice, Raven, Falcon) stay on the episode actor.
// Help: https://dev.epicgames.com/documentation/en-us/unreal-engine/interfaces-in-unreal-engine

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "HavocEpisodeFlow.generated.h"

UENUM(BlueprintType)
enum class EHavocFlowPhase : uint8
{
    Menu,
    Playing,
    Results
};

UINTERFACE(MinimalAPI, BlueprintType)
class UHavocEpisodeFlow : public UInterface
{
    GENERATED_BODY()
};

class MIDNIGHTHAVOC_API IHavocEpisodeFlow
{
    GENERATED_BODY()

public:
    /** Starts the episode from its first state. */
    virtual void StartEpisode() = 0;
    /** Pauses the game world (Playing phase only). */
    virtual void PauseEpisode() = 0;
    /** Resumes after PauseEpisode. */
    virtual void ResumeEpisode() = 0;
    /** Returns to the last checkpoint (after a fail state or from the pause menu). */
    virtual void RestartFromCheckpoint() = 0;
    /** The interact key (E). */
    virtual void HandleInteract() = 0;

    virtual EHavocFlowPhase GetPhase() const = 0;
    /** Objective text of the current mission state; empty before the episode starts. */
    virtual FString GetCurrentObjective() const = 0;
    virtual FString GetHint() const = 0;
    virtual FString GetToast() const = 0;
    virtual FString GetSupportStatus() const = 0;
    /** Result screen: summary text and outcome (SUCCESS / PARTIAL / FAILED). Valid in the Results phase. */
    virtual FString GetResultSummary() const = 0;
    virtual FString GetOutcome() const = 0;
};
