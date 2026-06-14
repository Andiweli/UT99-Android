/*=============================================================================
	DemoPlayPenLev.cpp: Unreal demo playback pending level class.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Jack Porter
=============================================================================*/

#include "EnginePrivate.h"
#include "UnNet.h"

/*-----------------------------------------------------------------------------
	UDemoPlayPendingLevel implementation.
-----------------------------------------------------------------------------*/

//
// Constructor.
//
UDemoPlayPendingLevel::UDemoPlayPendingLevel( UEngine* InEngine, const FURL& InURL )
:	UPendingLevel( InEngine, InURL )
{
	guard(UDemoPlayPendingLevel::UDemoPlayPendingLevel);

	// Try to create demo playback driver.
	UClass* DemoDriverClass = StaticLoadClass( UNetDriver::StaticClass(), NULL, TEXT("ini:Engine.Engine.DemoRecordingDevice"), NULL, LOAD_NoFail, NULL );
	DemoRecDriver = ConstructObject<UNetDriver>( DemoDriverClass );
	if( !DemoRecDriver->InitConnect( this, URL, Error ) )
	{
		delete DemoRecDriver;
		DemoRecDriver = NULL;
	}

	unguard;
}
//
// FNetworkNotify interface.
//
ULevel* UDemoPlayPendingLevel::NotifyGetLevel()
{
	guard(UDemoPlayPendingLevel::NotifyGetLevel);
	return NULL;
	unguard;
}
void UDemoPlayPendingLevel::NotifyReceivedText( UNetConnection* Connection, const TCHAR* Text )
{
	guard(UDemoPlayPendingLevel::NotifyReceivedText);
	debugf( NAME_DevNet, TEXT("DemoPlayPendingLevel received: %s"), Text );
	if( ParseCommand( &Text, TEXT("USES") ) )
	{
		// Dependency information.
		FPackageInfo& Info = *new(Connection->PackageMap->List)FPackageInfo(NULL);
		TCHAR PackageName[NAME_SIZE]=TEXT("");
		Parse( Text, TEXT("GUID="), Info.Guid );
		Parse( Text, TEXT("GEN=" ), Info.RemoteGeneration );
		Parse( Text, TEXT("SIZE="), Info.FileSize );
		Parse( Text, TEXT("FLAGS="), Info.PackageFlags );
		Parse( Text, TEXT("PKG="), PackageName, ARRAY_COUNT(PackageName) );
		Info.Parent = CreatePackage(NULL,PackageName);
	}
	else if( ParseCommand( &Text, TEXT("WELCOME") ) )
	{
		FURL URL;
	
		// Parse welcome message.
		Parse( Text, TEXT("LEVEL="), URL.Map );

		// Make sure all packages we need available
		for( INT i=0; i<Connection->PackageMap->List.Num(); i++ )
		{
			TCHAR Filename[256];

			FPackageInfo& Info = Connection->PackageMap->List(i);
			if( !appFindPackageFile( Info.Parent->GetName(), &Info.Guid, Filename ) )
			{
				debugf(TEXT("Don't have package for demo: %s"), Info.Parent->GetName() );//!!localize!!
				return;
			}
		}

		FString ServerDemo;
		if( Parse( Text, TEXT("SERVERDEMO"), ServerDemo ) )
			CastChecked<UDemoRecDriver>(DemoRecDriver)->ClientThirdPerson = 1;

		DemoRecDriver->Time = 0;
		Success = 1;
	}
	unguard;
}
//
// UPendingLevel interface.
//
void UDemoPlayPendingLevel::Tick( FLOAT DeltaTime )
{
	guard(UDemoPlayPendingLevel::Tick);
	check(DemoRecDriver);
	check(DemoRecDriver->ServerConnection);

	// Update demo recording driver.
	DemoRecDriver->TickDispatch( DeltaTime );
	DemoRecDriver->TickFlush();

	unguard;
}
IMPLEMENT_CLASS(UDemoPlayPendingLevel);

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
