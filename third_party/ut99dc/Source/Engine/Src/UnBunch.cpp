/*=============================================================================
	UnBunch.h: Unreal bunch (sub-packet) functions.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#include "EnginePrivate.h"
#include "UnNet.h"

/*-----------------------------------------------------------------------------
	FInBunch implementation.
-----------------------------------------------------------------------------*/

//
// Read an object.
//
FArchive& FInBunch::operator<<( UObject*& Object )
{
	guard(FInBunch<<UObject);
	Connection->PackageMap->SerializeObject( *this, UObject::StaticClass(), Object );
	return *this;
	unguard;
}

//
// Read a name.
//
FArchive& FInBunch::operator<<( class FName& N )
{
	guard(FInBunch<<FName);
	Connection->PackageMap->SerializeName( *this, N );
	return *this;
	unguard;
}

/*-----------------------------------------------------------------------------
	FOutBunch implementation.
-----------------------------------------------------------------------------*/

//
// Construct an outgoing bunch for a channel.
// It is ok to either send or discard an FOutbunch after construction.
//
FOutBunch::FOutBunch()
: FBitWriter( 0 )
{}
FOutBunch::FOutBunch( UChannel* InChannel, UBOOL bInClose )
:	FBitWriter	( InChannel->Connection->MaxPacket*8-MAX_BUNCH_HEADER_BITS-MAX_PACKET_TRAILER_BITS-MAX_PACKET_HEADER_BITS )
,	Channel		( InChannel )
,	ChIndex     ( InChannel->ChIndex )
,	ChType      ( InChannel->ChType )
,	bOpen		( 0 )
,	bClose		( bInClose )
,	bReliable	( 0 )
{
	guard(FOutBunch::FOutBunch);
	check(!Channel->Closing);
	check(Channel->Connection->Channels[Channel->ChIndex]==Channel);

	// Reserve channel and set bunch info.
	if( Channel->NumOutRec >= RELIABLE_BUFFER-1+bClose )
	{
		SetOverflowed();
		return;
	}

	unguard;
}

//
// Write a name.
//
FArchive& FOutBunch::operator<<( class FName& N )
{
	guard(FOutBunch<<FName);
	Channel->Connection->PackageMap->SerializeName( *this, N );
	return *this;
	unguard;
}

//
// Write an object.
//
FArchive& FOutBunch::operator<<( UObject*& Object )
{
	guard(FOutBunch<<UObject);
	Channel->Connection->PackageMap->SerializeObject( *this, UObject::StaticClass(), Object );
	return *this;
	unguard;
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
