// Automation tests for the Episode 01 port. Mirrors the mission01 cases in src/tests/episodes.test.ts
// and src/tests/helicopter-crash.test.ts. Filter "MidnightHavoc.Episode01".

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Missions/Episode01/HavocMountainShield.h"

#define HAVOC_E01_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHavocE01TutorialTest, "MidnightHavoc.Episode01.Tutorial", HAVOC_E01_TEST_FLAGS)
bool FHavocE01TutorialTest::RunTest(const FString& Parameters)
{
    FHavocDroneTutorial T;
    TestEqual(TEXT("out-of-order step is ignored"), (int32)T.Advance(EHavocTutorialStep::MoveRaven), (int32)EHavocTutorialStep::LaunchRaven);
    T.Advance(EHavocTutorialStep::LaunchRaven);
    T.Advance(EHavocTutorialStep::MoveRaven);
    T.Advance(EHavocTutorialStep::ObserveContact);
    T.Advance(EHavocTutorialStep::ConfirmContact);
    TestFalse(TEXT("prompt shown before the last step"), T.GetPrompt().IsEmpty());
    T.Advance(EHavocTutorialStep::ReturnRaven);
    TestEqual(TEXT("tutorial completes"), (int32)T.Step, (int32)EHavocTutorialStep::Complete);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHavocE01RidgeStationTest, "MidnightHavoc.Episode01.RidgeStation", HAVOC_E01_TEST_FLAGS)
bool FHavocE01RidgeStationTest::RunTest(const FString& Parameters)
{
    FHavocMountainShield M;
    M.Transition(TEXT("M01_RIDGE_STATION_CAPTURED"));
    TestTrue(TEXT("capture brings Aegis online"), M.bAegisOnline && M.RidgeStation.IsAegisOnline());
    M.Transition(TEXT("M01_COMPLETE"));
    TestFalse(TEXT("Aegis off at completion"), M.bAegisOnline);
    M.RidgeStation.Destroy();
    M.RidgeStation.Capture();
    TestEqual(TEXT("destroyed station cannot be recaptured"), (int32)M.RidgeStation.State, (int32)EHavocRidgeStationState::Destroyed);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHavocE01CrashRecoveryTest, "MidnightHavoc.Episode01.CrashRecovery", HAVOC_E01_TEST_FLAGS)
bool FHavocE01CrashRecoveryTest::RunTest(const FString& Parameters)
{
    FHavocMountainShield M;
    M.Transition(TEXT("M01_COUNTERATTACK"));
    TestEqual(TEXT("counterattack preparing"), (int32)M.Counterattack.State, (int32)EHavocCounterattackState::Preparing);
    for (int32 i = 0; i < 20 && M.State != TEXT("M01_FINAL_DRONE_DEFENSE"); ++i)
    {
        M.ContinueCrashSequence();
    }
    TestEqual(TEXT("chain ends at final defense"), M.State, FString(TEXT("M01_FINAL_DRONE_DEFENSE")));
    TestTrue(TEXT("crew recovered"), M.WasCrewRecovered());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHavocE01AbandonTest, "MidnightHavoc.Episode01.Abandon", HAVOC_E01_TEST_FLAGS)
bool FHavocE01AbandonTest::RunTest(const FString& Parameters)
{
    FHavocMountainShield M;
    M.Transition(TEXT("M01_COUNTERATTACK"));
    for (int32 i = 0; i < 8; ++i) { M.ContinueCrashSequence(); }   // to M01_CREW_RECOVERY
    M.AbandonCrashRecovery();
    TestEqual(TEXT("abandon goes to final defense"), M.State, FString(TEXT("M01_FINAL_DRONE_DEFENSE")));
    TestFalse(TEXT("crew not recovered when abandoned"), M.WasCrewRecovered());
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
