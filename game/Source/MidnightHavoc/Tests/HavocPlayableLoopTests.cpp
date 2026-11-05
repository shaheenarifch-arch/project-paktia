// Automation tests for the playable loop of Episode 04 (item 4 of the real-playable plan).
// They drive UHavocOpenStraitMission in the same order AHavocEpisode04Flow does (single Advance per
// secured sector, a hold at E04_ISLAND_SECURED, lane clearance, tanker escort, debrief, checkpoint
// restore with a death penalty, fail state), and check the data the loop depends on (enemy units for
// Iran, Houthi and ISIS; ISIS AI-only). World-level behaviour (AI, possession, HUD) is covered by the
// manual checklist in the pull request; see game/README.md "Running tests".
// Run: Tools > Session Frontend > Automation, filter "MidnightHavoc.Playable".

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Dom/JsonObject.h"
#include "JsonObjectConverter.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/StrongObjectPtr.h"
#include "Data/HavocMissionDef.h"
#include "Data/HavocUnitDef.h"
#include "Missions/HavocMissionJson.h"
#include "Missions/HavocMissionRunner.h"
#include "Missions/Episode04/HavocOpenStraitMission.h"
#include "Missions/Episode04/HavocShippingLanes.h"
#include "Missions/Episode04/HavocTankerEscort.h"

