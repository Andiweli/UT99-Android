/*=============================================================================
	EdHook.cpp: UnrealEd VB hooks.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Tim Sweeney.
=============================================================================*/

// Includes.
#pragma warning( disable : 4201 )
#pragma warning( disable : 4310 )
#define STRICT
#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include "EditorPrivate.h"
#include "Window.h"
#include "UnRender.h"

// Thread exchange.
HANDLE			hEngineThreadStarted;
HANDLE			hEngineThread;
HWND			hWndEngine;
DWORD			EngineThreadId;
FStringOutputDevice* GetPropResult;
const TCHAR*	GTopic;
const TCHAR*	GItem;
const TCHAR*	GValue;
TCHAR*			GCommand;

// Messages.
#define WM_USER_EXEC    (WM_USER+0 + 923)
#define WM_USER_SETPROP (WM_USER+1 + 923)
#define WM_USER_GETPROP (WM_USER+2 + 923)

// Misc.
DWORD hWndCallback, hWndMain;
UEngine* Engine;

// Memory allocator.
#include "FMallocWindows.h"
FMallocWindows Malloc;

// Log file.
#include "FOutputDeviceFile.h"
FOutputDeviceFile Log;

// Error handler.
#include "FOutputDeviceWindowsError.h"
FOutputDeviceWindowsError Error;

// Feedback.
#include "FFeedbackContextWindows.h"
FFeedbackContextWindows Warn;

// File manager.
#include "FFileManagerWindows.h"
FFileManagerWindows FileManager;

// Config.
#include "FConfigCacheIni.h"

/*-----------------------------------------------------------------------------
	Engine init.
-----------------------------------------------------------------------------*/

