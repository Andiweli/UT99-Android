/*=============================================================================
	UnMeshEd.cpp: Unreal editor mesh code
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#include "EditorPrivate.h"
#include "UnRender.h"

/*-----------------------------------------------------------------------------
	Data types for importing James' creature meshes.
-----------------------------------------------------------------------------*/

// James mesh info.
struct FJSDataHeader
{
	_WORD	NumPolys;
	_WORD	NumVertices;
	_WORD	BogusRot;
	_WORD	BogusFrame;
	DWORD	BogusNormX,BogusNormY,BogusNormZ;
	DWORD	FixScale;
	DWORD	Unused1,Unused2,Unused3;
};

// James animation info.
struct FJSAnivHeader
{
	_WORD	NumFrames;		// Number of animation frames.
	_WORD	FrameSize;		// Size of one frame of animation.
};

// Mesh triangle.
struct FJSMeshTri
{
	_WORD		iVertex[3];		// Vertex indices.
	BYTE		Type;			// James' mesh type.
	BYTE		Color;			// Color for flat and Gouraud shaded.
	FMeshUV		Tex[3];			// Texture UV coordinates.
	BYTE		TextureNum;		// Source texture offset.
	BYTE		Flags;			// Unreal mesh flags (currently unused).
};

// Byte describing effects for a mesh triangle.
enum EJSMeshTriType
{
	// Triangle types. Pick ONE AND ONLY ONE of these.
	MTT_Normal				= 0,	// Normal one-sided.
	MTT_NormalTwoSided      = 1,    // Normal but two-sided.
	MTT_Translucent			= 2,	// Translucent two-sided.
	MTT_Masked				= 3,	// Masked two-sided.
	MTT_Modulate			= 4,	// Modulation blended two-sided.
	MTT_Placeholder			= 8,	// Placeholder triangle for positioning weapon. Invisible.

	// Bit flags. Add any of these you want.
	MTT_Unlit				= 16,	// Full brightness, no lighting.
	MTT_Flat				= 32,	// Flat surface, don't do bMeshCurvy thing.
	MTT_Environment			= 64,	// Environment mapped.
	MTT_NoSmooth			= 128,	// No bilinear filtering on this poly's texture.
};

/*-----------------------------------------------------------------------------
	Import functions.
-----------------------------------------------------------------------------*/

// Mesh sorting function.
static QSORT_RETURN CDECL CompareTris( const FMeshTri* A, const FMeshTri* B )
{
	if     ( (A->PolyFlags&PF_Translucent) > (B->PolyFlags&PF_Translucent) ) return  1;
	else if( (A->PolyFlags&PF_Translucent) < (B->PolyFlags&PF_Translucent) ) return -1;
	else if( A->TextureIndex               > B->TextureIndex               ) return  1;
	else if( A->TextureIndex               < B->TextureIndex               ) return -1;
	else if( A->PolyFlags                  > B->PolyFlags                  ) return  1;
	else if( A->PolyFlags                  < B->PolyFlags                  ) return -1;
	else                                                                     return  0;
}

