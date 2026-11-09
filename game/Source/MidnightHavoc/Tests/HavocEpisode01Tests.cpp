// Automation tests for Episode 01 (Mountain Shield). Filter "MidnightHavoc.Missions.Episode01".
// Mirrors the Mission 01 rules in src/gameplay/missions/mission01/.

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "JsonObjectConverter.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/StrongObjectPtr.h"
#include "Data/HavocMissionDef.h"
#include "Missions/HavocMissionJson.h"
#include "Missions/HavocMissionRunner.h"
#include "Missions/Episode01/HavocEpisode01Flow.h"
#include "Missions/Episode01/HavocMountainShieldMission.h"
#include "Missions/HavocEpisodeFlow.h"

#define HAVOC_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace HavocEpisode01Tests
{
    TStrongObjectPtr<UHavocMountainShieldMission> Make(FAutomationTestBase& Test)
    {
        TStrongObjectPtr<UHavocMountainShieldMission> Mission;
        const FString Path = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../data/missions/episode-01-mountain-shield.json")));
        FString Text;
        FHavocMissionDef Def;
        if (!FFileHelper::LoadFileToString(Text, *Path))
        {
            Test.AddError(TEXT("Could not load data/missions/episode-01-mountain-shield.json"));
            return Mission;
        }
        const TSharedRef<FJsonObject> Json = HavocMissionJson::FromString(Text);
        if (!FJsonObjectConverter::JsonObjectToUStruct<FHavocMissionDef>(Json, &Def, 0, 0))
        {
            Test.AddError(TEXT("Episode 01 JSON did not parse"));
            return Mission;
        }
        const TArray<TSharedPtr<FJsonObject>> States = HavocMissionJson::ReadObjectArray(*Json, TEXT("states"));
        for (int32 i = 0; i < States.Num() && i < Def.States.Num(); ++i)
        {
            States[i]->TryGetBoolField(TEXT("checkpoint"), Def.States[i].bCheckpoint);
            States[i]->TryGetBoolField(TEXT("terminal"), Def.States[i].bTerminal);
        }
        Mission.Reset(NewObject<UHavocMountainShieldMission>(GetTransientPackage()));
        FString Error;
        if (!Mission->Initialize(Def, nullptr, Error))
        {
            Test.AddError(Error);
            Mission.Reset();
        }
        return Mission;
    }

    void FinishTutorial(UHavocMountainShieldMission& M)
    {
        M.Progress();
        for (int32 s = 0; s < static_cast<int32>(EHavocTutorialStep::Complete); ++s)
        {
            M.AdvanceTutorial(static_cast<EHavocTutorialStep>(s));
        }
        M.Progress();
    }

    void ReachCrashDecision(FAutomationTestBase& Test, UHavocMountainShieldMission& M)
    {
        FinishTutorial(M);
        Test.TestEqual(TEXT("hold echo"), M.GetState(), FString(TEXT("M01_HOLD_ECHO")));
        M.HoldEcho(); M.Progress();
        M.SetUpRidgeStation(); M.Progress();
        Test.TestTrue(TEXT("aegis online at Ridge Station"), M.IsAegisOnline());
        Test.TestEqual(TEXT("falcon down"), M.GetState(), FString(TEXT("M01_FALCON_DOWN")));
        M.NotifyCrashSeen(); M.Progress();
        Test.TestEqual(TEXT("decision"), M.GetState(), FString(TEXT("M01_CRASH_DECISION")));
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHavocE01TutorialTest, "MidnightHavoc.Missions.Episode01.Tutorial", HAVOC_TEST_FLAGS)
bool FHavocE01TutorialTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<UHavocMountainShieldMission> M = HavocEpisode01Tests::Make(*this);
    if (!M) { return false; }
    M->Progress();
    TestEqual(TEXT("tutorial blocks until done"), M->GetState(), FString(TEXT("M01_DRONE_TUTORIAL")));
    TestEqual(TEXT("out-of-order step ignored"), M->AdvanceTutorial(EHavocTutorialStep::LaunchRaven), EHavocTutorialStep::Move);
    TestEqual(TEXT("move"), M->AdvanceTutorial(EHavocTutorialStep::Move), EHavocTutorialStep::Aim);
    TestFalse(TEXT("every step has a prompt"), UHavocMountainShieldMission::GetTutorialPrompt(EHavocTutorialStep::Aim).IsEmpty());
    HavocEpisode01Tests::FinishTutorial(*M);
    TestEqual(TEXT("tutorial done"), M->GetState(), FString(TEXT("M01_HOLD_ECHO")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHavocE01RecoverTest, "MidnightHavoc.Missions.Episode01.CrewRecovered", HAVOC_TEST_FLAGS)
bool FHavocE01RecoverTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<UHavocMountainShieldMission> M = HavocEpisode01Tests::Make(*this);
    if (!M) { return false; }
    HavocEpisode01Tests::ReachCrashDecision(*this, *M);
    M->ChooseCrew(true);
    TestEqual(TEXT("recovery"), M->GetState(), FString(TEXT("M01_CREW_RECOVERY")));
    M->RecoverCrewMember(); M->RecoverCrewMember();
    M->Progress();
    TestEqual(TEXT("needs all three crew"), M->GetState(), FString(TEXT("M01_CREW_RECOVERY")));
    M->RecoverCrewMember(); M->Progress();
    TestEqual(TEXT("counterattack"), M->GetState(), FString(TEXT("M01_COUNTERATTACK")));
    TestTrue(TEXT("repelled"), M->RepelCounterattack());
    M->Finish();
    TestEqual(TEXT("success"), M->GetState(), FString(TEXT("M01_COMPLETE")));
    const FHavocEpisode01Result R = M->GetCarryover();
    TestEqual(TEXT("outcome"), R.Outcome, FString(TEXT("SUCCESS")));
    TestTrue(TEXT("crew recovered carryover"), R.bCrewRecovered);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHavocE01AbandonTest, "MidnightHavoc.Missions.Episode01.CrewAbandonedPartial", HAVOC_TEST_FLAGS)
bool FHavocE01AbandonTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<UHavocMountainShieldMission> M = HavocEpisode01Tests::Make(*this);
    if (!M) { return false; }
    HavocEpisode01Tests::ReachCrashDecision(*this, *M);
    M->ChooseCrew(false);
    M->Progress();
    TestEqual(TEXT("counterattack"), M->GetState(), FString(TEXT("M01_COUNTERATTACK")));
    M->RepelCounterattack();
    M->Finish();
    TestEqual(TEXT("partial"), M->GetState(), FString(TEXT("M01_COMPLETE_PARTIAL")));
    const FHavocEpisode01Result R = M->GetCarryover();
    TestEqual(TEXT("outcome"), R.Outcome, FString(TEXT("PARTIAL")));
    TestFalse(TEXT("crew not recovered"), R.bCrewRecovered);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHavocE01FailRestoreTest, "MidnightHavoc.Missions.Episode01.LoseAndRestore", HAVOC_TEST_FLAGS)