#define IDDIALOG_Splash 0
#include "UnEngineWin.h"

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
AUTOREGISTER_TOPIC(TEXT("Obj"),ObjTopicHandler);
void ObjTopicHandler::Get( ULevel* Level, const TCHAR* Item, FOutputDevice& Ar )
{
	guard(ObjTopicHandler::Get);
	if( ParseCommand(&Item,TEXT("QUERY")) )
	{
		UClass* Class;
		if( ParseObject<UClass>(Item,TEXT("TYPE="),Class,ANY_PACKAGE) )
		{
			UPackage* BasePackage;
			UPackage* RealPackage;
			TArray<UObject*> Results;
			if( !ParseObject<UPackage>( Item, TEXT("PACKAGE="), BasePackage, NULL ) )
			{
				// Objects in any package.
				for( FObjectIterator It; It; ++It )
					if( It->IsA(Class) )
						Results.AddItem( *It );
			}
			else if( !ParseObject<UPackage>( Item, TEXT("GROUP="), RealPackage, BasePackage ) )
			{
				// All objects beneath BasePackage.
				for( FObjectIterator It; It; ++It )
					if( It->IsA(Class) && It->IsIn(BasePackage) )
						Results.AddItem( *It );
			}
			else
			{
				// All objects within RealPackage.
				for( FObjectIterator It; It; ++It )
					if( It->IsA(Class) && It->IsIn(RealPackage) )
						Results.AddItem( *It );
			}
			for( INT i=0; i<Results.Num(); i++ )
			{
				if( i )
					Ar.Log( TEXT(" ") );
				Ar.Log( Results(i)->GetName() );
			}
		}
	}
	else if( ParseCommand(&Item,TEXT("PACKAGES")) )
	{
		UClass* Class;
		if( ParseObject<UClass>(Item,TEXT("CLASS="),Class,ANY_PACKAGE) )
		{
			TArray<UObject*> List;
			for( FObjectIterator It; It; ++It )
			{
				if( It->IsA(Class) && It->GetOuter()!=UObject::GetTransientPackage() )
				{
					check(It->GetOuter());
					for( UObject* TopParent=It->GetOuter(); TopParent->GetOuter()!=NULL; TopParent=TopParent->GetOuter() );
					if( Cast<UPackage>(TopParent) )
						List.AddUniqueItem( TopParent );
				}
			}
			for( INT i=0; i<List.Num(); i++ )
			{
				if( i )
					Ar.Log( TEXT(",") );
				Ar.Log( List(i)->GetName() );
			}
		}
	}
	else if( ParseCommand(&Item,TEXT("DELETE")) )
	{
		UPackage* Pkg=ANY_PACKAGE;
		UClass*   Class;
		UObject*  Object;
		ParseObject<UPackage>( Item, TEXT("PACKAGE="), Pkg, NULL );
		if
		(	!ParseObject<UClass>( Item,TEXT("CLASS="), Class, ANY_PACKAGE )
		||	!ParseObject(Item,TEXT("OBJECT="),Class,Object,Pkg) )
			Ar.Logf( TEXT("Object not found") );
		else if( UObject::IsReferenced( Object, RF_Native | RF_Public, 0 ) )
			Ar.Logf( TEXT("%s is in use"), Object->GetFullName() );
		else delete Object;
	}
	else if( ParseCommand(&Item,TEXT("GROUPS")) )
	{
		UClass* Class;
		UPackage* Pkg;
		if
		(	ParseObject<UPackage>(Item,TEXT("PACKAGE="),Pkg,NULL)
		&&	ParseObject<UClass>(Item,TEXT("CLASS="),Class,ANY_PACKAGE) )
		{
			TArray<UObject*> List;
			for( FObjectIterator It; It; ++It )
				if( It->IsA(Class) && It->GetOuter() && It->GetOuter()->GetOuter()==Pkg )
					List.AddUniqueItem( It->GetOuter() );
			for( INT i=0; i<List.Num(); i++ )
			{
				if( i )
					Ar.Log( TEXT(",") );
				Ar.Log( List(i)->GetName() );
			}
		}
	}
	else if( ParseCommand(&Item,TEXT("BROWSECLASS")) )
	{
		Ar.Log( GEditor->BrowseClass->GetName() );
	}
	unguard;
}
void ObjTopicHandler::Set( ULevel* Level, const TCHAR* Item, const TCHAR* Data )
{
	guard(ObjTopicHandler::Set);
	if( ParseCommand(&Item,TEXT("NOTECURRENT")) )
	{
		UClass* Class;
		UObject* Object;
		if
		(	GEditor->UseDest
		&&	ParseObject<UClass>( Data, TEXT("CLASS="), Class, ANY_PACKAGE )
		&&	ParseObject( Data, TEXT("OBJECT="), Class, Object, ANY_PACKAGE ) )
		{
			TCHAR Temp[256];
			appSprintf( Temp, TEXT("%s'%s'"), Object->GetClass()->GetName(), Object->GetName() );
			GEditor->UseDest->SetValue( Temp );
		}
	}
	unguard;
}
void UEditorEngine::NotifyExec( void* Src, const TCHAR* Cmd )
{
	guard(UEditorEngine::NotifyExec);
	if( ParseCommand(&Cmd,TEXT("BROWSECLASS")) )
	{
		ParseObject( Cmd, TEXT("CLASS="), BrowseClass, ANY_PACKAGE );
		UseDest = (WProperties*)Src;
		EdCallback( EDC_Browse, 1 );
	}
	else if( ParseCommand(&Cmd,TEXT("USECURRENT")) )
	{
		ParseObject( Cmd, TEXT("CLASS="), BrowseClass, ANY_PACKAGE );
		UseDest = (WProperties*)Src;
		EdCallback( EDC_UseCurrent, 1 );
	}
	unguard;
}
void UEditorEngine::UpdatePropertiesWindows()
{
	guard(UEditorEngine::UpdatePropertiesWindow);
	if( ActorProperties )
	{
		TArray<UObject*> SelectedActors;
		for( INT i=0; i<Level->Actors.Num(); i++ )
			if( Level->Actors(i) && Level->Actors(i)->bSelected )
				SelectedActors.AddItem( Level->Actors(i) );
		ActorProperties->Root.SetObjects( &SelectedActors(0), SelectedActors.Num() );
	}
	for( INT i=0; i<WProperties::PropertiesWindows.Num(); i++ )
	{
		WProperties* Properties=WProperties::PropertiesWindows(i);
		if( Properties!=ActorProperties && Properties!=Preferences )
			Properties->ForceRefresh();
	}
	unguard;
}
UBOOL UEditorEngine::HookExec( const TCHAR* Cmd, FOutputDevice& Ar )
{
	guard(UEditorEngine::HookExec);
	if( ParseCommand(&Cmd,TEXT("PLAYMAP")) )
	{
		TCHAR Parms[256];
		Exec( TEXT("MAP SAVE FILE=..\\Maps\\Autoplay.unr"), Ar );
		appSprintf( Parms, TEXT("Autoplay.unr HWND=%i %s"), (INT)hWndMain, GameCommandLine );
		appLaunchURL( GFileManager->FileSize(TEXT("Unreal.exe"))>0 ? TEXT("Unreal.exe") : TEXT("UnrealTournament.exe"), Parms );
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("APP")) )
	{
		if( ParseCommand(&Cmd,TEXT("SET")) )
		{
			Parse( Cmd, TEXT("PROGRESSBAR="),  Warn.hWndProgressBar );
			Parse( Cmd, TEXT("PROGRESSTEXT="), Warn.hWndProgressText );
			return 1;
		}
		else return 0;
	}
	else if( ParseCommand(&Cmd,TEXT("ACTORPROPERTIES")) )
	{
		if( !ActorProperties )
		{
			ActorProperties = new WObjectProperties( TEXT("ActorProperties"), CPF_Edit, TEXT(""), NULL, 1 );
			ActorProperties->OpenWindow( (HWND)hWndMain );
			ActorProperties->SetNotifyHook( GEditor );
		}
		UpdatePropertiesWindows();
		ActorProperties->Show(1);
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("PREFERENCES")) )
	{
		if( !Preferences )
		{
			Preferences = new WConfigProperties( TEXT("Preferences"), LocalizeGeneral("AdvancedOptionsTitle",TEXT("Window")) );
			Preferences->OpenWindow( (HWND)hWndMain );
			Preferences->SetNotifyHook( GEditor );
			Preferences->ForceRefresh();
		}
		Preferences->Show(1);
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("LEVELPROPERTIES")) )
	{
		if( !LevelProperties )
		{
			LevelProperties = new WObjectProperties( TEXT("LevelProperties"), CPF_Edit, TEXT("Level Properties"), NULL, 1 );
			LevelProperties->OpenWindow( (HWND)hWndMain );
			LevelProperties->SetNotifyHook( GEditor );
		}
		LevelProperties->Root.SetObjects( (UObject**)&Level->Actors(0), 1 );
		LevelProperties->Show(1);
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("TEXTUREPROPERTIES")) )
	{
		UTexture* Texture;
		if( ParseObject<UTexture>( Cmd, TEXT("TEXTURE="), Texture, ANY_PACKAGE ) )
		{
			TCHAR Title[256];
			appSprintf( Title, TEXT("Texture %s"), Texture->GetPathName() );
			WObjectProperties* TextureProperties = new WObjectProperties( TEXT("TextureProperties"), CPF_Edit, Title, NULL, 1 );
			TextureProperties->OpenWindow( (HWND)hWndMain );
			TextureProperties->Root.SetObjects( (UObject**)&Texture, 1 );
			TextureProperties->SetNotifyHook( GEditor );
			TextureProperties->Show(1);
		}
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("CLASSPROPERTIES")) )
	{
		UClass* Class;
		if( ParseObject<UClass>( Cmd, TEXT("Class="), Class, ANY_PACKAGE ) )
		{
			TCHAR Title[256];
			appSprintf( Title, TEXT("Default %s Properties"), Class->GetPathName() );
			WClassProperties* ClassProperties = new WClassProperties( TEXT("ClassProperties"), CPF_Edit, Title, Class );
			ClassProperties->OpenWindow( (HWND)hWndMain );
			ClassProperties->SetNotifyHook( GEditor );
			ClassProperties->ForceRefresh();
			ClassProperties->Show(1);
		}
		return 1;
	}
	return 0;
	unguard;
}

