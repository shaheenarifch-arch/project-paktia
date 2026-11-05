// Automation tests for AI squads (item 8 of the real-playable plan, game/docs/ai.md).
// They run the squad rules (HavocSquadLogic.h) with a simulated clock, the same code UHavocSquadSubsystem
// runs in the level, and check data/ai/squad-behaviour.json against the map, mission and unit data:
//   spawn      the deployments for each mission phase (which squad, where, how many, which team)
//   patrol     squads patrol until a threat is reported
//   engage     a reported threat moves the whole squad to Engage; losing it ends the fight
//   fallback   a weakened squad falls back, regroups and returns to patrol (full smoke cycle)
// World-level behaviour (NavMesh, EQS cover, moving and shooting) is the manual checklist in game/docs/ai.md.
// Run: Tools > Session Frontend > Automation, filter "MidnightHavoc.AI".

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "AI/HavocEnemyCharacter.h"
#include "AI/HavocSquadLogic.h"
#include "Missions/HavocMissionJson.h"

#define HAVOC_AI_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace HavocSquadTests
{
    FString DataPath(const TCHAR* Relative)
    {
        return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../data"), Relative));
    }

    bool LoadConfig(FAutomationTestBase& Test, HavocSquad::FConfig& Out)
    {
        FString Text, Error;
        if (!FFileHelper::LoadFileToString(Text, *DataPath(TEXT("ai/squad-behaviour.json"))))
        {
            Test.AddError(TEXT("Could not read data/ai/squad-behaviour.json"));
            return false;
        }
        if (!HavocSquad::ParseConfig(Text, Out, Error))
        {
            Test.AddError(FString::Printf(TEXT("squad-behaviour.json: %s"), *Error));
            return false;
        }
        return true;
    }

    /** Advances the model in tick steps up to Until; returns the first time the mode equals Wanted, or -1. */
    double RunUntilMode(HavocSquad::FSquadModel& M, const HavocSquad::FTuning& T, double& Now, double Until, int32 Alive, EHavocSquadMode Wanted, bool bThreatEachTick = false)
    {
        while (Now < Until)
        {
            Now += T.SquadTickSeconds;
            if (bThreatEachTick)
            {
                M.ReportThreat(Now);
            }
            M.Update(T, Now, Alive);
            if (M.Mode == Wanted)
            {
                return Now;
            }
        }
        return -1.0;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHavocSquadDataTest, "MidnightHavoc.AI.Squad.DataAndSpawnPlan", HAVOC_AI_TEST_FLAGS)
bool FHavocSquadDataTest::RunTest(const FString& Parameters)
{
    HavocSquad::FConfig Config;
    if (!HavocSquadTests::LoadConfig(*this, Config))
    {
        return false;
    }
    const HavocSquad::FTuning& T = Config.Tuning;
    TestTrue(TEXT("tick > 0"), T.SquadTickSeconds > 0.f);
    TestTrue(TEXT("fallback strength in 0..1"), T.FallbackStrength > 0.f && T.FallbackStrength < 1.f);
    TestTrue(TEXT("max fallback >= regroup"), T.MaxFallbackSeconds >= T.RegroupSeconds);
    TestTrue(TEXT("EQS cover on by default in data"), T.bUseEqsCover);

    // Spawn plan: allied squads at the landing sectors and at the terminal once the island is secured.
    const TArray<HavocSquad::FDeployment> North = Config.ForState(TEXT("E04_NORTH_LANDING"));
    TestEqual(TEXT("one squad at the north landing"), North.Num(), 1);
    if (North.Num() == 1)
    {
        TestEqual(TEXT("north squad sector"), North[0].Sector.ToString(), FString(TEXT("kharg_north_beach")));
        TestEqual(TEXT("north squad size"), North[0].Size, 3);
        TestTrue(TEXT("north squad is allied"), HavocSquad::TeamForFaction(North[0].Faction) == EHavocTeam::Allied);
    }
    TestEqual(TEXT("one squad at the west landing"), Config.ForState(TEXT("E04_WEST_LANDING")).Num(), 1);
    TestEqual(TEXT("one squad when the island is secured"), Config.ForState(TEXT("E04_ISLAND_SECURED")).Num(), 1);
    TestEqual(TEXT("no squads in the briefing"), Config.ForState(TEXT("E04_BRIEFING")).Num(), 0);

    // Every deployment points at a real sector, a real mission state and a faction with infantry in data/units.
    FString MapText, MissionText;
    TestTrue(TEXT("read map"), FFileHelper::LoadFileToString(MapText, *HavocSquadTests::DataPath(TEXT("maps/kharg-hormuz.json"))));
    TestTrue(TEXT("read mission"), FFileHelper::LoadFileToString(MissionText, *HavocSquadTests::DataPath(TEXT("missions/episode-04-open-strait.json"))));
    for (const HavocSquad::FDeployment& D : Config.Deployments)
    {
        TestTrue(FString::Printf(TEXT("sector %s is in the Kharg map"), *D.Sector.ToString()), MapText.Contains(FString::Printf(TEXT("\"%s\""), *D.Sector.ToString())));
        TestTrue(FString::Printf(TEXT("state %s is in the Episode 04 mission"), *D.State), MissionText.Contains(FString::Printf(TEXT("\"%s\""), *D.State)));
        FString UnitsText;
        TestTrue(TEXT("read units"), FFileHelper::LoadFileToString(UnitsText, *HavocSquadTests::DataPath(*FString::Printf(TEXT("units/%s.json"), *D.Faction))));
        TestTrue(FString::Printf(TEXT("%s has infantry"), *D.Faction), UnitsText.Contains(TEXT("\"branch\": \"infantry\"")) || UnitsText.Contains(TEXT("\"branch\":\"infantry\"")));
    }

    // Teams: ISIS is never on the player's side, and enemy units default to Opposing.
    TestTrue(TEXT("coalition allied"), HavocSquad::TeamForFaction(TEXT("coalition")) == EHavocTeam::Allied);
    for (const TCHAR* Faction : { TEXT("iran"), TEXT("houthi"), TEXT("isis") })
    {
        TestTrue(FString::Printf(TEXT("%s opposing"), Faction), HavocSquad::TeamForFaction(Faction) == EHavocTeam::Opposing);
    }
    TestTrue(TEXT("unknown actor counts as opposing"), AHavocEnemyCharacter::TeamOf(nullptr) == EHavocTeam::Opposing);

    // A deployment cannot choose its team: the file must not set one, and a bad file is rejected.
    HavocSquad::FConfig Bad;
    FString Error;
    TestFalse(TEXT("not JSON is rejected"), HavocSquad::ParseConfig(TEXT("{ nope"), Bad, Error));
    TestFalse(TEXT("deployment without size is rejected"), HavocSquad::ParseConfig(TEXT("{\"phases\":[{\"state\":\"X\",\"deployments\":[{\"faction\":\"iran\",\"sector\":\"s\"}]}]}"), Bad, Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHavocSquadTransitionTest, "MidnightHavoc.AI.Squad.PatrolEngageTransitions", HAVOC_AI_TEST_FLAGS)
bool FHavocSquadTransitionTest::RunTest(const FString& Parameters)
{
    const HavocSquad::FTuning T;
    HavocSquad::FSquadModel M;
    M.Size = 4;
    double Now = 100.0;
    M.ModeEnteredTime = Now;

    // Patrol: no threat, nothing changes.
    TestTrue(TEXT("no threat: stays on patrol"), HavocSquadTests::RunUntilMode(M, T, Now, Now + 30.0, 4, EHavocSquadMode::Engage) < 0.0);
    TestTrue(TEXT("patrol"), M.Mode == EHavocSquadMode::Patrol);

    // A member reports a threat: the squad engages on the next tick.
    M.ReportThreat(Now);
    TestTrue(TEXT("threat -> engage"), M.Update(T, Now + T.SquadTickSeconds, 4));
    TestTrue(TEXT("engage"), M.Mode == EHavocSquadMode::Engage);

    // Threat refreshed: keeps engaging.
    Now += 1.0;
    M.ReportThreat(Now);
    M.Update(T, Now + 5.0, 4);
    TestTrue(TEXT("still engaging while the threat is fresh"), M.Mode == EHavocSquadMode::Engage);

    // Threat gone for longer than DisengageSeconds: back to patrol.
    Now += 5.0;
    TestTrue(TEXT("threat lost -> patrol"), HavocSquadTests::RunUntilMode(M, T, Now, Now + T.DisengageSeconds + 2.0, 4, EHavocSquadMode::Patrol) > 0.0);

    // Assault posture presses the attack even when nearly wiped out.
    HavocSquad::FSquadModel Raiders;
    Raiders.Size = 4;
    Raiders.Posture = EHavocSquadPosture::Assault;
    Raiders.ReportThreat(Now);
    Raiders.Update(T, Now, 1);
    TestTrue(TEXT("raiders engage"), Raiders.Mode == EHavocSquadMode::Engage);
    Raiders.ReportThreat(Now);
    Raiders.Update(T, Now + 1.0, 1);
    TestTrue(TEXT("raiders never fall back"), Raiders.Mode == EHavocSquadMode::Engage);

    // Strength helper.
    TestTrue(TEXT("strength 1 of 4"), FMath::IsNearlyEqual(HavocSquad::Strength(1, 4), 0.25f));
    TestTrue(TEXT("empty squad has no strength"), HavocSquad::Strength(0, 0) == 0.f);
    return true;
}

// The smoke test of the issue: one squad runs a full patrol -> engage -> fallback -> patrol cycle.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHavocSquadCycleTest, "MidnightHavoc.AI.Squad.SmokePatrolEngageFallbackCycle", HAVOC_AI_TEST_FLAGS)
bool FHavocSquadCycleTest::RunTest(const FString& Parameters)
{
    HavocSquad::FConfig Config;
    if (!HavocSquadTests::LoadConfig(*this, Config))
    {
        return false;
    }
    const HavocSquad::FTuning& T = Config.Tuning;
    HavocSquad::FSquadModel M;
    M.Size = 5;
    double Now = 0.0;

    // 1. patrol for a while
    TestTrue(TEXT("patrols quietly"), HavocSquadTests::RunUntilMode(M, T, Now, 20.0, 5, EHavocSquadMode::Engage) < 0.0);

    // 2. the player is spotted and fired on: engage
    TestTrue(TEXT("engages"), HavocSquadTests::RunUntilMode(M, T, Now, Now + 3.0, 5, EHavocSquadMode::Engage, /*bThreatEachTick*/ true) > 0.0);
    TestTrue(TEXT("holds the engagement at full strength"), HavocSquadTests::RunUntilMode(M, T, Now, Now + 10.0, 5, EHavocSquadMode::Fallback, true) < 0.0);

    // 3. casualties: 2 of 5 left (0.4) is at the fallback threshold
    TestTrue(TEXT("falls back when weak"), HavocSquadTests::RunUntilMode(M, T, Now, Now + 3.0, 2, EHavocSquadMode::Fallback, true) > 0.0);

    // 4. cornered (threat keeps coming): still gives up the fallback after MaxFallbackSeconds
    const double FallbackStart = Now;
    TestTrue(TEXT("regroups once the fallback times out"), HavocSquadTests::RunUntilMode(M, T, Now, FallbackStart + T.MaxFallbackSeconds + 2.0, 2, EHavocSquadMode::Patrol, true) > 0.0);

    // 5. second round: falls back again, this time the threat is lost and the squad regroups early
    HavocSquad::FSquadModel M2;
    M2.Size = 5;
    double Now2 = 0.0;
    M2.ReportThreat(Now2);
    HavocSquadTests::RunUntilMode(M2, T, Now2, 2.0, 2, EHavocSquadMode::Fallback);
    TestTrue(TEXT("weak squad that sees a threat falls straight back"), M2.Mode == EHavocSquadMode::Fallback);
    const double Start2 = Now2;
    const double BackOnPatrol = HavocSquadTests::RunUntilMode(M2, T, Now2, Start2 + T.MaxFallbackSeconds + 2.0, 2, EHavocSquadMode::Patrol);
    TestTrue(TEXT("regroups after the threat is lost"), BackOnPatrol > 0.0);
    TestTrue(TEXT("regroup waits at least RegroupSeconds"), BackOnPatrol - Start2 >= T.RegroupSeconds - 0.01);
    TestTrue(TEXT("regroup is earlier than the cornered timeout"), BackOnPatrol - Start2 < T.MaxFallbackSeconds);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
