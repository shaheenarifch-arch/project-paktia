// Automation tests for the playable vehicles (item 7 of the real-playable plan): stats read from
// data/vehicles/*.json, spawn of the four vehicle classes, enter / exit, and the movement math.
// Run: Tools > Session Frontend > Automation, filter "MidnightHavoc.Vehicles".
// The world tests build a bare game world (no game instance), so stats are applied from the JSON
// directly with ApplyDefinition; level placement is checked by the manual checklist in docs/vehicles.md.

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AIController.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "JsonObjectConverter.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Data/HavocVehicleDef.h"
#include "Vehicles/HavocFlyingVehicles.h"
#include "Vehicles/HavocSurfaceVehicles.h"
#include "Vehicles/HavocVehicleBase.h"

#define HAVOC_VEHICLE_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace HavocVehicleTests
{
    /** Ids of the four vehicles placed in L_KhargHormuz. */
    const TCHAR* const VehicleIds[] = { TEXT("tern_patrol_boat"), TEXT("warden_tank"), TEXT("kestrel_scout_helicopter"), TEXT("wisp_recon_drone") };

    bool LoadDef(FAutomationTestBase& Test, const FString& Id, FHavocVehicleDef& Out)
    {
        const FString Dir = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../data/vehicles")));
        TArray<FString> Files;
        IFileManager::Get().FindFiles(Files, *FPaths::Combine(Dir, TEXT("*.json")), true, false);
        for (const FString& File : Files)
        {
            FString Text;
            TSharedPtr<FJsonObject> Json;
            if (!FFileHelper::LoadFileToString(Text, *FPaths::Combine(Dir, File))
                || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Json) || !Json.IsValid())
            {
                continue;
            }
            FString JsonId;
            if (!Json->TryGetStringField(TEXT("id"), JsonId) || JsonId != Id)
            {
                continue;
            }
            if (!FJsonObjectConverter::JsonObjectToUStruct<FHavocVehicleDef>(Json.ToSharedRef(), &Out, 0, 0))
            {
                Test.AddError(FString::Printf(TEXT("Could not parse %s"), *File));
                return false;
            }
            Json->TryGetBoolField(TEXT("fictional"), Out.bFictional);
            return true;
        }
        Test.AddError(FString::Printf(TEXT("No data/vehicles file with id '%s'"), *Id));
        return false;
    }

    /** A bare game world for spawning actors. */
    struct FTestWorld
    {
        UWorld* World = nullptr;

        FTestWorld()
        {
            World = UWorld::CreateWorld(EWorldType::Game, false);
            FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
            Context.SetCurrentWorld(World);
            World->InitializeActorsForPlay(FURL());
            World->BeginPlay();
        }

        ~FTestWorld()
        {
            if (World)
            {
                GEngine->DestroyWorldContext(World);
                World->DestroyWorld(false);
            }
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHavocVehicleDataTest, "MidnightHavoc.Vehicles.Data", HAVOC_VEHICLE_TEST_FLAGS)
bool FHavocVehicleDataTest::RunTest(const FString& Parameters)
{
    for (const TCHAR* Id : HavocVehicleTests::VehicleIds)
    {
        FHavocVehicleDef Def;
        if (!HavocVehicleTests::LoadDef(*this, Id, Def))
        {
            continue;
        }
        TestTrue(FString::Printf(TEXT("%s is fictional"), Id), Def.bFictional);
        TestTrue(FString::Printf(TEXT("%s has a seat"), Id), Def.Seats.Num() > 0);
        TestTrue(FString::Printf(TEXT("%s max speed read from JSON"), Id), Def.Stats.MaxSpeed > 0.0);
        TestTrue(FString::Printf(TEXT("%s acceleration read from JSON"), Id), Def.Stats.Acceleration > 0.0);
        TestTrue(FString::Printf(TEXT("%s turn rate read from JSON"), Id), Def.Stats.TurnRate > 0.0);
    }

    // Flyers have a climb rate, surface vehicles do not.
    FHavocVehicleDef Heli, Drone, Boat;
    if (HavocVehicleTests::LoadDef(*this, TEXT("kestrel_scout_helicopter"), Heli)) { TestEqual(TEXT("heli climb rate"), Heli.Stats.ClimbRate, 8.0); }
    if (HavocVehicleTests::LoadDef(*this, TEXT("wisp_recon_drone"), Drone)) { TestEqual(TEXT("drone climb rate"), Drone.Stats.ClimbRate, 10.0); }
    if (HavocVehicleTests::LoadDef(*this, TEXT("tern_patrol_boat"), Boat)) { TestEqual(TEXT("boat max speed"), Boat.Stats.MaxSpeed, 20.0); }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHavocVehicleMathTest, "MidnightHavoc.Vehicles.Math", HAVOC_VEHICLE_TEST_FLAGS)
bool FHavocVehicleMathTest::RunTest(const FString& Parameters)
{
    using namespace HavocVehicleMath;
    // Full throttle accelerates and caps at max speed.
    double Speed = 0.0;
    Speed = StepSpeed(Speed, 1.0, 1000.0, 500.0, 300.0, 1.0);
    TestEqual(TEXT("one second at 500 cm/s^2"), Speed, 500.0);
    Speed = StepSpeed(Speed, 1.0, 1000.0, 500.0, 300.0, 10.0);
    TestEqual(TEXT("capped at max speed"), Speed, 1000.0);
    // Reverse is allowed.
    TestTrue(TEXT("reverse"), StepSpeed(0.0, -1.0, 1000.0, 500.0, 300.0, 1.0) < 0.0);
    // No throttle slows by drag and stops at zero.
    TestEqual(TEXT("drag"), StepSpeed(1000.0, 0.0, 1000.0, 500.0, 300.0, 1.0), 700.0);
    TestEqual(TEXT("drag stops"), StepSpeed(100.0, 0.0, 1000.0, 500.0, 300.0, 1.0), 0.0);
    // Steering needs speed, and flips in reverse.
    TestEqual(TEXT("no turn when stopped"), SurfaceYawDelta(1.0, 0.0, 1000.0, 45.0, 1.0), 0.0);
    TestEqual(TEXT("full turn at max speed"), SurfaceYawDelta(1.0, 1000.0, 1000.0, 45.0, 1.0), 45.0);
    TestTrue(TEXT("reverse flips steering"), SurfaceYawDelta(1.0, -500.0, 1000.0, 45.0, 1.0) < 0.0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHavocVehicleSpawnTest, "MidnightHavoc.Vehicles.Spawn", HAVOC_VEHICLE_TEST_FLAGS)
bool FHavocVehicleSpawnTest::RunTest(const FString& Parameters)
{
    HavocVehicleTests::FTestWorld TestWorld;
    UWorld* World = TestWorld.World;
    if (!TestNotNull(TEXT("world"), World))
    {
        return false;
    }

    struct FCase { UClass* Class; const TCHAR* Id; };
    const FCase Cases[] = {
        { AHavocBoatVehicle::StaticClass(), TEXT("tern_patrol_boat") },
        { AHavocGroundVehicle::StaticClass(), TEXT("warden_tank") },
        { AHavocHelicopterVehicle::StaticClass(), TEXT("kestrel_scout_helicopter") },
        { AHavocDroneVehicle::StaticClass(), TEXT("wisp_recon_drone") },
    };
    for (const FCase& Case : Cases)
    {
        AHavocVehicleBase* Vehicle = Cast<AHavocVehicleBase>(World->SpawnActor(Case.Class, &FTransform::Identity));
        if (!TestNotNull(FString::Printf(TEXT("%s spawned"), Case.Id), Vehicle))
        {
            continue;
        }
        TestEqual(TEXT("vehicle id"), Vehicle->VehicleId, FName(Case.Id));
        TestFalse(TEXT("starts empty"), Vehicle->IsOccupied());
        FHavocVehicleDef Def;
        if (HavocVehicleTests::LoadDef(*this, Case.Id, Def))
        {
            Vehicle->ApplyDefinition(Def);
            TestEqual(TEXT("stats applied from JSON"), Vehicle->Stats.MaxSpeed, Def.Stats.MaxSpeed);
            TestEqual(TEXT("seats applied from JSON"), Vehicle->Seats.Num(), Def.Seats.Num());
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHavocVehicleEnterExitTest, "MidnightHavoc.Vehicles.EnterExit", HAVOC_VEHICLE_TEST_FLAGS)
bool FHavocVehicleEnterExitTest::RunTest(const FString& Parameters)
{
    HavocVehicleTests::FTestWorld TestWorld;
    UWorld* World = TestWorld.World;
    if (!TestNotNull(TEXT("world"), World))
    {
        return false;
    }

    ACharacter* Marine = World->SpawnActor<ACharacter>(FVector(0, 0, 100), FRotator::ZeroRotator);
    AAIController* Player = World->SpawnActor<AAIController>();
    AAIController* Other = World->SpawnActor<AAIController>();
    ACharacter* OtherMarine = World->SpawnActor<ACharacter>(FVector(0, 0, 100), FRotator::ZeroRotator);
    AHavocGroundVehicle* Vehicle = World->SpawnActor<AHavocGroundVehicle>(FVector(200, 0, 100), FRotator::ZeroRotator);
    AHavocBoatVehicle* FarBoat = World->SpawnActor<AHavocBoatVehicle>(FVector(500000, 0, 0), FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("actors spawned"), Marine) || !Player || !Other || !OtherMarine || !Vehicle || !FarBoat)
    {
        return false;
    }
    Player->Possess(Marine);
    Other->Possess(OtherMarine);

    TestTrue(TEXT("in range"), Vehicle->CanEnter(Player));
    TestFalse(TEXT("out of range"), FarBoat->CanEnter(Player));
    TestFalse(TEXT("cannot enter out of range"), FarBoat->Enter(Player));
    TestTrue(TEXT("nearest vehicle found"), AHavocVehicleBase::FindEnterable(Player) == Vehicle);

    TestTrue(TEXT("enter"), Vehicle->Enter(Player));
    TestTrue(TEXT("player now possesses the vehicle"), Player->GetPawn() == Vehicle);
    TestTrue(TEXT("vehicle occupied"), Vehicle->IsOccupied() && Vehicle->GetDriver() == Player);
    TestTrue(TEXT("on-foot pawn hidden while driving"), Marine->IsHidden());
    TestFalse(TEXT("occupied vehicle cannot be entered by another"), Vehicle->Enter(Other));
    TestTrue(TEXT("other player keeps their pawn"), Other->GetPawn() == OtherMarine);

    TestTrue(TEXT("exit"), Vehicle->Exit());
    TestTrue(TEXT("player back on the Marine"), Player->GetPawn() == Marine);
    TestFalse(TEXT("vehicle empty again"), Vehicle->IsOccupied());
    TestFalse(TEXT("Marine visible again"), Marine->IsHidden());
    TestFalse(TEXT("second exit does nothing"), Vehicle->Exit());

    // Re-enter works after leaving.
    TestTrue(TEXT("re-enter"), Vehicle->Enter(Player));
    TestTrue(TEXT("exit again"), Vehicle->Exit());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHavocVehicleMoveTest, "MidnightHavoc.Vehicles.Move", HAVOC_VEHICLE_TEST_FLAGS)
bool FHavocVehicleMoveTest::RunTest(const FString& Parameters)
{
    HavocVehicleTests::FTestWorld TestWorld;
    UWorld* World = TestWorld.World;
    if (!TestNotNull(TEXT("world"), World))
    {
        return false;
    }

    AHavocHelicopterVehicle* Heli = World->SpawnActor<AHavocHelicopterVehicle>(FVector(0, 0, 5000), FRotator::ZeroRotator);
    AHavocBoatVehicle* Boat = World->SpawnActor<AHavocBoatVehicle>(FVector(0, 10000, 0), FRotator::ZeroRotator);
    if (!Heli || !Boat)
    {
        AddError(TEXT("vehicles not spawned"));
        return false;
    }
    FHavocVehicleDef HeliDef, BoatDef;
    if (HavocVehicleTests::LoadDef(*this, TEXT("kestrel_scout_helicopter"), HeliDef)) { Heli->ApplyDefinition(HeliDef); }
    if (HavocVehicleTests::LoadDef(*this, TEXT("tern_patrol_boat"), BoatDef)) { Boat->ApplyDefinition(BoatDef); }

    // Helicopter climbs with lift and moves forward with throttle.
    const double StartZ = Heli->GetActorLocation().Z;
    Heli->SetInput(0.0, 0.0, 1.0);
    for (int32 i = 0; i < 10; ++i) { Heli->StepMovement(0.1f); }
    TestTrue(TEXT("helicopter climbs"), Heli->GetActorLocation().Z > StartZ + 100.0);

    Heli->SetInput(1.0, 0.0, 0.0);
    for (int32 i = 0; i < 10; ++i) { Heli->StepMovement(0.1f); }
    TestTrue(TEXT("helicopter moves forward"), Heli->GetActorLocation().X > 100.0);

    // The boat moves, stays on its water level and turns only while moving.
    Boat->SetInput(1.0, 1.0, 0.0);
    for (int32 i = 0; i < 20; ++i) { Boat->StepMovement(0.1f); }
    TestTrue(TEXT("boat moved"), FVector::Dist2D(Boat->GetActorLocation(), FVector(0, 10000, 0)) > 100.0);
    TestTrue(TEXT("boat turned"), !FMath::IsNearlyZero(Boat->GetActorRotation().Yaw, 1.0));
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
