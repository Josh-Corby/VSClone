#include "WorldGenerator.h"
#include "KismetProceduralMeshLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "ProceduralMeshComponent.h"

// Sets default values
AWorldGenerator::AWorldGenerator()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	ProceduralMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ProceduralMesh"));
	ProceduralMesh->bUseAsyncCooking = true;
	ProceduralMesh->SetupAttachment(GetRootComponent());
	UnloadDistance = CellSize * (NumSectionsX + NumSectionsY) / 2 * (XVertexCount + YVertexCount);
}

// Called when the game starts or when spawned
void AWorldGenerator::BeginPlay()
{
	Super::BeginPlay();	
}

void AWorldGenerator::GenerateTerrain(const int InXIndex, const int InYIndex)
{
	FVector Offset = CalculateOffset(InXIndex, InYIndex);
	TArray<FVector> Vertices;
	TArray<FVector2D> UVs;
	GenerateVerticesAndUVs(Offset, Vertices, UVs);

	if (Triangles.Num() == 0)
	{
		GenerateTriangles();
	}

	TArray<FVector> Normals;
	TArray<FColor> VertexColors;
	TArray<FProcMeshTangent> Tangents;

	UKismetProceduralMeshLibrary::CalculateTangentsForMesh(Vertices, Triangles, UVs, Normals, Tangents);

	GenerateSubsets(Vertices, UVs, Normals, Tangents);

	bTileDataReady = true;
}

void AWorldGenerator::GenerateTerrainAsync(const int InXIndex, const int InYIndex)
{
	bGeneratorBusy = true;
	SectionIndexX = InXIndex;
	SectionIndexY = InYIndex;
	QueuedTiles.Add(FIntPoint(InXIndex, InYIndex), SectionIndex);

	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [&]()
		{
			FAsyncTask<FAsyncWorldGenerator>* WorldGenTask = new FAsyncTask<FAsyncWorldGenerator>(this);
			WorldGenTask->StartBackgroundTask();
			WorldGenTask->EnsureCompletion();
			delete WorldGenTask;
		});
}

void AWorldGenerator::DrawTile()
{
	bTileDataReady = false;

	// Remove furthest tile
	int FurthestTileIndex = GetFurthestUpdatableTileIndex();

	// is furthest tile already created?
	if (FurthestTileIndex > -1)
	{
		RemoveTile(FurthestTileIndex);
	}

	else
	{
		CreateTile();
	}

	ClearSubData();
	bGeneratorBusy = false;
}

void AWorldGenerator::CreateTile()
{
	ProceduralMesh->CreateMeshSection(SectionIndex, SubVertices, SubTriangles, SubNormals, SubUVs, TArray<FColor>(), SubTangents, true);
	ProceduralMesh->SetMaterial(SectionIndex, TileMaterial);
	SectionIndex++;
}

void AWorldGenerator::RemoveTile(int InTileIndex)
{
	TArray<int> Values;
	TArray<FIntPoint> Keys;
	QueuedTiles.GenerateKeyArray(Keys);
	QueuedTiles.GenerateValueArray(Values);
	int UnloadMeshIndex = Values[InTileIndex];
	FIntPoint TileToUnload = Keys[InTileIndex];

	ProceduralMesh->ClearMeshSection(UnloadMeshIndex);
	ProceduralMesh->CreateMeshSection(UnloadMeshIndex, SubVertices, SubTriangles, SubNormals, SubUVs, TArray<FColor>(), SubTangents, true);
	QueuedTiles.Add(FIntPoint(SectionIndexX, SectionIndexY), UnloadMeshIndex);
	QueuedTiles.Remove(TileToUnload);
}

FVector AWorldGenerator::CalculateOffset(const int InXIndex, const int InYIndex) const
{
	float XOffset = InXIndex * (XVertexCount - 1) * CellSize;
	float YOffset = InYIndex * (YVertexCount - 1) * CellSize;
	FVector Offset = FVector(XOffset, YOffset, 0.f);
	return Offset;
}

void AWorldGenerator::GenerateVerticesAndUVs(FVector InOffset, TArray<FVector>& InVertices, TArray<FVector2D>& InUVs) const
{
	FVector Vertex;
	FVector2D UV;

	for (int32 y = -1; y <= YVertexCount; y++)
	{
		for (int32 x = -1; x <= XVertexCount; x++)
		{
			// Vertex Calculation
			Vertex.X = x * CellSize + InOffset.X;
			Vertex.Y = y * CellSize + InOffset.Y;
			Vertex.Z = 0.f;
			InVertices.Add(Vertex);

			// UVs
			UV.X = (x + (SectionIndexX * (XVertexCount - 1))) * CellSize / 100;
			UV.Y = (y + (SectionIndexY * (YVertexCount - 1))) * CellSize / 100;
			InUVs.Add(UV);
		}
	}
}

