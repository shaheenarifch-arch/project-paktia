#include "UI/HavocMenuWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Styling/SlateTypes.h"
#include "Engine/GameInstance.h"
#include "Input/Events.h"
#include "Campaign/HavocCampaign.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Missions/HavocEpisodeFlow.h"
#include "Missions/HavocEpisodeFlowBase.h"
#include "Missions/Episode03/HavocEpisode03Flow.h"
#include "Input/HavocPlayInput.h"
#include "Player/HavocPlaySettings.h"
#include "Player/HavocPlayerController.h"
#include "UI/HavocUIHelpers.h"

UHavocPlaySettings* UHavocMenuWidget::GetSettings() const
{
    const UGameInstance* GI = GetGameInstance();
    return GI ? GI->GetSubsystem<UHavocPlaySettings>() : nullptr;
}

UHavocCampaign* UHavocMenuWidget::GetCampaign() const
{
    const UGameInstance* GI = GetGameInstance();
    return GI ? GI->GetSubsystem<UHavocCampaign>() : nullptr;
}

IHavocEpisodeFlow* UHavocMenuWidget::GetFlow() const
{
    AHavocPlayerController* PC = Cast<AHavocPlayerController>(GetOwningPlayer());
    return PC ? PC->GetFlow() : nullptr;
}

AHavocEpisode03Flow* UHavocMenuWidget::GetFlow03() const
{
    AHavocPlayerController* PC = Cast<AHavocPlayerController>(GetOwningPlayer());
    return PC ? PC->GetFlow03() : nullptr;
}

void UHavocMenuWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    SetIsFocusable(true);
    if (!WidgetTree)
    {
        WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"), RF_Transient);
    }
    Backdrop = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Backdrop"));
    Backdrop->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.78f));
    Backdrop->SetHorizontalAlignment(HAlign_Center);
    Backdrop->SetVerticalAlignment(VAlign_Center);
    WidgetTree->RootWidget = Backdrop;

    List = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("List"));
    // A scroll box so the long settings page fits small screens; focus moves scroll it into view.
    Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("Scroll"));
    Scroll->AddChild(List);
    Backdrop->SetContent(Scroll);
    Rebuild();
}

void UHavocMenuWidget::SetMode(EHavocMenuMode NewMode)
{
    const auto IsSub = [](EHavocMenuMode M) { return M == EHavocMenuMode::Settings || M == EHavocMenuMode::HowToPlay; };
    if (IsSub(NewMode) && !IsSub(Mode))
    {
        ReturnMode = Mode;
    }
    bResetFocus = bResetFocus || Mode != NewMode;
    Mode = NewMode;
    Rebuild();
}

void UHavocMenuWidget::AddLabel(const FString& Text, int32 Size, const FLinearColor& Color)
{
    UTextBlock* Block = HavocUI::MakeText(WidgetTree, Text, Size, Color);
    Block->SetJustification(ETextJustify::Center);
    if (UVerticalBoxSlot* BoxSlot = List->AddChildToVerticalBox(Block))
    {
        BoxSlot->SetPadding(FMargin(40.f, 6.f));
        BoxSlot->SetHorizontalAlignment(HAlign_Center);
    }
}

void UHavocMenuWidget::AddButton(const FString& Label, FName Handler)
{
    UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
    UTextBlock* Text = HavocUI::MakeText(WidgetTree, Label, 24, FLinearColor::White);
    Text->SetJustification(ETextJustify::Center);
    Button->AddChild(Text);
    // A visible resting / hover / pressed colour so the mouse and the keyboard focus both show which item is chosen.
    FButtonStyle Style = Button->GetStyle();
    Style.Normal.TintColor = FSlateColor(FLinearColor(0.10f, 0.13f, 0.18f, 0.95f));
    Style.Hovered.TintColor = FSlateColor(FLinearColor(0.26f, 0.34f, 0.46f, 1.f));
    Style.Pressed.TintColor = FSlateColor(FLinearColor(0.40f, 0.48f, 0.62f, 1.f));
    Button->SetStyle(Style);
    Button->SetClickMethod(EButtonClickMethod::MouseDown); // a click registers even if the pointer drifts
    Buttons.Add(Button); // keyboard / gamepad focus (FocusButton) needs this list
    FScriptDelegate Delegate;
    Delegate.BindUFunction(this, Handler);
    Button->OnClicked.Add(Delegate);
    if (UVerticalBoxSlot* BoxSlot = List->AddChildToVerticalBox(Button))
    {
        BoxSlot->SetPadding(FMargin(0.f, 6.f));
        BoxSlot->SetHorizontalAlignment(HAlign_Fill);
    }
}

