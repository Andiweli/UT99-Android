#include "Mesh.h"

namespace
{

struct FVertexCornerData
{
	INT TextureIndex;
	DWORD PolyFlags;
	BYTE U;
	BYTE V;
	FVector Normal;
};

static void ReduceFramesBetween( const UMesh* Mesh, INT StartFrame, INT EndFrame, FLOAT BaseToleranceSq, const TArray<FLOAT>* PerVertexToleranceSq, TArray<BYTE>& KeepFlags );
static void SortIntArray( TArray<INT>& Array );
static FLOAT ComputeMeshScale( const UMesh* Mesh );
static void BuildVertexCornerData( const UMesh* Mesh, TArray<TArray<FVertexCornerData>>& OutCornerData );
static UBOOL AreCornerSetsEquivalent( const TArray<FVertexCornerData>& A, const TArray<FVertexCornerData>& B, INT UVTolerance, FLOAT CosNormalTolerance );
static void ComputeVertexMotionScales( const UMesh* Mesh, TArray<FLOAT>& OutMotion );
static void BuildPerVertexToleranceSq( const TArray<FLOAT>& Motion, FLOAT BaseTolerance, FLOAT MotionScale, TArray<FLOAT>& OutToleranceSq, FLOAT DefaultToleranceSq );
static void SnapUVs( const FMeshReducer::FOptions& Options, TArray<TArray<FVertexCornerData>>& CornerData );

static UBOOL AreTracksEquivalent( const TArray<FMeshVert>& A, const TArray<FMeshVert>& B, FLOAT PositionTolerance )
{
	if( A.Num() != B.Num() )
	{
		return 0;
	}

	const FLOAT ToleranceSq = PositionTolerance <= 0.0f ? 0.0f : PositionTolerance * PositionTolerance;

	for( INT i = 0; i < A.Num(); ++i )
	{
		const FVector Diff = A(i).Vector() - B(i).Vector();
		if( Diff.SizeSquared() > ToleranceSq )
		{
			return 0;
		}
	}

	return 1;
}

static void ReduceFramesBetween( const UMesh* Mesh, INT StartFrame, INT EndFrame, FLOAT BaseToleranceSq, const TArray<FLOAT>* PerVertexToleranceSq, TArray<BYTE>& KeepFlags )
{
	if( EndFrame <= StartFrame + 1 )
	{
		return;
	}

	if( BaseToleranceSq <= 0.0f && PerVertexToleranceSq == nullptr )
	{
		for( INT FrameIndex = StartFrame + 1; FrameIndex < EndFrame; ++FrameIndex )
		{
			if( KeepFlags.IsValidIndex(FrameIndex) )
				KeepFlags(FrameIndex) = 1;
		}
		return;
	}

	const INT FrameVerts = Mesh->FrameVerts;
	INT BestIndex = INDEX_NONE;
	FLOAT MaxError = 0.0f;

	for( INT FrameIndex = StartFrame + 1; FrameIndex < EndFrame; ++FrameIndex )
	{
		const FLOAT Alpha = FLOAT(FrameIndex - StartFrame) / FLOAT(EndFrame - StartFrame);
		FLOAT FrameError = 0.0f;

		for( INT VertIndex = 0; VertIndex < FrameVerts; ++VertIndex )
		{
			const INT StartVertIndex = StartFrame * FrameVerts + VertIndex;
			const INT EndVertIndex = EndFrame * FrameVerts + VertIndex;
			const INT ActualVertIndex = FrameIndex * FrameVerts + VertIndex;

			if( !Mesh->Verts.IsValidIndex(StartVertIndex) || !Mesh->Verts.IsValidIndex(EndVertIndex) || !Mesh->Verts.IsValidIndex(ActualVertIndex) )
				continue;

			const FVector StartPos = Mesh->Verts( StartVertIndex ).Vector();
			const FVector EndPos = Mesh->Verts( EndVertIndex ).Vector();
			const FVector ActualPos = Mesh->Verts( ActualVertIndex ).Vector();
			const FVector ExpectedPos = StartPos + (EndPos - StartPos) * Alpha;
			const FLOAT ErrorSq = (ActualPos - ExpectedPos).SizeSquared();
			const FLOAT AllowSq = (PerVertexToleranceSq && PerVertexToleranceSq->IsValidIndex( VertIndex ))
				? (*PerVertexToleranceSq)( VertIndex )
				: BaseToleranceSq;
			if( ErrorSq > AllowSq )
			{
				FrameError = ErrorSq;
				break;
			}
			FrameError = ::Max( FrameError, ErrorSq );
		}

		if( FrameError > MaxError )
		{
			MaxError = FrameError;
			BestIndex = FrameIndex;
		}
	}

	if( BestIndex == INDEX_NONE )
	{
		return;
	}

	if( MaxError > BaseToleranceSq )
	{
		if( KeepFlags.IsValidIndex(BestIndex) )
			KeepFlags( BestIndex ) = 1;
		ReduceFramesBetween( Mesh, StartFrame, BestIndex, BaseToleranceSq, PerVertexToleranceSq, KeepFlags );
		ReduceFramesBetween( Mesh, BestIndex, EndFrame, BaseToleranceSq, PerVertexToleranceSq, KeepFlags );
	}
}

static void SortIntArray( TArray<INT>& Array )
{
	const INT Count = Array.Num();
	for( INT i = 0; i < Count; ++i )
	{
		for( INT j = i + 1; j < Count; ++j )
		{
			if( Array(j) < Array(i) )
			{
				const INT Temp = Array(i);
				Array(i) = Array(j);
				Array(j) = Temp;
			}
		}
	}
}

static FLOAT ComputeMeshScale( const UMesh* Mesh )
{
	const INT TotalVerts = Mesh ? Mesh->Verts.Num() : 0;
	if( Mesh == nullptr || TotalVerts == 0 )
	{
		return 1.0f;
	}

	FVector Min(0,0,0);
	FVector Max(0,0,0);
	UBOOL bFirst = 1;

	for( INT i = 0; i < TotalVerts; ++i )
	{
		const FVector Pos = Mesh->Verts(i).Vector();
		if( bFirst )
		{
			Min = Pos;
			Max = Pos;
			bFirst = 0;
		}
		else
		{
			if( Pos.X < Min.X ) Min.X = Pos.X;
			if( Pos.Y < Min.Y ) Min.Y = Pos.Y;
			if( Pos.Z < Min.Z ) Min.Z = Pos.Z;
			if( Pos.X > Max.X ) Max.X = Pos.X;
			if( Pos.Y > Max.Y ) Max.Y = Pos.Y;
			if( Pos.Z > Max.Z ) Max.Z = Pos.Z;
		}
	}

	const FVector Extent = (Max - Min) * 0.5f;
	FLOAT Scale = Extent.Size();
	if( Scale < 1.0f )
	{
		Scale = 1.0f;
	}
	return Scale;
}

static void BuildVertexCornerData( const UMesh* Mesh, TArray<TArray<FVertexCornerData>>& OutCornerData )
{
	OutCornerData.Empty();

	if( Mesh == nullptr || Mesh->FrameVerts <= 0 )
	{
		return;
	}

	OutCornerData.AddZeroed( Mesh->FrameVerts );

	for( INT TriIndex = 0; TriIndex < Mesh->Tris.Num(); ++TriIndex )
	{
		const FMeshTri& Triangle = Mesh->Tris( TriIndex );

		FVector VertPos[3] = { FVector(0,0,0), FVector(0,0,0), FVector(0,0,0) };
		for( INT Corner = 0; Corner < 3; ++Corner )
		{
			const INT VertexIndex = Triangle.iVertex[Corner];
			if( VertexIndex >= 0 && VertexIndex < Mesh->FrameVerts )
			{
				VertPos[Corner] = Mesh->Verts( VertexIndex ).Vector();
			}
		}

		FVector FaceNormal = (VertPos[1] - VertPos[0]) ^ (VertPos[2] - VertPos[0]);
		const FLOAT NormalLenSq = FaceNormal.SizeSquared();
		if( NormalLenSq > SMALL_NUMBER )
		{
			FaceNormal /= appSqrt( NormalLenSq );
		}
		else
		{
			FaceNormal = FVector(0,0,0);
		}

		for( INT Corner = 0; Corner < 3; ++Corner )
		{
			const INT VertexIndex = Triangle.iVertex[Corner];
			if( VertexIndex < 0 || VertexIndex >= Mesh->FrameVerts )
			{
				continue;
			}

			FVertexCornerData CornerData;
			CornerData.TextureIndex = Triangle.TextureIndex;
			CornerData.PolyFlags = Triangle.PolyFlags;
			CornerData.U = Triangle.Tex[Corner].U;
			CornerData.V = Triangle.Tex[Corner].V;
			CornerData.Normal = FaceNormal;

			OutCornerData( VertexIndex ).AddItem( CornerData );
		}
	}
}

static void SnapUVs( const FMeshReducer::FOptions& Options, TArray<TArray<FVertexCornerData>>& CornerData )
{
	if( Options.UVSnapGrid <= 0.0f )
	{
		return;
	}

	const FLOAT Grid = Options.UVSnapGrid;
	const FLOAT InvGrid = 1.0f / Grid;

	for( INT VertIndex = 0; VertIndex < CornerData.Num(); ++VertIndex )
	{
		TArray<FVertexCornerData>& Corners = CornerData( VertIndex );
		for( INT i = 0; i < Corners.Num(); ++i )
		{
			BYTE& U = Corners(i).U;
			BYTE& V = Corners(i).V;
			const FLOAT UF = (FLOAT)U / 255.0f;
			const FLOAT VF = (FLOAT)V / 255.0f;
			const FLOAT SnappedU = appRound( UF * InvGrid ) * Grid;
			const FLOAT SnappedV = appRound( VF * InvGrid ) * Grid;
			U = (BYTE)Clamp<INT>( appRound( SnappedU * 255.0f ), 0, 255 );
			V = (BYTE)Clamp<INT>( appRound( SnappedV * 255.0f ), 0, 255 );
		}
	}
}

static void ComputeVertexMotionScales( const UMesh* Mesh, TArray<FLOAT>& OutMotion )
{
	OutMotion.Empty();
	if( Mesh == nullptr || Mesh->AnimFrames <= 1 || Mesh->FrameVerts <= 0 )
	{
		return;
	}

	OutMotion.AddZeroed( Mesh->FrameVerts );
	for( INT FrameIndex = 0; FrameIndex < Mesh->AnimFrames - 1; ++FrameIndex )
	{
		const INT OffsetA = FrameIndex * Mesh->FrameVerts;
		const INT OffsetB = (FrameIndex + 1) * Mesh->FrameVerts;
		for( INT VertIndex = 0; VertIndex < Mesh->FrameVerts; ++VertIndex )
		{
			const FVector PosA = Mesh->Verts( OffsetA + VertIndex ).Vector();
			const FVector PosB = Mesh->Verts( OffsetB + VertIndex ).Vector();
			const FLOAT Dist = (PosB - PosA).Size();
			OutMotion( VertIndex ) = ::Max( OutMotion( VertIndex ), Dist );
		}
	}
}

static void BuildPerVertexToleranceSq( const TArray<FLOAT>& Motion, FLOAT BaseTolerance, FLOAT MotionScale, TArray<FLOAT>& OutToleranceSq, FLOAT DefaultToleranceSq )
{
	OutToleranceSq.Empty();
	if( Motion.Num() == 0 || (BaseTolerance <= 0.0f && MotionScale <= 0.0f) )
	{
		return;
	}

	OutToleranceSq.AddZeroed( Motion.Num() );
	for( INT i = 0; i < Motion.Num(); ++i )
	{
		const FLOAT Tol = BaseTolerance + MotionScale * Motion(i);
		const FLOAT Applied = (Tol > 0.0f) ? Tol : appSqrt( DefaultToleranceSq );
		OutToleranceSq(i) = Applied * Applied;
	}
}

static UBOOL AreCornerSetsEquivalent( const TArray<FVertexCornerData>& A, const TArray<FVertexCornerData>& B, INT UVTolerance, FLOAT CosNormalTolerance )
{
	if( A.Num() != B.Num() )
	{
		return 0;
	}

	if( A.Num() == 0 )
	{
		return 1;
	}

	TArray<BYTE> Used;
	Used.AddZeroed( B.Num() );

	for( INT i = 0; i < A.Num(); ++i )
	{
		UBOOL bMatched = 0;
		for( INT j = 0; j < B.Num(); ++j )
		{
			if( Used(j) )
			{
				continue;
			}

			if( A(i).TextureIndex != B(j).TextureIndex )
			{
				continue;
			}

			if( A(i).PolyFlags != B(j).PolyFlags )
			{
				continue;
			}

			if( Abs( (INT)A(i).U - (INT)B(j).U ) > UVTolerance )
			{
				continue;
			}

			if( Abs( (INT)A(i).V - (INT)B(j).V ) > UVTolerance )
			{
				continue;
			}

			if( CosNormalTolerance > 0.0f )
			{
				const FLOAT Dot = (A(i).Normal | B(j).Normal);
				if( Dot < CosNormalTolerance )
				{
					continue;
				}
			}

			Used(j) = 1;
			bMatched = 1;
			break;
		}

		if( !bMatched )
		{
			return 0;
		}
	}

	return 1;
}

static void RebuildConnectivity( UMesh* Mesh )
{
	if( Mesh->FrameVerts <= 0 )
	{
		return;
	}

	Mesh->Connects.Empty();
	for( INT i = 0; i < Mesh->FrameVerts; i++ )
		Mesh->Connects.Add();
	Mesh->VertLinks.Empty();

	for( INT VertIndex = 0; VertIndex < Mesh->FrameVerts; ++VertIndex )
	{
		FMeshVertConnect& Connect = Mesh->Connects( VertIndex );
		Connect.NumVertTriangles = 0;
		Connect.TriangleListOffset = Mesh->VertLinks.Num();

		for( INT TriIndex = 0; TriIndex < Mesh->Tris.Num(); ++TriIndex )
		{
			const FMeshTri& Triangle = Mesh->Tris( TriIndex );
			for( INT Corner = 0; Corner < 3; ++Corner )
			{
				if( Triangle.iVertex[Corner] == VertIndex )
				{
					Mesh->VertLinks.AddItem( TriIndex );
					Connect.NumVertTriangles++;
				}
			}
		}
	}
}

static void RebuildBounds( UMesh* Mesh )
{
	if( Mesh->AnimFrames <= 0 || Mesh->FrameVerts <= 0 )
	{
		return;
	}

	Mesh->BoundingBoxes.Empty();
	Mesh->BoundingSpheres.Empty();
	for( INT i = 0; i < Mesh->AnimFrames; i++ )
	{
		Mesh->BoundingBoxes.AddItem(FBox());
		Mesh->BoundingSpheres.AddItem(FSphere());
	}

	TArray<FVector> AllFrames;

	for( INT FrameIndex = 0; FrameIndex < Mesh->AnimFrames; ++FrameIndex )
	{
		TArray<FVector> FramePoints;

		for( INT VertIndex = 0; VertIndex < Mesh->FrameVerts; ++VertIndex )
		{
			const FMeshVert& Vert = Mesh->Verts( FrameIndex * Mesh->FrameVerts + VertIndex );
			const FVector Pos = Vert.Vector();
			FramePoints.AddItem( Pos );
			AllFrames.AddItem( Pos );
		}

		Mesh->BoundingBoxes( FrameIndex ) = FBox( &FramePoints(0), FramePoints.Num() );
		Mesh->BoundingSpheres( FrameIndex ) = FSphere( &FramePoints(0), FramePoints.Num() );
	}

	Mesh->BoundingBox = FBox( &AllFrames(0), AllFrames.Num() );
	Mesh->BoundingSphere = FSphere( &AllFrames(0), AllFrames.Num() );
}

static UBOOL ReduceVertices( UMesh* Mesh, const FMeshReducer::FOptions& Options, FLOAT PositionTolerance, INT UVTolerance, FLOAT CosNormalTolerance );
static UBOOL RemoveDuplicateTriangles( UMesh* Mesh );

static UBOOL ReduceVertices( UMesh* Mesh, const FMeshReducer::FOptions& Options, FLOAT PositionTolerance, INT UVTolerance, FLOAT CosNormalTolerance )
{
	if( Mesh->FrameVerts <= 0 || Mesh->AnimFrames <= 0 )
	{
		return 0;
	}

	const INT OriginalFrameVerts = Mesh->FrameVerts;

	TArray<TArray<FMeshVert>> Tracks;
	Tracks.AddZeroed( Mesh->FrameVerts );

	TArray<TArray<FVertexCornerData>> CornerData;
	BuildVertexCornerData( Mesh, CornerData );
	SnapUVs( Options, CornerData );

	for( INT VertIndex = 0; VertIndex < Mesh->FrameVerts; ++VertIndex )
	{
		TArray<FMeshVert>& Track = Tracks( VertIndex );

		for( INT FrameIndex = 0; FrameIndex < Mesh->AnimFrames; ++FrameIndex )
		{
			Track.AddItem( Mesh->Verts( FrameIndex * Mesh->FrameVerts + VertIndex ) );
		}
	}

	TArray<TArray<FMeshVert>> UniqueTracks;
	TArray<TArray<FVertexCornerData>> UniqueCornerSets;
	TArray<INT> Remap;
	Remap.AddZeroed( Mesh->FrameVerts );

	for( INT VertIndex = 0; VertIndex < Mesh->FrameVerts; ++VertIndex )
	{
		const TArray<FMeshVert>& Track = Tracks( VertIndex );

		INT CanonicalIndex = INDEX_NONE;
		for( INT UniqueIndex = 0; UniqueIndex < UniqueTracks.Num(); ++UniqueIndex )
		{
			if( !AreTracksEquivalent( Track, UniqueTracks( UniqueIndex ), PositionTolerance ) )
			{
				continue;
			}

			if( !AreCornerSetsEquivalent( CornerData( VertIndex ), UniqueCornerSets( UniqueIndex ), UVTolerance, CosNormalTolerance ) )
			{
				continue;
			}

			CanonicalIndex = UniqueIndex;
			break;
		}

		if( CanonicalIndex == INDEX_NONE )
		{
			CanonicalIndex = UniqueTracks.AddZeroed();
			UniqueTracks( CanonicalIndex ) = Track;
			UniqueCornerSets.AddZeroed();
			UniqueCornerSets( CanonicalIndex ) = CornerData( VertIndex );
		}

		Remap( VertIndex ) = CanonicalIndex;
	}

	const INT NewFrameVerts = UniqueTracks.Num();
	if( NewFrameVerts == OriginalFrameVerts )
	{
		return 0;
	}

	TArray<FMeshVert> NewVerts;
	NewVerts.AddZeroed( Mesh->AnimFrames * NewFrameVerts );

	for( INT FrameIndex = 0; FrameIndex < Mesh->AnimFrames; ++FrameIndex )
	{
		for( INT VertIndex = 0; VertIndex < NewFrameVerts; ++VertIndex )
		{
			NewVerts( FrameIndex * NewFrameVerts + VertIndex ) = UniqueTracks( VertIndex )( FrameIndex );
		}
	}

	for( INT TriIndex = 0; TriIndex < Mesh->Tris.Num(); ++TriIndex )
	{
		FMeshTri& Triangle = Mesh->Tris( TriIndex );
		for( INT Corner = 0; Corner < 3; ++Corner )
		{
			const INT OldIndex = Triangle.iVertex[Corner];
			if( Remap.IsValidIndex( OldIndex ) )
			{
				Triangle.iVertex[Corner] = Remap( OldIndex );
			}
		}
	}

	Mesh->Verts.Empty();
	for( INT i = 0; i < NewVerts.Num(); i++ )
		Mesh->Verts.AddItem( NewVerts(i) );
	Mesh->FrameVerts = NewFrameVerts;

	RebuildConnectivity( Mesh );
	RebuildBounds( Mesh );

	return 1;
}

static UBOOL RemoveDuplicateTriangles( UMesh* Mesh )
{
	if( Mesh->Tris.Num() == 0 )
		return 0;

	struct FTriKey
	{
		INT V[3];
		DWORD PolyFlags;
		INT TextureIndex;

		bool operator<( const FTriKey& Other ) const
		{
			for( INT i = 0; i < 3; ++i )
			{
				if( V[i] != Other.V[i] )
					return V[i] < Other.V[i];
			}
			if( PolyFlags != Other.PolyFlags )
				return PolyFlags < Other.PolyFlags;
			return TextureIndex < Other.TextureIndex;
		}
	};

	TArray<FTriKey> Seen;
	TArray<FMeshTri> NewTris;
	NewTris.Empty();

	for( INT i = 0; i < Mesh->Tris.Num(); ++i )
	{
		FMeshTri Tri = Mesh->Tris(i);
		FTriKey Key;
		Key.TextureIndex = Tri.TextureIndex;
		Key.PolyFlags = Tri.PolyFlags;
		Key.V[0] = Tri.iVertex[0];
		Key.V[1] = Tri.iVertex[1];
		Key.V[2] = Tri.iVertex[2];
		if( Key.V[0] > Key.V[1] ) Exchange( Key.V[0], Key.V[1] );
		if( Key.V[1] > Key.V[2] ) Exchange( Key.V[1], Key.V[2] );
		if( Key.V[0] > Key.V[1] ) Exchange( Key.V[0], Key.V[1] );

		UBOOL bDuplicate = 0;
		for( INT s = 0; s < Seen.Num(); ++s )
		{
			if( Seen(s).TextureIndex == Key.TextureIndex &&
				Seen(s).PolyFlags == Key.PolyFlags &&
				Seen(s).V[0] == Key.V[0] &&
				Seen(s).V[1] == Key.V[1] &&
				Seen(s).V[2] == Key.V[2] )
			{
				bDuplicate = 1;
				break;
			}
		}
		if( !bDuplicate )
		{
			Seen.AddItem( Key );
			NewTris.AddItem( Tri );
		}
	}

	if( NewTris.Num() == Mesh->Tris.Num() )
		return 0;

	Mesh->Tris.Empty();
	for( INT i = 0; i < NewTris.Num(); i++ )
		Mesh->Tris.AddItem( NewTris(i) );
	return 1;
}
static UBOOL ReduceKeyframes( UMesh* Mesh, FLOAT FrameTolerance, const TArray<FLOAT>* PerVertexToleranceSq )
{
	if( Mesh->AnimFrames <= 2 || Mesh->FrameVerts <= 0 )
	{
		return 0;
	}

	if( FrameTolerance <= 0.0f )
	{
		return 0;
	}

	const FLOAT BaseToleranceSq = FrameTolerance * FrameTolerance;

	TArray<INT> ForcedFrames;
	ForcedFrames.AddUniqueItem( 0 );
	ForcedFrames.AddUniqueItem( Mesh->AnimFrames - 1 );

	for( INT SeqIndex = 0; SeqIndex < Mesh->AnimSeqs.Num(); ++SeqIndex )
	{
		const FMeshAnimSeq& Seq = Mesh->AnimSeqs( SeqIndex );
		if( Seq.NumFrames <= 0 )
		{
			continue;
		}

		ForcedFrames.AddUniqueItem( Seq.StartFrame );
		ForcedFrames.AddUniqueItem( Seq.StartFrame + Seq.NumFrames - 1 );
	}

	if( ForcedFrames.Num() < 2 )
	{
		return 0;
	}

	SortIntArray( ForcedFrames );

	TArray<BYTE> KeepFlags;
	KeepFlags.AddZeroed( Mesh->AnimFrames );

	for( INT i = 0; i < ForcedFrames.Num(); ++i )
	{
		const INT FrameIndex = ForcedFrames(i);
		if( KeepFlags.IsValidIndex( FrameIndex ) )
		{
			KeepFlags(FrameIndex) = 1;
		}
	}

	for( INT i = 0; i < ForcedFrames.Num() - 1; ++i )
	{
		const INT StartFrame = ForcedFrames(i);
		const INT EndFrame = ForcedFrames(i + 1);
		ReduceFramesBetween( Mesh, StartFrame, EndFrame, BaseToleranceSq, PerVertexToleranceSq, KeepFlags );
	}

	INT NewFrameCount = 0;
	TArray<INT> FrameRemap;
	FrameRemap.AddZeroed( Mesh->AnimFrames );

	for( INT FrameIndex = 0; FrameIndex < Mesh->AnimFrames; ++FrameIndex )
	{
		if( KeepFlags(FrameIndex) )
		{
			FrameRemap(FrameIndex) = NewFrameCount++;
		}
		else
		{
			FrameRemap(FrameIndex) = INDEX_NONE;
		}
	}

	if( NewFrameCount == Mesh->AnimFrames )
	{
		return 0;
	}

	TArray<FMeshVert> NewVerts;
	NewVerts.AddZeroed( Mesh->FrameVerts * NewFrameCount );

	for( INT OldFrame = 0; OldFrame < Mesh->AnimFrames; ++OldFrame )
	{
		const INT NewFrame = FrameRemap( OldFrame );
		if( NewFrame == INDEX_NONE )
		{
			continue;
		}

		for( INT VertIndex = 0; VertIndex < Mesh->FrameVerts; ++VertIndex )
		{
			NewVerts( NewFrame * Mesh->FrameVerts + VertIndex ) = Mesh->Verts( OldFrame * Mesh->FrameVerts + VertIndex );
		}
	}

	for( INT SeqIndex = 0; SeqIndex < Mesh->AnimSeqs.Num(); ++SeqIndex )
	{
		FMeshAnimSeq& Seq = Mesh->AnimSeqs( SeqIndex );
		if( Seq.NumFrames <= 0 )
		{
			continue;
		}

		const INT OldStart = Seq.StartFrame;
		const INT OldNumFrames = Seq.NumFrames;
		const INT OldEnd = OldStart + OldNumFrames;
		const FLOAT OldRate = Seq.Rate > SMALL_NUMBER ? Seq.Rate : 30.0f;
		const FLOAT OldDuration = OldNumFrames > 0 ? (FLOAT)OldNumFrames / OldRate : 0.0f;

		INT NewStart = FrameRemap( OldStart );
		INT NewNumFrames = 0;

		for( INT FrameIndex = OldStart; FrameIndex < OldEnd && FrameIndex < FrameRemap.Num(); ++FrameIndex )
		{
			if( FrameRemap(FrameIndex) != INDEX_NONE )
			{
				++NewNumFrames;
			}
		}

		if( NewStart == INDEX_NONE )
		{
			// Should not happen because start frames are forced, but guard anyway.
			NewStart = 0;
		}

		Seq.StartFrame = NewStart;
		Seq.NumFrames = Max( 1, NewNumFrames );

		if( OldDuration > SMALL_NUMBER )
		{
			Seq.Rate = Seq.NumFrames / OldDuration;
		}
	}

	Mesh->AnimFrames = NewFrameCount;
	Mesh->Verts.Empty();
	for( INT i = 0; i < NewVerts.Num(); i++ )
		Mesh->Verts.AddItem( NewVerts(i) );

	RebuildBounds( Mesh );

	return 1;
}

} // namespace

