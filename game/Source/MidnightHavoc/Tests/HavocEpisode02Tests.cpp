// Automation tests for Episode 02, "Operation Silent Corridor".
// Mirrors the Episode 02 cases of src/tests/episodes.test.ts (relay, outage and riverbed detour at full
// strength; drainage infiltration with losses; crew transport lost and checkpoint restore; no Falcon crew).
// Run from the editor: Tools > Session Frontend > Automation, filter "MidnightHavoc.Missions.Episode02"
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
#include "Missions/Episode02/HavocConvoyEscort.h"
#include "Missions/Episode02/HavocEpisode02Flow.h"
#include "Missions/Episode02/HavocRelayNetwork.h"
#include "Missions/Episode02/HavocSilentCorridorMission.h"

#define HAVOC_E02_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace HavocE02Tests
{
    FString DataPath(const TCHAR* Relative)
    {
        return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../data"), Relative));
    }

    /** Loads data/missions/episode-02-silent-corridor.json (same parse as UHavocDataSubsystem::RegisterMission). */
    bool LoadEpisode02(FHavocMissionDef& Out)
    {
        FString Text;
        if (!FFileHelper::LoadFileToString(Text, *DataPath(TEXT("missions/episode-02-silent-corridor.json"))))
        {
            return false;
        }
        const TSharedRef<FJsonObject> Json = HavocMissionJson::FromString(Text);
        if (!FJsonObjectConverter::JsonObjectToUStruct<FHavocMissionDef>(Json, &Out, 0, 0))
        {
            return false;
        }
        Json->TryGetBoolField(TEXT("fictional"), Out.bFictional);
        const TArray<TSharedPtr<FJsonObject>> States = HavocMissionJson::ReadObjectArray(*Json, TEXT("states"));
        for (int32 i = 0; i < States.Num() && i < Out.States.Num(); ++i)
        {
            States[i]->TryGetBoolField(TEXT("checkpoint"), Out.States[i].bCheckpoint);
            States[i]->TryGetBoolField(TEXT("terminal"), Out.States[i].bTerminal);
        }
        return true;
    }

    TStrongObjectPtr<UHavocSilentCorridorMission> MakeMission(FAutomationTestBase& Test, bool bCrewRecovered = true)
    {
        FHavocMissionDef Def;
        TStrongObjectPtr<UHavocSilentCorridorMission> Mission;
        if (!LoadEpisode02(Def))
        {
            Test.AddError(TEXT("Could not load data/missions/episode-02-silent-corridor.json"));
            return Mission;
        }
        Mission.Reset(NewObject<UHavocSilentCorridorMission>(GetTransientPackage()));
        FString Error;
        if (!Mission->Initialize(Def, bCrewRecovered, nullptr, Error))
        {
            Test.AddError(Error);
            Mission.Reset();
        }
        return Mission;
    }

    /** Plays Episode 02 up to the bridge choice (the TS toBridge helper). */
    void ToBridge(FAutomationTestBase& Test, UHavocSilentCorridorMission& M)
    {
        M.Progress(); // briefing -> form-up -> relay
        Test.TestEqual(TEXT("relay step"), M.GetState(), FString(TEXT("E02_RELAY_KILO")));
        Test.TestTrue(TEXT("place relay"), M.PlaceRelayKilo(HavocE02::RelayKilo()));
        M.Progress();
        Test.TestEqual(TEXT("scout step"), M.GetState(), FString(TEXT("E02_CORRIDOR_SCOUT")));
        FHavocE02Contact Roadblock;
        Roadblock.Id = TEXT("e02-roadblock");
        Roadblock.Position = FVector(1650.0, 0.0, 1480.0);
        Test.TestTrue(TEXT("scout"), M.ScoutCorridor({ Roadblock }, HavocE02::CorridorEnd()));
        M.Progress(); // advance -> comms outage (blocked)
        Test.TestEqual(TEXT("comms outage"), M.GetState(), FString(TEXT("E02_COMMS_OUTAGE")));
        Test.TestTrue(TEXT("outage jams the drone link"), M.GetRelays()->Classify(HavocE02::CorridorEnd()) == EHavocSignalState::Lost);
        Test.TestTrue(TEXT("blocked until the relay is restored"), M.GetRunner()->Missing().Contains(TEXT("relay_kilo_restored")));
        Test.TestFalse(TEXT("must be at the relay to restore it"), M.RestoreRelayKilo(FVector::ZeroVector));
        Test.TestTrue(TEXT("restore relay"), M.RestoreRelayKilo(FVector(610.0, 40.0, 905.0)));
        M.Progress();
        Test.TestEqual(TEXT("bridge obstructed"), M.GetState(), FString(TEXT("E02_BRIDGE_OBSTRUCTED")));
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHavocE02DefinitionTest, "MidnightHavoc.Missions.Episode02.Definition", HAVOC_E02_TEST_FLAGS)
bool FHavocE02DefinitionTest::RunTest(const FString& Parameters)
{
    FHavocMissionDef Def;
    if (!HavocE02Tests::LoadEpisode02(Def)) { AddError(TEXT("Could not load the Episode 02 JSON")); return false; }
    TestTrue(TEXT("fictional"), Def.bFictional);
    TestEqual(TEXT("episode"), Def.Episode, 2);
    TestEqual(TEXT("map"), Def.Map, FString(TEXT("gardun_city")));

    TSet<FString> Ids;
    for (const FHavocMissionStateDef& S : Def.States) { Ids.Add(S.Id); }
    TestTrue(TEXT("start exists"), Ids.Contains(Def.Start));
    TestTrue(TEXT("fail state exists"), Ids.Contains(Def.FailState));
    for (const FHavocMissionStateDef& S : Def.States)
    {
        if (!S.Next.IsEmpty()) { TestTrue(FString::Printf(TEXT("%s next exists"), *S.Id), Ids.Contains(S.Next)); }
        for (const TPair<FString, FString>& B : S.Branches) { TestTrue(FString::Printf(TEXT("%s branch %s exists"), *S.Id, *B.Key), Ids.Contains(B.Value)); }
    }

    // Every sector the mission names is a sector of the Gardun map.
    FString MapText;
    if (!FFileHelper::LoadFileToString(MapText, *HavocE02Tests::DataPath(TEXT("maps/gardun-city.json")))) { AddError(TEXT("Could not load the Gardun map")); return false; }
    TSet<FString> MapSectors;
    for (const TSharedPtr<FJsonObject>& Sector : HavocMissionJson::ReadObjectArray(*HavocMissionJson::FromString(MapText), TEXT("sectors")))
    {
        FString Id;
        if (Sector->TryGetStringField(TEXT("id"), Id)) { MapSectors.Add(Id); }
    }
    for (const FString& Sector : Def.Sectors) { TestTrue(FString::Printf(TEXT("sector %s is on the map"), *Sector), MapSectors.Contains(Sector)); }

    // A missing effect handler is a startup error: the mission registers every effect the JSON uses.
    TStrongObjectPtr<UHavocSilentCorridorMission> Mission = HavocE02Tests::MakeMission(*this);
    TestTrue(TEXT("mission starts"), Mission.IsValid());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHavocE02RiverbedTest, "MidnightHavoc.Missions.Episode02.RiverbedDetourFullStrength", HAVOC_E02_TEST_FLAGS)
bool FHavocE02RiverbedTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<UHavocSilentCorridorMission> Mission = HavocE02Tests::MakeMission(*this);
    if (!Mission.IsValid()) { return false; }
    UHavocSilentCorridorMission& M = *Mission;

    TestFalse(TEXT("cannot scout before the relay step"), M.ScoutCorridor(TArray<FHavocE02Contact>(), HavocE02::CorridorEnd()));
    HavocE02Tests::ToBridge(*this, M);
    TestEqual(TEXT("scouted contact is tracked"), M.GetContacts().Num(), 1);
    const TArray<FString> Choices = M.GetRunner()->GetChoices();
    TestEqual(TEXT("two routes"), Choices.Num(), 2);
    TestTrue(TEXT("riverbed first"), Choices.Num() == 2 && Choices[0] == TEXT("riverbed_detour") && Choices[1] == TEXT("drainage_infiltration"));

    M.ChooseRoute(TEXT("riverbed_detour"));
    TestTrue(TEXT("ambushed"), M.GetConvoy()->GetState() == EHavocConvoyState::Ambushed);
    TestTrue(TEXT("riverbed route"), M.GetConvoy()->GetRoute() == EHavocConvoyRoute::Riverbed);
    TestEqual(TEXT("three ambushers"), M.GetRemainingAmbush().Num(), 3);
    M.DamageVehicle(TEXT("nomad-2"), 0.4); // damaged, not destroyed
    for (const FString& Id : M.GetRemainingAmbush()) { M.ClearAmbushContact(Id); }
    TestEqual(TEXT("ambush over"), M.GetRemainingAmbush().Num(), 0);
    M.Progress();
    TestEqual(TEXT("south gate"), M.GetState(), FString(TEXT("E02_SOUTH_GATE_APPROACH")));
    M.ClearStaging();
    M.Progress();
    TestEqual(TEXT("staging"), M.GetState(), FString(TEXT("E02_STAGING_ESTABLISHED")));
    TestEqual(TEXT("full strength"), M.Finish(), FString(TEXT("E02_COMPLETE")));
    TestEqual(TEXT("outcome"), M.GetRunner()->GetOutcome(), FString(TEXT("SUCCESS")));

    const FHavocEpisode02Carryover C = M.GetCarryover();
    TestEqual(TEXT("episode"), C.Episode, 2);
    TestEqual(TEXT("carryover outcome"), C.Outcome, FString(TEXT("SUCCESS")));
    TestTrue(TEXT("crew delivered"), C.bCrewDelivered);
    TestEqual(TEXT("nomads"), C.NomadsRemaining, 2);
    TestTrue(TEXT("warden"), C.bWardenOperational);
    TestEqual(TEXT("route"), C.Route, FString(TEXT("RIVERBED")));
    TestEqual(TEXT("relays online"), C.RelaysOnline, 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHavocE02DrainageTest, "MidnightHavoc.Missions.Episode02.DrainageInfiltrationWithLosses", HAVOC_E02_TEST_FLAGS)
bool FHavocE02DrainageTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<UHavocSilentCorridorMission> Mission = HavocE02Tests::MakeMission(*this);
    if (!Mission.IsValid()) { return false; }
    UHavocSilentCorridorMission& M = *Mission;
    HavocE02Tests::ToBridge(*this, M);

    M.ChooseRoute(TEXT("drainage_infiltration"));
    TestFalse(TEXT("no vehicle fits the drainage tunnel"), M.GetConvoy()->Depart(EHavocConvoyRoute::RiverDrainage));
    M.ClearBridge(); // too early: far bank not clear
    M.Progress();
    TestEqual(TEXT("still in the tunnel"), M.GetState(), FString(TEXT("E02_DRAINAGE_INFILTRATION")));
    M.ClearFarBank();
    M.Progress();
    TestEqual(TEXT("bridge cleared state"), M.GetState(), FString(TEXT("E02_BRIDGE_CLEARED")));
    M.DamageVehicle(TEXT("nomad-2"), 1.0); // supplies truck destroyed
    TestEqual(TEXT("losing supplies is not mission failure"), M.GetState(), FString(TEXT("E02_BRIDGE_CLEARED")));
    M.ClearBridge();
    M.Progress();
    M.ClearStaging();
    M.Progress();
    TestEqual(TEXT("with losses"), M.Finish(), FString(TEXT("E02_COMPLETE_WITH_LOSSES")));
    TestEqual(TEXT("partial"), M.GetRunner()->GetOutcome(), FString(TEXT("PARTIAL")));
    TestEqual(TEXT("one nomad left"), M.GetCarryover().NomadsRemaining, 1);
    TestEqual(TEXT("drainage route"), M.GetCarryover().Route, FString(TEXT("RIVER_DRAINAGE")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHavocE02FailRestoreTest, "MidnightHavoc.Missions.Episode02.CrewLostFailsAndCheckpointRestores", HAVOC_E02_TEST_FLAGS)
bool FHavocE02FailRestoreTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<UHavocSilentCorridorMission> Mission = HavocE02Tests::MakeMission(*this);
    if (!Mission.IsValid()) { return false; }
    UHavocSilentCorridorMission& M = *Mission;
    HavocE02Tests::ToBridge(*this, M);

    TestTrue(TEXT("has a checkpoint"), M.GetRunner()->HasCheckpoint());
    TestEqual(TEXT("checkpoint at the bridge"), M.GetRunner()->GetCheckpoint().State, FString(TEXT("E02_BRIDGE_OBSTRUCTED")));
    M.ChooseRoute(TEXT("riverbed_detour"));
    M.DamageVehicle(TEXT("nomad-1"), 1.0);
    TestEqual(TEXT("convoy lost"), M.GetState(), FString(TEXT("E02_CONVOY_LOST")));
    TestEqual(TEXT("failed"), M.GetRunner()->GetOutcome(), FString(TEXT("FAILED")));
    TestTrue(TEXT("restore"), M.GetRunner()->RestoreCheckpoint());
    TestEqual(TEXT("back at the bridge"), M.GetState(), FString(TEXT("E02_BRIDGE_OBSTRUCTED")));
    TestTrue(TEXT("vehicles are back as they were at the checkpoint"), M.GetConvoy()->IsCrewSafe());
    TestTrue(TEXT("outcome cleared"), M.GetRunner()->GetOutcome().IsEmpty());
    TestEqual(TEXT("both routes again"), M.GetRunner()->GetChoices().Num(), 2);
    TestEqual(TEXT("ambush list restored"), M.GetRemainingAmbush().Num(), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHavocE02NoCrewTest, "MidnightHavoc.Missions.Episode02.WithoutFalconCrew", HAVOC_E02_TEST_FLAGS)
bool FHavocE02NoCrewTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<UHavocSilentCorridorMission> Mission = HavocE02Tests::MakeMission(*this, /*bCrewRecovered*/ false);
    if (!Mission.IsValid()) { return false; }
    UHavocSilentCorridorMission& M = *Mission;
    HavocE02Tests::ToBridge(*this, M);

    TestFalse(TEXT("no crew aboard"), M.GetConvoy()->CarriesCrew());
    M.DamageVehicle(TEXT("nomad-1"), 1.0);
    TestEqual(TEXT("one nomad is enough"), M.GetState(), FString(TEXT("E02_BRIDGE_OBSTRUCTED")));
    M.DamageVehicle(TEXT("nomad-2"), 1.0);
    TestEqual(TEXT("every nomad lost fails the mission"), M.GetState(), FString(TEXT("E02_CONVOY_LOST")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHavocE02PartsTest, "MidnightHavoc.Missions.Episode02.ConvoyRelaysAndFlowHelpers", HAVOC_E02_TEST_FLAGS)
bool FHavocE02PartsTest::RunTest(const FString& Parameters)
{
    // Convoy.
    TStrongObjectPtr<UHavocConvoyEscort> Convoy(NewObject<UHavocConvoyEscort>(GetTransientPackage()));
    Convoy->Form(true);
    TestEqual(TEXT("three vehicles"), Convoy->GetVehicles().Num(), 3);
    TestTrue(TEXT("full strength"), Convoy->IsFullStrength());
    TestTrue(TEXT("main road"), Convoy->Depart(EHavocConvoyRoute::MainRoad));
    TestTrue(TEXT("moving"), Convoy->GetState() == EHavocConvoyState::Moving);
    TestFalse(TEXT("drainage is infantry only"), Convoy->Depart(EHavocConvoyRoute::RiverDrainage));
    Convoy->Ambush();
    TestTrue(TEXT("ambushed"), Convoy->GetState() == EHavocConvoyState::Ambushed);
    Convoy->ApplyDamage(TEXT("nomad-2"), 0.4);
    TestTrue(TEXT("repair"), Convoy->Repair(TEXT("nomad-2"), 0.2));
    TestEqual(TEXT("damage then repair"), Convoy->GetVehicles()[2].Integrity, 0.8, 1e-9);
    Convoy->ApplyDamage(TEXT("nomad-2"), 5.0);
    TestEqual(TEXT("integrity floors at 0"), Convoy->GetVehicles()[2].Integrity, 0.0);
    TestFalse(TEXT("destroyed cannot be repaired"), Convoy->Repair(TEXT("nomad-2"), 0.5));
    TestFalse(TEXT("unknown vehicle"), Convoy->ApplyDamage(TEXT("nope"), 1.0));
    TestFalse(TEXT("no longer full strength"), Convoy->IsFullStrength());
    TestEqual(TEXT("one nomad"), Convoy->GetNomadsRemaining(), 1);
    TestFalse(TEXT("not critical while the crew truck lives"), Convoy->IsMissionCritical());
    Convoy->Arrive();
    TestTrue(TEXT("arrived"), Convoy->GetState() == EHavocConvoyState::Arrived);
    TestFalse(TEXT("arrived convoys do not depart"), Convoy->Depart(EHavocConvoyRoute::MainRoad));

    // Snapshot round trip.
    TStrongObjectPtr<UHavocConvoyEscort> Copy(NewObject<UHavocConvoyEscort>(GetTransientPackage()));
    Copy->Restore(Convoy->Snapshot());
    TestTrue(TEXT("state restored"), Copy->GetState() == EHavocConvoyState::Arrived);
    TestEqual(TEXT("vehicles restored"), Copy->GetVehicles().Num(), 3);
    TestEqual(TEXT("integrity restored"), Copy->GetVehicles()[2].Integrity, 0.0);
    TestTrue(TEXT("cargo restored"), Copy->CarriesCrew());

    // Relays: the base reaches 1600 m; Relay Kilo extends the link down the corridor.
    TStrongObjectPtr<UHavocRelayNetwork> Relays(NewObject<UHavocRelayNetwork>(GetTransientPackage()));
    TestTrue(TEXT("corridor end is out of the base's range"), Relays->Classify(HavocE02::CorridorEnd()) == EHavocSignalState::Lost);
    Relays->Place(TEXT("relay-kilo"), HavocE02::RelayKilo());
    TestFalse(TEXT("corridor end is no longer lost"), Relays->Classify(HavocE02::CorridorEnd()) == EHavocSignalState::Lost);
    TestEqual(TEXT("one field relay online"), Relays->GetOnline(), 1);
    Relays->JamAll();
    TestEqual(TEXT("jammed"), Relays->GetOnline(), 0);
    TestTrue(TEXT("jammed link is lost"), Relays->Classify(HavocE02::CorridorEnd()) == EHavocSignalState::Lost);
    TestFalse(TEXT("too far to restore"), Relays->Restore(TEXT("relay-kilo"), FVector(0.0, 0.0, 0.0)));
    TestTrue(TEXT("close enough to restore"), Relays->Restore(TEXT("relay-kilo"), FVector(610.0, 40.0, 905.0)));
    TestEqual(TEXT("interference gone"), Relays->GetInterference(), 0.0);
    TStrongObjectPtr<UHavocRelayNetwork> RelayCopy(NewObject<UHavocRelayNetwork>(GetTransientPackage()));
    RelayCopy->RestoreSnapshot(Relays->Snapshot());
    TestEqual(TEXT("relay snapshot round trip"), RelayCopy->GetOnline(), 1);

    // Flow helpers: the HUD tracker marks done / current / to do.
    const TArray<FString> Tracker = AHavocEpisode02Flow::BuildTracker(TEXT("E02_COMMS_OUTAGE"), false);
    TestEqual(TEXT("eight tracker steps"), Tracker.Num(), 8);
    TestTrue(TEXT("done before"), Tracker.Num() == 8 && Tracker[0].StartsWith(TEXT("[x]")) && Tracker[2].StartsWith(TEXT("[x]")));
    TestTrue(TEXT("current"), Tracker.Num() == 8 && Tracker[3].StartsWith(TEXT("[>]")));
    TestTrue(TEXT("to do after"), Tracker.Num() == 8 && Tracker[7].StartsWith(TEXT("[ ]")));
    const TArray<FString> Done = AHavocEpisode02Flow::BuildTracker(TEXT("E02_COMPLETE"), true);
    TestTrue(TEXT("all done on completion"), Done.Num() == 8 && Done[7].StartsWith(TEXT("[x]")));
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
