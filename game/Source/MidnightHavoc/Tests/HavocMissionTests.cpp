// Automation tests for the mission runner and Episode 04.
// Mirrors src/tests/episode04.test.ts and the runner cases in src/tests/episodes.test.ts.
// Run from the editor: Tools > Session Frontend > Automation, filter "MidnightHavoc.Missions"
// (see game/README.md, "Running tests").

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
#include "Tests/HavocTestSystems.h"
#include "Missions/Episode04/HavocOilTerminal.h"
#include "Missions/Episode04/HavocOpenStraitMission.h"
#include "Missions/Episode04/HavocShippingLanes.h"
#include "Missions/Episode04/HavocTankerEscort.h"

#define HAVOC_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace HavocMissionTests
{
    /** Loads data/missions/episode-04-open-strait.json (same parse as UHavocDataSubsystem::RegisterMission). */
    bool LoadEpisode04(FHavocMissionDef& Out)
    {
        const FString Path = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../data/missions/episode-04-open-strait.json")));
        FString Text;
        if (!FFileHelper::LoadFileToString(Text, *Path))
        {
            return false;
        }
        const TSharedRef<FJsonObject> Json = HavocMissionJson::FromString(Text);
        if (!FJsonObjectConverter::JsonObjectToUStruct<FHavocMissionDef>(Json, &Out, 0, 0))
        {
            return false;
        }
        Json->TryGetBoolField(TEXT("fictional"), Out.bFictional);
        if (Out.Setting.IsEmpty())
        {
            Out.Setting = TEXT("fictional");
        }
        const TArray<TSharedPtr<FJsonObject>> States = HavocMissionJson::ReadObjectArray(*Json, TEXT("states"));
        for (int32 i = 0; i < States.Num() && i < Out.States.Num(); ++i)
        {
            States[i]->TryGetBoolField(TEXT("checkpoint"), Out.States[i].bCheckpoint);
            States[i]->TryGetBoolField(TEXT("terminal"), Out.States[i].bTerminal);
        }
        return true;
    }

    TStrongObjectPtr<UHavocOpenStraitMission> MakeMission(FAutomationTestBase& Test, const FString& Episode3Outcome)
    {
        FHavocMissionDef Def;
        TStrongObjectPtr<UHavocOpenStraitMission> Mission;
        if (!LoadEpisode04(Def))
        {
            Test.AddError(TEXT("Could not load data/missions/episode-04-open-strait.json"));
            return Mission;
        }
        Mission.Reset(NewObject<UHavocOpenStraitMission>(GetTransientPackage()));
        FString Error;
        if (!Mission->Initialize(Def, Episode3Outcome, nullptr, Error))
        {
            Test.AddError(Error);
            Mission.Reset();
        }
        return Mission;
    }

    void TakeIsland(FAutomationTestBase& Test, UHavocOpenStraitMission& M, const FString& Beach)
    {
        M.Progress();
        Test.TestEqual(TEXT("recon"), M.GetState(), FString(TEXT("E04_BEACH_RECON")));
        Test.TestTrue(TEXT("scout"), M.ScoutBeaches(8, 3));
        M.ChooseBeach(Beach);
        M.SecureBeachhead(); M.Progress();
        if (Beach == TEXT("north_beach"))
        {
            M.SecureTerminal(); M.Progress(); M.SecureAirstrip(); M.Progress();
        }
        else
        {
            M.SecureAirstrip(); M.Progress(); M.SecureTerminal(); M.Progress();
        }
        Test.TestEqual(TEXT("lane clearance"), M.GetState(), FString(TEXT("E04_LANE_CLEARANCE")));
    }

    void ClearLanes(FAutomationTestBase& Test, UHavocOpenStraitMission& M)
    {
        Test.TestFalse(TEXT("hazards must be revealed before clearing"), M.ClearHazard(FName(TEXT("inbound"))));
        const TCHAR* LaneNames[] = { TEXT("inbound"), TEXT("outbound") };
        for (const TCHAR* Lane : LaneNames)
        {
            M.RevealHazards(FName(Lane), 10);
            while (M.ClearHazard(FName(Lane))) {}
        }
        M.Progress();
        Test.TestEqual(TEXT("tanker escort"), M.GetState(), FString(TEXT("E04_TANKER_ESCORT")));
    }

    FHavocMissionStateDef MakeState(const TCHAR* Id, const TCHAR* Next = TEXT(""))
    {
        FHavocMissionStateDef S;
        S.Id = Id;
        S.Objective = FString(Id) + TEXT(" objective");
        S.Next = Next;
        return S;
    }

    /** A small mission shaped like Episode 02: gate -> choose -> branch -> done / fail, with a checkpoint. */
    FHavocMissionDef MakeRunnerDef()
    {
        FHavocMissionDef Def;
        Def.Id = TEXT("test_mission");
        Def.Start = TEXT("S_START");
        Def.FailState = TEXT("S_LOST");

        FHavocMissionStateDef Start = MakeState(TEXT("S_START"), TEXT("S_GATE"));
        Def.States.Add(Start);

        FHavocMissionStateDef Gate = MakeState(TEXT("S_GATE"), TEXT("S_CHOOSE"));
        Gate.Requires.Add(TEXT("gate_open"));
        Gate.Effects.Add(TEXT("mark"));
        Gate.bCheckpoint = true;
        Def.States.Add(Gate);

        FHavocMissionStateDef Choose = MakeState(TEXT("S_CHOOSE"));
        Choose.Branches.Add(TEXT("riverbed"), TEXT("S_DONE"));
        Choose.Branches.Add(TEXT("drainage"), TEXT("S_DONE_LOSSES"));
        Def.States.Add(Choose);

        FHavocMissionStateDef Done = MakeState(TEXT("S_DONE"));
        Done.bTerminal = true; Done.Outcome = TEXT("SUCCESS");
        Def.States.Add(Done);

        FHavocMissionStateDef Losses = MakeState(TEXT("S_DONE_LOSSES"));
        Losses.bTerminal = true; Losses.Outcome = TEXT("PARTIAL");
        Def.States.Add(Losses);

        FHavocMissionStateDef Lost = MakeState(TEXT("S_LOST"));
        Lost.bTerminal = true; Lost.Outcome = TEXT("FAILED");
        Def.States.Add(Lost);
        return Def;
    }
}

