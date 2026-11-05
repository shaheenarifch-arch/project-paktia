#pragma once

// Minimap: a top-down box map of the AHavocSectorVolume actors in the level (the current sector is
// highlighted), lane hazards (hidden until revealed, then red, green once cleared) and the player.
// Drawn in NativePaint; it reads actors each frame, so there is nothing to subscribe to.
// Help: https://dev.epicgames.com/documentation/en-us/unreal-engine/umg-ui-designer-for-unreal-engine

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HavocMinimapWidget.generated.h"

UCLASS()
class MIDNIGHTHAVOC_API UHavocMinimapWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** Id of the sector the player pawn is in, or None. Updated on tick. */
    FName GetCurrentSector() const { return CurrentSector; }

protected:
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
        FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
    struct FSectorBox { FName Id; FBox2D Bounds; bool bHasPlayer = false; };
    struct FHazardDot { FVector2D World; bool bRevealed = false; bool bCleared = false; };

    void Refresh();

    TArray<FSectorBox> Sectors;
    TArray<FHazardDot> Hazards;
    FVector2D PlayerWorld = FVector2D::ZeroVector;
    float PlayerYaw = 0.f;
    bool bHavePlayer = false;
    FBox2D WorldBounds = FBox2D(ForceInit);
    FName CurrentSector;
    float RefreshTimer = 0.f;
};
