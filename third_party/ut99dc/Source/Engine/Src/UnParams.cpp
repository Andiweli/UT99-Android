/*=============================================================================
	UnParams.cpp: Functions to help parse commands.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	What's happening: When the Visual Basic level editor is being used,
	this code exchanges messages with Visual Basic.  This lets Visual Basic
	affect the world, and it gives us a way of sending world information back
	to Visual Basic.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#include "EnginePrivate.h"

/*-----------------------------------------------------------------------------
	Getters.
	All of these functions return 1 if the appropriate item was
	fetched, or 0 if not.
-----------------------------------------------------------------------------*/

//
// Get a floating-point vector (X=, Y=, Z=), return number of components parsed (0-3).
//
UBOOL ENGINE_API GetFVECTOR( const TCHAR* Stream, FVector& Value )
{
	guard(GetFVECTOR);
	int NumVects = 0;

	// Support for old format.
	NumVects += Parse( Stream, TEXT("X="), Value.X );
	NumVects += Parse( Stream, TEXT("Y="), Value.Y );
	NumVects += Parse( Stream, TEXT("Z="), Value.Z );

	// New format.
	if( NumVects==0 )
	{
		Value.X = appAtof(Stream);
		Stream = appStrchr(Stream,',');
		if( !Stream )
			return 0;
		Value.Y = appAtof(++Stream);
		Stream = appStrchr(Stream,',');
		if( !Stream ) return 0;
		Value.Z = appAtof(++Stream);
		NumVects=3;
	}
	return NumVects==3;
	unguard;
}

//
// Get a string enclosed in parenthesis.
//
UBOOL ENGINE_API GetSUBSTRING
(
	const TCHAR*	Stream, 
	const TCHAR*	Match,
	TCHAR*			Value,
	INT				MaxLen
)
{
	guard(GetSUBSTRING);

	const TCHAR* Found = appStrfind(Stream,Match);
	const TCHAR* Start;

	if( Found == NULL ) return 0; // didn't match.

	Start = Found + appStrlen(Match);
	if( *Start != '(' )
		return 0;

	appStrncpy( Value, Start+1, MaxLen );
	TCHAR* Temp=appStrchr( Value, ')' );
	if( Temp )
		*Temp=0;

	return 1;
	unguard;
}

//
// Get a floating-point vector (X=, Y=, Z=), return number of components parsed (0-3).
//
UBOOL ENGINE_API GetFVECTOR
(
	const TCHAR*	Stream, 
	const TCHAR*	Match, 
	FVector&		Value
)
{
	guard(GetFVECTOR);

	TCHAR Temp[80];
	if (!GetSUBSTRING(Stream,Match,Temp,80)) return 0;
	return GetFVECTOR(Temp,Value);

	unguard;
}

//
// Get a set of rotations (PITCH=, YAW=, ROLL=), return number of components parsed (0-3).
//
UBOOL ENGINE_API GetFROTATOR
(
	const TCHAR*	Stream, 
	FRotator&		Rotation,
	INT				ScaleFactor
)
{
	guard(GetFROTATOR);

	FLOAT	Temp=0.0;
	int 	N = 0;

	// Old format.
	if( Parse(Stream,TEXT("PITCH="),Temp) ) {Rotation.Pitch = (INT) (Temp * ScaleFactor); N++;}
	if( Parse(Stream,TEXT("YAW="),  Temp) ) {Rotation.Yaw   = (INT) (Temp * ScaleFactor); N++;}
	if( Parse(Stream,TEXT("ROLL="), Temp) ) {Rotation.Roll  = (INT) (Temp * ScaleFactor); N++;}

	// New format.
	if( N == 0 )
	{
		Rotation.Pitch = (INT) (appAtof(Stream) * ScaleFactor);
		Stream = appStrchr(Stream,',');
		if( !Stream )
			return 0;

		Rotation.Yaw = (INT) (appAtof(++Stream) * ScaleFactor);
		Stream = appStrchr(Stream,',');
		if( !Stream )
			return 0;

		Rotation.Roll = (INT) (appAtof(++Stream) * ScaleFactor);
		return 1;
	}
	return 0;
	unguard;
}

//
// Get a rotation value, return number of components parsed (0-3).
//
UBOOL ENGINE_API GetFROTATOR
(
	const TCHAR*	Stream, 
	const TCHAR*	Match, 
	FRotator&		Value,
	INT				ScaleFactor
)
{
	guard(GetFROTATOR);

	TCHAR Temp[80];
	if (!GetSUBSTRING(Stream,Match,Temp,80)) return 0;
	return GetFROTATOR(Temp,Value,ScaleFactor);

	unguard;
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
