#include "UI/HavocPlayHUD.h"

#include "Blueprint/WidgetTree.h"
#include "Combat/HavocWeaponComponent.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Drones/HavocDrone.h"
#include "Aircraft/HavocFalconHelicopter.h"
#include "Missions/Episode04/HavocEpisode04Flow.h"
#include "Missions/Episode04/HavocOpenStraitMission.h"
#include "Missions/HavocMissionRunner.h"
#include "Missions/HavocMissionSubsystem.h"
#include "Player/HavocPlayerCharacter.h"
#include "Player/HavocPlayerController.h"
#include "Player/HavocPlaySettings.h"
#include "UI/HavocHudModel.h"
#include "UI/HavocMinimapWidget.h"
#include "UI/HavocUIHelpers.h"
#include "Engine/GameInstance.h"

void UHavocPlayHUD::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    if (!WidgetTree)
    {
        WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"), RF_Transient);
    }
    using namespace HavocUI;
    UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Canvas"));
    WidgetTree->RootWidget = Canvas;

    ObjectiveText = MakeText(WidgetTree, TEXT(""), 24, FLinearColor(1.f, 0.95f, 0.75f));
    ObjectiveText->SetJustification(ETextJustify::Center);
    // 600 px and wrapping so it stays between the objective tracker (top left) and the minimap (top right).
    ObjectiveText->SetAutoWrapText(true);
    Place(Canvas, ObjectiveText, FAnchors(0.5f, 0.f), FVector2D(0.5f, 0.f), FVector2D(0.f, 18.f), FVector2D(600.f, 110.f));

    ToastText = MakeText(WidgetTree, TEXT(""), 26, FLinearColor(1.f, 0.85f, 0.3f));
    ToastText->SetJustification(ETextJustify::Center);
    Place(Canvas, ToastText, FAnchors(0.5f, 0.22f), FVector2D(0.5f, 0.5f), FVector2D::ZeroVector, FVector2D(1100.f, 80.f));

    Crosshair = MakeText(WidgetTree, TEXT("+"), 28, FLinearColor(1.f, 1.f, 1.f, 0.8f));
    Crosshair->SetJustification(ETextJustify::Center);
    Place(Canvas, Crosshair, FAnchors(0.5f, 0.5f), FVector2D(0.5f, 0.5f), FVector2D::ZeroVector, FVector2D(40.f, 40.f));

    HintText = MakeText(WidgetTree, TEXT(""), 20, FLinearColor::White);
    HintText->SetJustification(ETextJustify::Center);
    Place(Canvas, HintText, FAnchors(0.5f, 1.f), FVector2D(0.5f, 1.f), FVector2D(0.f, -95.f), FVector2D(1300.f, 70.f));

    FirstHintText = MakeText(WidgetTree, TEXT(""), 22, FLinearColor(0.7f, 1.f, 0.85f));
    FirstHintText->SetJustification(ETextJustify::Center);
    Place(Canvas, FirstHintText, FAnchors(0.5f, 0.35f), FVector2D(0.5f, 0.f), FVector2D::ZeroVector, FVector2D(1000.f, 140.f));

    UVerticalBox* HealthBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    HealthText = MakeText(WidgetTree, TEXT("HEALTH"), 20, FLinearColor::White);
    HealthBox->AddChild(HealthText);
    USizeBox* BarBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    BarBox->SetWidthOverride(320.f);
    BarBox->SetHeightOverride(18.f);
    HealthBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
    HealthBar->SetFillColorAndOpacity(FLinearColor(0.2f, 0.85f, 0.3f));
    BarBox->AddChild(HealthBar);
    HealthBox->AddChild(BarBox);
    Place(Canvas, HealthBox, FAnchors(0.f, 1.f), FVector2D(0.f, 1.f), FVector2D(30.f, -30.f), FVector2D(340.f, 60.f));

    AmmoText = MakeText(WidgetTree, TEXT(""), 30, FLinearColor::White);
    AmmoText->SetJustification(ETextJustify::Right);
    Place(Canvas, AmmoText, FAnchors(1.f, 1.f), FVector2D(1.f, 1.f), FVector2D(-30.f, -30.f), FVector2D(420.f, 50.f));

    // Top right: minimap with the sector name under it; the status block and links sit below.
    Minimap = WidgetTree->ConstructWidget<UHavocMinimapWidget>(UHavocMinimapWidget::StaticClass(), TEXT("Minimap"));
    Place(Canvas, Minimap, FAnchors(1.f, 0.f), FVector2D(1.f, 0.f), FVector2D(-30.f, 20.f), FVector2D(260.f, 200.f));

    SectorText = MakeText(WidgetTree, TEXT(""), 16, FLinearColor(1.f, 0.85f, 0.3f));
    SectorText->SetJustification(ETextJustify::Right);
    Place(Canvas, SectorText, FAnchors(1.f, 0.f), FVector2D(1.f, 0.f), FVector2D(-30.f, 224.f), FVector2D(420.f, 28.f));

    StatusText = MakeText(WidgetTree, TEXT(""), 18, FLinearColor(0.85f, 0.95f, 1.f));
    StatusText->SetJustification(ETextJustify::Right);
    Place(Canvas, StatusText, FAnchors(1.f, 0.f), FVector2D(1.f, 0.f), FVector2D(-30.f, 256.f), FVector2D(560.f, 120.f));

    LinkText = MakeText(WidgetTree, TEXT(""), 22, FLinearColor(0.4f, 1.f, 0.6f));
    LinkText->SetJustification(ETextJustify::Right);
    Place(Canvas, LinkText, FAnchors(1.f, 0.f), FVector2D(1.f, 0.f), FVector2D(-30.f, 380.f), FVector2D(560.f, 60.f));

    // Top left: the objective tracker.
    TrackerText = MakeText(WidgetTree, TEXT(""), 17, FLinearColor(0.85f, 0.95f, 1.f));
    Place(Canvas, TrackerText, FAnchors(0.f, 0.f), FVector2D(0.f, 0.f), FVector2D(30.f, 20.f), FVector2D(420.f, 190.f));

    // Bottom centre, above the hint line: vehicle instruments while flying the Raven or Falcon.
    VehicleText = MakeText(WidgetTree, TEXT(""), 20, FLinearColor(0.4f, 1.f, 0.6f));
    VehicleText->SetJustification(ETextJustify::Center);
    Place(Canvas, VehicleText, FAnchors(0.5f, 1.f), FVector2D(0.5f, 1.f), FVector2D(0.f, -170.f), FVector2D(900.f, 60.f));
}

