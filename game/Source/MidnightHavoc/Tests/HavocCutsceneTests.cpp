// Automation tests for the intro/outro fly-over maths (no world needed).
// Run: Tools > Session Frontend > Automation, filter "MidnightHavoc.Cutscene".

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Cutscene/HavocCutsceneMath.h"
#include "Misc/AutomationTest.h"

#define HAVOC_CUTSCENE_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHavocCutsceneShotsTest, "MidnightHavoc.Cutscene.ShotsAndSubtitles", HAVOC_CUTSCENE_TEST_FLAGS)
bool FHavocCutsceneShotsTest::RunTest(const FString& Parameters)
{
    const TArray<FVector> Targets = { FVector(0, 0, 0), FVector(10000, 0, 0), FVector(20000, 0, 0) };
    const TArray<FString> Lines = { TEXT("one"), TEXT("two") };
    const TArray<FHavocCutsceneShot> Shots = HavocCutsceneMath::BuildFlyover(Targets, Lines, 5.f);

    TestEqual(TEXT("one shot per target"), Shots.Num(), 3);
    TestEqual(TEXT("total length"), HavocCutsceneMath::TotalSeconds(Shots), 15.f);
    TestEqual(TEXT("first subtitle"), Shots[0].Subtitle, FString(TEXT("one")));
    TestTrue(TEXT("missing subtitle is blank"), Shots[2].Subtitle.IsEmpty());

    float Alpha = 0.f;
    TestEqual(TEXT("t=0 is shot 0"), HavocCutsceneMath::ShotAt(Shots, 0.f, Alpha), 0);
    TestEqual(TEXT("t=7.5 is shot 1"), HavocCutsceneMath::ShotAt(Shots, 7.5f, Alpha), 1);
    TestTrue(TEXT("halfway through shot 1"), FMath::IsNearlyEqual(Alpha, 0.5f, 0.001f));
    TestEqual(TEXT("past the end ends the cutscene"), HavocCutsceneMath::ShotAt(Shots, 15.f, Alpha), (int32)INDEX_NONE);
    TestEqual(TEXT("no shots, no cutscene"), HavocCutsceneMath::ShotAt(TArray<FHavocCutsceneShot>(), 0.f, Alpha), (int32)INDEX_NONE);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHavocCutsceneCameraTest, "MidnightHavoc.Cutscene.CameraLooksAtTarget", HAVOC_CUTSCENE_TEST_FLAGS)
bool FHavocCutsceneCameraTest::RunTest(const FString& Parameters)
{
    FHavocCutsceneShot Shot;
    Shot.Target = FVector(1000, 2000, 300);
    Shot.StartYawDeg = 0.f;
    Shot.EndYawDeg = 90.f;
    Shot.DistanceCm = 5000.f;
    Shot.HeightCm = 2000.f;

    const FVector Start = HavocCutsceneMath::CameraLocation(Shot, 0.f);
    const FVector End = HavocCutsceneMath::CameraLocation(Shot, 1.f);
    TestTrue(TEXT("starts east of the target"), Start.Equals(FVector(6000, 2000, 2300), 0.5f));
    TestTrue(TEXT("ends north of the target"), End.Equals(FVector(1000, 7000, 2300), 0.5f));
    for (float A : { 0.f, 0.3f, 1.f })
    {
        const FVector Loc = HavocCutsceneMath::CameraLocation(Shot, A);
        const FVector Ahead = Loc + HavocCutsceneMath::CameraRotation(Shot, Loc).Vector() * FVector::Dist(Loc, Shot.Target);
        TestTrue(TEXT("camera faces the target"), Ahead.Equals(Shot.Target, 1.f));
    }
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