bool FHavocE01FailRestoreTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<UHavocMountainShieldMission> M = HavocEpisode01Tests::Make(*this);
    if (!M) { return false; }
    HavocEpisode01Tests::ReachCrashDecision(*this, *M);
    M->LoseCheckpoint();
    TestEqual(TEXT("failed"), M->GetState(), FString(TEXT("M01_OPERATION_FAILED")));
    TestTrue(TEXT("restore from checkpoint"), M->GetRunner()->RestoreCheckpoint());
    TestEqual(TEXT("back at last checkpoint"), M->GetState(), FString(TEXT("M01_RIDGE_STATION")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHavocE01FlowHelpersTest, "MidnightHavoc.Missions.Episode01.FlowHelpers", HAVOC_TEST_FLAGS)
bool FHavocE01FlowHelpersTest::RunTest(const FString& Parameters)
{
    // The flow plugs into the controller, HUD and menus through the shared interface.
    TestTrue(TEXT("flow implements IHavocEpisodeFlow"), AHavocEpisode01Flow::StaticClass()->ImplementsInterface(UHavocEpisodeFlow::StaticClass()));

    // Tracker: [x] before the current step, [>] on it, [ ] after; everything [x] when complete.
    const TArray<FString> Hold = AHavocEpisode01Flow::BuildTracker(TEXT("M01_HOLD_ECHO"), false);
    TestEqual(TEXT("seven steps"), Hold.Num(), 7);
    TestTrue(TEXT("tutorial done"), Hold[0].StartsWith(TEXT("[x]")));
    TestTrue(TEXT("echo is current"), Hold[1].StartsWith(TEXT("[>]")));
    TestTrue(TEXT("ridge pending"), Hold[2].StartsWith(TEXT("[ ]")));
    const TArray<FString> Abandoned = AHavocEpisode01Flow::BuildTracker(TEXT("M01_ABANDONED"), false);
    TestTrue(TEXT("crew step covers the abandoned state"), Abandoned[4].StartsWith(TEXT("[>]")));
    for (const FString& Line : AHavocEpisode01Flow::BuildTracker(TEXT("M01_COMPLETE"), true))
    {
        TestTrue(TEXT("complete marks every step"), Line.StartsWith(TEXT("[x]")));
    }

    // Result text.
    const FString Success = AHavocEpisode01Flow::BuildSummary(TEXT("SUCCESS"), 3, 3, true);
    TestTrue(TEXT("success names the crew"), Success.Contains(TEXT("Falcon-2 crew is home")) && Success.Contains(TEXT("3 of 3")) && Success.Contains(TEXT("Aegis: online")));
    const FString Partial = AHavocEpisode01Flow::BuildSummary(TEXT("PARTIAL"), 0, 3, true);
    TestTrue(TEXT("partial says the crew was left"), Partial.Contains(TEXT("left behind")) && Partial.Contains(TEXT("0 of 3")));
    TestTrue(TEXT("failure says what was lost"), AHavocEpisode01Flow::BuildSummary(TEXT("FAILED"), 0, 3, false).Contains(TEXT("was lost")));

    // Greybox layout: every sector the flow looks up has its own point.
    TSet<FString> Seen;
    for (const FName& Id : AHavocEpisode01Flow::GetSectorIds())
    {
        Seen.Add(AHavocEpisode01Flow::GetGreyboxPoint(Id).ToString());
    }
    TestEqual(TEXT("four distinct sector points"), Seen.Num(), 4);
    TestTrue(TEXT("unknown sector is zero"), AHavocEpisode01Flow::GetGreyboxPoint(TEXT("nowhere")).IsZero());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHavocE01SectorIdsTest, "MidnightHavoc.Missions.Episode01.SectorIdsMatchData", HAVOC_TEST_FLAGS)
bool FHavocE01SectorIdsTest::RunTest(const FString& Parameters)
{
    // The flow finds places by sector id, so the ids must be the ones in data/missions and data/maps.
    const FString Path = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../data/missions/episode-01-mountain-shield.json")));
    FString Text;
    if (!FFileHelper::LoadFileToString(Text, *Path))
    {
        AddError(TEXT("Could not load data/missions/episode-01-mountain-shield.json"));
        return false;
    }
    const TSharedRef<FJsonObject> Json = HavocMissionJson::FromString(Text);
    const TArray<TSharedPtr<FJsonValue>>* Sectors = nullptr;
    if (!Json->TryGetArrayField(TEXT("sectors"), Sectors))
    {
        AddError(TEXT("Episode 01 JSON has no sectors"));
        return false;
    }
    TSet<FString> Ids;
    for (const TSharedPtr<FJsonValue>& V : *Sectors)
    {
        Ids.Add(V->AsString());
    }
    for (const FName& Id : AHavocEpisode01Flow::GetSectorIds())
    {
        TestTrue(*FString::Printf(TEXT("sector '%s' is in the mission data"), *Id.ToString()), Ids.Contains(Id.ToString()));
    }
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
