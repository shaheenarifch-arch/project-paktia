#pragma once

// Pure view-model for the tactical HUD: reads the Episode 04 mission (objective, terminal, lanes,
// escort) into plain values so widgets only format and draw them, and an automation test can check
// the numbers through a whole run without a world. No widgets here.
// Help: https://dev.epicgames.com/documentation/en-us/unreal-engine/umg-ui-designer-for-unreal-engine

#include "CoreMinimal.h"

class UHavocOpenStraitMission;

struct MIDNIGHTHAVOC_API FHavocHudSnapshot
{
    FString State;
    FString Objective;
    int32 Reinforcements = 0;
    double TerminalIntegrity = 1.0;
    int32 TankersThrough = 0;
    int32 TankersLost = 0;
    int32 LaneHazards = 0;
    int32 LaneRevealed = 0;
    int32 LaneCleared = 0;
    int32 LanesOpen = 0;
    int32 LaneCount = 0;
    bool bAirSupport = false;

    /** Lane progress, e.g. "Lanes: 2 of 7 hazards cleared (1 found, 0 of 2 lanes open)". Empty before lane clearance. */
    FString LaneLine() const;

    /** Terminal, reinforcements and escort lines for the status block. */
    FString StatusBlock() const;

    /** Ordered mission steps with a [x] / [>] / [ ] marker, for the objective tracker. */
    static TArray<FString> TrackerSteps();
    FString TrackerText() const;
};

namespace HavocHudModel
{
    /** Null mission gives a default snapshot. */
    MIDNIGHTHAVOC_API FHavocHudSnapshot Build(const UHavocOpenStraitMission* Mission);
}