void UHavocMenuWidget::Rebuild()
{
    if (!List)
    {
        return;
    }
    // Keep keyboard / gamepad focus on the same row after a rebuild (a toggle must not throw it back to the top).
    if (bResetFocus)
    {
        PendingFocus = 0;
        bResetFocus = false;
    }
    else
    {
        for (int32 i = 0; i < Buttons.Num(); ++i)
        {
            if (Buttons[i] && Buttons[i]->HasAnyUserFocus())
            {
                PendingFocus = i;
            }
        }
    }
    Buttons.Reset();
    List->ClearChildren();
    const FLinearColor Gold(1.f, 0.85f, 0.4f);
    const FLinearColor Soft(0.85f, 0.88f, 0.95f);

    switch (Mode)
    {
    case EHavocMenuMode::Main:
        AddLabel(TEXT("OPERATION MIDNIGHT HAVOC"), 44, Gold);
        if (GetFlow03())
        {
            AddLabel(TEXT("Episode 03: Operation Broken Citadel"), 28, FLinearColor::White);
            AddLabel(TEXT("Take Gardun in five phases: open the city, break enemy communications, hold Old Gardun, take the high ground, then assault the Citadel. (Greybox build.)"), 18, Soft);
        }
        else
        {
            AddLabel(TEXT("Episode 04: Operation Open Strait"), 28, FLinearColor::White);
            AddLabel(TEXT("A hypothetical scenario: land on Kharg Island, secure the oil terminal intact, clear the shipping lanes and escort the tankers through the strait."), 18, Soft);
        }
        AddButton(TEXT("Campaign"), TEXT("OnCampaign"));
        AddButton(TEXT("How to play"), TEXT("OnHowToPlay"));
        AddButton(TEXT("Settings"), TEXT("OnSettings"));
        AddButton(TEXT("Quit"), TEXT("OnQuit"));
        AddLabel(TEXT("Choose with the mouse, Up / Down and Enter, or a gamepad (D-pad, A). How to play lists every key."), 16, Soft);
        break;

    case EHavocMenuMode::Campaign:
    {
        // Lock state comes from the saved campaign (UHavocCampaign): an episode unlocks when the one before it
        // ended in SUCCESS or PARTIAL. "Start Episode N" opens that episode's level (UHavocCampaign::EpisodeMapPath).
        const FLinearColor Locked(0.55f, 0.58f, 0.64f);
        const UHavocCampaign* Campaign = GetCampaign();
        AddLabel(TEXT("CAMPAIGN"), 40, Gold);
        AddLabel(TEXT("Episodes unlock in order: finish one with SUCCESS or PARTIAL to open the next."), 18, Soft);
        bool bAnyLocked = false;
        for (int32 Episode = UHavocCampaign::FirstEpisode; Episode <= UHavocCampaign::LastEpisode; ++Episode)
        {
            const bool bOpen = !Campaign || Campaign->IsUnlocked(Episode);
            bAnyLocked |= !bOpen;
            const FString Status = Campaign ? Campaign->StatusText(Episode) : FString(TEXT("Unlocked"));
            AddLabel(FString::Printf(TEXT("Episode %02d: %s  -  %s"), Episode, *UHavocCampaign::EpisodeName(Episode), *Status),
                24, bOpen ? FLinearColor::White : Locked);
            if (bOpen)
            {
                static const FName Handlers[] = { TEXT("OnPlayEpisode1"), TEXT("OnPlayEpisode2"), TEXT("OnPlayEpisode3"), TEXT("OnPlayEpisode4") };
                AddButton(FString::Printf(TEXT("Start Episode %02d"), Episode), Handlers[Episode - UHavocCampaign::FirstEpisode]);
            }
        }
        AddLabel(TEXT("Episode 04: land on Kharg Island, secure the oil terminal intact, clear the shipping lanes and escort the tankers through the strait. A hypothetical scenario."), 18, Soft);
        if (Campaign && Campaign->IsUnlocked(4) && Campaign->GetEpisode3Outcome() == TEXT("SUCCESS"))
        {
            AddLabel(TEXT("Episode 03 success: +10 reinforcements."), 15, Gold);
        }
        if (bAnyLocked)
        {
            AddButton(TEXT("Unlock all episodes (testing)"), TEXT("OnUnlockAll"));
            AddLabel(TEXT("Opens every episode for testing. Your saved results are kept."), 15, Locked);
        }
        AddButton(TEXT("Back"), TEXT("OnMainMenu"));
        break;
    }

    case EHavocMenuMode::Pause:
        AddLabel(TEXT("PAUSED"), 40, Gold);
        AddButton(TEXT("Resume"), TEXT("OnResume"));
        AddButton(TEXT("Restart from last checkpoint"), TEXT("OnRestartCheckpoint"));
        AddButton(TEXT("How to play"), TEXT("OnHowToPlay"));
        AddButton(TEXT("Settings"), TEXT("OnSettings"));
        AddButton(TEXT("Quit to desktop"), TEXT("OnQuit"));
        break;

    case EHavocMenuMode::HowToPlay:
    {
        // Read from the live mapping contexts, so a rebind in Settings shows here at once.
        const UGameInstance* GI = GetGameInstance();
        const UHavocPlayInput* Input = GI ? GI->GetSubsystem<UHavocPlayInput>() : nullptr;
        AddLabel(TEXT("HOW TO PLAY"), 40, Gold);
        AddButton(TEXT("Back"), TEXT("OnBack"));
        AddLabel(UHavocPlaySettings::HelpFor(TEXT("howtoplay")), 15, Soft);
        FString LastSection;
        if (Input)
        {
            for (const FHavocControlRow& Row : Input->BuildControlRows())
            {
                if (Row.Section != LastSection)
                {
                    LastSection = Row.Section;
                    AddLabel(Row.Section, 24, Gold);
                }
                AddLabel(FString::Printf(TEXT("%s:  %s"), *Row.Action, *Row.Keys), 18, FLinearColor::White);
            }
        }
        AddLabel(TEXT("Gamepad: menus only for now (D-pad or stick, A to choose, B back, Start pause). Flying and driving use keyboard and mouse. Change keys in Settings."), 15, Soft);
        AddLabel(TEXT("Scroll with the mouse wheel, Up / Down, Page Up / Page Down or the D-pad."), 15, Soft);
        AddButton(TEXT("Back"), TEXT("OnBack"));
        break;
    }

    case EHavocMenuMode::Settings:
    {
        UHavocPlaySettings* Settings = GetSettings();
        const FLinearColor Dim(0.7f, 0.74f, 0.82f);
        AddLabel(TEXT("SETTINGS"), 40, Gold);
        AddButton(FString::Printf(TEXT("Graphics quality: %s"), UHavocPlaySettings::QualityName(Settings ? Settings->GraphicsQuality : 2)), TEXT("OnCycleQuality"));
        AddLabel(UHavocPlaySettings::HelpFor(TEXT("quality")), 15, Dim);
        AddButton(FString::Printf(TEXT("Subtitles: %s"), Settings && !Settings->bSubtitles ? TEXT("OFF") : TEXT("ON")), TEXT("OnToggleSubtitles"));
        AddLabel(UHavocPlaySettings::HelpFor(TEXT("subtitles")), 15, Dim);
        AddButton(FString::Printf(TEXT("Reduce flashes and camera shake: %s"), Settings && Settings->bReduceFlashes ? TEXT("ON") : TEXT("OFF")), TEXT("OnToggleFlashes"));
        AddLabel(UHavocPlaySettings::HelpFor(TEXT("flashes")), 15, Dim);
        AddLabel(FString::Printf(TEXT("Look sensitivity: %.1f"), Settings ? Settings->LookSensitivity : 1.f), 22, FLinearColor::White);
        AddButton(TEXT("Sensitivity -"), TEXT("OnSensitivityDown"));
        AddButton(TEXT("Sensitivity +"), TEXT("OnSensitivityUp"));
        AddLabel(UHavocPlaySettings::HelpFor(TEXT("sensitivity")), 15, Dim);
        AddLabel(FString::Printf(TEXT("Master volume: %d%%"), Settings ? FMath::RoundToInt(Settings->MasterVolume * 100.f) : 100), 22, FLinearColor::White);
        AddButton(TEXT("Volume -"), TEXT("OnVolumeDown"));
        AddButton(TEXT("Volume +"), TEXT("OnVolumeUp"));
        AddLabel(UHavocPlaySettings::HelpFor(TEXT("volume")), 15, Dim);

        AddLabel(TEXT("Key bindings"), 22, FLinearColor::White);
        const auto KeyLabel = [&](const TCHAR* Name, FName Id, const TCHAR* Handler)
        {
            const bool bWaiting = RebindingAction == Id;
            const FKey* Key = Settings ? BindingFor(*Settings, Id) : nullptr;
            AddButton(FString::Printf(TEXT("%s: %s"), Name, bWaiting ? TEXT("press a key...") : Key ? *Key->GetDisplayName().ToString() : TEXT("?")), Handler);
        };
        KeyLabel(TEXT("Interact / mark hazard / resupply"), TEXT("interact"), TEXT("OnRebindInteract"));
        KeyLabel(TEXT("Reload"), TEXT("reload"), TEXT("OnRebindReload"));
        KeyLabel(TEXT("Raven drone"), TEXT("raven"), TEXT("OnRebindRaven"));
        KeyLabel(TEXT("Falcon support"), TEXT("falcon"), TEXT("OnRebindFalcon"));
        KeyLabel(TEXT("Return to Marine"), TEXT("return"), TEXT("OnRebindReturn"));
        AddLabel(RebindMessage.IsEmpty() ? UHavocPlaySettings::HelpFor(TEXT("bind")) : RebindMessage, 15, RebindMessage.IsEmpty() ? Dim : Gold);

        AddButton(TEXT("Reset to defaults"), TEXT("OnResetSettings"));
        AddLabel(UHavocPlaySettings::HelpFor(TEXT("reset")), 15, Dim);
        AddButton(TEXT("Back"), TEXT("OnBack"));
        break;
    }

    case EHavocMenuMode::Results:
    {
        const IHavocEpisodeFlow* Flow = GetFlow();
        // Episodes built on AHavocEpisodeFlowBase (Episode 02) word their own results title and replay button.
        const AHavocEpisodeFlowBase* FlowBase = Flow ? Cast<AHavocEpisodeFlowBase>(Flow->_getUObject()) : nullptr;
        const FString Outcome = Flow ? Flow->GetOutcome() : FString();
        const bool bFailed = Outcome == TEXT("FAILED");
        const FString Title = FlowBase ? FlowBase->GetResultTitle()
            : Outcome == TEXT("SUCCESS") ? FString(TEXT("MISSION COMPLETE"))
            : Outcome == TEXT("PARTIAL") ? FString(TEXT("PARTIAL SUCCESS"))
            : FString(TEXT("OPERATION FAILED"));
        AddLabel(Title, 40, bFailed ? FLinearColor(1.f, 0.4f, 0.35f) : Gold);
        AddLabel(Flow ? Flow->GetResultSummary() : FString(), 22, FLinearColor::White);
        if (bFailed)
        {
            AddButton(TEXT("Restart from last checkpoint"), TEXT("OnRestartCheckpoint"));
        }
        AddButton(FlowBase ? FlowBase->GetReplayLabel() : FString(TEXT("Play again")), TEXT("OnPlay"));
        AddButton(TEXT("Quit"), TEXT("OnQuit"));
        break;
    }
    }
    FocusButton(PendingFocus);
}

