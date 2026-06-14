/*=============================================================================
	UnNetDrv.cpp: Unreal network driver base class.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#include "EnginePrivate.h"
#include "UnNet.h"

/*-----------------------------------------------------------------------------
	UPackageMapLevel implementation.
-----------------------------------------------------------------------------*/

UBOOL UPackageMapLevel::CanSerializeObject( UObject* Obj )
{
	guardSlow(UPackageMapLevel::CanSerializeObject);
	AActor* Actor = Cast<AActor>(Obj);
	if( Actor && !Actor->bStatic && !Actor->bNoDelete )
	{
		UActorChannel* Ch = Connection->ActorChannels.FindRef(Actor);
		//old: induces a bit of lag. return Ch && Ch->OpenAcked;
		return Ch!=NULL; //new: reduces lag, increases bandwidth slightly.
	}
	else return 1;
	unguardSlow;
}
UBOOL UPackageMapLevel::SerializeObject( FArchive& Ar, UClass* Class, UObject*& Object )
{
	guard(UPackageMapLevel::SerializeObject);
	DWORD Index=0;
	if( Ar.IsLoading() )
	{
		Object = NULL;
		BYTE B=0; Ar.SerializeBits( &B, 1 );
		if( B )
		{
			// Dynamic actor or None.
			Ar.SerializeInt( Index, UNetConnection::MAX_CHANNELS );
			if( Index==0 )
			{
				Object = NULL;
			}
			else if
			(	!Ar.IsError()
			&&	Index>=0
			&&	Index<UNetConnection::MAX_CHANNELS
			&&	Connection->Channels[Index]
			&&	Connection->Channels[Index]->ChType==CHTYPE_Actor 
			&&	!Connection->Channels[Index]->Closing )
				Object = ((UActorChannel*)Connection->Channels[Index])->GetActor();
		}
		else
		{
			// Static object.
			Ar.SerializeInt( Index, MaxObjectIndex );
			if( !Ar.IsError() )
				Object = IndexToObject( Index, 1 );
		}
		if( Object && !Object->IsA(Class) )
		{
			debugf(TEXT("Forged object: got %s, expecting %s"),Object->GetFullName(),Class->GetFullName());
			Object = NULL;
		}
		return 1;
	}
	else
	{
		AActor* Actor = Cast<AActor>(Object);
		if( Actor && !Actor->bStatic && !Actor->bNoDelete )
		{
			// Map dynamic actor through channel index.
			BYTE B=1; Ar.SerializeBits( &B, 1 );
			UActorChannel* Ch = Connection->ActorChannels.FindRef(Actor);
			UBOOL Mapped = 0;
			if( Ch )
			{
				Index  = Ch->ChIndex;
				Mapped = Ch->OpenAcked;
			}
			Ar.SerializeInt( Index, UNetConnection::MAX_CHANNELS );
			return Mapped;
		}
		else if( !Object || (Index=ObjectToIndex(Object))==INDEX_NONE )
		{
			BYTE B=1; Ar.SerializeBits( &B, 1 );
			Ar.SerializeInt( Index, UNetConnection::MAX_CHANNELS );
			return 1;
		}
		else
		{
			// Map regular object.
			// Since mappability doesn't change dynamically, there is no advantage to setting Result!=0.
			BYTE B=0; Ar.SerializeBits( &B, 1 );
			Ar.SerializeInt( Index, MaxObjectIndex );
			return 1;
		}
	}
	unguardf(( TEXT("(%s %s)"), Class->GetFullName(), Object->GetFullName() ));
}
IMPLEMENT_CLASS(UPackageMapLevel);

/*-----------------------------------------------------------------------------
	UNetDriver implementation.
-----------------------------------------------------------------------------*/