#define HAVOC_PLAY_TEST_FLAGS (EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace HavocPlayableTests
{
    FString DataPath(const TCHAR* Relative)
    {
        return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../data"), Relative));
    }

    TStrongObjectPtr<UHavocOpenStraitMission> MakeMission(FAutomationTestBase& Test)
    {
        TStrongObjectPtr<UHavocOpenStraitMission> Mission;
        FString Text;
        if (!FFileHelper::LoadFileToString(Text, *DataPath(TEXT("missions/episode-04-open-strait.json"))))
        {
            Test.AddError(TEXT("Could not read data/missions/episode-04-open-strait.json"));
            return Mission;
        }
        const TSharedRef<FJsonObject> Json = HavocMissionJson::FromString(Text);
        FHavocMissionDef Def;
        if (!FJsonObjectConverter::JsonObjectToUStruct<FHavocMissionDef>(Json, &Def, 0, 0))
        {
            Test.AddError(TEXT("Could not parse the Episode 04 mission"));
            return Mission;
        }
        Json->TryGetBoolField(TEXT("fictional"), Def.bFictional);
        const TArray<TSharedPtr<FJsonObject>> States = HavocMissionJson::ReadObjectArray(*Json, TEXT("states"));
        for (int32 i = 0; i < States.Num() && i < Def.States.Num(); ++i)
        {
            States[i]->TryGetBoolField(TEXT("checkpoint"), Def.States[i].bCheckpoint);
            States[i]->TryGetBoolField(TEXT("terminal"), Def.States[i].bTerminal);
        }
        Mission.Reset(NewObject<UHavocOpenStraitMission>(GetTransientPackage()));
        FString Error;
        if (!Mission->Initialize(Def, TEXT(""), nullptr, Error))
        {
            Test.AddError(Error);
            Mission.Reset();
        }
        return Mission;
    }

    /** What the flow does from the briefing to the end of the hold, one Advance per secured sector. */
    void PlayToHold(FAutomationTestBase& Test, UHavocOpenStraitMission& M, bool bNorth)
    {
        M.Progress(); // StartEpisode: briefing -> beach recon
        Test.TestEqual(TEXT("recon"), M.GetState(), FString(TEXT("E04_BEACH_RECON")));
        Test.TestTrue(TEXT("Raven scouts both beaches"), M.ScoutBeaches(6, 3));
        M.Progress(); // -> choose beach
        Test.TestEqual(TEXT("choose"), M.GetState(), FString(TEXT("E04_CHOOSE_BEACH")));
        M.ChooseBeach(bNorth ? TEXT("north_beach") : TEXT("west_beach"));
        Test.TestTrue(TEXT("landing"), M.GetState().EndsWith(TEXT("_LANDING")));
        M.SecureBeachhead();
        M.GetRunner()->Advance();
        if (bNorth)
        {
            Test.TestEqual(TEXT("terminal first"), M.GetState(), FString(TEXT("E04_TERMINAL_FIRST")));
            M.SecureTerminal(); M.GetRunner()->Advance();
            M.SecureAirstrip(); M.GetRunner()->Advance();
        }
        else
        {
            Test.TestEqual(TEXT("airstrip first"), M.GetState(), FString(TEXT("E04_AIRSTRIP_FIRST")));
            M.SecureAirstrip(); M.GetRunner()->Advance();
            M.SecureTerminal(); M.GetRunner()->Advance();
        }
        // The flow waits here for the hold timer; a single Advance must not skip the hold.
        Test.TestEqual(TEXT("island secured (hold)"), M.GetState(), FString(TEXT("E04_ISLAND_SECURED")));
        Test.TestTrue(TEXT("Falcon support unlocked by the airstrip"), M.HasAirSupport());
        M.Progress(); // hold finished
        Test.TestEqual(TEXT("lane clearance"), M.GetState(), FString(TEXT("E04_LANE_CLEARANCE")));
    }

    void ClearLanes(FAutomationTestBase& Test, UHavocOpenStraitMission& M)
    {
        // Reveal one at a time (as the Raven's sensor does) and clear one at a time (as the marked-hazard timer does).
        const TCHAR* Lanes[] = { TEXT("inbound"), TEXT("outbound") };
        for (const TCHAR* Lane : Lanes)
        {
            while (M.RevealHazards(FName(Lane), 1) > 0)
            {
                Test.TestTrue(TEXT("clear revealed hazard"), M.ClearHazard(FName(Lane)));
            }
        }
        Test.TestTrue(TEXT("lanes_open"), M.GetRunner()->HasFlag(TEXT("lanes_open")));
        M.Progress();
        Test.TestEqual(TEXT("tanker escort"), M.GetState(), FString(TEXT("E04_TANKER_ESCORT")));
    }

    /** Reads data/units/<faction>.json and returns the infantry unit ids. */
    TArray<FString> InfantryUnits(FAutomationTestBase& Test, const TCHAR* Faction)
    {
        TArray<FString> Ids;
        FString Text;
        if (!FFileHelper::LoadFileToString(Text, *DataPath(*FString::Printf(TEXT("units/%s.json"), Faction))))
        {
            Test.AddError(FString::Printf(TEXT("Could not read data/units/%s.json"), Faction));
            return Ids;
        }
        const TSharedRef<FJsonObject> Json = HavocMissionJson::FromString(Text);
        for (const TSharedPtr<FJsonObject>& Unit : HavocMissionJson::ReadObjectArray(*Json, TEXT("units")))
        {
            FString Branch, Id;
            if (Unit->TryGetStringField(TEXT("branch"), Branch) && Branch == TEXT("infantry") && Unit->TryGetStringField(TEXT("id"), Id))
            {
                Ids.Add(Id);
            }
        }
        return Ids;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHavocPlayNorthLoopTest, "MidnightHavoc.Playable.NorthBeachLoopToSuccess", HAVOC_PLAY_TEST_FLAGS)
bool FHavocPlayNorthLoopTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<UHavocOpenStraitMission> Mission = HavocPlayableTests::MakeMission(*this);
    if (!Mission.IsValid()) { return false; }
    UHavocOpenStraitMission& M = *Mission;
    HavocPlayableTests::PlayToHold(*this, M, /*bNorth*/ true);
    TestEqual(TEXT("six north-beach defences cost reinforcements"), M.GetReinforcements(), 24);
    HavocPlayableTests::ClearLanes(*this, M);

    // Tankers alternate lanes, as the flow launches them.
    const TArray<FHavocTanker> Tankers = M.GetEscort()->GetTankers();
    TestEqual(TEXT("six tankers"), Tankers.Num(), 6);
    for (int32 i = 0; i < Tankers.Num(); ++i)
    {
        TestTrue(TEXT("transit starts on an open lane"), M.StartTransit(Tankers[i].Id, FName(i % 2 == 0 ? TEXT("inbound") : TEXT("outbound"))));
        M.TankerArrived(Tankers[i].Id);
    }
    TestEqual(TEXT("debrief reached after the last tanker"), M.GetState(), FString(TEXT("E04_DEBRIEF")));
    TestEqual(TEXT("flow calls Finish at the debrief"), M.Finish(), FString(TEXT("E04_STRAIT_OPEN")));
    TestEqual(TEXT("outcome"), M.GetResult().Outcome, FString(TEXT("SUCCESS")));
    TestTrue(TEXT("runner complete (results screen)"), M.GetRunner()->IsComplete());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHavocPlayWestLoopTest, "MidnightHavoc.Playable.WestBeachLoopPartial", HAVOC_PLAY_TEST_FLAGS)
bool FHavocPlayWestLoopTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<UHavocOpenStraitMission> Mission = HavocPlayableTests::MakeMission(*this);
    if (!Mission.IsValid()) { return false; }
    UHavocOpenStraitMission& M = *Mission;
    HavocPlayableTests::PlayToHold(*this, M, /*bNorth*/ false);
    HavocPlayableTests::ClearLanes(*this, M);

    // Threats hit half the tankers three times each (0.34 x 3 empties a hull): three are lost.
    const TArray<FHavocTanker> Tankers = M.GetEscort()->GetTankers();
    for (int32 i = 0; i < Tankers.Num(); ++i)
    {
        TestTrue(TEXT("transit"), M.StartTransit(Tankers[i].Id, FName(TEXT("inbound"))));
        if (i % 2 == 0)
        {
            for (int32 Hit = 0; Hit < 3; ++Hit) { M.DamageTanker(Tankers[i].Id, 0.34); }
        }
        else
        {
            M.TankerArrived(Tankers[i].Id);
        }
    }
    TestEqual(TEXT("debrief"), M.GetState(), FString(TEXT("E04_DEBRIEF")));
    TestEqual(TEXT("too few tankers: partial"), M.Finish(), FString(TEXT("E04_STRAIT_OPEN_PARTIAL")));
    TestEqual(TEXT("tankers lost"), M.GetResult().TankersLost, 3);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHavocPlayFailTest, "MidnightHavoc.Playable.FailStateAndCheckpoint", HAVOC_PLAY_TEST_FLAGS)
bool FHavocPlayFailTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<UHavocOpenStraitMission> Mission = HavocPlayableTests::MakeMission(*this);
    if (!Mission.IsValid()) { return false; }
    UHavocOpenStraitMission& M = *Mission;
    M.Progress();
    M.ScoutBeaches(2, 2);
    M.Progress();
    M.ChooseBeach(TEXT("west_beach"));
    const int32 AtLanding = M.GetReinforcements();

    // A death: restore the checkpoint, then charge the penalty from the checkpoint's count (what the flow does).
    TestTrue(TEXT("checkpoint restored"), M.GetRunner()->RestoreCheckpoint());
    TestEqual(TEXT("back in the landing"), M.GetState(), FString(TEXT("E04_WEST_LANDING")));
    M.SpendReinforcements(2);
    TestEqual(TEXT("one death costs 2"), M.GetReinforcements(), AtLanding - 2);

    // Running out of reinforcements before the terminal is secured fails the operation.
    M.SpendReinforcements(1000);
    TestEqual(TEXT("fail state"), M.GetState(), FString(TEXT("E04_OPERATION_FAILED")));
    TestEqual(TEXT("outcome"), M.GetRunner()->GetOutcome(), FString(TEXT("FAILED")));
    TestTrue(TEXT("results screen shows"), M.GetRunner()->IsComplete());

    // "Restart from last checkpoint" on the results screen.
    TestTrue(TEXT("restart"), M.GetRunner()->RestoreCheckpoint());
    TestFalse(TEXT("no longer complete"), M.GetRunner()->IsComplete());
    TestEqual(TEXT("landing again"), M.GetState(), FString(TEXT("E04_WEST_LANDING")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHavocPlayEnemyDataTest, "MidnightHavoc.Playable.EnemyUnitsAndIsisAiOnly", HAVOC_PLAY_TEST_FLAGS)
bool FHavocPlayEnemyDataTest::RunTest(const FString& Parameters)
{
    // The spawner needs infantry for every enemy faction in the loop.
    for (const TCHAR* Faction : { TEXT("iran"), TEXT("houthi"), TEXT("isis") })
    {
        TestTrue(FString::Printf(TEXT("%s has infantry in data/units"), Faction), HavocPlayableTests::InfantryUnits(*this, Faction).Num() > 0);
    }
    TestFalse(TEXT("ISIS is never playable"), FHavocUnitDef::IsFactionPlayable(TEXT("isis")));
    TestTrue(TEXT("Iran is a normal faction"), FHavocUnitDef::IsFactionPlayable(TEXT("iran")));

    FString Text;
    TestTrue(TEXT("read data/factions/isis.json"), FFileHelper::LoadFileToString(Text, *HavocPlayableTests::DataPath(TEXT("factions/isis.json"))));
    bool bPlayable = true;
    HavocMissionJson::FromString(Text)->TryGetBoolField(TEXT("playable"), bPlayable);
    TestFalse(TEXT("data says ISIS playable=false"), bPlayable);

    // The weapon the Marine fires exists in data/weapons.
    TestTrue(TEXT("data/weapons/kr47.json exists"), FPaths::FileExists(HavocPlayableTests::DataPath(TEXT("weapons/kr47.json"))));
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