//
// Import a mesh from James' editor.  Uses file commands instead of object
// manager.  Slow but works fine.
//
void UEditorEngine::meshImport
(
	const TCHAR*		MeshName,
	UObject*			InParent,
	const TCHAR*		AnivFname, 
	const TCHAR*		DataFname,
	UBOOL				Unmirror,
	UBOOL				ZeroTex,
	INT					UnMirrorTex,
	ULODProcessInfo*	LODInfo
)
{
	guard(UEditorEngine::meshImport);

	UMesh*			Mesh;
	FArchive*		AnivFile;
	FArchive*		DataFile;
	FJSDataHeader	JSDataHdr;
	FJSAnivHeader	JSAnivHdr;
	INT				i;
	INT				Ok = 0;
	INT				MaxTextureIndex = 0;

	debugf( NAME_Log, TEXT("Importing %s"), MeshName );
	GWarn->BeginSlowTask( TEXT("Importing mesh"), 1, 0 );
	GWarn->StatusUpdatef( 0, 0, TEXT("%s"), TEXT("Reading files") );

	// Open James' animation vertex file and read header.
	AnivFile = GFileManager->CreateFileReader( AnivFname, 0, GLog );
	if( !AnivFile )
	{
		debugf( NAME_Log, TEXT("Error opening file %s"), AnivFname );
		goto Out1;
	}
	AnivFile->Serialize( &JSAnivHdr, sizeof(FJSAnivHeader) );
	if( AnivFile->IsError() )
	{
		debugf( NAME_Log, TEXT("Error reading %s"), AnivFname );
		goto Out2;
	}

	// Open James' mesh data file and read header.
	DataFile = GFileManager->CreateFileReader( DataFname, 0, GLog );
	if( !DataFile )
	{
		debugf( NAME_Log, TEXT("Error opening file %s"), DataFname );
		goto Out2;
	}
	DataFile->Serialize( &JSDataHdr, sizeof(FJSDataHeader) );
	if( DataFile->IsError() )
	{
		debugf( NAME_Log, TEXT("Error reading %s"), DataFname );
		goto Out3;
	}

	// Allocate mesh or lodmesh object.
	if( !LODInfo->LevelOfDetail )
		Mesh = new( InParent, MeshName, RF_Public|RF_Standalone )UMesh( JSDataHdr.NumPolys, JSDataHdr.NumVertices, JSAnivHdr.NumFrames );
	else 
		Mesh = new( InParent, MeshName, RF_Public|RF_Standalone )ULodMesh( JSDataHdr.NumPolys, JSDataHdr.NumVertices, JSAnivHdr.NumFrames );

	// Display summary info.
	debugf(NAME_Log,TEXT(" * Triangles  %i"),Mesh->Tris.Num());
	debugf(NAME_Log,TEXT(" * Vertices   %i"),Mesh->FrameVerts);
	debugf(NAME_Log,TEXT(" * AnimFrames %i"),Mesh->AnimFrames);
	debugf(NAME_Log,TEXT(" * FrameSize  %i"),JSAnivHdr.FrameSize);
	debugf(NAME_Log,TEXT(" * AnimSeqs   %i"),Mesh->AnimSeqs.Num());

	// Import mesh triangles.
	debugf( NAME_Log, TEXT("Importing triangles") );
	GWarn->StatusUpdatef( 0, 0, TEXT("%s"), TEXT("Importing Triangles") );
	DataFile->Seek( DataFile->Tell() + 12 );
	for( i=0; i<Mesh->Tris.Num(); i++ )
	{
		guard(Importing triangles);

		// Load triangle.
		FJSMeshTri Tri;
		DataFile->Serialize( &Tri, sizeof(Tri) );
		if( DataFile->IsError() )
		{
			debugf( NAME_Log, TEXT("Error processing %s"), DataFname );
			goto Out4;
		}
		if( Unmirror )
		{
			Exchange( Tri.iVertex[1], Tri.iVertex[2] );
			Exchange( Tri.Tex    [1], Tri.Tex    [2] );
			if( Tri.TextureNum == UnMirrorTex )
			{
				Tri.Tex[0].U = 255 - Tri.Tex[0].U;
				Tri.Tex[1].U = 255 - Tri.Tex[1].U;
				Tri.Tex[2].U = 255 - Tri.Tex[2].U;
			}
		}
		if( ZeroTex )
		{
			Tri.TextureNum = 0;
		}

		// Copy to Unreal structures.
		Mesh->Tris(i).iVertex[0]	= Tri.iVertex[0];
		Mesh->Tris(i).iVertex[1]	= Tri.iVertex[1];
		Mesh->Tris(i).iVertex[2]	= Tri.iVertex[2];
		Mesh->Tris(i).Tex[0]		= Tri.Tex[0];
		Mesh->Tris(i).Tex[1]		= Tri.Tex[1];
		Mesh->Tris(i).Tex[2]		= Tri.Tex[2];
		Mesh->Tris(i).TextureIndex	= Tri.TextureNum;
		MaxTextureIndex = Max<INT>(MaxTextureIndex,Tri.TextureNum);
		while( Tri.TextureNum >= Mesh->Textures.Num() )
			Mesh->Textures.AddItem( NULL );

		// Set style based on triangle type.
		DWORD PolyFlags=0;
		if     ( (Tri.Type&15)==MTT_Normal         ) PolyFlags |= 0;
		else if( (Tri.Type&15)==MTT_NormalTwoSided ) PolyFlags |= PF_TwoSided;
		else if( (Tri.Type&15)==MTT_Modulate       ) PolyFlags |= PF_TwoSided | PF_Modulated;
		else if( (Tri.Type&15)==MTT_Translucent    ) PolyFlags |= PF_TwoSided | PF_Translucent;
		else if( (Tri.Type&15)==MTT_Masked         ) PolyFlags |= PF_TwoSided | PF_Masked;
		else if( (Tri.Type&15)==MTT_Placeholder    ) PolyFlags |= PF_TwoSided | PF_Invisible;

		// Handle effects.
		if     ( Tri.Type&MTT_Unlit             ) PolyFlags |= PF_Unlit;
		if     ( Tri.Type&MTT_Flat              ) PolyFlags |= PF_Flat;
		if     ( Tri.Type&MTT_Environment       ) PolyFlags |= PF_Environment;
		if     ( Tri.Type&MTT_NoSmooth          ) PolyFlags |= PF_NoSmooth;

		// Set flags.
		Mesh->Tris(i).PolyFlags = PolyFlags;

		unguard;
	}

	// Sort triangles by texture and flags.
	appQsort( &Mesh->Tris(0), Mesh->Tris.Num(), sizeof(Mesh->Tris(0)), (QSORT_COMPARE)CompareTris );

	// Texture LOD.
	for( i=0; i<MaxTextureIndex+1; i++ )
	{
		Mesh->TextureLOD.Add( 1.0 );
		Mesh->Textures.Add( NULL );
	}

	// Import mesh vertices.
	debugf( NAME_Log, TEXT("Importing vertices") );
	GWarn->StatusUpdatef( 0, 0, TEXT("%s"), TEXT("Importing Vertices") );
	for( i=0; i<Mesh->AnimFrames; i++ )
	{
		guard(Importing animation frames);
		AnivFile->Serialize( &Mesh->Verts(i * Mesh->FrameVerts), sizeof(FMeshVert) * Mesh->FrameVerts );
		if( AnivFile->IsError() )
		{
			debugf( NAME_Log, TEXT("Vertex error in mesh %s, frame %i: expecting %i verts"), AnivFname, i, Mesh->FrameVerts );
			break;
		}
		if( Unmirror )
			for( INT j=0; j<Mesh->FrameVerts; j++ )
				Mesh->Verts(i * Mesh->FrameVerts + j).X *= -1;
		AnivFile->Seek( AnivFile->Tell() + JSAnivHdr.FrameSize - Mesh->FrameVerts * sizeof(FMeshVert) );
		unguard;
	}

	// Build list of triangles per vertex.
	if( !LODInfo->LevelOfDetail )
	{
		GWarn->StatusUpdatef( i, Mesh->FrameVerts, TEXT("%s"), TEXT("Linking mesh") );
		for( i=0; i<Mesh->FrameVerts; i++ )
		{
			guard(ImportingVertices);
			Mesh->Connects(i).NumVertTriangles = 0;
			Mesh->Connects(i).TriangleListOffset = Mesh->VertLinks.Num();
			for( INT j=0; j<Mesh->Tris.Num(); j++ )
			{
				for( INT k=0; k<3; k++ )
				{
					if( Mesh->Tris(j).iVertex[k] == i )
					{
						Mesh->VertLinks.AddItem(j);
						Mesh->Connects(i).NumVertTriangles++;
					}
				}
			}
			unguard;
		}
		debugf( NAME_Log, TEXT("Made %i links"), Mesh->VertLinks.Num() );
	}

	// Compute per-frame bounding volumes plus overall bounding volume.
	meshBuildBounds(Mesh);

	// Process for LOD. Called last; needs the mesh bounds from above.
	if( LODInfo->LevelOfDetail )
		meshLODProcess( (ULodMesh*)Mesh, LODInfo );

	// Exit labels.
	Ok = 1;
	Out4: if (!Ok) {delete Mesh;}
	Out3: delete DataFile;
	Out2: delete AnivFile;
	Out1: GWarn->EndSlowTask();
	unguard;
}