UNetDriver::UNetDriver()
:	ClientConnections()
,	Time( 0.0 )
{
	guard(UNetDriver::UNetDriver);
	RoleProperty       = FindObjectChecked<UProperty>( AActor::StaticClass(), TEXT("Role"      ) );
	RemoteRoleProperty = FindObjectChecked<UProperty>( AActor::StaticClass(), TEXT("RemoteRole") );
	MasterMap          = new UPackageMap;
	ProfileStats	   = ParseParam(appCmdLine(),TEXT("profilestats"));
	unguard;
}
void UNetDriver::StaticConstructor()
{
	guard(UNetDriver::StaticConstructor);

	// Expose CPF_Config properties to be loaded from .ini.
	new(GetClass(),TEXT("ConnectionTimeout"),    RF_Public)UFloatProperty(CPP_PROPERTY(ConnectionTimeout    ), TEXT("Client"), CPF_Config );
	new(GetClass(),TEXT("InitialConnectTimeout"),RF_Public)UFloatProperty(CPP_PROPERTY(InitialConnectTimeout), TEXT("Client"), CPF_Config );
	new(GetClass(),TEXT("KeepAliveTime"),        RF_Public)UFloatProperty(CPP_PROPERTY(KeepAliveTime        ), TEXT("Client"), CPF_Config );
	new(GetClass(),TEXT("RelevantTimeout"),      RF_Public)UFloatProperty(CPP_PROPERTY(RelevantTimeout      ), TEXT("Client"), CPF_Config );
	new(GetClass(),TEXT("SpawnPrioritySeconds"), RF_Public)UFloatProperty(CPP_PROPERTY(SpawnPrioritySeconds ), TEXT("Client"), CPF_Config );
	new(GetClass(),TEXT("ServerTravelPause"),    RF_Public)UFloatProperty(CPP_PROPERTY(ServerTravelPause    ), TEXT("Client"), CPF_Config );
	new(GetClass(),TEXT("MaxClientRate"),		 RF_Public)UIntProperty  (CPP_PROPERTY(MaxClientRate        ), TEXT("Client"), CPF_Config );
	new(GetClass(),TEXT("NetServerMaxTickRate"), RF_Public)UIntProperty  (CPP_PROPERTY(NetServerMaxTickRate ), TEXT("Client"), CPF_Config );
	new(GetClass(),TEXT("LanServerMaxTickRate"), RF_Public)UIntProperty  (CPP_PROPERTY(LanServerMaxTickRate ), TEXT("Client"), CPF_Config );
	new(GetClass(),TEXT("AllowDownloads"),       RF_Public)UBoolProperty (CPP_PROPERTY(AllowDownloads       ), TEXT("Client"), CPF_Config );

	// Default values.
	MaxClientRate = 25000;

	unguard;
}
void UNetDriver::AssertValid()
{
	guard(UNetDriver::AssertValid);
	unguard;
}
void UNetDriver::TickFlush()
{
	guard(UNetDriver::TickFlush);

	// Poll all sockets.
	if( ServerConnection )
		ServerConnection->Tick();
	for( INT i=0; i<ClientConnections.Num(); i++ )
		ClientConnections(i)->Tick();

	unguard;
}
UBOOL UNetDriver::InitConnect( FNetworkNotify* InNotify, FURL& URL, FString& Error )
{
	guard(UNetDriver::Init);
	Notify = InNotify;
	return 1;
	unguard;
}
UBOOL UNetDriver::InitListen( FNetworkNotify* InNotify, FURL& URL, FString& Error )
{
	guard(UNetDriver::Init);
	Notify = InNotify;
	return 1;
	unguard;
}
void UNetDriver::TickDispatch( FLOAT DeltaTime )
{
	guard(UNetDriver::TickDispatch);
	SendCycles=RecvCycles=0;

	// Get new time.
	Time += DeltaTime;

	// Delete any straggler connections.
	if( !ServerConnection )
		for( INT i=ClientConnections.Num()-1; i>=0; i-- )
			if( ClientConnections(i)->State==USOCK_Closed )
				delete ClientConnections(i);

	unguard;
}
void UNetDriver::Serialize( FArchive& Ar )
{
	guard(UNetDriver::Serialize);
	Super::Serialize( Ar );

	// Prevent referenced objects from being garbage collected.
	Ar << ClientConnections << ServerConnection << MasterMap << RoleProperty << RemoteRoleProperty;

	unguard;
}
void UNetDriver::Destroy()
{
	guard(UNetDriver::Destroy);

	// Destroy server connection.
	guard(DestroyServerConnection);
	if( ServerConnection )
		delete ServerConnection;
	unguard;

	// Destroy client connections.
	guard(DestroyClientConnections);
	while( ClientConnections.Num() )
		delete ClientConnections( 0 );
	unguard;

	// Low level destroy.
	LowLevelDestroy();

	// Delete the master package map.
	delete MasterMap;

	Super::Destroy();
	unguard;
}
UBOOL UNetDriver::Exec( const TCHAR* Cmd, FOutputDevice& Ar )
{
	guard(UNetDriver::Exec);
	if( ParseCommand(&Cmd,TEXT("SOCKETS")) )
	{
		// Print list of open connections.
		Ar.Logf( TEXT("Connections:") );
		if( ServerConnection )
		{
			Ar.Logf( TEXT("   Server %s"), *ServerConnection->LowLevelDescribe() );
			for( INT i=0; i<ServerConnection->OpenChannels.Num(); i++ )
				Ar.Logf( TEXT("      Channel %i: %s"), ServerConnection->OpenChannels(i)->ChIndex, *ServerConnection->OpenChannels(i)->Describe() );
		}
		for( INT i=0; i<ClientConnections.Num(); i++ )
		{
			UNetConnection* Connection = ClientConnections(i);
			Ar.Logf( TEXT("   Client %s"), *Connection->LowLevelDescribe() );
			for( INT i=0; i<Connection->OpenChannels.Num(); i++ )
				Ar.Logf( TEXT("      Channel %i: %s"), Connection->OpenChannels(i)->ChIndex, *Connection->OpenChannels(i)->Describe() );
		}
		return 1;
	}
	else return 0;
	unguard;
}
void UNetDriver::NotifyActorDestroyed( AActor* ThisActor )
{
	guard(UNetDriver::NotifyActorDestroyed);
	for( INT i=ClientConnections.Num()-1; i>=0; i-- )
	{
		UNetConnection* Connection = ClientConnections(i);
		if( ThisActor->bNetTemporary )
			Connection->SentTemporaries.RemoveItem( ThisActor );
		UActorChannel* Channel = Connection->ActorChannels.FindRef(ThisActor);
		if( Channel )
		{
			check(Channel->OpenedLocally);
			Channel->Close();
		}
	}
	unguard;
}
IMPLEMENT_CLASS(UNetDriver);

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
