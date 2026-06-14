/*=============================================================================
	EdHook.cpp: UnrealEd VB hooks.
	Copyright 1997 Epic MegaGames, Inc. This software is a trade secret.

Revision history:
	* Created by Tim Sweeney.
=============================================================================*/

// Includes.
#include "EditorPrivate.h"

// Thread exchange.
const char* GTopic;
const char* GItem;
const char* GValue;


// Misc.
UEngine* Engine;

/*-----------------------------------------------------------------------------
	Editor hook exec.
-----------------------------------------------------------------------------*/

void UEditorEngine::NotifyDestroy( void* Src )
{
	guard(UEditorEngine::NotifyDestroy);
	if( Src==ActorProperties )
		ActorProperties = NULL;
	if( Src==LevelProperties )
		LevelProperties = NULL;
	if( Src==Preferences )
		Preferences = NULL;
	if( Src==UseDest )
		UseDest = NULL;
	unguard;
}

void UEditorEngine::NotifyPreChange( void* Src )
{
	guard(UEditorEngine::NotifyPreChange);
	Trans->Begin( TEXT("Edit Properties") );
	unguard;
}

void UEditorEngine::NotifyPostChange( void* Src )
{
	guard(UEditorEngine::NotifyPostChange);
	Trans->End();
	if( Src==Preferences )
	{
		GCache.Flush();
		for( TObjectIterator<UViewport> It; It; ++It )
			It->Actor->FovAngle = FovAngle;
	}
	RedrawLevel( Level );
	unguard;
}

void UEditorEngine::NotifyExec( void* Src, const char* Cmd )
{
	guard(UEditorEngine::NotifyExec);
	if( ParseCommand(&Cmd,"BROWSECLASS") )
	{
		ParseObject( Cmd, "CLASS=", BrowseClass, ANY_PACKAGE );
		UseDest = (WProperties*)Src;
		EdCallback( EDC_Browse, 1 );
	}
	else if( ParseCommand(&Cmd,"USECURRENT") )
	{
		ParseObject( Cmd, "CLASS=", BrowseClass, ANY_PACKAGE );
		UseDest = (WProperties*)Src;
		EdCallback( EDC_UseCurrent, 1 );
	}
	unguard;
}

void UEditorEngine::UpdatePropertiesWindows()
{
	guard(UEditorEngine::UpdatePropertiesWindow);
	unguard;
}

UBOOL UEditorEngine::HookExec( const TCHAR* Cmd, FOutputDevice& Ar )
{
	guard(UEditorEngine::HookExec);
	return 0;
	unguard;
}

void UEditorEngine::EdCallback( DWORD Code, UBOOL Send )
{
	guard(FGlobalPlatform::EdCallback);
	unguard;
}

/*-----------------------------------------------------------------------------
	The end.
-----------------------------------------------------------------------------*/
