#pragma once

// Pure maths for the intro/outro fly-overs (no world needed, so it is unit-tested:
// Tests/HavocCutsceneTests.cpp). A cutscene is a list of shots; each shot orbits a target point on an
// arc (start yaw -> end yaw) at a fixed distance and height, always looking at the target, and carries
// one subtitle line. Help: https://dev.epicgames.com/documentation/en-us/unreal-engine/cinematics-and-movie-making-in-unreal-engine

#include "CoreMinimal.h"

struct FHavocCutsceneShot
{
    FVector Target = FVector::ZeroVector; // world point the camera looks at (cm)
    float StartYawDeg = 0.f;              // position on the arc around the target
    float EndYawDeg = 60.f;
    float DistanceCm = 9000.f;            // horizontal distance from the target
    float HeightCm = 5000.f;              // camera height above the target
    float Seconds = 4.f;
    FString Subtitle;
};

namespace HavocCutsceneMath
{
    inline float TotalSeconds(const TArray<FHavocCutsceneShot>& Shots)
    {
        float Total = 0.f;
        for (const FHavocCutsceneShot& Shot : Shots) { Total += FMath::Max(0.f, Shot.Seconds); }
        return Total;
    }

    /** Which shot is on screen at time T, and how far through it (0..1). Returns INDEX_NONE once T is past the end. */
    inline int32 ShotAt(const TArray<FHavocCutsceneShot>& Shots, float T, float& OutAlpha)
    {
        OutAlpha = 0.f;
        if (T < 0.f) { return Shots.Num() > 0 ? 0 : INDEX_NONE; }
        float Start = 0.f;
        for (int32 i = 0; i < Shots.Num(); ++i)
        {
            const float Len = FMath::Max(0.f, Shots[i].Seconds);
            if (T < Start + Len)
            {
                OutAlpha = Len > 0.f ? (T - Start) / Len : 1.f;
                return i;
            }
            Start += Len;
        }
        return INDEX_NONE;
    }

    /** Camera location for a shot at Alpha 0..1 (smoothstep eased so the fly-over starts and ends gently). */
    inline FVector CameraLocation(const FHavocCutsceneShot& Shot, float Alpha)
    {
        const float Eased = FMath::SmoothStep(0.f, 1.f, FMath::Clamp(Alpha, 0.f, 1.f));
        const float Yaw = FMath::DegreesToRadians(FMath::Lerp(Shot.StartYawDeg, Shot.EndYawDeg, Eased));
        return Shot.Target + FVector(FMath::Cos(Yaw) * Shot.DistanceCm, FMath::Sin(Yaw) * Shot.DistanceCm, Shot.HeightCm);
    }

    inline FRotator CameraRotation(const FHavocCutsceneShot& Shot, const FVector& CameraLoc)
    {
        return (Shot.Target - CameraLoc).Rotation();
    }

    /** Builds one shot per target, each with its subtitle line (extra lines are ignored, missing ones are blank). */
    inline TArray<FHavocCutsceneShot> BuildFlyover(const TArray<FVector>& Targets, const TArray<FString>& Lines, float SecondsPerShot)
    {
        TArray<FHavocCutsceneShot> Shots;
        for (int32 i = 0; i < Targets.Num(); ++i)
        {
            FHavocCutsceneShot Shot;
            Shot.Target = Targets[i];
            Shot.StartYawDeg = -40.f + 25.f * i;
            Shot.EndYawDeg = Shot.StartYawDeg + 55.f;
            Shot.Seconds = SecondsPerShot;
            Shot.Subtitle = Lines.IsValidIndex(i) ? Lines[i] : FString();
            Shots.Add(Shot);
        }
        return Shots;
    }
}
