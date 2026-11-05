#include "UI/HavocMinimapWidget.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Rendering/DrawElements.h"
#include "World/HavocLaneHazard.h"
#include "World/HavocSectorVolume.h"

void UHavocMinimapWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    RefreshTimer -= InDeltaTime;
    if (RefreshTimer <= 0.f)
    {
        RefreshTimer = 0.1f; // 10 Hz is plenty for a map and keeps the actor scan cheap.
        Refresh();
    }
}

void UHavocMinimapWidget::Refresh()
{
    Sectors.Reset();
    Hazards.Reset();
    WorldBounds = FBox2D(ForceInit);
    CurrentSector = NAME_None;

    UWorld* World = GetWorld();
    const APlayerController* PC = GetOwningPlayer();
    const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
    bHavePlayer = Pawn != nullptr;
    if (Pawn)
    {
        PlayerWorld = FVector2D(Pawn->GetActorLocation());
        PlayerYaw = Pawn->GetActorRotation().Yaw;
        WorldBounds += PlayerWorld;
    }
    if (!World)
    {
        return;
    }
    for (TActorIterator<AHavocSectorVolume> It(World); It; ++It)
    {
        const FBox Box = It->GetComponentsBoundingBox(true);
        if (!Box.IsValid)
        {
            continue;
        }
        FSectorBox Entry;
        Entry.Id = It->SectorId;
        Entry.Bounds = FBox2D(FVector2D(Box.Min), FVector2D(Box.Max));
        Entry.bHasPlayer = Pawn && Box.IsInsideOrOn(Pawn->GetActorLocation());
        if (Entry.bHasPlayer && CurrentSector.IsNone())
        {
            CurrentSector = Entry.Id;
        }
        WorldBounds += Entry.Bounds;
        Sectors.Add(Entry);
    }
    for (TActorIterator<AHavocLaneHazard> It(World); It; ++It)
    {
        FHazardDot Dot;
        Dot.World = FVector2D(It->GetActorLocation());
        Dot.bRevealed = It->IsRevealed();
        Dot.bCleared = It->IsCleared();
        WorldBounds += Dot.World;
        Hazards.Add(Dot);
    }
}

int32 UHavocMinimapWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
    FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
    LayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
    if (!WorldBounds.bIsValid)
    {
        return LayerId;
    }
    FPaintContext Context(AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
    const FVector2D Size = AllottedGeometry.GetLocalSize();

    // Panel background and frame.
    const FVector2D Corners[4] = { FVector2D(0, 0), FVector2D(Size.X, 0), Size, FVector2D(0, Size.Y) };
    UWidgetBlueprintLibrary::DrawLine(Context, Corners[0], Corners[1], FLinearColor(1, 1, 1, 0.6f), true, 2.f);
    UWidgetBlueprintLibrary::DrawLine(Context, Corners[1], Corners[2], FLinearColor(1, 1, 1, 0.6f), true, 2.f);
    UWidgetBlueprintLibrary::DrawLine(Context, Corners[2], Corners[3], FLinearColor(1, 1, 1, 0.6f), true, 2.f);
    UWidgetBlueprintLibrary::DrawLine(Context, Corners[3], Corners[0], FLinearColor(1, 1, 1, 0.6f), true, 2.f);

    // World X is forward (up on the map), world Y is right. Keep the aspect ratio, with a small margin.
    const FVector2D Extent = FVector2D::Max(WorldBounds.GetSize(), FVector2D(100.0, 100.0));
    const double Margin = 10.0;
    const double Scale = FMath::Min((Size.X - 2 * Margin) / Extent.Y, (Size.Y - 2 * Margin) / Extent.X);
    const FVector2D Center = WorldBounds.GetCenter();
    auto ToMap = [&](const FVector2D& W)
    {
        return FVector2D(Size.X * 0.5 + (W.Y - Center.Y) * Scale, Size.Y * 0.5 - (W.X - Center.X) * Scale);
    };

    for (const FSectorBox& Sector : Sectors)
    {
        const FVector2D A = ToMap(FVector2D(Sector.Bounds.Min.X, Sector.Bounds.Min.Y));
        const FVector2D B = ToMap(FVector2D(Sector.Bounds.Min.X, Sector.Bounds.Max.Y));
        const FVector2D C = ToMap(FVector2D(Sector.Bounds.Max.X, Sector.Bounds.Max.Y));
        const FVector2D D = ToMap(FVector2D(Sector.Bounds.Max.X, Sector.Bounds.Min.Y));
        const FLinearColor Color = Sector.bHasPlayer ? FLinearColor(1.f, 0.85f, 0.3f, 0.95f) : FLinearColor(0.5f, 0.7f, 0.9f, 0.6f);
        const float Thickness = Sector.bHasPlayer ? 3.f : 1.5f;
        UWidgetBlueprintLibrary::DrawLine(Context, A, B, Color, true, Thickness);
        UWidgetBlueprintLibrary::DrawLine(Context, B, C, Color, true, Thickness);
        UWidgetBlueprintLibrary::DrawLine(Context, C, D, Color, true, Thickness);
        UWidgetBlueprintLibrary::DrawLine(Context, D, A, Color, true, Thickness);
    }

    for (const FHazardDot& Dot : Hazards)
    {
        if (!Dot.bRevealed && !Dot.bCleared)
        {
            continue; // hidden until a drone pass reveals it
        }
        const FVector2D P = ToMap(Dot.World);
        const FLinearColor Color = Dot.bCleared ? FLinearColor(0.3f, 0.9f, 0.4f) : FLinearColor(1.f, 0.3f, 0.25f);
        UWidgetBlueprintLibrary::DrawLine(Context, P + FVector2D(-4, -4), P + FVector2D(4, 4), Color, true, 2.f);
        UWidgetBlueprintLibrary::DrawLine(Context, P + FVector2D(-4, 4), P + FVector2D(4, -4), Color, true, 2.f);
    }

    if (bHavePlayer)
    {
        // A short arrow pointing the way the pawn faces (yaw 0 = world +X = up on the map).
        const FVector2D P = ToMap(PlayerWorld);
        const float Rad = FMath::DegreesToRadians(PlayerYaw);
        const FVector2D Dir(FMath::Sin(Rad), -FMath::Cos(Rad));
        const FVector2D Side(-Dir.Y, Dir.X);
        const FLinearColor Color(1.f, 1.f, 1.f, 1.f);
        UWidgetBlueprintLibrary::DrawLine(Context, P - Dir * 6.0 + Side * 4.0, P + Dir * 8.0, Color, true, 2.f);
        UWidgetBlueprintLibrary::DrawLine(Context, P - Dir * 6.0 - Side * 4.0, P + Dir * 8.0, Color, true, 2.f);
    }
    return FMath::Max(LayerId, Context.MaxLayer);
}
