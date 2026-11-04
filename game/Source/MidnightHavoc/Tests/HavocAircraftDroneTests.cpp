// Automation tests for the Falcon helicopter model and the drone model.
// Mirrors the drone cases in src/tests/system.test.ts and the crash/recovery sequence in
// src/tests/helicopter-crash.test.ts (the sequence the mission drives in HelicopterCrashEvent.ts).
// Run from the editor: Tools > Session Frontend > Automation, filter "MidnightHavoc.Aircraft"
// or "MidnightHavoc.Drones" (see game/README.md, "Running tests").
// The tests use the pure-data models only, so no world or actor is needed.

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Aircraft/HavocCrashSequence.h"
#include "Aircraft/HavocHelicopterModel.h"
#include "Drones/HavocDroneModel.h"

#define HAVOC_AD_TEST_FLAGS (EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace HavocAircraftDroneTests
{
    FHavocDroneCore MakeLaunched(const FHavocDroneDefinition& Def)
    {
        FHavocDroneCore Core;
        Core.Definition = Def;
        Core.Launch(FVector::ZeroVector);
        Core.Activate();
        return Core;
    }

    const FVector CrashPos(740.0, 22.0, 910.0);
}

// ---------------------------------------------------------------- drones

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHavocDroneLaunchTest, "MidnightHavoc.Drones.Launch", HAVOC_AD_TEST_FLAGS)
bool FHavocDroneLaunchTest::RunTest(const FString& Parameters)
{
    // 'Raven launches and reports telemetry'
    FHavocDroneCore Core;
    Core.Definition = FHavocDroneDefinition::MakeRaven();
    TestEqual(TEXT("raven id"), Core.Definition.Id, FString(TEXT("raven-1")));
    TestTrue(TEXT("launch"), Core.Launch(FVector::ZeroVector));
    Core.Activate();
    TestTrue(TEXT("active"), Core.State == EHavocDroneState::Active);
    TestFalse(TEXT("cannot launch twice"), Core.Launch(FVector::ZeroVector));

    const FHavocDroneTelemetry T = Core.Telemetry();
    TestEqual(TEXT("telemetry name"), T.DisplayName, FString(TEXT("Raven-1")));
    TestEqual(TEXT("telemetry battery"), T.Battery, 1.0);
    TestTrue(TEXT("telemetry task empty"), T.Task.IsEmpty());

    // TS parameters of the other two drones.
    const FHavocDroneDefinition Warden = FHavocDroneDefinition::MakeWarden();
    TestEqual(TEXT("warden speed"), Warden.MaxSpeed, 22.0);
    TestTrue(TEXT("warden holds"), Warden.FallbackBehavior == EHavocLostLinkBehavior::HoldPosition);
    const FHavocDroneDefinition Talon = FHavocDroneDefinition::MakeTalon();
    TestEqual(TEXT("talon drain"), Talon.BatteryDrainPerSecond, 0.003);
    TestTrue(TEXT("talon mission defined"), Talon.FallbackBehavior == EHavocLostLinkBehavior::MissionDefined);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHavocDroneBatteryTest, "MidnightHavoc.Drones.Battery", HAVOC_AD_TEST_FLAGS)
