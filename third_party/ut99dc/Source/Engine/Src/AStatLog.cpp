/*=============================================================================
	AStatLog.cpp: Unreal Tournament stat logging.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#include "EnginePrivate.h"

/*-----------------------------------------------------------------------------
	Stat Log Implementation.
-----------------------------------------------------------------------------*/

#if ENGINE_VERSION>=230
IMPLEMENT_CLASS(AMutator);
#endif

void AStatLog::execExecuteLocalLogBatcher( FFrame& Stack, RESULT_DECL )
{
	guard(AStatLog::execExecuteLocalLogBatcher);
	P_FINISH;

	appCreateProc( *LocalBatcherURL, *Level->Game->LocalLogFileName );

	unguardexec;
}

void AStatLog::execExecuteSilentLogBatcher( FFrame& Stack, RESULT_DECL )
{
	guard(AStatLog::execExecuteSilentLogBatcher);
	P_FINISH;

	FString ProcArgs = FString::Printf( TEXT("-b false %s"), *Level->Game->LocalLogFileName );
	appCreateProc( *LocalBatcherURL, *ProcArgs );

	unguardexec;
}

void AStatLog::execBatchLocal( FFrame& Stack, RESULT_DECL )
{
	guard(AStatLog::execBatchLocal);
	P_FINISH;

	appCreateProc( *(((AStatLog*)GetClass()->GetDefaultObject())->LocalBatcherURL), *(((AStatLog*)GetClass()->GetDefaultObject())->LocalLogDir) );
	unguardexec;
}

void AStatLog::execBrowseRelativeLocalURL( FFrame& Stack, RESULT_DECL )
{
	guard(AStatLog::execBrowseRelativeLocalURL);
	P_GET_STR(URL);
	P_FINISH;

	appLaunchURL( *(GFileManager->GetDefaultDirectory() * URL) );

	unguardexec;
}

void AStatLog::execExecuteWorldLogBatcher( FFrame& Stack, RESULT_DECL )
{
	guard(AStatLog::execExecuteWorldLogBatcher);
	P_FINISH;

	appCreateProc( *WorldBatcherURL, *WorldBatcherParams );

	unguardexec;
}


void AStatLog::execInitialCheck( FFrame& Stack, RESULT_DECL )
{
	guard(AStatLog::execInitialCheck);
	P_GET_OBJECT(AGameInfo, Game);
	P_FINISH;

	// Log the class in C++ to avoid trickery.
	eventLogGameSpecial(TEXT("GameClass"), Game->GetClass()->GetFullName());

	// Log all the loaded code packages and their checksums.
	TArray<UPackage*> Packages;
	for( TObjectIterator<UClass> It; It; ++It )
		Packages.AddUniqueItem(CastChecked<UPackage>((*It)->GetOuter()));
	for (INT i=0; i<Packages.Num(); i++)
	{
		// Get checksum values.
		FString FileName = FString::Printf( TEXT("%s.u"), Packages(i)->GetFullName() );
		INT Space = FileName.InStr(TEXT(" "));
		FileName = FileName.Right( FileName.Len() - (Space+1) );
		INT FileSize = GFileManager->FileSize( *FileName );

		// Promote lowercase character values (a cool way of saying CAPITALIZE)
		FString CapsName;
		for (INT j=0; j<FileName.Len(); j++)
		{
			TCHAR c = (*FileName)[j];
			if ((c >= 'a') && (c <= 'z'))
				c = c + ('A' - 'a');
			CapsName += FString::Printf( TEXT("%c"), c );
		}

		// Checksum the .u files.
		FString CheckString = CapsName + FString::Printf( TEXT("%i"), FileSize );
		if (FileSize != -1)
		{
			FMD5Context PContext;
			appMD5Init( &PContext );
			appMD5Update( &PContext, (BYTE*) *CheckString, CheckString.Len() * sizeof(TCHAR) );
			BYTE Digest[16];
			appMD5Final( Digest, &PContext );
			FString Checksum;
			for (INT j=0; j<16; j++)
				Checksum += FString::Printf(TEXT("%02x"), Digest[j]);
			eventLogGameSpecial2(TEXT("CodePackageChecksum"), *FileName, *Checksum);
		}

		// Get checksum values.
		FileName = FString::Printf( TEXT("%s%s"), Packages(i)->GetFullName(), DLLEXT );
		Space = FileName.InStr(TEXT(" "));
		FileName = FileName.Right( FileName.Len() - (Space+1) );
		FileSize = GFileManager->FileSize( *FileName );

		// Capitalize.
		for (INT j=0; j<FileName.Len(); j++)
		{
			TCHAR c = (*FileName)[j];
			if ((c >= 'a') && (c <= 'z'))
				c = c + ('A' - 'a');
			CapsName += FString::Printf( TEXT("%c"), c );
		}

		// Checksum the .dll files.
		CheckString = CapsName + FString::Printf( TEXT("%i"), FileSize );
		if (FileSize != -1)
		{
			FMD5Context PContext;
			appMD5Init( &PContext );
			appMD5Update( &PContext, (BYTE*) *CheckString, CheckString.Len() * sizeof(TCHAR) );
			BYTE Digest[16];
			appMD5Final( Digest, &PContext );
			FString Checksum;
			for (INT j=0; j<16; j++)
				Checksum += FString::Printf(TEXT("%02x"), Digest[j]);
			eventLogGameSpecial2(TEXT("CodePackageChecksum"), *FileName, *Checksum);
		}
	}

	unguardexec;
}