void UHavocMenuWidget::FocusButton(int32 Index)
{
    if (Buttons.Num() == 0)
    {
        return;
    }
    UButton* Target = Buttons[FMath::Clamp(Index, 0, Buttons.Num() - 1)];
    if (Target && GetOwningPlayer())
    {
        Target->SetUserFocus(GetOwningPlayer());
    }
}

FKey* UHavocMenuWidget::BindingFor(UHavocPlaySettings& Settings, FName ActionId) const
{
    if (ActionId == TEXT("interact")) { return &Settings.InteractKey; }
    if (ActionId == TEXT("reload")) { return &Settings.ReloadKey; }
    if (ActionId == TEXT("raven")) { return &Settings.RavenKey; }
    if (ActionId == TEXT("falcon")) { return &Settings.FalconKey; }
    if (ActionId == TEXT("return")) { return &Settings.ReturnKey; }
    return nullptr;
}

void UHavocMenuWidget::BeginRebind(FName ActionId)
{
    RebindingAction = ActionId;
    RebindMessage = TEXT("Press the new key now. Esc or gamepad B cancels.");
    Rebuild();
}

void UHavocMenuWidget::FinishRebind(const FKey& Key)
{
    UHavocPlaySettings* Settings = GetSettings();
    FKey* Target = Settings ? BindingFor(*Settings, RebindingAction) : nullptr;
    const FName Action = RebindingAction;
    RebindingAction = NAME_None;
    if (!Target)
    {
        Rebuild();
        return;
    }
    static const TArray<FKey> Reserved = { EKeys::Escape, EKeys::P, EKeys::One, EKeys::Two, EKeys::W, EKeys::A, EKeys::S, EKeys::D,
        EKeys::SpaceBar, EKeys::LeftShift, EKeys::LeftControl, EKeys::C, EKeys::Q, EKeys::F, EKeys::H };
    if (Reserved.Contains(Key))
    {
        RebindMessage = FString::Printf(TEXT("%s is used for movement, pause or beach choice. Pick another key."), *Key.GetDisplayName().ToString());
        Rebuild();
        return;
    }
    // Give the displaced action this action's old key so no two actions share a key.
    static const FName Ids[] = { TEXT("interact"), TEXT("reload"), TEXT("raven"), TEXT("falcon"), TEXT("return") };
    for (const FName Other : Ids)
    {
        FKey* OtherKey = BindingFor(*Settings, Other);
        if (Other != Action && OtherKey && *OtherKey == Key)
        {
            *OtherKey = *Target;
        }
    }
    *Target = Key;
    Settings->ApplyAndSave();
    RebindMessage = FString::Printf(TEXT("Bound to %s."), *Key.GetDisplayName().ToString());
    Rebuild();
}

