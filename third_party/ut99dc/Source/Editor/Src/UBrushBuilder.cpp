/*=============================================================================
	UBrushBuilder.cpp: UnrealEd brush builder.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Tim Sweeney.
=============================================================================*/

#include "EditorPrivate.h"

/*-----------------------------------------------------------------------------
	UBrushBuilder.
-----------------------------------------------------------------------------*/

void UBrushBuilder::execBeginBrush( FFrame& Stack, RESULT_DECL )
{
	guard(UBrushBuilder::execBeginBrush);
	P_GET_UBOOL_OPTX(Merge,0);
	P_GET_NAME_OPTX(GroupName,NAME_None);
	P_FINISH;
	Group = GroupName;
	MergeCoplanars = Merge;
	Vertices.Empty();
	Polys.Empty();
	unguard;
}
void UBrushBuilder::execEndBrush( FFrame& Stack, RESULT_DECL )
{
	guard(UBrushBuilder::execEndBrush);
	P_FINISH;
	//!!validate
	UModel* Brush = GEditor->Level ? GEditor->Level->Brush()->Brush : NULL;
	if( Brush )
	{
		GEditor->Trans->Begin( TEXT("Brush Set") );
		Brush->Modify();
		GEditor->Level->Brush()->Modify();
		GEditor->Level->Brush()->Group = Group;
		GEditor->Constraints.Snap( NULL, GEditor->Level->Brush()->Location, FVector(0,0,0), GEditor->Level->Brush()->Rotation );
		FModelCoords TempCoords;
		GEditor->Level->Brush()->BuildCoords( &TempCoords, NULL );
		GEditor->Level->Brush()->Location -= GEditor->Level->Brush()->PrePivot.TransformVectorBy( TempCoords.PointXform );
		GEditor->Level->Brush()->PrePivot = FVector(0,0,0);
		{
			Brush->Polys->Element.Empty();
			for( TArray<FBuilderPoly>::TIterator It(Polys); It; ++It )
			{
				if( It->Direction<0 )
					for( INT i=0; i<It->VertexIndices.Num()/2; i++ )
						Exchange( It->VertexIndices(i), It->VertexIndices.Last(i) );
				for( ; ; )
				{
					INT iMax = Min<INT>(FPoly::MAX_VERTICES,It->VertexIndices.Num());
					FPoly Poly;
					Poly.Init();
					Poly.ItemName = It->ItemName;
					Poly.Base = Vertices(It->VertexIndices(0));
					Poly.PolyFlags = It->PolyFlags;
					for( INT j=0; j<iMax; j++ )
						Poly.Vertex[Poly.NumVertices++] = Vertices(It->VertexIndices(j));
					Poly.Finalize( 1 );
					new(Brush->Polys->Element)FPoly(Poly);
					if( iMax==It->VertexIndices.Num() )
						break;
					It->VertexIndices.Remove(1,iMax-2);
				}
			}
		}
		if( MergeCoplanars )
			GEditor->bspMergeCoplanars( Brush, 0, 1 );
		GEditor->bspValidateBrush( Brush, 1, 1 );
		Brush->BuildBound();
		GEditor->Trans->End();
		GEditor->RedrawLevel( GEditor->Level );
		GEditor->NoteSelectionChange( GEditor->Level );
	}
	*(BITFIELD*)Result=1;
	unguard;
}
void UBrushBuilder::execGetVertexCount( FFrame& Stack, RESULT_DECL )
{
	guard(UBrushBuilder::execGetVertexCount);
	P_FINISH;
	*(INT*)Result = Vertices.Num();
	unguard;
}
void UBrushBuilder::execGetVertex( FFrame& Stack, RESULT_DECL )
{
	guard(UBrushBuilder::execGetVertex);
	P_GET_INT(i);
	P_FINISH;
	*(FVector*)Result = Vertices.IsValidIndex(i) ? Vertices(i) : FVector(0,0,0);
	unguard;
}
void UBrushBuilder::execGetPolyCount( FFrame& Stack, RESULT_DECL )
{
	guard(UBrushBuilder::execGetPolyCount);
	P_FINISH;
	*(INT*)Result = Polys.Num();
	unguard;
}
void UBrushBuilder::execBadParameters( FFrame& Stack, RESULT_DECL )
{
	guard(UBrushBuilder::execBadParameters);
	P_GET_STR(Msg);
	P_FINISH;
	GWarn->Logf(NAME_UserPrompt,Msg!=TEXT("") ? *Msg : TEXT("Bad parameters in brush builder"));
	unguard;
}
void UBrushBuilder::execVertexv( FFrame& Stack, RESULT_DECL )
{
	guard(UBrushBuilder::execVertexv);
	P_GET_STRUCT(FVector,V);
	P_FINISH;
	*(INT*)Result = Vertices.Num();
	new(Vertices)FVector(V);
	unguard;
}
void UBrushBuilder::execVertex3f( FFrame& Stack, RESULT_DECL )
{
	guard(UBrushBuilder::execVertex3f);
	P_GET_FLOAT(X);
	P_GET_FLOAT(Y);
	P_GET_FLOAT(Z);
	P_FINISH;
	*(INT*)Result = Vertices.Num();
	new(Vertices)FVector(X,Y,Z);
	unguard;
}
void UBrushBuilder::execPoly3i( FFrame& Stack, RESULT_DECL )
{
	guard(UBrushBuilder::execPoly3i);
	P_GET_INT(Direction);
	P_GET_INT(i);
	P_GET_INT(j);
	P_GET_INT(k);
	P_GET_NAME_OPTX(ItemName,NAME_None);
	P_GET_INT_OPTX(PolyFlags,0);
	P_FINISH;
	new(Polys)FBuilderPoly;
	Polys.Last().Direction=Direction;
	new(Polys.Last().VertexIndices)INT(i);
	new(Polys.Last().VertexIndices)INT(j);
	new(Polys.Last().VertexIndices)INT(k);
	Polys.Last().PolyFlags = PolyFlags;
	unguard;
}
void UBrushBuilder::execPoly4i( FFrame& Stack, RESULT_DECL )
{
	guard(UBrushBuilder::execPoly4i);
	P_GET_INT(Direction);
	P_GET_INT(i);
	P_GET_INT(j);
	P_GET_INT(k);
	P_GET_INT(l);
	P_GET_NAME_OPTX(ItemName,NAME_None);
	P_GET_INT_OPTX(PolyFlags,0);
	P_FINISH;
	new(Polys)FBuilderPoly;
	Polys.Last().Direction=Direction;
	new(Polys.Last().VertexIndices)INT(i);
	new(Polys.Last().VertexIndices)INT(j);
	new(Polys.Last().VertexIndices)INT(k);
	new(Polys.Last().VertexIndices)INT(l);
	Polys.Last().PolyFlags = PolyFlags;
	unguard;
}
void UBrushBuilder::execPolyBegin( FFrame& Stack, RESULT_DECL )
{
	guard(UBrushBuilder::execPolyBegin);
	P_GET_INT(Direction);
	P_GET_NAME_OPTX(ItemName,NAME_None);
	P_GET_INT_OPTX(PolyFlags,0);
	P_FINISH;
	new(Polys)FBuilderPoly;
	Polys.Last().Direction = Direction;
	Polys.Last().PolyFlags = PolyFlags;
	unguard;
}
void UBrushBuilder::execPolyi( FFrame& Stack, RESULT_DECL )
{
	guard(UBrushBuilder::execPolyi);
	P_GET_INT(i);
	P_FINISH;
	new(Polys.Last().VertexIndices)INT(i);
	unguard;
}
void UBrushBuilder::execPolyEnd( FFrame& Stack, RESULT_DECL )
{
	guard(UBrushBuilder::execPolyEnd);
	P_FINISH;
	unguard;
}
IMPLEMENT_CLASS(UBrushBuilder)

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
