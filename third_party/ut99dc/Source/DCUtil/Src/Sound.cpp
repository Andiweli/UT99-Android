#include "Sound.h"
#include <unistd.h>
#include <stdlib.h>

extern CORE_API void appCreateTempFilename( const char* Path, char* Result256 );
extern CORE_API INT appUnlink( const char* Filename );

static FString CreateTempFile( const char* Ext )
{
	char Temp[256] = {0};
	appCreateTempFilename( ".", Temp );
	FString Path = Temp;
	Path += Ext;
	return Path;
}

static FString SaveSoundToTempWav( USound* Sound )
{
	if( !Sound || Sound->Data.Num() == 0 )
		return "";

	FString TempPath = CreateTempFile( ".wav" );
	FILE* Out = fopen( TCHAR_TO_ANSI(*TempPath), "wb" );
	if( !Out )
		return "";

	fwrite( &Sound->Data(0), 1, Sound->Data.Num(), Out );
	fclose( Out );
	return TempPath;
}

static FString RunFfmpegAdpcm( const FString& InPath )
{
	FString OutPath = CreateTempFile( ".adpcm.wav" );
	char Cmd[1024];
	appSprintf( Cmd, "ffmpeg -y -i \"%s\" -ac 1 -ar 11025 -f wav -acodec adpcm_yamaha \"%s\"", *InPath, *OutPath );
	if( system( Cmd ) != 0 )
		return "";
	return OutPath;
}

static UBOOL LoadAdpcmBack( USound* Sound, const FString& AdpcmPath )
{
	FILE* In = fopen( TCHAR_TO_ANSI(*AdpcmPath), "rb" );
	if( !In )
		return 0;
	fseek( In, 0, SEEK_END );
	INT Size = ftell( In );
	fseek( In, 0, SEEK_SET );
	TArray<BYTE> Data;
	Data.Add( Size );
	if( fread( &Data(0), 1, Size, In ) != (size_t)Size )
	{
		fclose( In );
		return 0;
	}
	fclose( In );

	Sound->Data.Empty();
	Sound->Data.Add( Data.Num() );
	appMemcpy( &Sound->Data(0), &Data(0), Data.Num() );
	Sound->FileType = FName("WAV");
	Sound->OriginalSize = Sound->Data.Num();
	Sound->Handle = nullptr;
	return 1;
}

UBOOL FSoundCompressor::CompressUSound( USound* Sound )
{
	if( !Sound || Sound->Data.Num() == 0 )
		return 0;

	const FString Wav = SaveSoundToTempWav( Sound );
	if( !appStrlen( *Wav ) )
		return 0;
	const FString Adpcm = RunFfmpegAdpcm( Wav );
	if( !appStrlen( *Adpcm ) )
		return 0;
	const UBOOL Result = LoadAdpcmBack( Sound, Adpcm );
	if( Result )
	{
		Sound->OriginalSize = Sound->Data.Num();
	}
	unlink( *Wav );
	unlink( *Adpcm );
	return Result;
}