/*-----------------------------------------------------------------------------
	UnrealEd startup and VB hooks.
-----------------------------------------------------------------------------*/

LONG APIENTRY VbWindowProc( HWND hWnd, UINT Message, UINT wParam, LONG lParam )
{
	guard(VbWindowProc);
	if( GEditor && Message==WM_USER_EXEC )
	{
		guard(WM_USER_EXEC);
		GEditor->Exec( GCommand );
		free( GCommand );
		return 0;
		unguard;
	}
	else if( GEditor && Message==WM_USER_GETPROP )
	{
		guard(WM_USER_GETPROP);
		*GetPropResult = FStringOutputDevice();
		GEditor->Get( GTopic, GItem, *GetPropResult );
		return 0;
		unguard;
	}
	else if( GEditor && Message==WM_USER_SETPROP )
	{
		guard(WM_USER_SETPROP);
		GEditor->Set( GTopic, GItem, GValue );
		return 0;
		unguard;
	}
	else
	{
		guard(DefaultWindowProc);
		return DefWindowProcX( hWnd, Message, wParam, lParam );
		unguard;
	}
	unguard;
}

DWORD __stdcall ThreadEntry( void* Arg )
{
	try
	{
		// Create this thread's window.
		GIsStarted = 1;
		hWndEngine = TCHAR_CALL_OS(CreateWindow(TEXT("VbWindowProc"),TEXT("VbWindowProc"),0,0,0,0,0,NULL,NULL,hInstance,NULL),CreateWindowA("VbWindowProc","VbWindowProc",0,0,0,0,0,NULL,NULL,hInstance,NULL));
		check(hWndEngine);

		// Set mode.
		GIsClient = GIsServer = GIsEditor = GLazyLoad = 1;
		GIsScriptable = 0;

		// Init.
		GIsGuarded=1;
		appStrcpy( Log.Filename, TEXT("Editor.log") );
		appInit( TEXT("Unreal"), GetCommandLine(), &Malloc, &Log, &Error, &Warn, &FileManager, FConfigCacheIni::Factory, 1 );
		GetPropResult = new FStringOutputDevice;
		InitWindowing();

		// Init log window.
		GLogWindow = new WLog( Log.Filename, Log.LogAr, TEXT("Log") );
		GLogWindow->OpenWindow( 1, 0 );

		// Init engine.
		Engine = InitEngine();

		// Init input.
		UInput::StaticInitInput();

		// Message pump.
		SetEvent( hEngineThreadStarted );
		MainLoop( Engine );

		// Exit.
		GFileManager->Delete(TEXT("Running.ini"),0,0);
		GExec = NULL;
		appPreExit();
		if( GLogWindow )
			delete GLogWindow;
		GLogWindow = NULL;
		GLogHook = NULL;
		*GetPropResult = FStringOutputDevice();
		GIsGuarded = 0;
	}
	catch( ... )
	{
		Error.HandleError();
	}
	appExit();
	GIsStarted = 0;
	return 0;
}

