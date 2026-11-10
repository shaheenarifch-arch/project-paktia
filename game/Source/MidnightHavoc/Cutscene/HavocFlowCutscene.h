#pragma once

// Shared intro/outro plumbing for the episode flows (Episodes 01-03). AHavocEpisodeFlowBase::PlayCutscene
// calls Play() for Episodes 01 and 02; Episode 03 (not on the base yet) calls it directly. It spawns the
// AHavocCutscene once, builds a 5 s-per-target fly-over and logs "HavocCutscene: <NN> intro|outro
// started|finished|skipped" (Cutscene/HavocCutscene.cpp), which game/scripts/smoke-run.ps1 checks.
// Episode 04 keeps its own copy (AHavocEpisode04Flow::PlayCutscene).

#include "CoreMinimal.h"

class AActor;
class AHavocCutscene;
class AHavocPlayerController;

namespace HavocFlowCutscene
{
    /** Subtitle lines for an episode ("01".."03"); the outro line depends on Outcome (SUCCESS / PARTIAL / FAILED). */
    MIDNIGHTHAVOC_API TArray<FString> BuildLines(const FString& Episode, bool bIntro, const FString& Outcome);

    /**
     * Plays the fly-over; Then runs when it finishes or is skipped, and straight away if it cannot start
     * (no camera actor; with no player or targets AHavocCutscene::Begin logs "missing" and finishes), so a
     * missing cutscene never blocks the episode.
     */
    MIDNIGHTHAVOC_API void Play(AActor& Owner, TObjectPtr<AHavocCutscene>& Slot, AHavocPlayerController* Player,
                                const FString& Episode, bool bIntro, const TArray<FVector>& Targets,
                                const TArray<FString>& Lines, TFunction<void()> Then);
}
