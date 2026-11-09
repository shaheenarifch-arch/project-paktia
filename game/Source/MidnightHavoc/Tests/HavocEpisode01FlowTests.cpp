// Automation tests for the Episode 01 flow (AHavocEpisode01Flow). Filter "MidnightHavoc.Missions.Episode01Flow".
// They drive the mission through the same calls the flow makes on E and keys 1/2 (InteractStep), and check
// the hint and tracker text. World-level behaviour is covered by the smoke test.

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Dom/JsonObject.h"
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

#define HAVOC_E01FLOW_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace HavocEpisode01FlowTests
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

    /** E presses until the crash decision (what the flow does from StartEpisode on). */
    void ReachDecision(FAutomationTestBase& Test, UHavocMountainShieldMission& M)
    {
        M.Progress();
        for (int32 i = 0; i < 20 && M.GetState() != TEXT("M01_CRASH_DECISION"); ++i)
        {
            Test.TestTrue(TEXT("E does something"), AHavocEpisode01Flow::InteractStep(M));
        }
        Test.TestEqual(TEXT("decision"), M.GetState(), FString(TEXT("M01_CRASH_DECISION")));
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHavocE01FlowRecoverTest, "MidnightHavoc.Missions.Episode01Flow.RecoverPath", HAVOC_E01FLOW_FLAGS)
bool FHavocE01FlowRecoverTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<UHavocMountainShieldMission> M = HavocEpisode01FlowTests::Make(*this);
    if (!M) { return false; }
    HavocEpisode01FlowTests::ReachDecision(*this, *M);
    TestTrue(TEXT("aegis online"), M->IsAegisOnline());
    TestFalse(TEXT("E does nothing at the decision"), AHavocEpisode01Flow::InteractStep(*M));

    M->ChooseCrew(true); // key 1
    TestEqual(TEXT("recovery"), M->GetState(), FString(TEXT("M01_CREW_RECOVERY")));
    for (int32 i = 0; i < M->GetCrewSize(); ++i)
    {
        TestTrue(TEXT("carry a crew member"), AHavocEpisode01Flow::InteractStep(*M));
    }
    TestEqual(TEXT("counterattack"), M->GetState(), FString(TEXT("M01_COUNTERATTACK")));
    TestTrue(TEXT("repel"), AHavocEpisode01Flow::InteractStep(*M));
    TestEqual(TEXT("debrief"), M->GetState(), FString(TEXT("M01_DEBRIEF")));
    TestTrue(TEXT("finish"), AHavocEpisode01Flow::InteractStep(*M));
    TestTrue(TEXT("complete"), M->GetRunner()->IsComplete());
    TestEqual(TEXT("outcome"), M->GetRunner()->GetOutcome(), FString(TEXT("SUCCESS")));
    TestTrue(TEXT("carryover"), M->GetCarryover().bCrewRecovered);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHavocE01FlowAbandonTest, "MidnightHavoc.Missions.Episode01Flow.AbandonPath", HAVOC_E01FLOW_FLAGS)
bool FHavocE01FlowAbandonTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<UHavocMountainShieldMission> M = HavocEpisode01FlowTests::Make(*this);
    if (!M) { return false; }
    HavocEpisode01FlowTests::ReachDecision(*this, *M);
    M->ChooseCrew(false); // key 2
    M->Progress();        // the flow's Tick does this
    TestEqual(TEXT("counterattack"), M->GetState(), FString(TEXT("M01_COUNTERATTACK")));
    TestTrue(TEXT("repel"), AHavocEpisode01Flow::InteractStep(*M));
    TestTrue(TEXT("finish"), AHavocEpisode01Flow::InteractStep(*M));
    TestEqual(TEXT("outcome"), M->GetRunner()->GetOutcome(), FString(TEXT("PARTIAL")));
    TestFalse(TEXT("crew not recovered"), M->GetCarryover().bCrewRecovered);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHavocE01FlowHintTest, "MidnightHavoc.Missions.Episode01Flow.HintAndTracker", HAVOC_E01FLOW_FLAGS)
bool FHavocE01FlowHintTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<UHavocMountainShieldMission> M = HavocEpisode01FlowTests::Make(*this);
    if (!M) { return false; }
    M->Progress();
    TestEqual(TEXT("tutorial"), M->GetState(), FString(TEXT("M01_DRONE_TUTORIAL")));
    const FString First = UHavocMountainShieldMission::GetTutorialPrompt(EHavocTutorialStep::Move);
    TestTrue(TEXT("hint starts with the step prompt"), AHavocEpisode01Flow::BuildHint(*M).StartsWith(First));
    AHavocEpisode01Flow::InteractStep(*M);
    TestEqual(TEXT("tutorial step advanced"), M->GetTutorialStep(), EHavocTutorialStep::Aim);
    TestTrue(TEXT("hint follows the step"), AHavocEpisode01Flow::BuildHint(*M).StartsWith(UHavocMountainShieldMission::GetTutorialPrompt(EHavocTutorialStep::Aim)));

    const TArray<FString> Tracker = AHavocEpisode01Flow::BuildTracker(TEXT("M01_HOLD_ECHO"), false);
    TestTrue(TEXT("first step done"), Tracker[0].StartsWith(TEXT("[x]")));
    TestTrue(TEXT("second step current"), Tracker[1].StartsWith(TEXT("[>]")));
    TestTrue(TEXT("third step pending"), Tracker[2].StartsWith(TEXT("[ ]")));
    for (const FString& Line : AHavocEpisode01Flow::BuildTracker(TEXT("M01_COMPLETE"), true))
    {
        TestTrue(TEXT("all done"), Line.StartsWith(TEXT("[x]")));
    }
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
