// Automation tests for Episode 03, "Operation Broken Citadel": phase rules, unlocks, reinforcements,
// the seeded surprise event, checkpoint restore and the flow's pure helpers.
// Mirrors the Episode 03 cases in src/tests/episodes.test.ts.
// Run from the editor: Tools > Session Frontend > Automation, filter "MidnightHavoc.Missions.Episode03"
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
#include "Missions/Episode03/HavocBrokenCitadelMission.h"
#include "Missions/Episode03/HavocEpisode03Flow.h"
#include "Missions/Episode03/HavocSurpriseEventDirector.h"

#define HAVOC_E03_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace HavocE03Tests
{
    bool LoadJson(const TCHAR* Relative, TSharedPtr<FJsonObject>& Out)
    {
        const FString Path = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../data"), Relative));
        FString Text;
        if (!FFileHelper::LoadFileToString(Text, *Path))
        {
            return false;
        }
        Out = HavocMissionJson::FromString(Text);
        return true;
    }

    /** Same parse as UHavocDataSubsystem::RegisterMission. */
    bool LoadMission(const TCHAR* Relative, FHavocMissionDef& Out)
    {
        TSharedPtr<FJsonObject> Json;
        if (!LoadJson(Relative, Json) || !FJsonObjectConverter::JsonObjectToUStruct<FHavocMissionDef>(Json.ToSharedRef(), &Out, 0, 0))
        {
            return false;
        }
        Json->TryGetBoolField(TEXT("fictional"), Out.bFictional);
        if (Out.Setting.IsEmpty())
        {
            Out.Setting = TEXT("fictional");
        }
        return true;
    }

    TArray<FString> SurpriseEvents()
    {
        TSharedPtr<FJsonObject> Json;
        return LoadJson(TEXT("maps/gardun-city.json"), Json) ? HavocMissionJson::ReadStringArray(*Json, TEXT("surpriseEvents")) : TArray<FString>();
    }

    TStrongObjectPtr<UHavocBrokenCitadelMission> MakeMission(FAutomationTestBase& Test, bool bCrew, int32 Nomads, const FString& Seed)
    {
        TStrongObjectPtr<UHavocBrokenCitadelMission> Mission;
        FHavocMissionDef Episode, Stronghold;
        if (!LoadMission(TEXT("missions/episode-03-broken-citadel.json"), Episode) || !LoadMission(TEXT("missions/operation-broken-citadel.json"), Stronghold))
        {
            Test.AddError(TEXT("Could not load the Episode 03 / Broken Citadel JSON"));
            return Mission;
        }
        FHavocEpisode02Carryover Carry;
        Carry.bCrewDelivered = bCrew;
        Carry.NomadsRemaining = Nomads;
        Mission.Reset(NewObject<UHavocBrokenCitadelMission>(GetTransientPackage()));
        FString Error;
        if (!Mission->Initialize(Episode, Stronghold, SurpriseEvents(), Carry, Seed, nullptr, Error))
        {
            Test.AddError(Error);
            Mission.Reset();
        }
        return Mission;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHavocE03PhasesTest, "MidnightHavoc.Missions.Episode03.FivePhasesUnlocksAndSurprise", HAVOC_E03_TEST_FLAGS)
bool FHavocE03PhasesTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<UHavocBrokenCitadelMission> M = HavocE03Tests::MakeMission(*this, true, 2, TEXT("match-1"));
    if (!M) { return false; }

    TestEqual(TEXT("20 base + 5 per surviving Nomad"), M->GetReinforcements(), 30);
    TestEqual(TEXT("briefing first"), M->GetState(), FString(TEXT("E03_BRIEFING")));
    TestEqual(TEXT("assault begins in phase 1"), M->BeginAssault(), FString(TEXT("E03_PHASE_1")));
    TestEqual(TEXT("phase number"), M->GetPhase(), 1);
    TestFalse(TEXT("later-phase objectives do not count yet"), M->CaptureObjective(TEXT("operations_room")));

    // Phase 1 takes either objective ("any").
    TestTrue(TEXT("airfield"), M->CaptureObjective(TEXT("capture_airfield")));
    TestEqual(TEXT("phase 2"), M->GetState(), FString(TEXT("E03_PHASE_2")));
    TestTrue(TEXT("Falcon support: airfield + crew"), M->HasAirSupport());
    TestFalse(TEXT("phase 1 is over"), M->CaptureObjective(TEXT("secure_industrial_depot")));

    // Phase 2 needs both relay sites ("all"); the second one weakens counterattacks to 60%.
    TestTrue(TEXT("radio"), M->CaptureObjective(TEXT("west_ridge_radio")));
    TestEqual(TEXT("still phase 2"), M->GetState(), FString(TEXT("E03_PHASE_2")));
    TestEqual(TEXT("full strength"), M->GetCounterattackStrength(), 1.0);
    TestFalse(TEXT("repeat capture does not count"), M->CaptureObjective(TEXT("west_ridge_radio")));
    TestTrue(TEXT("relay"), M->CaptureObjective(TEXT("government_relay")));
    TestEqual(TEXT("60% counterattacks"), M->GetCounterattackStrength(), 0.6);
    TestEqual(TEXT("phase 3"), M->GetState(), FString(TEXT("E03_PHASE_3")));

    // The seeded surprise event fires on entering phase 3 and ends with it.
    TestEqual(TEXT("seed match-1 picks bridge_obstruction (same as the TS)"), M->GetSurprise().GetPlanned(), FString(TEXT("bridge_obstruction")));
    TestEqual(TEXT("active in phase 3"), M->GetSurprise().GetActive(), FString(TEXT("bridge_obstruction")));
    TestFalse(TEXT("medical station not protected yet"), M->IsMedicalStationProtected());
    for (const TCHAR* Objective : { TEXT("market_square"), TEXT("medical_station"), TEXT("municipal_ruins") })
    {
        TestTrue(Objective, M->CaptureObjective(Objective));
    }
    TestTrue(TEXT("medical station is now a protected zone"), M->IsMedicalStationProtected());
    TestEqual(TEXT("phase 4"), M->GetState(), FString(TEXT("E03_PHASE_4")));
    TestFalse(TEXT("the surprise event ends with its phase"), M->GetSurprise().IsActive());
    TestTrue(TEXT("and has fired once"), M->GetSurprise().HasFired());

    TestFalse(TEXT("Aegis offline before the Mountain Fort"), M->IsAegisOnline());
    TestTrue(TEXT("fort"), M->CaptureObjective(TEXT("mountain_fort")));
    TestTrue(TEXT("Mountain Fort brings Aegis online"), M->IsAegisOnline());
    TestTrue(TEXT("power station"), M->CaptureObjective(TEXT("east_power_station")));
    TestEqual(TEXT("phase 5"), M->GetState(), FString(TEXT("E03_PHASE_5")));
    for (const TCHAR* Objective : { TEXT("outer_gate"), TEXT("inner_courtyard"), TEXT("command_hall"), TEXT("operations_room") })
    {
        TestTrue(Objective, M->CaptureObjective(Objective));
    }
    TestEqual(TEXT("citadel secured"), M->GetState(), FString(TEXT("E03_CITADEL_SECURED")));
    TestEqual(TEXT("outcome"), M->GetRunner()->GetOutcome(), FString(TEXT("SUCCESS")));

    const FHavocEpisode03Result R = M->GetResult();
    TestEqual(TEXT("phases reached"), R.PhaseReached, 5);
    TestEqual(TEXT("objectives taken (1 + 2 + 3 + 2 + 4)"), R.ObjectivesCaptured, 12);
    TestTrue(TEXT("Aegis in the result"), R.bAegisOnline);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHavocE03CarryoverTest, "MidnightHavoc.Missions.Episode03.CarryoverReinforcementsAndCheckpoint", HAVOC_E03_TEST_FLAGS)
