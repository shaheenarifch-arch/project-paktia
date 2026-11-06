// Automation tests for the campaign (Campaign/HavocCampaign.h, ports src/gameplay/campaign/Campaign.ts):
// unlock order, "a worse replay never overwrites a better result", save / load through a SaveGame slot,
// and carryover (Ep01 crew -> Ep02; Ep02 crew / Nomads / Warden -> Ep03; Ep03 SUCCESS -> +10 Ep04 reinforcements).
// Run: Tools > Session Frontend > Automation, filter "MidnightHavoc.Campaign".
// Help: https://dev.epicgames.com/documentation/en-us/unreal-engine/saving-and-loading-your-game-in-unreal-engine

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/StrongObjectPtr.h"
#include "Campaign/HavocCampaign.h"
#include "Missions/Episode04/HavocOpenStraitMission.h"

#define HAVOC_CAMPAIGN_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

// Defined in HavocMissionTests.cpp.
namespace HavocMissionTests
{
    TStrongObjectPtr<UHavocOpenStraitMission> MakeMission(FAutomationTestBase& Test, const FString& Episode3Outcome);
}

namespace HavocCampaignTests
{
    /** A campaign on its own save slot, so the player's real save is never touched. Delete the slot when done. */
    struct FTestCampaign
    {
        TStrongObjectPtr<UHavocCampaign> Campaign;
        FString Slot;

        explicit FTestCampaign(const TCHAR* Name, bool bClear = true)
            : Campaign(NewObject<UHavocCampaign>(GetTransientPackage()))
            , Slot(FString::Printf(TEXT("HavocCampaignTest_%s"), Name))
        {
            Campaign->SlotName = Slot;
            if (bClear)
            {
                UGameplayStatics::DeleteGameInSlot(Slot, 0);
            }
        }
        ~FTestCampaign() { UGameplayStatics::DeleteGameInSlot(Slot, 0); }
        UHavocCampaign* operator->() const { return Campaign.Get(); }
    };

    FHavocCarryover None() { return FHavocCarryover(); }
}

// Unlock: 01 open first; SUCCESS or PARTIAL opens the next; FAILED does not.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHavocCampaignUnlockTest, "MidnightHavoc.Campaign.Unlock", HAVOC_CAMPAIGN_TEST_FLAGS)
bool FHavocCampaignUnlockTest::RunTest(const FString& Parameters)
{
    HavocCampaignTests::FTestCampaign C(TEXT("Unlock"));
    TestTrue(TEXT("01 unlocked first"), C->IsUnlocked(1));
    TestFalse(TEXT("02 locked at start"), C->IsUnlocked(2));
    TestFalse(TEXT("03 locked at start"), C->IsUnlocked(3));
    TestFalse(TEXT("04 locked at start"), C->IsUnlocked(4));
    TestFalse(TEXT("episode 5 does not exist"), C->IsUnlocked(5));
    TestEqual(TEXT("next is 01"), C->GetNextEpisode(), 1);
    TestEqual(TEXT("locked text"), C->StatusText(2), FString(TEXT("Locked")));
    TestEqual(TEXT("unlocked text"), C->StatusText(1), FString(TEXT("Unlocked")));

    TestTrue(TEXT("record 01 FAILED"), C->Record(1, TEXT("FAILED"), HavocCampaignTests::None(), false));
    TestFalse(TEXT("FAILED does not unlock 02"), C->IsUnlocked(2));
    TestEqual(TEXT("still next is 01"), C->GetNextEpisode(), 1);

    C->Record(1, TEXT("PARTIAL"), HavocCampaignTests::None(), false);
    TestTrue(TEXT("PARTIAL unlocks 02"), C->IsUnlocked(2));
    TestFalse(TEXT("but not 03"), C->IsUnlocked(3));
    TestEqual(TEXT("next is 02"), C->GetNextEpisode(), 2);
    TestEqual(TEXT("completed text"), C->StatusText(1), FString(TEXT("Completed: PARTIAL")));

    C->Record(2, TEXT("SUCCESS"), HavocCampaignTests::None(), false);
    TestTrue(TEXT("SUCCESS unlocks 03"), C->IsUnlocked(3));
    TestFalse(TEXT("04 still locked"), C->IsUnlocked(4));
    C->Record(3, TEXT("SUCCESS"), HavocCampaignTests::None(), false);
    TestTrue(TEXT("04 unlocked"), C->IsUnlocked(4));
    C->Record(4, TEXT("SUCCESS"), HavocCampaignTests::None(), false);
    TestEqual(TEXT("campaign finished"), C->GetNextEpisode(), 0);

    TestFalse(TEXT("unknown episode rejected"), C->Record(9, TEXT("SUCCESS"), HavocCampaignTests::None(), false));
    TestFalse(TEXT("unknown outcome rejected"), C->Record(1, TEXT("WON"), HavocCampaignTests::None(), false));
    return true;
}

