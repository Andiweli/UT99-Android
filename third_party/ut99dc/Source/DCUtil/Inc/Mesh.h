#pragma once

#include "Engine.h"

struct FMeshReductionStats
{
	FString MeshName;
	INT OriginalVerts = 0;
	INT OriginalTriangles = 0;
	INT OriginalFrames = 0;
	INT ReducedVerts = 0;
	INT ReducedTriangles = 0;
	INT ReducedFrames = 0;
	UBOOL bChanged = false;
};

class FMeshReducer
{
public:
	struct FOptions
	{
		FLOAT PositionTolerance = 0.01f;
		FLOAT NormalTolerance = 0.01f;
		FLOAT UVTolerance = 0.01f;
		FLOAT FrameErrorTolerance = 0.25f;
		FLOAT MotionErrorScale = 0.25f;
		FLOAT UVSnapGrid = 1.0f / 64.0f;
		FLOAT SeamPositionTolerance = 0.25f;
		FLOAT SeamNormalAngleDeg = 15.0f;
		INT UVToleranceBytes = 0;
		FLOAT NormalAngleToleranceDeg = 5.0f;
		INT MaxMeshletVertices = 128;
	};

	static UBOOL Reduce( UMesh* Mesh, const FOptions& Options, FMeshReductionStats* OutStats = nullptr );
};