FReply UHavocMenuWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    if (!RebindingAction.IsNone())
    {
        const FKey Key = InKeyEvent.GetKey();
        if (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right)
        {
            RebindingAction = NAME_None;
            RebindMessage.Reset();
            Rebuild();
        }
        else if (!Key.IsGamepadKey() && !Key.IsMouseButton())
        {
            FinishRebind(Key);
        }
        return FReply::Handled(); // swallow everything while waiting, so Enter does not click a button
    }
    return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

FReply UHavocMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    if (InKeyEvent.GetKey() == EKeys::Escape || InKeyEvent.GetKey() == EKeys::Gamepad_FaceButton_Right)
    {
        if (Mode == EHavocMenuMode::Pause)
        {
            OnResume();
            return FReply::Handled();
        }
        if (Mode == EHavocMenuMode::Settings || Mode == EHavocMenuMode::HowToPlay)
        {
            OnBack();
            return FReply::Handled();
        }
        if (Mode == EHavocMenuMode::Campaign)
        {
            OnMainMenu();
            return FReply::Handled();
        }
    }
    if (Mode == EHavocMenuMode::HowToPlay && Scroll)
    {
        // Few buttons on this page, so keys scroll the list (the mouse wheel works too).
        const FKey Key = InKeyEvent.GetKey();
        float Delta = 0.f;
        if (Key == EKeys::Down || Key == EKeys::Gamepad_DPad_Down || Key == EKeys::Gamepad_LeftStick_Down) { Delta = 120.f; }
        else if (Key == EKeys::Up || Key == EKeys::Gamepad_DPad_Up || Key == EKeys::Gamepad_LeftStick_Up) { Delta = -120.f; }
        else if (Key == EKeys::PageDown) { Delta = 600.f; }
        else if (Key == EKeys::PageUp) { Delta = -600.f; }
        if (Delta != 0.f)
        {
            Scroll->SetScrollOffset(FMath::Clamp(Scroll->GetScrollOffset() + Delta, 0.f, Scroll->GetScrollOffsetOfEnd()));
            return FReply::Handled();
        }
    }
    return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UHavocMenuWidget::PlayEpisode(int32 Episode)
{
    if (AHavocPlayerController* PC = Cast<AHavocPlayerController>(GetOwningPlayer()))
    {
        PC->PlayEpisode(Episode);
    }
}

