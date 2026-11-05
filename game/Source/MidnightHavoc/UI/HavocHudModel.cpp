#include "UI/HavocHudModel.h"

#include "Missions/Episode04/HavocOpenStraitMission.h"
#include "Missions/Episode04/HavocShippingLanes.h"
#include "Missions/HavocMissionRunner.h"

namespace
{
    struct FStep { const TCHAR* Label; TArray<FString> States; };

    // The mission's phases in play order. The beach landing, terminal and airstrip states are grouped.
    const TArray<FStep>& Steps()
    {
        static const TArray<FStep> Value = {
            { TEXT("Recon both beaches with the Raven"), { TEXT("E04_BRIEFING"), TEXT("E04_BEACH_RECON"), TEXT("E04_CHOOSE_BEACH") } },
            { TEXT("Land and secure the beachhead"), { TEXT("E04_NORTH_LANDING"), TEXT("E04_WEST_LANDING") } },
            { TEXT("Secure the terminal and airstrip"), { TEXT("E04_TERMINAL_FIRST"), TEXT("E04_AIRSTRIP_FIRST"), TEXT("E04_TERMINAL_SECOND"), TEXT("E04_AIRSTRIP_SECOND") } },
            { TEXT("Hold the oil terminal"), { TEXT("E04_ISLAND_SECURED") } },
            { TEXT("Clear both shipping lanes"), { TEXT("E04_LANE_CLEARANCE") } },
            { TEXT("Escort the tankers"), { TEXT("E04_TANKER_ESCORT") } },
            { TEXT("Debrief"), { TEXT("E04_DEBRIEF"), TEXT("E04_STRAIT_OPEN"), TEXT("E04_STRAIT_OPEN_PARTIAL") } },
        };
        return Value;
    }

    int32 StepIndexFor(const FString& State)
    {
        const TArray<FStep>& All = Steps();
        for (int32 i = 0; i < All.Num(); ++i)
        {
            if (All[i].States.Contains(State))
            {
                return i;
            }
        }
        return INDEX_NONE;
    }
}

FHavocHudSnapshot HavocHudModel::Build(const UHavocOpenStraitMission* Mission)
{
    FHavocHudSnapshot S;
    if (!Mission)
    {
        return S;
    }
    S.State = Mission->GetState();
    if (const UHavocMissionRunner* Runner = Mission->GetRunner())
    {
        S.Objective = Runner->GetObjective();
    }
    S.Reinforcements = Mission->GetReinforcements();
    S.bAirSupport = Mission->HasAirSupport();
    const FHavocEpisode04Result R = Mission->GetResult();
    S.TerminalIntegrity = R.TerminalIntegrity;
    S.TankersThrough = R.TankersThrough;
    S.TankersLost = R.TankersLost;
    if (const UHavocShippingLanes* Lanes = Mission->GetLanes())
    {
        for (const FHavocLane& Lane : Lanes->GetLanes())
        {
            S.LaneHazards += Lane.Hazards;
            S.LaneRevealed += Lane.Revealed;
            S.LaneCleared += Lane.Cleared;
            S.LanesOpen += Lanes->IsOpen(Lane.Id) ? 1 : 0;
            ++S.LaneCount;
        }
    }
    return S;
}

FString FHavocHudSnapshot::LaneLine() const
{
    if (LaneHazards <= 0)
    {
        return FString();
    }
    return FString::Printf(TEXT("Lanes: %d of %d hazards cleared (%d found), %d of %d lanes open"),
        LaneCleared, LaneHazards, LaneRevealed, LanesOpen, LaneCount);
}

FString FHavocHudSnapshot::StatusBlock() const
{
    FString Text = FString::Printf(TEXT("Reinforcements: %d\nTerminal integrity: %d%%"), Reinforcements, FMath::RoundToInt(TerminalIntegrity * 100.0));
    const FString Lane = LaneLine();
    if (!Lane.IsEmpty() && (State == TEXT("E04_LANE_CLEARANCE") || StepIndexFor(State) >= 5))
    {
        Text += TEXT("\n") + Lane;
    }
    if (TankersThrough + TankersLost > 0 || State == TEXT("E04_TANKER_ESCORT"))
    {
        Text += FString::Printf(TEXT("\nTankers through: %d  lost: %d"), TankersThrough, TankersLost);
    }
    return Text;
}

TArray<FString> FHavocHudSnapshot::TrackerSteps()
{
    TArray<FString> Out;
    for (const FStep& Step : Steps())
    {
        Out.Add(Step.Label);
    }
    return Out;
}

FString FHavocHudSnapshot::TrackerText() const
{
    const int32 Current = StepIndexFor(State);
    const TArray<FString> Labels = TrackerSteps();
    FString Text;
    for (int32 i = 0; i < Labels.Num(); ++i)
    {
        // At the debrief everything before it is done; the last step stays the current one.
        const TCHAR* Mark = (Current == INDEX_NONE || i > Current) ? TEXT("[ ]") : (i == Current ? TEXT("[>]") : TEXT("[x]"));
        Text += FString::Printf(TEXT("%s %s%s"), Mark, *Labels[i], i + 1 < Labels.Num() ? TEXT("\n") : TEXT(""));
    }
    return Text;
}