bool FHavocDroneBatteryTest::RunTest(const FString& Parameters)
{
    // DroneBattery.ts
    FHavocDroneBattery Battery;
    TestEqual(TEXT("starts full"), Battery.Charge, 1.0);
    TestEqual(TEXT("consume"), Battery.Consume(0.5), 0.5);
    TestEqual(TEXT("negative consume ignored"), Battery.Consume(-1.0), 0.5);
    TestEqual(TEXT("recharge clamps"), Battery.Recharge(2.0), 1.0);
    Battery.Charge = 0.25;
    TestTrue(TEXT("low at 0.25"), Battery.IsLow());
    TestFalse(TEXT("not critical at 0.25"), Battery.IsCritical());
    Battery.Charge = 0.1;
    TestTrue(TEXT("critical at 0.1"), Battery.IsCritical());
    TestEqual(TEXT("floors at zero"), Battery.Consume(5.0), 0.0);

    // DroneBase.update drains batteryDrainPerSecond * delta.
    FHavocDroneCore Raven = HavocAircraftDroneTests::MakeLaunched(FHavocDroneDefinition::MakeRaven());
    Raven.Update(10.0, 1.0);
    TestNearlyEqual(TEXT("raven drains 0.002/s"), Raven.Battery.Charge, 0.98, 1e-9);
    TestTrue(TEXT("still active"), Raven.State == EHavocDroneState::Active);

    // At or below 0.1 a controllable drone returns home.
    Raven.Battery.Charge = 0.101;
    Raven.Update(1.0, 1.0);
    TestTrue(TEXT("low battery returns"), Raven.State == EHavocDroneState::Returning);
    TestEqual(TEXT("return task"), Raven.Task, FString(TEXT("RETURN_HOME")));

    // A stowed drone neither drains nor changes state.
    FHavocDroneCore Stowed;
    Stowed.Definition = FHavocDroneDefinition::MakeRaven();
    Stowed.Update(100.0, 0.0);
    TestEqual(TEXT("stowed battery untouched"), Stowed.Battery.Charge, 1.0);
    TestTrue(TEXT("stowed stays stowed"), Stowed.State == EHavocDroneState::Stowed);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHavocDroneSignalTest, "MidnightHavoc.Drones.Signal", HAVOC_AD_TEST_FLAGS)
bool FHavocDroneSignalTest::RunTest(const FString& Parameters)
{
    // DroneSignalManager.ts
    const FVector Relay = FVector::ZeroVector;
    TestEqual(TEXT("at relay"), FHavocDroneSignalManager::Calculate(Relay, Relay, 1600.0), 1.0);
    TestNearlyEqual(TEXT("half range"), FHavocDroneSignalManager::Calculate(FVector(800, 0, 0), Relay, 1600.0), 0.5, 1e-9);
    TestEqual(TEXT("out of range"), FHavocDroneSignalManager::Calculate(FVector(5000, 0, 0), Relay, 1600.0), 0.0);
    TestNearlyEqual(TEXT("interference"), FHavocDroneSignalManager::Calculate(Relay, Relay, 1600.0, 0.25), 0.75, 1e-9);
    TestEqual(TEXT("max range floor of 1"), FHavocDroneSignalManager::Calculate(Relay, Relay, 0.0), 1.0);

    TestTrue(TEXT("lost"), FHavocDroneSignalManager::Classify(0.0) == EHavocSignalState::Lost);
    TestTrue(TEXT("critical"), FHavocDroneSignalManager::Classify(0.14) == EHavocSignalState::Critical);
    TestTrue(TEXT("degraded"), FHavocDroneSignalManager::Classify(0.15) == EHavocSignalState::Degraded);
    TestTrue(TEXT("stable"), FHavocDroneSignalManager::Classify(0.4) == EHavocSignalState::Stable);
    TestTrue(TEXT("strong"), FHavocDroneSignalManager::Classify(0.75) == EHavocSignalState::Strong);

    // Lost-link fallback per definition (DroneBase.handleSignalLost).
    FHavocDroneCore Raven = HavocAircraftDroneTests::MakeLaunched(FHavocDroneDefinition::MakeRaven());
    Raven.Update(1.0, 0.0);
    TestTrue(TEXT("raven returns"), Raven.State == EHavocDroneState::Returning);
    TestEqual(TEXT("raven task"), Raven.Task, FString(TEXT("LOST_LINK_RETURN")));

    FHavocDroneCore Warden = HavocAircraftDroneTests::MakeLaunched(FHavocDroneDefinition::MakeWarden());
    Warden.Update(1.0, 0.0);
    TestTrue(TEXT("warden holds, link lost"), Warden.State == EHavocDroneState::SignalLost);
    TestEqual(TEXT("warden task"), Warden.Task, FString(TEXT("HOLD")));
    TestFalse(TEXT("lost drone is not controllable"), Warden.MoveTo(FVector(1, 2, 3)));

    FHavocDroneCore Talon = HavocAircraftDroneTests::MakeLaunched(FHavocDroneDefinition::MakeTalon());
    Talon.Update(1.0, 0.0);
    TestTrue(TEXT("talon link lost"), Talon.State == EHavocDroneState::SignalLost);
    TestEqual(TEXT("talon task"), Talon.Task, FString(TEXT("AWAIT_MISSION_RULE")));

    FHavocDroneDefinition LandDef = FHavocDroneDefinition::MakeRaven(TEXT("lander"));
    LandDef.FallbackBehavior = EHavocLostLinkBehavior::Land;
    FHavocDroneCore Lander = HavocAircraftDroneTests::MakeLaunched(LandDef);
    Lander.Update(1.0, 0.0);
    TestTrue(TEXT("land disables"), Lander.State == EHavocDroneState::Disabled);
    TestEqual(TEXT("land task"), Lander.Task, FString(TEXT("LAND")));

    // Weak (but not lost) signal degrades, except while returning.
    FHavocDroneCore Weak = HavocAircraftDroneTests::MakeLaunched(FHavocDroneDefinition::MakeRaven());
    Weak.Update(1.0, 0.2);
    TestTrue(TEXT("degraded"), Weak.State == EHavocDroneState::SignalDegraded);
    TestEqual(TEXT("signal stored"), Weak.Signal, 0.2);

    FHavocDroneCore Returning = HavocAircraftDroneTests::MakeLaunched(FHavocDroneDefinition::MakeRaven());
    Returning.ReturnHome();
    Returning.Update(1.0, 0.2);
    TestTrue(TEXT("returning stays returning"), Returning.State == EHavocDroneState::Returning);

    // Flight controller step.
    TestTrue(TEXT("step snaps when in reach"),
        FHavocDroneFlightController::Step(FVector::ZeroVector, FVector(10, 0, 0), 28.0, 1.0).Equals(FVector(10, 0, 0)));
    TestTrue(TEXT("step moves speed*dt"),
        FHavocDroneFlightController::Step(FVector::ZeroVector, FVector(100, 0, 0), 28.0, 1.0).Equals(FVector(28, 0, 0)));

    // Sensor: visible and in range only.
    TArray<FHavocSensorCandidate> Candidates;
    FHavocSensorCandidate Near; Near.Id = TEXT("near"); Near.Position = FVector(100, 0, 0); Near.bVisible = true;
    FHavocSensorCandidate Hidden = Near; Hidden.Id = TEXT("hidden"); Hidden.bVisible = false;
    FHavocSensorCandidate Far = Near; Far.Id = TEXT("far"); Far.Position = FVector(1000, 0, 0);
    Candidates.Add(Near); Candidates.Add(Hidden); Candidates.Add(Far);
    const TArray<FHavocSensorCandidate> Seen = FHavocDroneSensor::Scan(FVector::ZeroVector, 450.0, Candidates);
    TestEqual(TEXT("one detection"), Seen.Num(), 1);
    TestTrue(TEXT("the near visible one"), Seen.Num() == 1 && Seen[0].Id == FName(TEXT("near")));
    return true;
}

