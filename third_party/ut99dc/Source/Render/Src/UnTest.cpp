/*=============================================================================
	UnTest.cpp: File for testing optimizations. Check out the VC++ 4.0 
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "RenderPrivate.h"

int g_int1, g_int2, g_int3;
char g_char1, g_char2, g_char3;
short g_short1, g_short2, g_short3;
QWORD g_q1, g_q2;

FVector V(2,3,4);

class DLL_EXPORT myclass
{
public:
	virtual void __stdcall xyzzy( const FString& Str, int b )
	{
		g_int1 = b;
	}
	virtual void timtim()
	{
		xyzzy( TEXT("abc"), 456 );
	}
};

/*------------------------------------------------------------------------------
	The End.
------------------------------------------------------------------------------*/