// ---------------------------------------------------------------- runner (episodes.test.ts)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHavocRunnerStartupTest, "MidnightHavoc.Missions.Runner.Startup", HAVOC_TEST_FLAGS)
bool FHavocRunnerStartupTest::RunTest(const FString& Parameters)
{
    // A state whose effect has no handler is a startup error.
    {
        TStrongObjectPtr<UHavocMissionRunner> Runner(NewObject<UHavocMissionRunner>(GetTransientPackage()));
        FString Error;
        AddExpectedError(TEXT("has no handler for effect mark"), EAutomationExpectedErrorFlags::Contains, 1);
        TestFalse(TEXT("missing handler rejected"), Runner->Initialize(HavocMissionTests::MakeRunnerDef(), {}, nullptr, nullptr, Error));
        TestTrue(TEXT("error names the effect"), Error.Contains(TEXT("no handler for effect mark")));
    }
    // An unknown start state is a startup error.
    {
        TStrongObjectPtr<UHavocMissionRunner> Runner(NewObject<UHavocMissionRunner>(GetTransientPackage()));
        FHavocMissionDef Def = HavocMissionTests::MakeRunnerDef();
        Def.Start = TEXT("S_MISSING");
        TMap<FString, FHavocEffectHandler> Handlers;
        Handlers.Add(TEXT("mark"), [](const FHavocMissionStateDef&) {});
        FString Error;
        AddExpectedError(TEXT("has no state S_MISSING"), EAutomationExpectedErrorFlags::Contains, 1);
        TestFalse(TEXT("unknown start rejected"), Runner->Initialize(Def, Handlers, nullptr, nullptr, Error));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHavocRunnerFlowTest, "MidnightHavoc.Missions.Runner.Flow", HAVOC_TEST_FLAGS)
bool FHavocRunnerFlowTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<UHavocTestSystems> Systems(NewObject<UHavocTestSystems>(GetTransientPackage()));
    TStrongObjectPtr<UHavocMissionRunner> Runner(NewObject<UHavocMissionRunner>(GetTransientPackage()));
    int32 Marked = 0;
    TMap<FString, FHavocEffectHandler> Handlers;
    Handlers.Add(TEXT("mark"), [&Marked](const FHavocMissionStateDef&) { ++Marked; });

    FString Error;
    TestTrue(TEXT("initialize"), Runner->Initialize(HavocMissionTests::MakeRunnerDef(), Handlers, nullptr, Systems.Get(), Error));

    TArray<FString> Seen, Blocked, Checkpoints;
    FString CompleteOutcome;
    Runner->OnState.AddLambda([&Seen](const FString&, const FString& To) { Seen.Add(To); });
    Runner->OnBlocked.AddLambda([&Blocked](const FString&, const TArray<FString>& Missing) { Blocked.Append(Missing); });
    Runner->OnCheckpoint.AddLambda([&Checkpoints](const FHavocMissionCheckpoint& C) { Checkpoints.Add(C.State); });
    Runner->OnComplete.AddLambda([&CompleteOutcome](const FString&, const FString& Outcome) { CompleteOutcome = Outcome; });

    TestEqual(TEXT("start state"), Runner->GetState(), FString(TEXT("S_START")));
    Runner->Advance();
    TestEqual(TEXT("at gate"), Runner->GetState(), FString(TEXT("S_GATE")));
    TestEqual(TEXT("effect ran on entry"), Marked, 1);
    TestEqual(TEXT("checkpoint recorded"), Checkpoints.Num(), 1);

    // requires: blocked until the flag is set.
    Systems->Counter = 5;
    TestEqual(TEXT("blocked stays"), Runner->Advance(), FString(TEXT("S_GATE")));
    TestTrue(TEXT("blocked event lists the flag"), Blocked.Contains(TEXT("gate_open")));
    TestEqual(TEXT("missing"), Runner->Missing().Num(), 1);
    Runner->SetFlag(TEXT("gate_open"));
    Runner->Advance();
    TestEqual(TEXT("at choose"), Runner->GetState(), FString(TEXT("S_CHOOSE")));

    // named branches, in definition order.
    TestEqual(TEXT("choices count"), Runner->GetChoices().Num(), 2);
    TestEqual(TEXT("first choice"), Runner->GetChoices()[0], FString(TEXT("riverbed")));
    TestEqual(TEXT("second choice"), Runner->GetChoices()[1], FString(TEXT("drainage")));
    TestFalse(TEXT("unknown branch"), Runner->HasBranch(TEXT("nope")));
    AddExpectedError(TEXT("has no branch"), EAutomationExpectedErrorFlags::Contains, 1);
    TestEqual(TEXT("unknown branch leaves state"), Runner->Choose(TEXT("nope")), FString(TEXT("S_CHOOSE")));

    Runner->Choose(TEXT("riverbed"));
    TestEqual(TEXT("done"), Runner->GetState(), FString(TEXT("S_DONE")));
    TestTrue(TEXT("complete"), Runner->IsComplete());
    TestEqual(TEXT("outcome"), Runner->GetOutcome(), FString(TEXT("SUCCESS")));
    TestEqual(TEXT("complete event"), CompleteOutcome, FString(TEXT("SUCCESS")));
    TestEqual(TEXT("seen last"), Seen.Last(), FString(TEXT("S_DONE")));

    // fail() does nothing once complete; restore returns to the checkpoint with systems.
    TestEqual(TEXT("fail ignored when complete"), Runner->Fail(), FString(TEXT("S_DONE")));
    Systems->Counter = 99;
    TestTrue(TEXT("restore"), Runner->RestoreCheckpoint());
    TestEqual(TEXT("restored state"), Runner->GetState(), FString(TEXT("S_GATE")));
    TestEqual(TEXT("outcome cleared"), Runner->GetOutcome(), FString());
    TestEqual(TEXT("systems restored"), Systems->Counter, 0); // snapshot taken on entering S_GATE (counter was 0)
    TestEqual(TEXT("flags restored (set after checkpoint)"), Runner->GetFlags().Num(), 0);
    TestEqual(TEXT("history restored"), Runner->GetHistory().Num(), 2);

    // fail() ends in the fail state with FAILED.
    Runner->SetFlag(TEXT("gate_open"));
    Runner->Advance();
    TestEqual(TEXT("fail state"), Runner->Fail(), FString(TEXT("S_LOST")));
    TestEqual(TEXT("failed outcome"), Runner->GetOutcome(), FString(TEXT("FAILED")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHavocRunnerPayloadTest, "MidnightHavoc.Missions.Runner.CheckpointPayload", HAVOC_TEST_FLAGS)
bool FHavocRunnerPayloadTest::RunTest(const FString& Parameters)
{
    FHavocMissionCheckpoint C;
    C.MissionId = TEXT("m");
    C.State = TEXT("S");
    C.Flags = { TEXT("a") };
    C.History = { TEXT("S0"), TEXT("S") };
    C.SystemsJson = TEXT("{\"counter\":3}");
    const TSharedRef<FJsonObject> Json = UHavocMissionRunner::CheckpointToJson(C);
    TestEqual(TEXT("missionId"), Json->GetStringField(TEXT("missionId")), FString(TEXT("m")));
    TestEqual(TEXT("state"), Json->GetStringField(TEXT("state")), FString(TEXT("S")));
    TestEqual(TEXT("history"), HavocMissionJson::ReadStringArray(*Json, TEXT("history")).Num(), 2);
    TestEqual(TEXT("systems is an object"), static_cast<int32>(Json->GetObjectField(TEXT("systems"))->GetNumberField(TEXT("counter"))), 3);
    return true;
}

// ---------------------------------------------------------------- Episode 04 (episode04.test.ts)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHavocE04SettingTest, "MidnightHavoc.Missions.Episode04.RealWorldSetting", HAVOC_TEST_FLAGS)
bool FHavocE04SettingTest::RunTest(const FString& Parameters)
{
    FHavocMissionDef Def;
    if (!HavocMissionTests::LoadEpisode04(Def)) { AddError(TEXT("Could not load Episode 04 JSON")); return false; }
    TestEqual(TEXT("setting"), Def.Setting, FString(TEXT("real_world_hypothetical")));
    TestFalse(TEXT("fictional flag agrees"), Def.bFictional);

    // A fictional definition is refused.
    Def.Setting = TEXT("fictional");
    TStrongObjectPtr<UHavocOpenStraitMission> Mission(NewObject<UHavocOpenStraitMission>(GetTransientPackage()));
    FString Error;
    AddExpectedError(TEXT("real-world hypothetical"), EAutomationExpectedErrorFlags::Contains, 1);
    TestFalse(TEXT("refused"), Mission->Initialize(Def, TEXT("SUCCESS"), nullptr, Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHavocE04NorthSuccessTest, "MidnightHavoc.Missions.Episode04.NorthBeachSuccess", HAVOC_TEST_FLAGS)
bool FHavocE04NorthSuccessTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<UHavocOpenStraitMission> Mission = HavocMissionTests::MakeMission(*this, TEXT("SUCCESS"));
    if (!Mission.IsValid()) { return false; }
    UHavocOpenStraitMission& M = *Mission;
    TestEqual(TEXT("30 + 10 for winning Episode 03"), M.GetReinforcements(), 40);
    HavocMissionTests::TakeIsland(*this, M, TEXT("north_beach"));
    TestEqual(TEXT("north beach defences cost 8"), M.GetReinforcements(), 32);
    TestTrue(TEXT("air support"), M.HasAirSupport());
    TestFalse(TEXT("no transits before the escort phase"), M.StartTransit(TEXT("tanker-1"), FName(TEXT("inbound"))));
    HavocMissionTests::ClearLanes(*this, M);
    TestEqual(TEXT("six tankers"), M.GetEscort()->NumTankers(), 6);
    for (const FHavocTanker& T : M.GetEscort()->GetTankers())
    {
        TestTrue(TEXT("transit"), M.StartTransit(T.Id, FName(TEXT("outbound"))));
        M.TankerArrived(T.Id);
    }
    TestEqual(TEXT("debrief"), M.GetState(), FString(TEXT("E04_DEBRIEF")));
    TestEqual(TEXT("strait open"), M.Finish(), FString(TEXT("E04_STRAIT_OPEN")));
    const FHavocEpisode04Result R = M.GetResult();
    TestEqual(TEXT("episode"), R.Episode, 4);
    TestEqual(TEXT("outcome"), R.Outcome, FString(TEXT("SUCCESS")));
    TestEqual(TEXT("beach"), R.Beach, FString(TEXT("NORTH")));
    TestEqual(TEXT("integrity"), R.TerminalIntegrity, 1.0);
    TestTrue(TEXT("can export"), R.bCanExport);
    TestEqual(TEXT("through"), R.TankersThrough, 6);
    TestEqual(TEXT("lost"), R.TankersLost, 0);
    TestEqual(TEXT("supply flow"), R.SupplyFlow, 1.0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHavocE04WestPartialTest, "MidnightHavoc.Missions.Episode04.WestBeachPartial", HAVOC_TEST_FLAGS)
bool FHavocE04WestPartialTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<UHavocOpenStraitMission> Mission = HavocMissionTests::MakeMission(*this, TEXT("PARTIAL"));
    if (!Mission.IsValid()) { return false; }
    UHavocOpenStraitMission& M = *Mission;
    TestEqual(TEXT("no bonus without a full Episode 03 win"), M.GetReinforcements(), 30);
    HavocMissionTests::TakeIsland(*this, M, TEXT("west_beach"));
    TestEqual(TEXT("beach"), M.GetResult().Beach, FString(TEXT("WEST")));
    TestEqual(TEXT("west defences cost 3"), M.GetReinforcements(), 27);
    M.DamageTerminal(FName(TEXT("tank_farm")), 0.9);
    M.DamageTerminal(FName(TEXT("east_jetty")), 0.5);
    HavocMissionTests::ClearLanes(*this, M);
    TArray<FHavocTanker> Tankers = M.GetEscort()->GetTankers();
    for (int32 i = 0; i < 2; ++i)
    {
        M.StartTransit(Tankers[i].Id, FName(TEXT("inbound")));
        M.DamageTanker(Tankers[i].Id, 1.0);
    }
    for (int32 i = 2; i < Tankers.Num(); ++i)
    {
        M.StartTransit(Tankers[i].Id, FName(TEXT("outbound")));
        M.TankerArrived(Tankers[i].Id);
    }
    TestEqual(TEXT("partial"), M.Finish(), FString(TEXT("E04_STRAIT_OPEN_PARTIAL")));
    const FHavocEpisode04Result R = M.GetResult();
    TestEqual(TEXT("tankers lost"), R.TankersLost, 2);
    TestTrue(TEXT("supply flow below target"), R.SupplyFlow < 0.8);
    TestTrue(TEXT("terminal integrity below target"), R.TerminalIntegrity < 0.8);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHavocE04ZonesFailRestoreTest, "MidnightHavoc.Missions.Episode04.ProtectedZonesFailAndRestore", HAVOC_TEST_FLAGS)
bool FHavocE04ZonesFailRestoreTest::RunTest(const FString& Parameters)
{
    // Protected zones cover the terminal and town (Aegis itself is not ported; it must honour these).
    FString ZoneId;
    const FHavocProtectedZoneDef& Terminal = UHavocOpenStraitMission::GetProtectedZones()[0];
    TestTrue(TEXT("terminal centre protected"), UHavocOpenStraitMission::IsInProtectedZone(Terminal.Position.X, Terminal.Position.Y, Terminal.Position.Z, ZoneId));
    TestEqual(TEXT("terminal zone id"), ZoneId, FString(TEXT("kharg_oil_terminal")));
    TestTrue(TEXT("town protected"), UHavocOpenStraitMission::IsInProtectedZone(1500.0, 0.0, 1600.0, ZoneId));
    TestEqual(TEXT("town zone id"), ZoneId, FString(TEXT("kharg_town")));
    TestFalse(TEXT("open sea is not protected"), UHavocOpenStraitMission::IsInProtectedZone(0.0, 0.0, 0.0, ZoneId));

    TStrongObjectPtr<UHavocOpenStraitMission> Mission = HavocMissionTests::MakeMission(*this, TEXT("SUCCESS"));
    if (!Mission.IsValid()) { return false; }
    UHavocOpenStraitMission& M = *Mission;
    HavocMissionTests::TakeIsland(*this, M, TEXT("north_beach"));
    HavocMissionTests::ClearLanes(*this, M);
    TestTrue(TEXT("checkpoint"), M.GetRunner()->HasCheckpoint());
    TestEqual(TEXT("checkpoint state"), M.GetRunner()->GetCheckpoint().State, FString(TEXT("E04_TANKER_ESCORT")));
    for (const FHavocTanker& T : M.GetEscort()->GetTankers())
    {
        M.StartTransit(T.Id, FName(TEXT("inbound")));
        M.DamageTanker(T.Id, 1.0);
    }
    TestEqual(TEXT("losing every tanker fails"), M.GetState(), FString(TEXT("E04_OPERATION_FAILED")));
    TestEqual(TEXT("failed outcome"), M.GetRunner()->GetOutcome(), FString(TEXT("FAILED")));
    TestTrue(TEXT("restore"), M.GetRunner()->RestoreCheckpoint());
    TestEqual(TEXT("back at escort"), M.GetState(), FString(TEXT("E04_TANKER_ESCORT")));
    TestTrue(TEXT("lane work kept"), M.GetLanes()->AllOpen());
    bool bAllWaiting = true;
    for (const FHavocTanker& T : M.GetEscort()->GetTankers())
    {
        bAllWaiting &= T.State == EHavocTankerState::Waiting;
    }
    TestTrue(TEXT("tankers back in harbour"), bAllWaiting);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHavocE04ReinforcementsTest, "MidnightHavoc.Missions.Episode04.ReinforcementsRunOut", HAVOC_TEST_FLAGS)
bool FHavocE04ReinforcementsTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<UHavocOpenStraitMission> Mission = HavocMissionTests::MakeMission(*this, TEXT("PARTIAL"));
    if (!Mission.IsValid()) { return false; }
    UHavocOpenStraitMission& M = *Mission;
    M.Progress();
    M.ScoutBeaches(8, 3);
    M.ChooseBeach(TEXT("north_beach"));
    TestEqual(TEXT("spend"), M.SpendReinforcements(10), 12);
    TestEqual(TEXT("still landing"), M.GetState(), FString(TEXT("E04_NORTH_LANDING")));
    TestEqual(TEXT("run out"), M.SpendReinforcements(100), 0);
    TestEqual(TEXT("assault fails"), M.GetState(), FString(TEXT("E04_OPERATION_FAILED")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHavocE04PartsTest, "MidnightHavoc.Missions.Episode04.TerminalLanesTankers", HAVOC_TEST_FLAGS)
bool FHavocE04PartsTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<UHavocOilTerminal> Terminal(NewObject<UHavocOilTerminal>(GetTransientPackage()));
    TestEqual(TEXT("four facilities"), Terminal->GetFacilities().Num(), 4);
    TestTrue(TEXT("unknown facility"), !Terminal->Damage(FName(TEXT("nope")), 1.0));
    Terminal->Damage(FName(TEXT("pump_station")), 0.75);
    TestFalse(TEXT("pump at 0.25 stops exports"), Terminal->CanExport());
    TestEqual(TEXT("average integrity"), Terminal->GetIntegrity(), 0.813, 0.0005);
    Terminal->Damage(FName(TEXT("pump_station")), 5.0);
    TestEqual(TEXT("integrity floors at 0"), Terminal->GetFacilities()[3].Integrity, 0.0);

    TStrongObjectPtr<UHavocShippingLanes> Lanes(NewObject<UHavocShippingLanes>(GetTransientPackage()));
    TestFalse(TEXT("no lanes yet"), Lanes->AllOpen());
    Lanes->Seed(4, 3);
    TestEqual(TEXT("reveal capped by hazards"), Lanes->Reveal(FName(TEXT("outbound")), 10), 3);
    for (int32 i = 0; i < 3; ++i) { TestTrue(TEXT("clear"), Lanes->Clear(FName(TEXT("outbound")))); }
    TestFalse(TEXT("nothing left revealed"), Lanes->Clear(FName(TEXT("outbound"))));
    TestTrue(TEXT("outbound open"), Lanes->IsOpen(FName(TEXT("outbound"))));
    TestFalse(TEXT("inbound closed"), Lanes->IsOpen(FName(TEXT("inbound"))));

    TStrongObjectPtr<UHavocTankerEscort> Escort(NewObject<UHavocTankerEscort>(GetTransientPackage()));
    Escort->Assemble(6);
    TestFalse(TEXT("closed lane blocks transit"), Escort->Begin(TEXT("tanker-1"), FName(TEXT("inbound")), false));
    TestTrue(TEXT("open lane allows transit"), Escort->Begin(TEXT("tanker-1"), FName(TEXT("outbound")), true));
    Escort->Damage(TEXT("tanker-1"), 1.0);
    TestEqual(TEXT("lost"), Escort->GetLost(), 1);
    TestFalse(TEXT("lost tanker cannot arrive"), Escort->Arrive(TEXT("tanker-1")));
    TestEqual(TEXT("supply flow with none through"), Escort->GetSupplyFlow(), 0.0);
    TestFalse(TEXT("not settled"), Escort->IsSettled());
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