// ---------------------------------------------------------------- aircraft

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHavocHelicopterDamageTest, "MidnightHavoc.Aircraft.Damage", HAVOC_AD_TEST_FLAGS)
bool FHavocHelicopterDamageTest::RunTest(const FString& Parameters)
{
    // HelicopterDamageState.ts
    FHavocHelicopterDamageState Damage;
    TestTrue(TEXT("operational"), Damage.GetLevel() == EHavocHelicopterDamageLevel::Operational);
    Damage.ApplyDamage(0.45, 0.55, 0.25);
    TestNearlyEqual(TEXT("flight control"), Damage.FlightControl, 0.55, 1e-9);
    TestNearlyEqual(TEXT("engine"), Damage.EngineCondition, 0.45, 1e-9);
    TestNearlyEqual(TEXT("structure"), Damage.StructuralCondition, 0.75, 1e-9);
    TestTrue(TEXT("damaged (lowest 0.45)"), Damage.GetLevel() == EHavocHelicopterDamageLevel::Damaged);
    Damage.ApplyDamage(-5.0, 0.2, 0.0);
    TestNearlyEqual(TEXT("negative damage ignored"), Damage.FlightControl, 0.55, 1e-9);
    TestTrue(TEXT("critical (lowest 0.25)"), Damage.GetLevel() == EHavocHelicopterDamageLevel::Critical);
    Damage.ApplyDamage(0.0, 0.15, 0.0);
    TestTrue(TEXT("disabled (lowest 0.10)"), Damage.GetLevel() == EHavocHelicopterDamageLevel::Disabled);
    Damage.ApplyDamage(0.0, 1.0, 0.0);
    TestTrue(TEXT("destroyed"), Damage.GetLevel() == EHavocHelicopterDamageLevel::Destroyed);
    TestEqual(TEXT("floors at 0"), Damage.EngineCondition, 0.0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHavocHelicopterCrashRecoveryTest, "MidnightHavoc.Aircraft.CrashRecovery", HAVOC_AD_TEST_FLAGS)
bool FHavocHelicopterCrashRecoveryTest::RunTest(const FString& Parameters)
{
    using namespace HavocAircraftDroneTests;
    FHavocHelicopterState Heli;
    Heli.Id = TEXT("falcon-2");
    Heli.Callsign = TEXT("FALCON-2");
    FHavocCrashSequence Seq;

    TestTrue(TEXT("inactive"), Seq.State == EHavocCrashSequenceState::Inactive);

    // Out-of-order calls are ignored.
    Seq.TriggerDistress(Heli);
    Seq.RegisterCrash(Heli, CrashPos);
    Seq.BeginSearch();
    TestTrue(TEXT("still inactive"), Seq.State == EHavocCrashSequenceState::Inactive);

    Seq.BeginInbound(Heli, FVector(920, 210, 680));
    TestTrue(TEXT("inbound"), Seq.State == EHavocCrashSequenceState::AircraftInbound);
    TestTrue(TEXT("flight inbound"), Heli.FlightState == EHavocHelicopterFlightState::Inbound);

    Seq.TriggerDistress(Heli);
    TestTrue(TEXT("distress"), Seq.State == EHavocCrashSequenceState::DistressReceived);
    TestTrue(TEXT("flight distress"), Heli.FlightState == EHavocHelicopterFlightState::Distress);
    TestTrue(TEXT("damaged"), Heli.Damage.GetLevel() == EHavocHelicopterDamageLevel::Damaged);

    Seq.LoseSignal();
    TestTrue(TEXT("signal lost"), Seq.State == EHavocCrashSequenceState::SignalLost);

    Seq.RegisterCrash(Heli, CrashPos);
    TestTrue(TEXT("crash confirmed"), Seq.State == EHavocCrashSequenceState::CrashConfirmed);
    TestTrue(TEXT("flight crashed"), Heli.FlightState == EHavocHelicopterFlightState::Crashed);
    Heli.UpdatePosition(FVector::ZeroVector);
    TestTrue(TEXT("crashed aircraft ignores position updates"), Heli.Position.Equals(CrashPos));

    Seq.BeginSearch();
    TestTrue(TEXT("search"), Seq.State == EHavocCrashSequenceState::SearchActive);
    TestTrue(TEXT("site searching"), Seq.CrashSite.State == EHavocCrashSiteState::Searching);

    Seq.LocateCrashSite(CrashPos);
    TestTrue(TEXT("located"), Seq.State == EHavocCrashSequenceState::SiteLocated);
    TestTrue(TEXT("beacon on"), Seq.CrashSite.bBeaconActive && Seq.CrashSite.bSmokeVisible);

    Seq.SecureCrashSite();
    TestTrue(TEXT("secured"), Seq.State == EHavocCrashSequenceState::SiteSecured);

    Seq.BeginCrewRecovery();
    TestTrue(TEXT("crew recovery"), Seq.State == EHavocCrashSequenceState::CrewRecovery);
    TestTrue(TEXT("zone active"), Seq.RecoveryZone.State == EHavocRecoveryZoneState::Active);
    TestTrue(TEXT("zone contains the site"), Seq.RecoveryZone.Contains(CrashPos));
    TestFalse(TEXT("zone excludes far points"), Seq.RecoveryZone.Contains(CrashPos + FVector(500, 0, 0)));

    TestTrue(TEXT("locate pilot"), Seq.Crew.Locate(TEXT("falcon-pilot"), EHavocCrewCondition::Injured));
    TestTrue(TEXT("locate copilot"), Seq.Crew.Locate(TEXT("falcon-copilot"), EHavocCrewCondition::Critical));
    TestTrue(TEXT("locate chief"), Seq.Crew.Locate(TEXT("falcon-chief"), EHavocCrewCondition::Operational));
    TestFalse(TEXT("unknown crew"), Seq.Crew.Locate(TEXT("nobody"), EHavocCrewCondition::Operational));
    TestEqual(TEXT("located count"), Seq.Crew.GetLocatedCount(), 3);

    Seq.StartRecoveryDefense();
    TestTrue(TEXT("defense"), Seq.State == EHavocCrashSequenceState::RecoveryDefense);
    TestTrue(TEXT("site contested"), Seq.CrashSite.State == EHavocCrashSiteState::Contested);
    TestTrue(TEXT("zone contested"), Seq.RecoveryZone.State == EHavocRecoveryZoneState::Contested);

    TestFalse(TEXT("cannot complete before recovery"), Seq.CompleteEvacuation());
    TestTrue(TEXT("pilot recovered"), Seq.Crew.Recover(TEXT("falcon-pilot")));
    TestTrue(TEXT("copilot recovered"), Seq.Crew.Recover(TEXT("falcon-copilot")));
    TestFalse(TEXT("two of three is not enough"), Seq.CompleteEvacuation());
    TestTrue(TEXT("chief recovered"), Seq.Crew.Recover(TEXT("falcon-chief")));

    // Recovery defense leaves the site contested: secure again before completing (HelicopterCrashEvent.evacuateCrew).
    Seq.SecureCrashSite();
    TestTrue(TEXT("re-secured"), Seq.State == EHavocCrashSequenceState::SiteSecured);
    TestTrue(TEXT("evacuation complete"), Seq.CompleteEvacuation());
    TestTrue(TEXT("sequence complete"), Seq.State == EHavocCrashSequenceState::EvacuationComplete);
    TestEqual(TEXT("three recovered"), Seq.Crew.GetRecoveredCount(), 3);
    TestTrue(TEXT("site recovery complete"), Seq.CrashSite.State == EHavocCrashSiteState::RecoveryComplete);
    TestTrue(TEXT("zone evacuation complete"), Seq.RecoveryZone.State == EHavocRecoveryZoneState::EvacuationComplete);
    TestFalse(TEXT("beacon off"), Seq.CrashSite.bBeaconActive);

    // Aircraft recovery: CRASHED -> RECOVERED only.
    Heli.RecoverAircraft();
    TestTrue(TEXT("aircraft recovered"), Heli.FlightState == EHavocHelicopterFlightState::Recovered);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHavocHelicopterAbandonTest, "MidnightHavoc.Aircraft.Abandon", HAVOC_AD_TEST_FLAGS)
bool FHavocHelicopterAbandonTest::RunTest(const FString& Parameters)
{
    FHavocHelicopterState Heli;
    FHavocCrashSequence Seq;
    Seq.BeginInbound(Heli, FVector::ZeroVector);
    Seq.TriggerDistress(Heli);
    Seq.RegisterCrash(Heli, HavocAircraftDroneTests::CrashPos);
    Seq.BeginSearch();
    Seq.LocateCrashSite(HavocAircraftDroneTests::CrashPos);

    Seq.Abandon();
    TestTrue(TEXT("abandoned"), Seq.State == EHavocCrashSequenceState::Abandoned);
    TestTrue(TEXT("site abandoned"), Seq.CrashSite.State == EHavocCrashSiteState::Abandoned);
    TestFalse(TEXT("beacon off"), Seq.CrashSite.bBeaconActive);

    // A lost crew member does not block completion, but cannot be recovered.
    FHavocCrewState Crew;
    Crew.Members = FHavocCrewState::MakeFalconCrew();
    Crew.Locate(TEXT("falcon-pilot"), EHavocCrewCondition::Lost);
    TestFalse(TEXT("lost crew cannot be recovered"), Crew.Recover(TEXT("falcon-pilot")));
    Crew.Locate(TEXT("falcon-copilot"), EHavocCrewCondition::Injured);
    Crew.Locate(TEXT("falcon-chief"), EHavocCrewCondition::Injured);
    Crew.Recover(TEXT("falcon-copilot"));
    Crew.Recover(TEXT("falcon-chief"));
    TestTrue(TEXT("all recovered or lost"), Crew.AllRecovered());

    // Seats come from data/aircraft/falcon-helicopter.json: pilot, gunner, passenger.
    Crew.AssignSeats({ TEXT("pilot"), TEXT("gunner") });
    TestTrue(TEXT("first seat"), Crew.Members[0].Seat == FName(TEXT("pilot")));
    TestTrue(TEXT("no seat left"), Crew.Members[2].Seat.IsNone());
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