void UHavocMenuWidget::OnPlay()
{
    if (IHavocEpisodeFlow* Flow = GetFlow())
    {
        Flow->StartEpisode();
    }
    else if (AHavocEpisode03Flow* Flow03 = GetFlow03())
    {
        Flow03->StartEpisode();
    }
}

void UHavocMenuWidget::OnUnlockAll()
{
    if (UHavocCampaign* Campaign = GetCampaign())
    {
        Campaign->SetUnlockAll(true);
    }
    Rebuild();
}

void UHavocMenuWidget::OnCampaign() { SetMode(EHavocMenuMode::Campaign); }
void UHavocMenuWidget::OnMainMenu() { SetMode(EHavocMenuMode::Main); }

void UHavocMenuWidget::OnResume()
{
    if (AHavocPlayerController* PC = Cast<AHavocPlayerController>(GetOwningPlayer()))
    {
        PC->HideMenu();
    }
}

void UHavocMenuWidget::OnRestartCheckpoint()
{
    if (IHavocEpisodeFlow* Flow = GetFlow())
    {
        Flow->RestartFromCheckpoint();
    }
    else if (AHavocEpisode03Flow* Flow03 = GetFlow03())
    {
        Flow03->RestartFromCheckpoint();
    }
}

void UHavocMenuWidget::OnHowToPlay() { RebindMessage.Reset(); SetMode(EHavocMenuMode::HowToPlay); }
void UHavocMenuWidget::OnSettings() { RebindMessage.Reset(); SetMode(EHavocMenuMode::Settings); }
void UHavocMenuWidget::OnBack() { RebindingAction = NAME_None; RebindMessage.Reset(); SetMode(ReturnMode); }

