/*=============================================================================
	WebServer.cpp: Unreal Web Server
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Jack Porter
=============================================================================*/

#include "UWebPrivate.h"

/*-----------------------------------------------------------------------------
	Declarations.
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
	UWebRequest functions.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UWebRequest);

//
// Decode a base64 encoded string - used for HTTP authentication
//
void UWebRequest::execDecodeBase64( FFrame& Stack, RESULT_DECL )
{
	guard(UWebRequest::execDecodeBase64);
	P_GET_STR(Encoded);
	P_FINISH;

	TCHAR *Decoded = (TCHAR *)appAlloca((Encoded.Len() / 4 * 3 + 1) * sizeof(TCHAR));
	check(Decoded);

	FString Base64Map(TEXT("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"));
	INT ch, i=0, j=0;
	TCHAR Junk[2] = {0, 0};
	TCHAR *Current = (TCHAR *)*Encoded;

    while((ch = (INT)(*Current++)) != '\0')
	{
		if (ch == '=')
			break;

		Junk[0] = ch;
		ch = Base64Map.InStr(FString(Junk));
		if( ch == -1 )
		{
			*(FString*)Result = FString(TEXT(""));
			return;
		}

		switch(i % 4) {
		case 0:
			Decoded[j] = ch << 2;
			break;
		case 1:
			Decoded[j++] |= ch >> 4;
			Decoded[j] = (ch & 0x0f) << 4;
			break;
		case 2:
			Decoded[j++] |= ch >>2;
			Decoded[j] = (ch & 0x03) << 6;
			break;
		case 3:
			Decoded[j++] |= ch;
			break;
		}
		i++;
	}

    /* clean up if we ended on a boundary */
    if (ch == '=') 
	{
		switch(i % 4)
		{
		case 0:
		case 1:
			*(FString*)Result = FString(TEXT(""));
			return;
		case 2:
			j++;
		case 3:
			Decoded[j++] = 0;
		}
	}
	Decoded[j] = '\0';
	*(FString*)Result = FString(Decoded);
	unguard;
}
IMPLEMENT_FUNCTION( UWebRequest, INDEX_NONE, execDecodeBase64 );

void UWebRequest::execAddVariable( FFrame& Stack, RESULT_DECL )
{
	guard(WebRequest::execAddVariable);
	P_GET_STR(VariableName);
	P_GET_STR(Value);	
	P_FINISH;
	VariableMap.Add(*(VariableName.Caps()), *Value);
	unguard;
}
IMPLEMENT_FUNCTION( UWebRequest, INDEX_NONE, execAddVariable );

void UWebRequest::execGetVariable( FFrame& Stack, RESULT_DECL )
{
	guard(WebRequest::execGetVariable);
	P_GET_STR(VariableName);
	P_GET_STR_OPTX(DefaultValue, TEXT(""));
	P_FINISH;
	FString *S = VariableMap.Find(VariableName.Caps());
	if(S)
		*(FString*)Result = *S;
	else
		*(FString*)Result = DefaultValue;
	unguard;
}
IMPLEMENT_FUNCTION( UWebRequest, INDEX_NONE, execGetVariable );

void UWebRequest::execGetVariableCount( FFrame& Stack, RESULT_DECL )
{
	guard(WebRequest::execGetVariableCount);
	P_GET_STR(VariableName);
	P_FINISH;

	TArray<FString> List;
	VariableMap.MultiFind( VariableName.Caps(), List );
	*(INT *)Result = List.Num();
	unguard;
}
IMPLEMENT_FUNCTION( UWebRequest, INDEX_NONE, execGetVariableCount );

void UWebRequest::execGetVariableNumber( FFrame& Stack, RESULT_DECL )
{
	guard(UWebRequest::execGetVariableNumber);
	P_GET_STR(VariableName);
	P_GET_INT(Number);
	P_GET_STR_OPTX(DefaultValue, TEXT(""));
	P_FINISH;

	TArray<FString> List;
	VariableMap.MultiFind( VariableName.Caps(), List );
	if(Number >= List.Num())
		*(FString*)Result = DefaultValue;
	else
		*(FString*)Result = List(Number);
	unguard;
}
IMPLEMENT_FUNCTION( UWebRequest, INDEX_NONE, execGetVariableNumber );