// A worse replay never overwrites a better result; an equal or better one does.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHavocCampaignReplayTest, "MidnightHavoc.Campaign.WorseReplayKeepsBest", HAVOC_CAMPAIGN_TEST_FLAGS)
bool FHavocCampaignReplayTest::RunTest(const FString& Parameters)
{
    HavocCampaignTests::FTestCampaign C(TEXT("Replay"));
    FHavocCarryover Good; Good.bCrewRecovered = true;
    FHavocCarryover Bad; Bad.bCrewRecovered = false;

    TestTrue(TEXT("first result stored"), C->Record(1, TEXT("SUCCESS"), Good, false));
    TestFalse(TEXT("PARTIAL replay ignored"), C->Record(1, TEXT("PARTIAL"), Bad, false));
    TestFalse(TEXT("FAILED replay ignored"), C->Record(1, TEXT("FAILED"), Bad, false));
    TestEqual(TEXT("still SUCCESS"), C->GetOutcome(1), FString(TEXT("SUCCESS")));
    TestTrue(TEXT("carryover not overwritten"), C->WasCrewRecoveredInEpisode01());
    TestTrue(TEXT("02 stays unlocked"), C->IsUnlocked(2));

    // From PARTIAL a SUCCESS replaces it.
    C->Record(2, TEXT("PARTIAL"), Bad, false);
    TestTrue(TEXT("SUCCESS improves PARTIAL"), C->Record(2, TEXT("SUCCESS"), Good, false));
    TestEqual(TEXT("now SUCCESS"), C->GetOutcome(2), FString(TEXT("SUCCESS")));
    // A FAILED run after a pass does not lock the next episode again.
    C->Record(1, TEXT("FAILED"), Bad, false);
    TestTrue(TEXT("02 still unlocked"), C->IsUnlocked(2));
    return true;
}

// Save / load: results, carryover and the unlock-all choice survive a reload; a missing slot loads empty.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHavocCampaignSaveLoadTest, "MidnightHavoc.Campaign.SaveLoad", HAVOC_CAMPAIGN_TEST_FLAGS)
bool FHavocCampaignSaveLoadTest::RunTest(const FString& Parameters)
{
    HavocCampaignTests::FTestCampaign Writer(TEXT("SaveLoad"));
    TestFalse(TEXT("no save yet"), Writer->LoadProgress());

    FHavocCarryover E1; E1.bCrewRecovered = false;
    FHavocCarryover E2; E2.bCrewDelivered = true; E2.NomadsRemaining = 2; E2.bWardenOperational = true; E2.Route = TEXT("RIVERBED"); E2.RelaysOnline = 1;
    TestTrue(TEXT("record 01 and save"), Writer->Record(1, TEXT("PARTIAL"), E1));
    TestTrue(TEXT("record 02 and save"), Writer->Record(2, TEXT("SUCCESS"), E2));
    TestTrue(TEXT("slot exists"), UGameplayStatics::DoesSaveGameExist(Writer.Slot, 0));

    // A second campaign on the same slot, as after restarting the game.
    HavocCampaignTests::FTestCampaign Reader(TEXT("SaveLoad"), false);
    TestTrue(TEXT("load"), Reader->LoadProgress());

    TestEqual(TEXT("01 outcome"), Reader->GetOutcome(1), FString(TEXT("PARTIAL")));
    TestEqual(TEXT("02 outcome"), Reader->GetOutcome(2), FString(TEXT("SUCCESS")));
    TestEqual(TEXT("03 has no result"), Reader->GetOutcome(3), FString());
    TestFalse(TEXT("crew lost in 01 survives the reload"), Reader->WasCrewRecoveredInEpisode01());
    TestTrue(TEXT("03 unlocked after reload"), Reader->IsUnlocked(3));
    TestFalse(TEXT("04 locked after reload"), Reader->IsUnlocked(4));
    TestEqual(TEXT("next after reload"), Reader->GetNextEpisode(), 3);

    // Worse replay after a reload still cannot overwrite the saved best.
    TestFalse(TEXT("worse replay after reload ignored"), Reader->Record(2, TEXT("FAILED"), HavocCampaignTests::None()));
    TestEqual(TEXT("saved best kept"), Reader->GetOutcome(2), FString(TEXT("SUCCESS")));

    // Unlock-all persists, and reset clears everything.
    Reader->SetUnlockAll(true);
    TestTrue(TEXT("04 open with unlock all"), Reader->IsUnlocked(4));
    TestTrue(TEXT("reload"), Writer->LoadProgress());
    TestTrue(TEXT("unlock all saved"), Writer->GetUnlockAll());
    Writer->ResetProgress();
    TestFalse(TEXT("slot deleted"), UGameplayStatics::DoesSaveGameExist(Writer.Slot, 0));
    TestEqual(TEXT("progress cleared"), Writer->GetOutcome(1), FString());
    TestFalse(TEXT("04 locked again"), Writer->IsUnlocked(4));
    return true;
}