void UHavocMenuWidget::OnQuit()
{
    UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}

void UHavocMenuWidget::OnToggleFlashes()
{
    if (UHavocPlaySettings* S = GetSettings())
    {
        S->bReduceFlashes = !S->bReduceFlashes;
        S->ApplyAndSave();
    }
    Rebuild();
}

void UHavocMenuWidget::OnSensitivityDown()
{
    if (UHavocPlaySettings* S = GetSettings()) { S->LookSensitivity -= 0.1f; S->ApplyAndSave(); }
    Rebuild();
}

void UHavocMenuWidget::OnSensitivityUp()
{
    if (UHavocPlaySettings* S = GetSettings()) { S->LookSensitivity += 0.1f; S->ApplyAndSave(); }
    Rebuild();
}

void UHavocMenuWidget::OnVolumeDown()
{
    if (UHavocPlaySettings* S = GetSettings()) { S->MasterVolume -= 0.1f; S->ApplyAndSave(); }
    Rebuild();
}

void UHavocMenuWidget::OnVolumeUp()
{
    if (UHavocPlaySettings* S = GetSettings()) { S->MasterVolume += 0.1f; S->ApplyAndSave(); }
    Rebuild();
}

void UHavocMenuWidget::OnCycleQuality()
{
    if (UHavocPlaySettings* S = GetSettings()) { S->GraphicsQuality = (S->GraphicsQuality + 1) % 4; S->ApplyAndSave(); }
    Rebuild();
}

void UHavocMenuWidget::OnToggleSubtitles()
{
    if (UHavocPlaySettings* S = GetSettings()) { S->bSubtitles = !S->bSubtitles; S->ApplyAndSave(); }
    Rebuild();
}

void UHavocMenuWidget::OnResetSettings()
{
    if (UHavocPlaySettings* S = GetSettings()) { S->ResetToDefaults(); }
    RebindMessage.Reset();
    Rebuild();
}

void UHavocMenuWidget::OnRebindInteract() { BeginRebind(TEXT("interact")); }
void UHavocMenuWidget::OnRebindReload() { BeginRebind(TEXT("reload")); }
void UHavocMenuWidget::OnRebindRaven() { BeginRebind(TEXT("raven")); }
void UHavocMenuWidget::OnRebindFalcon() { BeginRebind(TEXT("falcon")); }
void UHavocMenuWidget::OnRebindReturn() { BeginRebind(TEXT("return")); }