/*-----------------------------------------------------------------------------
	UWebResponse functions.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UWebResponse);

#define UHTMPACKETSIZE 512
void UWebResponse::SendInParts( const FString &S )
{
	guard(UWebResponse::SendInParts);
	INT Pos = 0, L;
	L = S.Len();

	if(L <= UHTMPACKETSIZE)
	{
		if(L > 0)
			eventSendText(S, 1);
		return;
	}
	while( L - Pos > UHTMPACKETSIZE )
	{
		eventSendText(S.Mid(Pos, UHTMPACKETSIZE), 1);
		Pos += UHTMPACKETSIZE;
	}
	if(Pos > 0)
		eventSendText(S.Mid(Pos), 1);
	unguard;
}

void UWebResponse::execIncludeBinaryFile( FFrame& Stack, RESULT_DECL )
{
	guard(UWebResponse::execIncludeBinaryFile);
	P_GET_STR(Filename);
	P_FINISH;
	if( Filename.Left(1) == TEXT("\\") || Filename.InStr(TEXT("..\\")) != -1 ||
		Filename.Left(1) == TEXT("/") || Filename.InStr(TEXT("../")) != -1)
	{
		debugf( NAME_Log, TEXT("WebServer: Dangerous characters in filename: %s"), *Filename );//!!localize!!
		return;
	}
	if( IncludePath == TEXT("") )
	{
		debugf( NAME_Log, TEXT("WebServer: Bad IncludePath: %s"), *IncludePath);//!!localize!!
		return;
	}
	TArray<BYTE> Data;
	if( !appLoadFileToArray( Data, *(IncludePath + PATH_SEPARATOR + Filename)) )
	{
		debugf( NAME_Log, TEXT("WebServer: Unable to open include file %s%s%s"), *IncludePath, PATH_SEPARATOR, *Filename );//!!localize!!
		return;
	}
	for( INT i=0; i<Data.Num(); i += 255)
		eventSendBinary( Min<INT>(Data.Num()-i, 255), &Data(i) );

	unguard;
}
IMPLEMENT_FUNCTION( UWebResponse, INDEX_NONE, execIncludeBinaryFile );

void UWebResponse::execIncludeUHTM( FFrame& Stack, RESULT_DECL )
{
	guard(UWebResponse::execIncludeUHTM);
	P_GET_STR(Filename);
	P_FINISH;

	if( Filename.Left(1) == TEXT("\\") || Filename.InStr(TEXT("..\\")) != -1 ||
		Filename.Left(1) == TEXT("/") || Filename.InStr(TEXT("../")) != -1)
	{
		debugf( NAME_Log, TEXT("WebServer: Dangerous characters in filename: %s"), *Filename );//!!localize!!
		return;
	}
	if( IncludePath == TEXT("") )
	{
		debugf( NAME_Log, TEXT("WebServer: Bad IncludePath: %s"), *IncludePath);//!!localize!!
		return;
	}
	FString Text;
	if( !appLoadFileToString( Text, *(IncludePath + PATH_SEPARATOR + Filename)) )
	{
		debugf( NAME_Log, TEXT("WebServer: Unable to open include file %s%s%s"), *IncludePath, PATH_SEPARATOR, *Filename );//!!localize!!
		return;
	}

	INT Pos = 0;
	TCHAR* T = const_cast<TCHAR*>( *Text );
	TCHAR* P;
	while( (P = appStrchr(T, '%')) != NULL)
	{
		SendInParts( Text.Mid(Pos, (P - T)) );
		Pos += (P - T);
		T = P;

		guard(FindClosing);
		// Find the close percentage
		TCHAR *PEnd = appStrchr(P+1, '%');
		if(PEnd)
		{
			guard(PerformReplacement);
			FString Key = Text.Mid(Pos + (P - T) + 1, (PEnd - P) - 1);
			FString *V, Value;
			if(Key.Len() == 0)
				Value = TEXT("%");
			else
			{
				V = ReplacementMap.Find(Key);
				if(V)
					Value = *V;
				else
					Value = TEXT("");
			}
			
			SendInParts(Value);
			Pos += (PEnd - P) + 1;
			T = PEnd + 1;
			unguard;
		}
		else
		{
			Pos++;
			T++;
		}
		unguard;
	}	
	SendInParts(Text.Mid(Pos));

	unguard;
}
IMPLEMENT_FUNCTION( UWebResponse, INDEX_NONE, execIncludeUHTM );

void UWebResponse::execClearSubst( FFrame& Stack, RESULT_DECL )
{
	guard(UWebResponse::execClearSubst);
	P_FINISH;
	ReplacementMap.Empty();
	unguard;
}
IMPLEMENT_FUNCTION( UWebResponse, INDEX_NONE, execClearSubst );

void UWebResponse::execSubst( FFrame& Stack, RESULT_DECL )
{
	guard(UWebResponse::execSubst);
	P_GET_STR(Variable);
	P_GET_STR(Value);
	P_GET_UBOOL_OPTX(bClear, 0);
	P_FINISH;

	if(bClear)
		ReplacementMap.Empty();
	ReplacementMap.Set( *Variable, *Value );
	unguard;
}
IMPLEMENT_FUNCTION( UWebResponse, INDEX_NONE, execSubst );

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

