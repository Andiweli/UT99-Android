/*=============================================================================
	UnActor.cpp: Actor list functions.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#include "EnginePrivate.h"

/*-----------------------------------------------------------------------------
	UPlayer object implementation.
-----------------------------------------------------------------------------*/

UPlayer::UPlayer()
{}
void UPlayer::Serialize( FArchive& Ar )
{
	guard(UPlayer::Serialize);
	Super::Serialize( Ar );
	unguard;
}
void UPlayer::Destroy()
{
	guard(UPlayer::Destroy);
	if( GIsRunning && Actor )
	{
		ULevel* Level = Actor->GetLevel();
		Actor->Player = NULL;
		Level->DestroyActor( Actor, 1 );
		Actor = NULL;
	}
	Super::Destroy();
	unguard;
}
UBOOL UPlayer::Exec( const TCHAR* Cmd, FOutputDevice& Ar )
{
	guard(UPlayer::Exec);
	if( Actor && Actor->GetLevel()->Exec(Cmd,Ar) )
	{
		return 1;
	}
	if( Actor && Actor->Level && Actor->Level->Game && Actor->Level->Game->ScriptConsoleExec(Cmd,Ar,Actor) )
	{
		return 1;
	}
	if( Actor && Actor->myHUD && Actor->myHUD->ScriptConsoleExec(Cmd,Ar,Actor) )
	{
		return 1;
	}
	if( Actor && Actor->ScriptConsoleExec(Cmd,Ar,Actor) )
	{
		return 1;
	}
	if( Actor )
	{
		for( AInventory* Inv=Actor->Inventory; Inv; Inv=Inv->Inventory )
			if( Inv->ScriptConsoleExec(Cmd,Ar,Actor) )
				return 1;
	}
	if( Actor && Actor->GetLevel()->Engine->Exec(Cmd,Ar) )
	{
		return 1;
	}
	else return 0;
	unguard;
}
IMPLEMENT_CLASS(UPlayer);

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
