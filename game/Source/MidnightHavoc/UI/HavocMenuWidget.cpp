#include "UI/HavocMenuWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Input/Events.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Missions/Episode04/HavocEpisode04Flow.h"
#include "Player/HavocPlaySettings.h"
#include "Player/HavocPlayerController.h"
#include "UI/HavocUIHelpers.h"

UHavocPlaySettings* UHavocMenuWidget::GetSettings() const
{
    const UGameInstance* GI = GetGameInstance();
    return GI ? GI->GetSubsystem<UHavocPlaySettings>() : nullptr;
}

AHavocEpisode04Flow* UHavocMenuWidget::GetFlow() const
{
    AHavocPlayerController* PC = Cast<AHavocPlayerController>(GetOwningPlayer());
    return PC ? PC->GetFlow() : nullptr;
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
    Backdrop->SetContent(List);
    Rebuild();
}

void UHavocMenuWidget::SetMode(EHavocMenuMode NewMode)
{
    if (NewMode == EHavocMenuMode::Settings && Mode != EHavocMenuMode::Settings)
    {
        ReturnMode = Mode;
    }
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
    List->ClearChildren();
    const FLinearColor Gold(1.f, 0.85f, 0.4f);
    const FLinearColor Soft(0.85f, 0.88f, 0.95f);

    switch (Mode)
    {
    case EHavocMenuMode::Main:
        AddLabel(TEXT("OPERATION MIDNIGHT HAVOC"), 44, Gold);
        AddLabel(TEXT("Episode 04: Operation Open Strait"), 28, FLinearColor::White);
        AddLabel(TEXT("A hypothetical scenario: land on Kharg Island, secure the oil terminal intact, clear the shipping lanes and escort the tankers through the strait."), 18, Soft);
        AddButton(TEXT("Play Episode 04"), TEXT("OnPlay"));
        AddButton(TEXT("Settings"), TEXT("OnSettings"));
        AddButton(TEXT("Quit"), TEXT("OnQuit"));
        AddLabel(TEXT("Move W/A/S/D   Look mouse   Shift sprint   C crouch   Right mouse aim   Left mouse fire   R reload   E interact\nT Raven drone   G Falcon support   Tab back to Marine   1 / 2 choose beach   Esc pause"), 16, Soft);
        break;

    case EHavocMenuMode::Pause:
        AddLabel(TEXT("PAUSED"), 40, Gold);
        AddButton(TEXT("Resume"), TEXT("OnResume"));
        AddButton(TEXT("Restart from last checkpoint"), TEXT("OnRestartCheckpoint"));
        AddButton(TEXT("Settings"), TEXT("OnSettings"));
        AddButton(TEXT("Quit to desktop"), TEXT("OnQuit"));
        break;

    case EHavocMenuMode::Settings:
    {
        const UHavocPlaySettings* Settings = GetSettings();
        AddLabel(TEXT("SETTINGS"), 40, Gold);
        AddButton(FString::Printf(TEXT("Reduce flashes and camera shake: %s"), Settings && Settings->bReduceFlashes ? TEXT("ON") : TEXT("OFF")), TEXT("OnToggleFlashes"));
        AddLabel(FString::Printf(TEXT("Look sensitivity: %.1f"), Settings ? Settings->LookSensitivity : 1.f), 22, FLinearColor::White);
        AddButton(TEXT("Sensitivity -"), TEXT("OnSensitivityDown"));
        AddButton(TEXT("Sensitivity +"), TEXT("OnSensitivityUp"));
        AddLabel(FString::Printf(TEXT("Master volume: %d%%"), Settings ? FMath::RoundToInt(Settings->MasterVolume * 100.f) : 100), 22, FLinearColor::White);
        AddButton(TEXT("Volume -"), TEXT("OnVolumeDown"));
        AddButton(TEXT("Volume +"), TEXT("OnVolumeUp"));
        AddButton(TEXT("Back"), TEXT("OnBack"));
        break;
    }

    case EHavocMenuMode::Results:
    {
        const AHavocEpisode04Flow* Flow = GetFlow();
        const FString Outcome = Flow ? Flow->GetOutcome() : FString();
        const bool bFailed = Outcome == TEXT("FAILED");
        AddLabel(Outcome == TEXT("SUCCESS") ? TEXT("STRAIT OPEN - MISSION COMPLETE")
            : Outcome == TEXT("PARTIAL") ? TEXT("STRAIT OPEN - PARTIAL SUCCESS")
            : TEXT("OPERATION FAILED"), 40, bFailed ? FLinearColor(1.f, 0.4f, 0.35f) : Gold);
        AddLabel(Flow ? Flow->GetResultSummary() : FString(), 22, FLinearColor::White);
        if (bFailed)
        {
            AddButton(TEXT("Restart from last checkpoint"), TEXT("OnRestartCheckpoint"));
        }
        AddButton(TEXT("Play Episode 04 again"), TEXT("OnPlay"));
        AddButton(TEXT("Quit"), TEXT("OnQuit"));
        break;
    }
    }
}

FReply UHavocMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    if (InKeyEvent.GetKey() == EKeys::Escape)
    {
        if (Mode == EHavocMenuMode::Pause)
        {
            OnResume();
            return FReply::Handled();
        }
        if (Mode == EHavocMenuMode::Settings)
        {
            OnBack();
            return FReply::Handled();
        }
    }
    return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UHavocMenuWidget::OnPlay()
{
    if (AHavocEpisode04Flow* Flow = GetFlow())
    {
        Flow->StartEpisode();
    }
}

void UHavocMenuWidget::OnResume()
{
    if (AHavocPlayerController* PC = Cast<AHavocPlayerController>(GetOwningPlayer()))
    {
        PC->HideMenu();
    }
}

void UHavocMenuWidget::OnRestartCheckpoint()
{
    if (AHavocEpisode04Flow* Flow = GetFlow())
    {
        Flow->RestartFromCheckpoint();
    }
}

void UHavocMenuWidget::OnSettings() { SetMode(EHavocMenuMode::Settings); }
void UHavocMenuWidget::OnBack() { SetMode(ReturnMode); }

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