bool FHavocE03CarryoverTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<UHavocBrokenCitadelMission> Lean = HavocE03Tests::MakeMission(*this, false, 0, TEXT("match-1"));
    if (!Lean) { return false; }
    TestEqual(TEXT("no Nomads: base reinforcements only"), Lean->GetReinforcements(), 20);
    Lean->BeginAssault();
    Lean->CaptureObjective(TEXT("capture_airfield"));
    TestFalse(TEXT("no Falcon crew, no air support"), Lean->HasAirSupport());
    TestTrue(TEXT("but the unlock is recorded"), Lean->GetUnlocks().Contains(TEXT("falcon_air_support")));

    TestTrue(TEXT("a checkpoint exists"), Lean->GetRunner()->HasCheckpoint());
    TestEqual(TEXT("checkpoint at phase 2"), Lean->GetRunner()->GetCheckpoint().State, FString(TEXT("E03_PHASE_2")));
    TestEqual(TEXT("spending is clamped at zero"), Lean->SpendReinforcements(25), 0);
    TestEqual(TEXT("running out fails the assault"), Lean->GetState(), FString(TEXT("E03_ASSAULT_FAILED")));
    TestEqual(TEXT("FAILED"), Lean->GetRunner()->GetOutcome(), FString(TEXT("FAILED")));

    TestTrue(TEXT("restore"), Lean->GetRunner()->RestoreCheckpoint());
    TestEqual(TEXT("back at phase 2"), Lean->GetState(), FString(TEXT("E03_PHASE_2")));
    TestEqual(TEXT("reinforcements restored"), Lean->GetReinforcements(), 20);
    TestEqual(TEXT("captures restored"), Lean->GetCapturedObjectives().Num(), 1);
    TestEqual(TEXT("the airfield capture"), Lean->GetCapturedObjectives()[0], FString(TEXT("capture_airfield")));

    // Restore across the surprise event: it fired once and cannot fire again.
    Lean->CaptureObjective(TEXT("west_ridge_radio"));
    Lean->CaptureObjective(TEXT("government_relay"));
    TestTrue(TEXT("surprise active in phase 3"), Lean->GetSurprise().IsActive());
    Lean->CaptureObjective(TEXT("market_square"));
    Lean->CaptureObjective(TEXT("medical_station"));
    Lean->CaptureObjective(TEXT("municipal_ruins"));
    TestFalse(TEXT("ended in phase 4"), Lean->GetSurprise().IsActive());
    Lean->SpendReinforcements(100);
    TestTrue(TEXT("restore"), Lean->GetRunner()->RestoreCheckpoint());
    TestEqual(TEXT("phase 4 checkpoint"), Lean->GetState(), FString(TEXT("E03_PHASE_4")));
    TestTrue(TEXT("the event has fired and cannot fire again"), Lean->GetSurprise().HasFired());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHavocE03SeededEventTest, "MidnightHavoc.Missions.Episode03.SeededSurpriseEvent", HAVOC_E03_TEST_FLAGS)
