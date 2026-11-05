#pragma once

// Pure squad rules, no world needed, so the automation tests can run them with a simulated clock.
// UHavocSquadSubsystem owns one FSquadModel per squad and applies the mode to the members.
// Tuning and phase deployments are read from data/ai/squad-behaviour.json (see game/docs/ai.md).

#include "CoreMinimal.h"
#include "AI/HavocSquadTypes.h"

namespace HavocSquad
{
    struct FTuning
    {
        float SquadTickSeconds = 0.25f;
        /** A threat reported this recently counts as known (Patrol -> Engage). */
        float LoseThreatSeconds = 6.f;
        /** Engage -> Patrol after this long with no threat. */
        float DisengageSeconds = 8.f;
        /** Engage -> Fallback when alive / size is at or below this (0..1). */
        float FallbackStrength = 0.4f;
        /** Fallback -> Patrol after at least this long, once the threat is lost. */
        float RegroupSeconds = 8.f;
        /** Fallback -> Patrol after this long even if the threat is still known. */
        float MaxFallbackSeconds = 20.f;
        float PatrolRadiusCm = 1800.f;
        float CoverSearchRadiusCm = 1500.f;
        float CoverGridSpacingCm = 250.f;
        bool bUseEqsCover = true;
    };

    /** One squad to place when a mission state is entered. */
    struct FDeployment
    {
        FString State;
        FString Faction;
        FName Sector;
        int32 Size = 3;
        EHavocSquadPosture Posture = EHavocSquadPosture::Patrol;
    };

    struct FConfig
    {
        FTuning Tuning;
        TArray<FDeployment> Deployments;

        TArray<FDeployment> ForState(const FString& State) const
        {
            return Deployments.FilterByPredicate([&State](const FDeployment& D) { return D.State == State; });
        }
    };

    /** coalition is allied, every other faction (iran, houthi, isis) is opposing. ISIS can never be Allied. */
    MIDNIGHTHAVOC_API EHavocTeam TeamForFaction(const FString& Faction);

    /** Parses data/ai/squad-behaviour.json. Returns false and sets Error on a bad file; unset tuning keys keep their defaults. */
    MIDNIGHTHAVOC_API bool ParseConfig(const FString& JsonText, FConfig& OutConfig, FString& OutError);

    inline float Strength(int32 Alive, int32 Size)
    {
        return Size > 0 ? FMath::Clamp(static_cast<float>(Alive) / static_cast<float>(Size), 0.f, 1.f) : 0.f;
    }

    /** One squad's shared state. Update() is the whole patrol / engage / fallback rule set. */
    struct FSquadModel
    {
        EHavocSquadMode Mode = EHavocSquadMode::Patrol;
        EHavocSquadPosture Posture = EHavocSquadPosture::Patrol;
        double ModeEnteredTime = 0.0;
        double LastThreatTime = -1.0e9;
        int32 Size = 0;

        void ReportThreat(double Now) { LastThreatTime = Now; }

        /** Returns true if the mode changed. */
        bool Update(const FTuning& Tuning, double Now, int32 Alive)
        {
            const double SinceThreat = Now - LastThreatTime;
            const double InMode = Now - ModeEnteredTime;
            const float Str = Strength(Alive, Size);
            const bool bWeak = Posture != EHavocSquadPosture::Assault && Str <= Tuning.FallbackStrength;
            EHavocSquadMode Next = Mode;

            switch (Mode)
            {
            case EHavocSquadMode::Patrol:
                if (SinceThreat <= Tuning.LoseThreatSeconds)
                {
                    Next = bWeak ? EHavocSquadMode::Fallback : EHavocSquadMode::Engage;
                }
                break;
            case EHavocSquadMode::Engage:
                if (bWeak)
                {
                    Next = EHavocSquadMode::Fallback;
                }
                else if (SinceThreat > Tuning.DisengageSeconds)
                {
                    Next = EHavocSquadMode::Patrol;
                }
                break;
            case EHavocSquadMode::Fallback:
                if ((InMode >= Tuning.RegroupSeconds && SinceThreat > Tuning.LoseThreatSeconds) || InMode >= Tuning.MaxFallbackSeconds)
                {
                    Next = EHavocSquadMode::Patrol;
                }
                break;
            }

            if (Next == Mode)
            {
                return false;
            }
            Mode = Next;
            ModeEnteredTime = Now;
            return true;
        }
    };
}
