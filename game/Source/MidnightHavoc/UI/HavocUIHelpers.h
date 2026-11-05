#pragma once

// Tiny helpers for building UMG widget trees in C++ (HUD and menus).

#include "CoreMinimal.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"

namespace HavocUI
{
    inline UTextBlock* MakeText(UWidgetTree* Tree, const FString& Text, int32 Size, const FLinearColor& Color)
    {
        UTextBlock* Block = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        Block->SetText(FText::FromString(Text));
        FSlateFontInfo Font = Block->GetFont();
        Font.Size = Size;
        Block->SetFont(Font);
        Block->SetColorAndOpacity(FSlateColor(Color));
        Block->SetShadowOffset(FVector2D(1.f, 1.f));
        Block->SetShadowColorAndOpacity(FLinearColor::Black);
        Block->SetAutoWrapText(true);
        return Block;
    }

    inline void Place(UCanvasPanel* Canvas, UWidget* Child, const FAnchors& Anchors, const FVector2D& Alignment, const FVector2D& Position, const FVector2D& Size)
    {
        if (UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Child))
        {
            Slot->SetAnchors(Anchors);
            Slot->SetAlignment(Alignment);
            Slot->SetPosition(Position);
            Slot->SetSize(Size);
            Slot->SetAutoSize(false);
        }
    }
}
