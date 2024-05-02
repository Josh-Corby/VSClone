#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "WorldGenerator.generated.h"

UCLASS()
class VSCLONE_API AWorldGenerator : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AWorldGenerator();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation Settings")
	int XVertexCount = 10;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation Settings")
	int YVertexCount = 10;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation Settings")
	float CellSize = 200;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation Settings")
	int NumSectionsX = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation Settings")
	int NumSectionsY = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation Settings")
	int SectionIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation Settings")
	float UnloadDistance;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation Settings")
	UMaterialInterface* TileMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation Settings")
	TMap<FIntPoint,int> QueuedTiles;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	class UProceduralMeshComponent* ProceduralMesh;

	UPROPERTY(BlueprintReadOnly)
	bool bGeneratorBusy = false;

	UPROPERTY(BlueprintReadOnly)
	bool bTileDataReady = false;

	int SectionIndexX = 0;
	int SectionIndexY = 0;

	TArray<int32> Triangles;
	// Subset mesh data for threading
	TArray<int32> SubTriangles;
	TArray<FVector> SubVertices;
	TArray<FVector2D> SubUVs;
	TArray<FVector> SubNormals;
	TArray<FProcMeshTangent> SubTangents;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	FVector CalculateOffset(const int InXIndex, const int InYIndex) const;

	void GenerateVerticesAndUVs(FVector InOffset, TArray<FVector>& InVertices, TArray<FVector2D>& InUVs) const;

	void GenerateSubsets(const TArray<FVector>& InVertices, const TArray<FVector2D>& InUVs, const TArray<FVector>& InNormals, const TArray<FProcMeshTangent>& InTangents);

	void GenerateTriangles();

	void GenerateSubTriangles();

public:
	UFUNCTION(BlueprintCallable)
	void GenerateTerrain(const int InXIndex, const int InYIndex);

	UFUNCTION(BlueprintCallable)
	void GenerateTerrainAsync(const int InXIndex, const int InYIndex);

	UFUNCTION(BlueprintCallable)
	void DrawTile();

	void CreateTile();

	void RemoveTile(int InTileIndex);

	UFUNCTION(BlueprintCallable)
	FVector2D GetPlayerLocation() const;

	UFUNCTION(BlueprintCallable)
	FVector2D GetTileLocation(FIntPoint InTileCoordinate) const;

	UFUNCTION(BlueprintCallable)
	FIntPoint GetClosestQueuedTile() const;

	UFUNCTION(BlueprintCallable)
	int GetFurthestUpdatableTileIndex() const;

	void ClearSubData();
};

class FAsyncWorldGenerator : public FNonAbandonableTask
{
public:
	FAsyncWorldGenerator(AWorldGenerator* InWorldGenerator) : WorldGenerator(InWorldGenerator) {}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FAsyncWorldGenerator, STATGROUP_ThreadPoolAsyncTasks);
	}

	void DoWork();

private:
	AWorldGenerator* WorldGenerator;

};