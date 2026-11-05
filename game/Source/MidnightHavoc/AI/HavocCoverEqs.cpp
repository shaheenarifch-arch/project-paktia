#include "AI/HavocCoverEqs.h"

#include "AI/HavocAIController.h"
#include "Engine/World.h"
#include "EnvironmentQuery/EnvQuery.h"
#include "EnvironmentQuery/EnvQueryOption.h"
#include "EnvironmentQuery/Generators/EnvQueryGenerator_SimpleGrid.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_VectorBase.h"
#include "GameFramework/Pawn.h"

#define LOCTEXT_NAMESPACE "HavocCoverEqs"

UHavocEnvQueryTest_Cover::UHavocEnvQueryTest_Cover(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    Cost = EEnvTestCost::High; // line traces
    ValidItemType = UEnvQueryItemType_VectorBase::StaticClass();
    SetWorkOnFloatValues(false);
    TestPurpose = EEnvTestPurpose::Filter;
    FilterType = EEnvTestFilterType::Match;
    BoolValue.DefaultValue = true;
}

void UHavocEnvQueryTest_Cover::RunTest(FEnvQueryInstance& QueryInstance) const
{
    UObject* QueryOwner = QueryInstance.Owner.Get();
    const APawn* Querier = Cast<APawn>(QueryOwner);
    const AHavocAIController* Controller = Querier ? Cast<AHavocAIController>(Querier->GetController()) : nullptr;
    const AActor* Threat = Controller ? Controller->GetThreat() : nullptr;
    const UWorld* World = Querier ? Querier->GetWorld() : nullptr;
    if (!Threat || !World)
    {
        return; // no threat to hide from: leave the scores alone
    }

    BoolValue.BindData(QueryOwner, QueryInstance.QueryID);
    const bool bWantsCover = BoolValue.GetValue();
    const FVector ThreatEye = Threat->GetActorLocation() + FVector(0.f, 0.f, 60.f);

    FCollisionQueryParams Params(SCENE_QUERY_STAT(HavocCoverEqs), false, Querier);
    Params.AddIgnoredActor(Threat);

    for (FEnvQueryInstance::ItemIterator It(this, QueryInstance); It; ++It)
    {
        const FVector ItemLocation = GetItemLocation(QueryInstance, It.GetIndex());
        // Cover = something solid between the point (chest height) and the threat.
        const bool bBlocked = World->LineTraceTestByChannel(ItemLocation + FVector(0.f, 0.f, 90.f), ThreatEye, ECC_Visibility, Params);
        It.SetScore(TestPurpose, FilterType, bBlocked, bWantsCover);
    }
}

FText UHavocEnvQueryTest_Cover::GetDescriptionTitle() const
{
    return LOCTEXT("CoverTitle", "Cover from the unit's threat");
}

FText UHavocEnvQueryTest_Cover::GetDescriptionDetails() const
{
    return LOCTEXT("CoverDetails", "Passes points whose line to the threat is blocked");
}

UEnvQuery* HavocCoverEqs::BuildCoverQuery(UObject* Outer, float GridHalfSizeCm, float SpacingCm)
{
    UEnvQuery* Query = NewObject<UEnvQuery>(Outer, TEXT("HavocCoverQuery"));
    UEnvQueryOption* Option = NewObject<UEnvQueryOption>(Query, TEXT("CoverOption"));

    UEnvQueryGenerator_SimpleGrid* Grid = NewObject<UEnvQueryGenerator_SimpleGrid>(Option, TEXT("CoverGrid"));
    Grid->GridSize.DefaultValue = GridHalfSizeCm;
    Grid->SpaceBetween.DefaultValue = SpacingCm;
    Option->Generator = Grid;

    UHavocEnvQueryTest_Cover* Cover = NewObject<UHavocEnvQueryTest_Cover>(Option, TEXT("CoverTest"));
    Option->Tests.Add(Cover);

    Query->GetOptionsMutable().Add(Option);
    return Query;
}

#undef LOCTEXT_NAMESPACE
