// Automation tests for the intro/outro camera targets of Episodes 01-03 (no world needed): each episode has
// targets for both cutscenes, and every target lies inside its map. Filter "MidnightHavoc.Cutscene.Targets".
// Episode 04's own targets are sector-driven and covered by the smoke test.

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Missions/HavocMissionJson.h"
#include "Missions/Episode01/HavocEpisode01Flow.h"
#include "Missions/Episode02/HavocEpisode02Flow.h"
#include "Missions/Episode03/HavocEpisode03Flow.h"
#include "Cutscene/HavocFlowCutscene.h"

#define HAVOC_CUTSCENE_TARGET_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace HavocCutsceneTargetTests
{
    /** Half-width of a level's playable square in cm: the Paktia terrain is 20 km across (build_paktia_highlands_level.py TERRAIN_HALF). */
    constexpr double PaktiaHalfCm = 1000000.0;
    /** Gardun's core is 2.5 km; the greybox sits inside a 3 km square (Episode03Flow.h). */
    constexpr double GardunHalfCm = 400000.0;

    bool InsideSquare(const FVector& P, double HalfCm, const FVector& Centre = FVector::ZeroVector)
    {
        return FMath::Abs(P.X - Centre.X) <= HalfCm && FMath::Abs(P.Y - Centre.Y) <= HalfCm;
    }

    void CheckTargets(FAutomationTestBase& Test, const TCHAR* Label, const TArray<FVector>& Targets, double HalfCm, const FVector& Centre = FVector::ZeroVector)
    {
        Test.TestTrue(FString::Printf(TEXT("%s has targets"), Label), Targets.Num() > 0);
        for (const FVector& P : Targets)
        {
            Test.TestTrue(FString::Printf(TEXT("%s target %s is inside the map"), Label, *P.ToString()), InsideSquare(P, HalfCm, Centre));
        }
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHavocEpisode01CutsceneTargetsTest, "MidnightHavoc.Cutscene.Targets.Episode01", HAVOC_CUTSCENE_TARGET_FLAGS)
bool FHavocEpisode01CutsceneTargetsTest::RunTest(const FString& Parameters)
{
    using namespace HavocCutsceneTargetTests;
    // The sectors come from data/maps/paktia-highlands.json; the intro is staging -> echo -> ridge station.
    const TArray<FName> Intro = AHavocEpisode01Flow::CutsceneSectorIds(true);
    TestEqual(TEXT("intro has 3 shots"), Intro.Num(), 3);
    TestTrue(TEXT("intro order"), Intro.Num() == 3 && Intro[0] == TEXT("highlands_staging") && Intro[1] == TEXT("checkpoint_echo") && Intro[2] == TEXT("ridge_station"));

    FString Text;
    const FString Path = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../data/maps/paktia-highlands.json")));
    TArray<FString> MapSectors;
    if (FFileHelper::LoadFileToString(Text, *Path))
    {
        const TSharedRef<FJsonObject> Json = HavocMissionJson::FromString(Text);
        const TArray<TSharedPtr<FJsonValue>>* Sectors = nullptr;
        if (Json->TryGetArrayField(TEXT("sectors"), Sectors))
        {
            for (const TSharedPtr<FJsonValue>& V : *Sectors)
            {
                FString Id;
                if (V.IsValid() && V->AsObject().IsValid() && V->AsObject()->TryGetStringField(TEXT("id"), Id))
                {
                    MapSectors.Add(Id);
                }
            }
        }
    }
    else
    {
        AddError(TEXT("Could not load data/maps/paktia-highlands.json"));
    }
    for (const bool bIntro : { true, false })
    {
        TArray<FVector> Targets;
        for (const FName& Id : AHavocEpisode01Flow::CutsceneSectorIds(bIntro))
        {
            TestTrue(FString::Printf(TEXT("%s is a Paktia sector"), *Id.ToString()), MapSectors.Contains(Id.ToString()));
            const FVector Centre = AHavocEpisode01Flow::FallbackSectorCentre(Id);
            TestFalse(FString::Printf(TEXT("%s has a fallback centre"), *Id.ToString()), Centre.IsNearlyZero());
            Targets.Add(Centre);
        }
        CheckTargets(*this, bIntro ? TEXT("Episode 01 intro") : TEXT("Episode 01 outro"), Targets, PaktiaHalfCm);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHavocEpisode02CutsceneTargetsTest, "MidnightHavoc.Cutscene.Targets.Episode02", HAVOC_CUTSCENE_TARGET_FLAGS)
bool FHavocEpisode02CutsceneTargetsTest::RunTest(const FString& Parameters)
{
    using namespace HavocCutsceneTargetTests;
    // The greybox corridor floats above the Paktia map at GreyboxOrigin (default 3 km up).
    const FVector Origin(0.0, 0.0, 300000.0);
    TestEqual(TEXT("intro has 3 shots"), AHavocEpisode02Flow::BuildCutsceneTargets(Origin, true).Num(), 3);
    CheckTargets(*this, TEXT("Episode 02 intro"), AHavocEpisode02Flow::BuildCutsceneTargets(Origin, true), PaktiaHalfCm);
    CheckTargets(*this, TEXT("Episode 02 outro"), AHavocEpisode02Flow::BuildCutsceneTargets(Origin, false), PaktiaHalfCm);
    // The origin moves the whole route with it.
    const TArray<FVector> Moved = AHavocEpisode02Flow::BuildCutsceneTargets(Origin + FVector(1000.0, 0.0, 0.0), true);
    const TArray<FVector> Base = AHavocEpisode02Flow::BuildCutsceneTargets(Origin, true);
    TestTrue(TEXT("targets follow GreyboxOrigin"), Moved.Num() == Base.Num() && Moved[0].Equals(Base[0] + FVector(1000.0, 0.0, 0.0), 0.5));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHavocEpisode03CutsceneTargetsTest, "MidnightHavoc.Cutscene.Targets.Episode03", HAVOC_CUTSCENE_TARGET_FLAGS)
bool FHavocEpisode03CutsceneTargetsTest::RunTest(const FString& Parameters)
{
    using namespace HavocCutsceneTargetTests;
    const TArray<FName> Intro = AHavocEpisode03Flow::CutsceneSectorIds(true);
    TestTrue(TEXT("intro ends at the citadel"), Intro.Num() == 3 && Intro[0] == TEXT("south_gate") && Intro.Last() == TEXT("citadel"));
    const FVector Origin(0.0, 0.0, 300000.0);
    for (const bool bIntro : { true, false })
    {
        const TArray<FVector> Targets = AHavocEpisode03Flow::BuildGreyboxCutsceneTargets(Origin, bIntro);
        CheckTargets(*this, bIntro ? TEXT("Episode 03 intro") : TEXT("Episode 03 outro"), Targets, GardunHalfCm, FVector(Origin.X + 118000.0, Origin.Y + 110000.0, 0.0));
        for (const FName& Id : AHavocEpisode03Flow::CutsceneSectorIds(bIntro))
        {
            TestFalse(FString::Printf(TEXT("%s has a greybox position"), *Id.ToString()), AHavocEpisode03Flow::GetGreyboxSectorCentre(Id).IsNearlyZero());
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHavocFlowCutsceneLinesTest, "MidnightHavoc.Cutscene.Targets.Lines", HAVOC_CUTSCENE_TARGET_FLAGS)
bool FHavocFlowCutsceneLinesTest::RunTest(const FString& Parameters)
{
    for (const TCHAR* Episode : { TEXT("01"), TEXT("02"), TEXT("03") })
    {
        TestTrue(FString::Printf(TEXT("Episode %s intro has subtitles"), Episode), HavocFlowCutscene::BuildLines(Episode, true, FString()).Num() > 0);
        for (const TCHAR* Outcome : { TEXT("SUCCESS"), TEXT("PARTIAL"), TEXT("FAILED") })
        {
            TestTrue(FString::Printf(TEXT("Episode %s outro (%s) has subtitles"), Episode, Outcome), HavocFlowCutscene::BuildLines(Episode, false, Outcome).Num() > 0);
        }
    }
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