bool FHavocE03SeededEventTest::RunTest(const FString& Parameters)
{
    const TArray<FString> Events = HavocE03Tests::SurpriseEvents();
    TestEqual(TEXT("five events on the map"), Events.Num(), 5);

    FString A, B, Error;
    TestTrue(TEXT("pick"), FHavocSurpriseEventDirector::Pick(Events, TEXT("abc"), A, Error));
    TestTrue(TEXT("pick again"), FHavocSurpriseEventDirector::Pick(Events, TEXT("abc"), B, Error));
    TestEqual(TEXT("same seed, same event"), A, B);
    TestEqual(TEXT("seed abc (same as the TS)"), A, FString(TEXT("communications_outage")));
    TestTrue(TEXT("pick"), FHavocSurpriseEventDirector::Pick(Events, TEXT("match-1"), A, Error));
    TestEqual(TEXT("seed match-1 (same as the TS)"), A, FString(TEXT("bridge_obstruction")));

    TSet<FString> Seen;
    for (int32 i = 0; i < 40; ++i)
    {
        FString Event;
        TestTrue(TEXT("pick"), FHavocSurpriseEventDirector::Pick(Events, FString::Printf(TEXT("seed-%d"), i), Event, Error));
        Seen.Add(Event);
    }
    TestTrue(TEXT("different seeds reach different events"), Seen.Num() > 1);

    FString Ignored;
    TestFalse(TEXT("an empty list is refused"), FHavocSurpriseEventDirector::Pick(TArray<FString>(), TEXT("x"), Ignored, Error));
    TestFalse(TEXT("an unknown event is refused"), FHavocSurpriseEventDirector::Pick({ TEXT("meteor") }, TEXT("x"), Ignored, Error));

    // Fires once, then never again; modifiers follow the active event.
    FHavocSurpriseEventDirector Director;
    Director.SetPlanned(TEXT("sandstorm"));
    TestEqual(TEXT("neutral before"), Director.GetModifiers().VisibilityScale, 1.0);
    TestEqual(TEXT("fires the planned event"), Director.Trigger(), FString(TEXT("sandstorm")));
    TestEqual(TEXT("sandstorm halves drone sensors"), Director.GetModifiers().DroneSensorRangeScale, 0.5);
    TestEqual(TEXT("and cuts visibility"), Director.GetModifiers().VisibilityScale, 0.6);
    Director.End();
    TestTrue(TEXT("a second trigger does nothing"), Director.Trigger().IsEmpty());
    TestFalse(TEXT("so nothing is active"), Director.IsActive());
    TestEqual(TEXT("tunnel opens a route"), FHavocSurpriseEventDirector::ModifiersFor(TEXT("tunnel_discovery")).OpenedRoutes.Num(), 1);
    TestEqual(TEXT("bridge blocks a route"), FHavocSurpriseEventDirector::ModifiersFor(TEXT("bridge_obstruction")).BlockedRoutes.Num(), 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHavocE03FlowHelpersTest, "MidnightHavoc.Missions.Episode03.FlowObjectivesAndZones", HAVOC_E03_TEST_FLAGS)
bool FHavocE03FlowHelpersTest::RunTest(const FString& Parameters)
{
    FHavocMissionDef Episode, Stronghold;
    if (!HavocE03Tests::LoadMission(TEXT("missions/episode-03-broken-citadel.json"), Episode) || !HavocE03Tests::LoadMission(TEXT("missions/operation-broken-citadel.json"), Stronghold))
    {
        AddError(TEXT("Could not load the Episode 03 JSON"));
        return false;
    }
    TestEqual(TEXT("five phases"), Stronghold.Phases.Num(), 5);

    // Every stronghold objective has a Gardun sector and a greybox position.
    TSharedPtr<FJsonObject> MapJson;
    TestTrue(TEXT("map"), HavocE03Tests::LoadJson(TEXT("maps/gardun-city.json"), MapJson));
    TArray<FString> SectorIds;
    for (const TSharedPtr<FJsonObject>& Sector : HavocMissionJson::ReadObjectArray(*MapJson, TEXT("sectors")))
    {
        FString Id;
        Sector->TryGetStringField(TEXT("id"), Id);
        SectorIds.Add(Id);
    }
    for (const FHavocMissionPhaseDef& Phase : Stronghold.Phases)
    {
        for (const FString& Objective : Phase.Objectives)
        {
            const FName Sector = AHavocEpisode03Flow::GetSectorForObjective(Objective);
            TestFalse(*FString::Printf(TEXT("%s has a sector"), *Objective), Sector.IsNone());
            TestTrue(*FString::Printf(TEXT("%s sector is on the map"), *Objective), SectorIds.Contains(Sector.ToString()));
        }
    }
    TestTrue(TEXT("unknown objective"), AHavocEpisode03Flow::GetSectorForObjective(TEXT("nope")).IsNone());
    for (const FString& Id : SectorIds)
    {
        TestFalse(*FString::Printf(TEXT("greybox position for %s"), *Id), AHavocEpisode03Flow::GetGreyboxSectorCentre(FName(*Id)).IsZero());
    }

    // Garrisons: none at the protected medical station, one more per objective in the Citadel phase.
    const AHavocEpisode03Flow* Flow = GetDefault<AHavocEpisode03Flow>();
    TestEqual(TEXT("medical station has no garrison"), Flow->GetGarrisonFor(TEXT("medical_station"), 3), 0);
    TestEqual(TEXT("market square"), Flow->GetGarrisonFor(TEXT("market_square"), 3), Flow->GarrisonPerObjective);
    TestEqual(TEXT("citadel objectives get extra defenders"), Flow->GetGarrisonFor(TEXT("outer_gate"), 5), Flow->GarrisonPerObjective + Flow->CitadelExtraGarrison);
    TestEqual(TEXT("unknown objective"), Flow->GetGarrisonFor(TEXT("nope"), 1), 0);

    // The medical station zone: protected from the start; the greybox puts Old Gardun on it.
    TStrongObjectPtr<UHavocBrokenCitadelMission> M = HavocE03Tests::MakeMission(*this, true, 0, TEXT("match-1"));
    if (!M) { return false; }
    TestEqual(TEXT("one protected zone"), M->GetProtectedZones().Num(), 1);
    const FHavocProtectedZoneDef& Zone = M->GetProtectedZones()[0];
    TestEqual(TEXT("medical station zone"), Zone.Id, FString(TEXT("medical_station")));
    FString ZoneId;
    TestTrue(TEXT("zone centre is protected"), M->IsInProtectedZone(Zone.Position.X, Zone.Position.Y, Zone.Position.Z, ZoneId));
    TestEqual(TEXT("zone id"), ZoneId, Zone.Id);
    TestFalse(TEXT("South Gate is not"), M->IsInProtectedZone(1180.0, 0.0, -100.0, ZoneId));
    const FVector2D OldGardun = AHavocEpisode03Flow::GetGreyboxSectorCentre(TEXT("old_gardun"));
    TestTrue(TEXT("greybox Old Gardun is centred on the zone"), M->IsInProtectedZone(OldGardun.X, 0.0, OldGardun.Y, ZoneId));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHavocE03DefinitionTest, "MidnightHavoc.Missions.Episode03.DefinitionFromData", HAVOC_E03_TEST_FLAGS)
bool FHavocE03DefinitionTest::RunTest(const FString& Parameters)
{
    FHavocMissionDef Episode, Stronghold, Def;
    if (!HavocE03Tests::LoadMission(TEXT("missions/episode-03-broken-citadel.json"), Episode) || !HavocE03Tests::LoadMission(TEXT("missions/operation-broken-citadel.json"), Stronghold))
    {
        AddError(TEXT("Could not load the Episode 03 JSON"));
        return false;
    }
    FString Error;
    TestTrue(TEXT("builds"), UHavocBrokenCitadelMission::BuildDefinition(Episode, Stronghold, Def, Error));
    // briefing, staging, five phases, secured, failed
    TestEqual(TEXT("nine states"), Def.States.Num(), 9);
    TestEqual(TEXT("start"), Def.Start, FString(TEXT("E03_BRIEFING")));
    TestEqual(TEXT("fail state"), Def.FailState, FString(TEXT("E03_ASSAULT_FAILED")));
    TestTrue(TEXT("fictional"), Def.bFictional);
    int32 SurpriseStates = 0;
    for (const FHavocMissionStateDef& State : Def.States)
    {
        if (State.Effects.Contains(TEXT("surprise.trigger")))
        {
            ++SurpriseStates;
            TestEqual(TEXT("surprise on its phase"), State.Id, FString(TEXT("E03_PHASE_3")));
        }
    }
    TestEqual(TEXT("one surprise state"), SurpriseStates, 1);

    // A mismatched stronghold is refused.
    Stronghold.Id = TEXT("something_else");
    TestFalse(TEXT("wrong stronghold refused"), UHavocBrokenCitadelMission::BuildDefinition(Episode, Stronghold, Def, Error));
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
