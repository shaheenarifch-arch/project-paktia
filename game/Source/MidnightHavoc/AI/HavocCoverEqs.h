#pragma once

// Environment Query System (EQS) cover query for AI squads.
//
// EQS query assets (UEnvQuery) are binary editor assets that cannot be committed from the repo, so the
// query is built in C++ at runtime: a grid of points around the querier (projected onto the NavMesh by
// the generator) filtered by UHavocEnvQueryTest_Cover, which keeps only points where solid geometry
// blocks the line to the unit's current threat. AHavocAIController picks the nearest surviving point;
// if EQS finds nothing (or there is no NavMesh) it falls back to its own trace search.
// Help: https://dev.epicgames.com/documentation/en-us/unreal-engine/environment-query-system-in-unreal-engine

#include "CoreMinimal.h"
#include "EnvironmentQuery/EnvQueryTest.h"
#include "HavocCoverEqs.generated.h"

class UEnvQuery;

/** Filter test: passes points that are out of the line of sight of the querier's threat (AHavocAIController::GetThreat). */
UCLASS()
class MIDNIGHTHAVOC_API UHavocEnvQueryTest_Cover : public UEnvQueryTest
{
    GENERATED_BODY()

public:
    UHavocEnvQueryTest_Cover(const FObjectInitializer& ObjectInitializer);

    virtual void RunTest(FEnvQueryInstance& QueryInstance) const override;
    virtual FText GetDescriptionTitle() const override;
    virtual FText GetDescriptionDetails() const override;
};

namespace HavocCoverEqs
{
    /** Builds the cover query: grid of GridHalfSizeCm around the querier, spaced SpacingCm, filtered by UHavocEnvQueryTest_Cover. */
    MIDNIGHTHAVOC_API UEnvQuery* BuildCoverQuery(UObject* Outer, float GridHalfSizeCm, float SpacingCm);
}