void AStatLog::execLogMutator( FFrame& Stack, RESULT_DECL )
{
	guard(AStatLog::execInitialCheck);
	P_GET_OBJECT(AMutator, M);
	P_FINISH;

	eventLogGameSpecial(TEXT("GameMutator"), M->GetClass()->GetFullName());

	unguardexec;
}

void AStatLog::execGetGMTRef( FFrame& Stack, RESULT_DECL )
{
	guard(AStatLog::execGetGMTRef);
	P_FINISH;

	*(FString*)Result = appGetGMTRef();

	unguardexec;
}

void AStatLog::execGetMapFileName( FFrame& Stack, RESULT_DECL )
{
	guard(AStatLog::execGetMapFileName);
	P_FINISH;

	*(FString*)Result = XLevel->URL.Map;

	unguardexec;
}

void AStatLog::execGetPlayerChecksum( FFrame& Stack, RESULT_DECL )
{
	guard(AStatLog::execGetPlayerChecksum);
	P_GET_OBJECT(APlayerPawn, P);
	P_GET_STR_REF(Checksum);
	P_FINISH;

	FMD5Context PContext;
	appMD5Init( &PContext );
	appMD5Update( &PContext, (BYTE*)*(P->PlayerReplicationInfo->PlayerName), P->PlayerReplicationInfo->PlayerName.Len()*sizeof(TCHAR) );
	appMD5Update( &PContext, (BYTE*)*(P->ngWorldSecret), P->ngWorldSecret.Len()*sizeof(TCHAR) );
	BYTE Digest[16];
	appMD5Final( Digest, &PContext );
	*Checksum = FString::Printf( TEXT("") );
	for (INT i=0; i<16; i++)
		*Checksum += FString::Printf(TEXT("%02x"), Digest[i]);

	unguardexec;
}

void AStatLogFile::execOpenLog( FFrame& Stack, RESULT_DECL )
{
	guard(AStatLogFile::execOpenLog);
	P_FINISH;

	GFileManager->MakeDirectory( TEXT("..") PATH_SEPARATOR TEXT("Logs") );
	LogAr = (INT) GFileManager->CreateFileWriter( *StatLogFile, FILEWRITE_EvenIfReadOnly );
	if( bWorld )
	{
		Context = (INT) new FMD5Context;
		appMD5Init((FMD5Context*) Context);
	}
	unguardexec;
}

void AStatLogFile::execCloseLog( FFrame& Stack, RESULT_DECL )
{
	guard(AStatLogFile::execCloseLog);
	P_FINISH;

	if( Context )
		delete (FMD5Context*)Context;
	Context = 0;

	if( LogAr )
		delete (FArchive*)LogAr;
	LogAr = 0;

	GFileManager->Move( *StatLogFinal, *StatLogFile, 1, 1, 1 );

	unguardexec;
}

void AStatLogFile::execWatermark( FFrame& Stack, RESULT_DECL )
{
	guard(AStatLogFile::execWatermark);
	P_GET_STR(EventString);
	P_FINISH;

	// Update the context...
	EventString += TEXT("\r\n");
	appMD5Update( (FMD5Context*) Context, (BYTE*)*EventString, EventString.Len()*sizeof(TCHAR) );

	unguardexec;
}