void AWorldGenerator::GenerateTriangles()
{
	for (int32 y = 0; y <= YVertexCount; y++)
	{
		for (int32 x = 0; x <= XVertexCount; x++)
		{
			// Tri 1
			Triangles.Add(x + y * (XVertexCount + 2));
			Triangles.Add(x + (y + 1) * (XVertexCount + 2));
			Triangles.Add(x + y * (XVertexCount + 2) + 1);

			// Tri 2
			Triangles.Add(x + (y + 1) * (XVertexCount + 2));
			Triangles.Add(x + (y + 1) * (XVertexCount + 2) + 1);
			Triangles.Add(x + y * (XVertexCount + 2) + 1);
		}
	}
}

void AWorldGenerator::GenerateSubTriangles()
{
	for (int32 y = 0; y <= YVertexCount - 2; y++)
	{
		for (int32 x = 0; x <= XVertexCount - 2; x++)
		{
			SubTriangles.Add(x + y * XVertexCount);
			SubTriangles.Add(x + y * XVertexCount + XVertexCount);
			SubTriangles.Add(x + y * XVertexCount + 1);

			SubTriangles.Add(x + y * XVertexCount + XVertexCount);
			SubTriangles.Add(x + y * XVertexCount + XVertexCount + 1);
			SubTriangles.Add(x + y * XVertexCount + 1);
		}
	}
}

void AWorldGenerator::GenerateSubsets(const TArray<FVector>& InVertices, const TArray<FVector2D>& InUVs, const TArray<FVector>& InNormals, const TArray<FProcMeshTangent>& InTangents)
{
	int VertexIndex = 0;
	// Subset vertices and UVs
	for (int32 y = -1; y <= YVertexCount; y++)
	{
		for (int32 x = -1; x <= XVertexCount; x++)
		{
			if (-1 < y && y < YVertexCount && -1 < x && x < XVertexCount)
			{
				SubVertices.Add(InVertices[VertexIndex]);
				SubUVs.Add(InUVs[VertexIndex]);
				SubNormals.Add(InNormals[VertexIndex]);
				SubTangents.Add(InTangents[VertexIndex]);
			}
			VertexIndex++;
		}
	}

	// Calculate subtriangles
	if (SubTriangles.Num() == 0)
	{
		GenerateSubTriangles();
	}
}

FVector2D AWorldGenerator::GetPlayerLocation() const
{
	AActor* Player = Cast<AActor>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));
	if (Player)
	{
		return FVector2D(Player->GetActorLocation());
	}
	return FVector2D(0.f, 0.f);
}

FVector2D AWorldGenerator::GetTileLocation(FIntPoint InTileCoordinate) const
{
	FVector2D TileLocation = FVector2D(InTileCoordinate * FIntPoint(XVertexCount - 1, YVertexCount - 1) * CellSize) + FVector2D(XVertexCount - 1, YVertexCount - 1) * CellSize / 2;
	return TileLocation;
}

FIntPoint AWorldGenerator::GetClosestQueuedTile() const
{
	float ClosestDistance = TNumericLimits<float>::Max();
	FIntPoint ClosestTile;

	for (const TPair<FIntPoint,int>& Entry : QueuedTiles)
	{
		if (Entry.Value == -1)
		{
			FVector2D TileLocation = GetTileLocation(Entry.Key);
			FVector2D PlayerLocation = GetPlayerLocation();
			float Distance = FVector2D::Distance(TileLocation, PlayerLocation);
			if (Distance < ClosestDistance)
			{
				ClosestTile = Entry.Key;
				ClosestDistance = Distance;
			}
		}
	}
	return ClosestTile;
}

int AWorldGenerator::GetFurthestUpdatableTileIndex() const
{
	float FurthestDistance = TNumericLimits<float>::Min();
	int FurthestTileIndex = -1;
	int CurrentIndex = 0;

	for (const TPair<FIntPoint, int>& Entry : QueuedTiles)
	{
		if (Entry.Value != -1)
		{
			FVector2D TileLocation = GetTileLocation(Entry.Key);
			FVector2D PlayerLocation = GetPlayerLocation();
			float Distance = FVector2D::Distance(TileLocation, PlayerLocation);
			if (Distance > FurthestDistance && Distance > UnloadDistance)
			{
				FurthestDistance = Distance;
				FurthestTileIndex = CurrentIndex;
			}
		}
		CurrentIndex++;
	}

	return FurthestTileIndex;
}

void AWorldGenerator::ClearSubData()
{
	SubVertices.Empty();
	SubNormals.Empty();
	SubUVs.Empty();
	SubTangents.Empty();
}

void FAsyncWorldGenerator::DoWork()
{
	WorldGenerator->GenerateTerrain(WorldGenerator->SectionIndexX, WorldGenerator->SectionIndexY);
}