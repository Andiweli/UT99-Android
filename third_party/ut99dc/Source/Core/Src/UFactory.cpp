/*=============================================================================
	UFactory.cpp: Factory class implementation.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

// Core includes.
#include "CorePrivate.h"

/*----------------------------------------------------------------------------
	UFactory.
----------------------------------------------------------------------------*/

void UFactory::StaticConstructor()
{
	guard(UFactory::StaticConstructor);

	UArrayProperty* A = new(GetClass(),TEXT("Formats"),RF_Public)UArrayProperty(CPP_PROPERTY(Formats),TEXT(""),0);
	A->Inner = new(A,TEXT("StrProperty0"),RF_Public)UStrProperty;

	unguard;
}
UFactory::UFactory()
: Formats( E_NoInit )
{}
void UFactory::Serialize( FArchive& Ar )
{
	guard(UFactory::Serialize);
	Super::Serialize( Ar );
	if( !Ar.IsLoading() && !Ar.IsSaving() )
		Ar << SupportedClass << ContextClass;
	unguard;
}
static INT Compare( UFactory* A, UFactory* B )
{
	guard(AutoPriorityCompare);
	return A->AutoPriority-B->AutoPriority;
	unguard;
}
UObject* UFactory::StaticImportObject
(
	UClass*				Class,
	UObject*			InOuter,
	FName				Name,
	DWORD				Flags,
	const TCHAR*		Filename,
	UObject*			Context,
	UFactory*			InFactory,
	const TCHAR*		Parms,
	FFeedbackContext*	Warn
)
{
	guard(UFactory::StaticImportObject);
	check(Class);

	// Make list of all applicable factories.
	TArray<UFactory*> Factories;
	if( InFactory )
	{
		// Use just the specified factory.
		check(InFactory->SupportedClass->IsChildOf(Class));
		Factories.AddItem( InFactory );
	}
	else
	{
		// Try all automatic factories, sorted by priority.
		for( TObjectIterator<UClass> It; It; ++It )
		{
			if( It->IsChildOf(UFactory::StaticClass()) )
			{
				UFactory* Default = (UFactory*)It->GetDefaultObject();
				if( Default->SupportedClass==Class && Default->AutoPriority>=0 )
					Factories.AddItem( ConstructObject<UFactory>(*It) );
			}
		}
		Sort( &Factories(0), Factories.Num() );
	}

	// Try each factory in turn.
	for( INT i=0; i<Factories.Num(); i++ )
	{
		UFactory* Factory = Factories(i);
		UObject* Result = NULL;
		if( Factory->bCreateNew )
		{
			if( appStricmp(Filename,TEXT(""))==0 )
			{
				debugf( NAME_Log, TEXT("FactoryCreateNew: %s with %s (%i %i %s)"), Class->GetName(), Factories(i)->GetClass()->GetName(), Factory->bCreateNew, Factory->bText, Filename );
				Factory->ParseParms( Parms );
				Result = Factory->FactoryCreateNew( Class, InOuter, Name, Flags, NULL, Warn );
			}
		}
		else if( appStricmp(Filename,TEXT(""))!=0 )
		{
			if( Factory->bText )
			{
				debugf( NAME_Log, TEXT("FactoryCreateText: %s with %s (%i %i %s)"), Class->GetName(), Factories(i)->GetClass()->GetName(), Factory->bCreateNew, Factory->bText, Filename );
				FString Data;
				if( appLoadFileToString( Data, Filename ) )
				{
					const TCHAR* Ptr = *Data;
					Factory->ParseParms( Parms );
					Result = Factory->FactoryCreateText( Class, InOuter, Name, Flags, NULL, appFExt(Filename), Ptr, Ptr+Data.Len(), Warn );
				}
			}
			else
			{
				debugf( NAME_Log, TEXT("FactoryCreateBinary: %s with %s (%i %i %s)"), Class->GetName(), Factories(i)->GetClass()->GetName(), Factory->bCreateNew, Factory->bText, Filename );
				TArray<BYTE> Data;
				if( appLoadFileToArray( Data, Filename ) )
				{
					Data.AddItem( 0 );
					const BYTE* Ptr = &Data( 0 );
					Factory->ParseParms( Parms );
					Result = Factory->FactoryCreateBinary( Class, InOuter, Name, Flags, NULL, appFExt(Filename), Ptr, Ptr+Data.Num()-1, Warn );
				}
			}
		}
		if( Result )
		{
			check(Result->IsA(Class));
			if( !InFactory )
				for( INT i=0; i<Factories.Num(); i++ )
					delete Factories(i);
			return Result;
		}
	}
	if( !InFactory )
		for( INT i=0; i<Factories.Num(); i++ )
			delete Factories(i);
	Warn->Logf( LocalizeError("NoFindImport"), Filename );
	return NULL;
	unguard;
}
IMPLEMENT_CLASS(UFactory);

/*----------------------------------------------------------------------------
	The End.
----------------------------------------------------------------------------*/