void AStatLogFile::execGetChecksum( FFrame& Stack, RESULT_DECL )
{
	guard(AStatLogFile::execGetChecksum);
	P_GET_STR_REF(Checksum);
	P_FINISH;

	BYTE Secret[16];	// Must be bytes.  Used by MD5.
	Secret[0] = 'M';
	Secret[5] = 'p';
	Secret[2] = 'y';
	Secret[3] = 'f';
	Secret[1] = '4';
	Secret[11] = 'd';
	Secret[7] = '9';
	Secret[4] = 'G';
	Secret[12] = 'D';
	Secret[6] = '6';
	Secret[9] = 'e';
	Secret[10] = 'J';
	Secret[14] = '1';
	Secret[15] = 'q';
	Secret[8] = 'k';
	Secret[13] = 'V';

	BYTE Digest[16];

	appMD5Update( (FMD5Context*) Context, Secret, 16 );
	appMD5Final( Digest, (FMD5Context*) Context ); // Outputs a 16 byte digest.

	// Copy each byte into a string of arbitrary character size. (UNICODE safe.)
	INT i;
	for (i=0; i<16; i++) {
		*Checksum += FString::Printf(TEXT("%02x"), Digest[i]);
	}

	unguardexec;
}

void AStatLogFile::execFileFlush( FFrame& Stack, RESULT_DECL )
{
	guard(AStatLogFile::execFileFlush);
	P_FINISH;

	if( LogAr )
		((FArchive*)LogAr)->Flush();

	unguardexec;
}

void AStatLogFile::execFileLog( FFrame& Stack, RESULT_DECL )
{
	guard(AStatLogFile::execFileLog);
	P_GET_STR(EventString);
	P_FINISH;

	#if _MSC_VER
	FString LogString = EventString + TEXT("\r\n");
	if( bWorld )
	{
		FString EncodedString;
		for( INT i=0; i<LogString.Len(); i++ )
		{
			TCHAR c = 0;
			BYTE* a = (BYTE*) &(*LogString)[i];
			BYTE* b = (BYTE*) &c;
			for( INT j=0; j<sizeof(TCHAR); j++ )
				b[j] = a[j] ^ 0xa7;
			EncodedString += FString::Printf(TEXT("%c"), c);
		}
		if ( LogAr )
			((FArchive*)LogAr)->Serialize( const_cast<TCHAR*>(*EncodedString), EncodedString.Len()*sizeof(TCHAR) );
	} else {
		if( LogAr )
			((FArchive*)LogAr)->Serialize( const_cast<TCHAR*>(*LogString), LogString.Len()*sizeof(TCHAR) );
	}
	#else
	// Emulate UNICODE for Linux.
	TCHAR* LogString = (TCHAR*) appMalloc( EventString.Len() * 2 + 4, TEXT("Temporary String Memory") );
	for( INT i=0; i<EventString.Len(); i++ )
	{
		LogString[i*2] = (*EventString)[i];
		LogString[i*2 + 1] = 0;
	}
	LogString[ EventString.Len()*2 ] = '\r';
	LogString[ EventString.Len()*2 + 1 ] = 0;
	LogString[ EventString.Len()*2 + 2 ] = '\n';
	LogString[ EventString.Len()*2 + 3 ] = 0;
	if( bWorld )
	{
		TCHAR* EncodedString = (TCHAR*) appMalloc( EventString.Len() * 2 + 4, TEXT("Temporary String Memory") );
		for( INT i=0; i<EventString.Len()*2 + 4; i++ )
		{
			TCHAR c = 0;
			BYTE* a = (BYTE*) &(LogString[i]);
			BYTE* b = (BYTE*) &c;
			for( INT j=0; j<sizeof(TCHAR); j++ )
				b[j] = a[j] ^ 0xa7;
			EncodedString[i] = c;
		}
		if ( LogAr )
			((FArchive*)LogAr)->Serialize( const_cast<TCHAR*>(EncodedString), EventString.Len() * 2 + 4 );
		appFree( EncodedString );
	} else {
		if( LogAr )
			((FArchive*)LogAr)->Serialize( const_cast<TCHAR*>(LogString), EventString.Len() * 2 + 4 );
	}
	appFree( LogString );
	#endif

	unguardexec;
}

/*-----------------------------------------------------------------------------
	The end.
-----------------------------------------------------------------------------*/