void UEditorEngine::meshDropFrames
(
	UMesh*			Mesh,
	INT				StartFrame,
	INT				NumFrames
)
{
	guard(UEditorEngine::meshDropFrames);
	Mesh->Verts.Remove( StartFrame*Mesh->FrameVerts, NumFrames*Mesh->FrameVerts );
	Mesh->AnimFrames -= NumFrames;
	unguard;
}

/*-----------------------------------------------------------------------------
	Bounds.
-----------------------------------------------------------------------------*/

//
// Build bounding boxes for each animation frame of the mesh,
// and one bounding box enclosing all animation frames.
//
void UEditorEngine::meshBuildBounds( UMesh* Mesh )
{
	guard(UEditorEngine::meshBuildBounds);
	GWarn->StatusUpdatef( 0, 0, TEXT("%s"), TEXT("Bounding mesh") );

	// Bound all frames.
	TArray<FVector> AllFrames;
	for( INT i=0; i<Mesh->AnimFrames; i++ )
	{
		TArray<FVector> OneFrame;
		for( INT j=0; j<Mesh->FrameVerts; j++ )
		{
			FVector Vertex = Mesh->Verts( i * Mesh->FrameVerts + j ).Vector();
			OneFrame .AddItem( Vertex );
			AllFrames.AddItem( Vertex );
		}
		Mesh->BoundingBoxes  (i) = FBox   ( &OneFrame(0), OneFrame.Num() );
		Mesh->BoundingSpheres(i) = FSphere( &OneFrame(0), OneFrame.Num() );
	}
	Mesh->BoundingBox    = FBox   ( &AllFrames(0), AllFrames.Num() );
	Mesh->BoundingSphere = FSphere( &AllFrames(0), AllFrames.Num() );

	// Display bounds.
	debugf
	(
		NAME_Log,
		TEXT("BoundingBox (%f,%f,%f)-(%f,%f,%f) BoundingSphere (%f,%f,%f) %f"),
		Mesh->BoundingBox.Min.X,
		Mesh->BoundingBox.Min.Y,
		Mesh->BoundingBox.Min.Z,
		Mesh->BoundingBox.Max.X,
		Mesh->BoundingBox.Max.Y,
		Mesh->BoundingBox.Max.Z,
		Mesh->BoundingSphere.X,
		Mesh->BoundingSphere.Y,
		Mesh->BoundingSphere.Z,
		Mesh->BoundingSphere.W
	);
	unguard;
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