void UEditorEngine::EdCallback( DWORD Code, UBOOL Send )
{
	guard(UEditorEngine::EdCallback);
	if( hWndCallback )
	{
		if( Send ) SendMessageX( (HWND)hWndCallback, WM_CHAR, 32+Code, 0 );
		else       PostMessageX( (HWND)hWndCallback, WM_CHAR, 32+Code, 0 );
	}
	unguard;
}

#define EDHOOK_TRY \
	try {

#define EDHOOK_CATCH \
	} catch( ... ) { \
		Error.HandleError(); \
		appRequestExit( 1 ); \
	}

extern "C"
{
	DLL_EXPORT void __stdcall EdInitServer( HWND hInWndMain, HWND hInWndCallback )
	{
		EDHOOK_TRY;

		hWndCallback = (DWORD)hInWndCallback;
		hWndMain     = (DWORD)hInWndMain;

#if UNICODE
		if( GUnicode && !GUnicodeOS )
		{
			WNDCLASSA Cls;
			memset( &Cls, 0, sizeof(Cls) );
			Cls.lpfnWndProc   = VbWindowProc;
			Cls.hInstance     = hInstance;
			Cls.lpszClassName = "VbWindowProc"; 
			verify(RegisterClassA( &Cls ));
		}
		else
#endif
		{
			WNDCLASS Cls;
			memset( &Cls, 0, sizeof(Cls) );
			Cls.lpfnWndProc   = VbWindowProc;
			Cls.hInstance     = hInstance;
			Cls.lpszClassName = TEXT("VbWindowProc"); 
			verify(RegisterClass( &Cls ));
		}
		hEngineThreadStarted = CreateEvent( NULL, 0, 0, NULL );
		hEngineThread        = CreateThread( NULL, 0, ThreadEntry, NULL, 0, &EngineThreadId );
		check(EngineThreadId!=-1);
		WaitForSingleObject( hEngineThreadStarted, INFINITE );

		EDHOOK_CATCH;
	}
	DLL_EXPORT void __stdcall EdExitServer()
	{
		EDHOOK_TRY;
		verify(TCHAR_CALL_OS(PostThreadMessage(EngineThreadId,WM_QUIT,0,0),PostThreadMessageA(EngineThreadId,WM_QUIT,0,0)));
		WaitForSingleObject( hEngineThread, INFINITE );
		EDHOOK_CATCH;
	}
	DLL_EXPORT void __stdcall EdExec( const ANSICHAR* Cmd )
	{
		EDHOOK_TRY;
		INT Len = strlen(Cmd);
		GCommand = (TCHAR*)malloc((Len+1)*sizeof(TCHAR));
		for( INT i=0; i<Len; i++ )
			GCommand[i] = FromAnsi(Cmd[i]);
		GCommand[Len]=0;
		SendMessageX( hWndEngine, WM_USER_EXEC, 0, 0 );
		EDHOOK_CATCH;
	}
	DLL_EXPORT void __stdcall EdSetProp( const ANSICHAR* Topic, const ANSICHAR* Item, const ANSICHAR* Value )
	{
		EDHOOK_TRY;
		GTopic = appFromAnsi(Topic);
		GItem  = appFromAnsi(Item );
#if UNICODE
		INT Count    = 1+strlen(Value);
		UNICHAR* UCh = (UNICHAR*)appAlloca((Count+1)*sizeof(TCHAR));
		for( INT i=0; i<Count; i++ )
			UCh[i] = ToUnicode( Value[i] );
		GValue = UCh;
#else
		GValue = Value;
#endif
		SendMessageX( hWndEngine, WM_USER_SETPROP, 0, 0 );
		EDHOOK_CATCH;
	}
	DLL_EXPORT BSTR __stdcall EdGetProp( const ANSICHAR* Topic, const ANSICHAR* Item )
	{
		EDHOOK_TRY;
		GTopic = appFromAnsi(Topic);
		GItem  = appFromAnsi(Item);
		SendMessageX( hWndEngine, WM_USER_GETPROP, 0, 0 );
#if UNICODE
		INT Count = GetPropResult->Len()+1;
		ANSICHAR* ACh = (ANSICHAR*)appAlloca(Count);
		for( INT i=0; i<Count; i++ )
			ACh[i] = ToAnsi((**GetPropResult)[i]);
		return SysAllocStringByteLen( ACh, Count-1 );
#else
		return SysAllocStringByteLen( **GetPropResult, GetPropResult->Len() );
#endif
		EDHOOK_CATCH
		return SysAllocStringByteLen( "", 0 );
	}
}

/*-----------------------------------------------------------------------------
	The end.
-----------------------------------------------------------------------------*/