void UHavocPlayHUD::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    AHavocPlayerController* PC = Cast<AHavocPlayerController>(GetOwningPlayer());
    AHavocEpisode04Flow* Flow = PC ? PC->GetFlow() : nullptr;
    const UHavocMissionSubsystem* Missions = GetWorld() ? GetWorld()->GetSubsystem<UHavocMissionSubsystem>() : nullptr;
    UHavocOpenStraitMission* Mission = Missions ? Missions->GetOpenStrait() : nullptr;

    // Subtitles setting: when off, the toast and hint lines are hidden (the objective and tracker stay).
    const UGameInstance* GI = GetGameInstance();
    const UHavocPlaySettings* Settings = GI ? GI->GetSubsystem<UHavocPlaySettings>() : nullptr;
    const bool bSubtitles = !Settings || Settings->bSubtitles;
    const FHavocHudSnapshot Snapshot = HavocHudModel::Build(Mission);

    ObjectiveText->SetText(FText::FromString(Snapshot.Objective));
    TrackerText->SetText(FText::FromString(Mission ? Snapshot.TrackerText() : FString()));
    HintText->SetText(FText::FromString(bSubtitles && Flow ? Flow->GetHint() : FString()));
    ToastText->SetText(FText::FromString(bSubtitles && Flow ? Flow->GetToast() : FString()));
    // First-time control hint: not tied to the Subtitles setting, because it explains the controls.
    FirstHintText->SetText(FText::FromString(PC ? PC->GetActiveHint() : FString()));

    // Current sector, from the minimap's sector volumes.
    FString SectorName;
    if (Minimap && !Minimap->GetCurrentSector().IsNone())
    {
        SectorName = TEXT("Sector: ") + Minimap->GetCurrentSector().ToString().Replace(TEXT("_"), TEXT(" "));
    }
    SectorText->SetText(FText::FromString(SectorName));

    // Marine health and ammo (the Marine stays alive in the world while the Raven or Falcon is flown).
    const AHavocPlayerCharacter* Marine = PC ? PC->GetMarine() : nullptr;
    if (Marine)
    {
        const float Fraction = Marine->MaximumHealth > 0.f ? FMath::Clamp(Marine->Health / Marine->MaximumHealth, 0.f, 1.f) : 0.f;
        HealthBar->SetPercent(Fraction);
        HealthBar->SetFillColorAndOpacity(Fraction > 0.4f ? FLinearColor(0.2f, 0.85f, 0.3f) : FLinearColor(0.95f, 0.25f, 0.2f));
        HealthText->SetText(FText::FromString(FString::Printf(TEXT("HEALTH %d"), FMath::CeilToInt(Marine->Health))));
        const UHavocWeaponComponent* Weapon = Marine->Weapon;
        AmmoText->SetText(FText::FromString(Weapon->bReloading
            ? TEXT("KR-47  RELOADING...")
            : *FString::Printf(TEXT("KR-47  %d / %d"), Weapon->AmmoInMagazine, Weapon->ReserveAmmo)));
    }

    // Mission status block: reinforcements, terminal, lane progress, escort, Falcon.
    FString Status;
    if (Mission)
    {
        Status = Snapshot.StatusBlock();
        if (Snapshot.bAirSupport && Flow)
        {
            Status += TEXT("\n") + Flow->GetSupportStatus();
        }
    }
    StatusText->SetText(FText::FromString(Status));

    // Drone link while flying the Raven, Falcon note while flying it.
    FString Link;
    if (const AHavocDrone* Drone = PC ? Cast<AHavocDrone>(PC->GetPawn()) : nullptr)
    {
        const UEnum* SignalEnum = StaticEnum<EHavocSignalState>();
        const FString SignalName = SignalEnum ? SignalEnum->GetNameStringByValue(static_cast<int64>(Drone->GetSignalState())) : FString();
        Link = FString::Printf(TEXT("RAVEN LINK %s %d%%   BATTERY %d%%%s"), *SignalName,
            FMath::RoundToInt(Drone->Core.Signal * 100.0), FMath::RoundToInt(Drone->Core.Battery.Charge * 100.0),
            Drone->IsBatteryLow() ? TEXT("  LOW") : TEXT(""));
    }
    else if (PC && Cast<AHavocFalconHelicopter>(PC->GetPawn()))
    {
        Link = TEXT("FALCON: Space = lift, W/A/S/D = tilt, Q/E = yaw, Tab = back to Marine");
    }
    LinkText->SetText(FText::FromString(Link));

    // Vehicle instruments: altitude above the sea (world Z, cm to m) and speed while flying.
    FString Vehicle;
    if (const APawn* Flying = PC ? PC->GetPawn() : nullptr)
    {
        if (Cast<AHavocDrone>(Flying) || Cast<AHavocFalconHelicopter>(Flying))
        {
            Vehicle = FString::Printf(TEXT("%s   ALT %d m   SPD %d m/s"), Cast<AHavocDrone>(Flying) ? TEXT("RAVEN") : TEXT("FALCON"),
                FMath::RoundToInt(Flying->GetActorLocation().Z / 100.0), FMath::RoundToInt(Flying->GetVelocity().Size() / 100.0));
        }
    }
    VehicleText->SetText(FText::FromString(Vehicle));
    Crosshair->SetVisibility(Marine && PC && PC->GetPawn() == Marine ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}