// Carryover: Ep01 crew -> Ep02; Ep02 crew / Nomads / Warden -> Ep03; Ep03 SUCCESS -> +10 reinforcements in Ep04.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHavocCampaignCarryoverTest, "MidnightHavoc.Campaign.Carryover", HAVOC_CAMPAIGN_TEST_FLAGS)
bool FHavocCampaignCarryoverTest::RunTest(const FString& Parameters)
{
    HavocCampaignTests::FTestCampaign C(TEXT("Carryover"));
    TestTrue(TEXT("Episode 01 crew assumed recovered before it is played"), C->WasCrewRecoveredInEpisode01());
    FHavocCarryover Unused;
    TestFalse(TEXT("no Episode 02 carryover yet"), C->GetEpisode02Carryover(Unused));

    FHavocCarryover E1; E1.bCrewRecovered = false;
    C->Record(1, TEXT("PARTIAL"), E1, false);
    TestFalse(TEXT("Episode 02 sees the crew lost in Episode 01"), C->WasCrewRecoveredInEpisode01());

    FHavocCarryover E2; E2.bCrewDelivered = true; E2.NomadsRemaining = 1; E2.bWardenOperational = false; E2.Route = TEXT("RIVER_DRAINAGE"); E2.RelaysOnline = 2;
    C->Record(2, TEXT("PARTIAL"), E2, false);
    FHavocCarryover Got;
    TestTrue(TEXT("Episode 03 gets Episode 02's carryover"), C->GetEpisode02Carryover(Got));
    TestTrue(TEXT("crew delivered"), Got.bCrewDelivered);
    TestEqual(TEXT("Nomads remaining"), Got.NomadsRemaining, 1);
    TestFalse(TEXT("Warden lost"), Got.bWardenOperational);
    TestEqual(TEXT("route"), Got.Route, FString(TEXT("RIVER_DRAINAGE")));
    TestEqual(TEXT("relays"), Got.RelaysOnline, 2);

    // Episode 04 reinforcements: base without a full Episode 03 win, +10 with SUCCESS.
    C->Record(3, TEXT("PARTIAL"), HavocCampaignTests::None(), false);
    const TStrongObjectPtr<UHavocOpenStraitMission> Without = HavocMissionTests::MakeMission(*this, C->GetEpisode3Outcome());
    if (!Without.IsValid()) { return false; }
    const int32 Base = Without->GetReinforcements();
    TestEqual(TEXT("Episode 03 PARTIAL: no bonus"), Base, 30);

    C->Record(3, TEXT("SUCCESS"), HavocCampaignTests::None(), false);
    const TStrongObjectPtr<UHavocOpenStraitMission> With = HavocMissionTests::MakeMission(*this, C->GetEpisode3Outcome());
    if (!With.IsValid()) { return false; }
    TestEqual(TEXT("Episode 03 SUCCESS: +10 reinforcements"), With->GetReinforcements(), Base + 10);

    // A worse Episode 03 replay cannot take the bonus away.
    C->Record(3, TEXT("FAILED"), HavocCampaignTests::None(), false);
    TestEqual(TEXT("bonus kept after a worse replay"), C->GetEpisode3Outcome(), FString(TEXT("SUCCESS")));
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