UBOOL FMeshReducer::Reduce( UMesh* Mesh, const FOptions& Options, FMeshReductionStats* OutStats )
{
	guard(FMeshReducer::Reduce);

	if( Mesh == nullptr )
	{
		return 0;
	}

	FMeshReductionStats Stats;
	Stats.MeshName = Mesh->GetPathName();
	Stats.OriginalVerts = Mesh->FrameVerts;
	Stats.OriginalTriangles = Mesh->Tris.Num();
	Stats.OriginalFrames = Mesh->AnimFrames;
	Stats.ReducedVerts = Mesh->FrameVerts;
	Stats.ReducedTriangles = Mesh->Tris.Num();
	Stats.ReducedFrames = Mesh->AnimFrames;
	Stats.bChanged = 0;

	const FLOAT MeshScale = ComputeMeshScale( Mesh );
	const FLOAT PositionToleranceUnits = Options.PositionTolerance * MeshScale;
	const FLOAT FrameToleranceUnits = Options.FrameErrorTolerance * MeshScale;

	INT UVToleranceValue = (INT)appFloor( Options.UVTolerance * 255.0f + 0.5f );
	if( UVToleranceValue < 0 )
	{
		UVToleranceValue = 0;
	}
	else if( UVToleranceValue > 255 )
	{
		UVToleranceValue = 255;
	}

	const FLOAT CosNormalTolerance = (Options.NormalAngleToleranceDeg > 0.0f)
		? appCos( Options.NormalAngleToleranceDeg * (FLOAT)PI / 180.0f )
		: -1.0f;
#if 1
	const UBOOL bReducedVertices = 0;
	const UBOOL bRemovedTris = 0;
#else
	const UBOOL bReducedVertices = ReduceVertices( Mesh, Options, PositionToleranceUnits, UVToleranceValue, CosNormalTolerance );
	const UBOOL bRemovedTris = RemoveDuplicateTriangles( Mesh );
#endif

	TArray<FLOAT> MotionScales;
	TArray<FLOAT> PerVertexToleranceSq;
	if( Options.MotionErrorScale > 0.0f )
	{
		ComputeVertexMotionScales( Mesh, MotionScales );
		BuildPerVertexToleranceSq( MotionScales, FrameToleranceUnits, Options.MotionErrorScale, PerVertexToleranceSq, FrameToleranceUnits * FrameToleranceUnits );
	}

	const UBOOL bReducedFrames = ReduceKeyframes( Mesh, FrameToleranceUnits, PerVertexToleranceSq.Num() ? &PerVertexToleranceSq : nullptr );

	Stats.ReducedVerts = Mesh->FrameVerts;
	Stats.ReducedTriangles = Mesh->Tris.Num();
	Stats.ReducedFrames = Mesh->AnimFrames;
	Stats.ReducedVerts = Mesh->FrameVerts;
	Stats.ReducedFrames = Mesh->AnimFrames;
	Stats.bChanged = bReducedVertices | bReducedFrames | bRemovedTris;

	if( OutStats )
	{
		*OutStats = Stats;
	}

	return Stats.bChanged;
	unguard;
}

