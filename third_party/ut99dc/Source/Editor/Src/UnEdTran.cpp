/*=============================================================================
	UnEdTran.cpp: Unreal transaction-tracking functions.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#include "EditorPrivate.h"

/*-----------------------------------------------------------------------------
	A single transaction.
-----------------------------------------------------------------------------*/

//
// A single transaction, representing a set of serialized, undoable changes to a set of objects.
//
//warning: The undo buffer cannot be made persistent because of its dependence on offsets 
// of arrays from their owning UObjects.
//
//warning: Transactions which rely on Preload calls cannot be garbage collected
// since references to objects point to the most recent version of the object, not
// the ordinally correct version which was referred to at the time of serialization.
// Therefore, Preload-sensitive transactions may only be performed using
// a temporary UTransactor::CreateInternalTransaction transaction, not a
// garbage-collectable UTransactor::Begin transaction.
//
//warning: UObject::Serialize implicitly assumes that class properties do not change
// in between transaction resets.
//
class FTransaction : public FTransactionBase
{
private:
	// Record of an object.
	class FObjectRecord
	{
	public:
		// Variables.
		TArray<BYTE>	Data;
		UObject*		Object;
		FArray*			Array;
		INT				Index;
		INT				Count;
		INT				Oper;
		INT				ElementSize;
		STRUCT_AR		Serializer;
		STRUCT_DTOR		Destructor;
		UBOOL			Restored;

		// Constructors.
		FObjectRecord()
		{}
		FObjectRecord( FTransaction* Owner, UObject* InObject, FArray* InArray, INT InIndex, INT InCount, INT InOper, INT InElementSize, STRUCT_AR InSerializer, STRUCT_DTOR InDestructor )
		:	Object		( InObject )
		,	Array		( InArray )
		,	Index		( InIndex )
		,	Count		( InCount )
		,	Oper		( InOper )
		,	ElementSize	( InElementSize )
		,	Serializer	( InSerializer )
		,	Destructor	( InDestructor )
		{
			guard(FObjectRecord::FObjectRecord);
			FWriter Writer( Data );
			SerializeContents( Writer, Oper );
			unguardf(( Array ? TEXT("(%s %i)") : TEXT("(%s)"), Object->GetFullName(), Index ));
		}

		// Functions.
		void SerializeContents( FArchive& Ar, INT InOper )
		{
			guard(FTransaction::SerializeContents);
			if( Array )
			{
				//debugf( "Array %s %i*%i: %i",Object->GetFullName(),Index,ElementSize,InOper);
				checkSlow((SIZE_T)Array>=(SIZE_T)Object+sizeof(UObject));
				checkSlow((SIZE_T)Array+sizeof(FArray)<=(SIZE_T)Object+Object->GetClass()->GetPropertiesSize());
				checkSlow(ElementSize!=0);
				checkSlow(Serializer!=NULL);
				checkSlow(Index>=0);
				checkSlow(Count>=0);
				if( InOper==1 )
				{
					// "Saving add order" or "Undoing add order" or "Redoing remove order".
					if( Ar.IsLoading() )
					{
						checkSlow(Index+Count<=Array->Num());
						for( INT i=Index; i<Index+Count; i++ )
							Destructor( (BYTE*)Array->GetData() + i*ElementSize );
						Array->Remove( Index, Count, ElementSize );
					}
				}
				else
				{
					// "Undo/Redo Modify" or "Saving remove order" or "Undoing remove order" or "Redoing add order".
					if( InOper==-1 && Ar.IsLoading() )
					{
						Array->Insert( Index, Count, ElementSize );
						appMemzero( (BYTE*)Array->GetData() + Index*ElementSize, Count*ElementSize );
					}

					// Serialize changed items.
					checkSlow(Index+Count<=Array->Num());
					for( INT i=Index; i<Index+Count; i++ )
						Serializer( Ar, (BYTE*)Array->GetData() + i*ElementSize );
				}
			}
			else
			{
				//debugf( "Object %s",Object->GetFullName());
				checkSlow(Index==0);
				checkSlow(ElementSize==0);
				checkSlow(Serializer==NULL);
				Object->Serialize( Ar );
			}
			unguard;
		}
		void Restore( FTransaction* Owner )
		{
			guard(FObjectRecord::Restore);
			if( !Restored )
			{
				Restored = 1;
				TArray<BYTE> FlipData;
				if( Owner->Flip )
				{
					FWriter Writer( FlipData );
					SerializeContents( Writer, -Oper );
				}
				FReader Reader( Owner, Data );
				SerializeContents( Reader, Oper );
				if( Owner->Flip )
				{
					ExchangeArray( Data, FlipData );
					Oper *= -1;
				}
			}
			unguardf(( Array ? TEXT("(%s %i)") : TEXT("(%s)"), Object->GetFullName(), Index ));
		}
		friend FArchive& operator<<( FArchive& Ar, FObjectRecord& R )
		{
			guard(FObjectRecord<<);
			checkSlow(R.Object);
			FMemMark Mark(GMem);
			Ar << R.Object;
			FReader Reader( NULL, R.Data );
			if( !R.Array )
			{
				guard(Object);
				//warning: Relies on the safety of calling UObject::Serialize
				// on pseudoobjects.
				UClass*  Class        = R.Object->GetClass();
				UObject* PseudoObject = (UObject*)New<BYTE>(GMem,Class->GetPropertiesSize());
				PseudoObject->InitClassDefaultObject( Class );
				Class->ClassConstructor( PseudoObject );
				PseudoObject->Serialize( Reader );
				PseudoObject->Serialize( Ar );
				PseudoObject->~UObject();
				unguard;
			}
			else if( R.Data.Num() )
			{
				guard(Array);
				checkSlow(R.Serializer);
				FArray* Temp = (FArray*)NewZeroed<BYTE>(GMem,R.ElementSize);
				for( INT i=R.Index; i<R.Index+R.Count; i++ )
				{
					appMemzero( Temp, R.ElementSize );
					R.Serializer( Reader, Temp );
					R.Serializer( Ar,     Temp );
					R.Destructor( Temp );
				}
				unguard;
			}
			Mark.Pop();
			return Ar;
			unguard;
		}

		// Transfers data from an array.
		class FReader : public FArchive
		{
		public:
			FReader( FTransaction* InOwner, TArray<BYTE>& InBytes )
			:	Bytes	( InBytes )
			,	Offset	( 0 )
			,	Owner	( InOwner )
			{
				ArIsLoading = ArIsTrans = 1;
			}
		private:
			void Serialize( void* Data, INT Num )
			{
				checkSlow(Offset+Num<=Bytes.Num());
				appMemcpy( Data, &Bytes(Offset), Num );
				Offset += Num;
			}
			FArchive& operator<<( class FName& N )
			{
				checkSlow(Offset+(INT)sizeof(FName)<=Bytes.Num());
				N = *(FName*)&Bytes(Offset);
				Offset += sizeof(FName);
				return *this;
			}
			FArchive& operator<<( class UObject*& Res )
			{
				checkSlow(Offset+(INT)sizeof(UObject*)<=Bytes.Num());
				Res = *(UObject**)&Bytes(Offset);
				Offset += sizeof(UObject*);
				return *this;
			}
			void Preload( UObject* Object )
			{
				guard(FReader::Preload);
				if( Owner )
					for( INT i=0; i<Owner->Records.Num(); i++ )
						if( Owner->Records(i).Object==Object )
							Owner->Records(i).Restore( Owner );
				unguard;
			}
			FTransaction* Owner;
			TArray<BYTE>& Bytes;
			INT Offset;
		};

		// Transfers data to an array.
		class FWriter : public FArchive
		{
		public:
			FWriter( TArray<BYTE>& InBytes )
			: Bytes( InBytes )
			{
				ArIsSaving = ArIsTrans = 1;
			}
		private:
			void Serialize( void* Data, INT Num )
			{
				INT Index = Bytes.Add(Num);
				appMemcpy( &Bytes(Index), Data, Num );
			}
			FArchive& operator<<( class FName& N )
			{
				INT Index = Bytes.Add( sizeof(FName) );
				*(FName*)&Bytes(Index) = N;
				return *this;
			}
			FArchive& operator<<( class UObject*& Res )
			{
				INT Index = Bytes.Add( sizeof(UObject*) );
				*(UObject**)&Bytes(Index) = Res;
				return *this;
			}
			TArray<BYTE>& Bytes;
		};
	};

	// Transaction variables.
	TArray<FObjectRecord>	Records;
	FString					Title;
	UBOOL					Flip;
	INT						Inc;

public:
	// Constructor.
	FTransaction( const TCHAR* InTitle=NULL, UBOOL InFlip=0 )
	: Title( InTitle ? InTitle : TEXT("") )
	, Flip( InFlip )
	, Inc( -1 )
	{}

	// FTransactionBase interface.
	void SaveObject( UObject* Object )
	{
		guard(FTransaction::SaveObject);
		check(Object);

		// Save the object.
		new( Records )FObjectRecord( this, Object, NULL, 0, 0, 0, 0, NULL, NULL );

		unguard;
	}
	void SaveArray( UObject* Object, FArray* Array, INT Index, INT Count, INT Oper, INT ElementSize, STRUCT_AR Serializer, STRUCT_DTOR Destructor )
	{
		guard(FTransaction::SaveArray);
		checkSlow(Object);
		checkSlow(Array);
		checkSlow(ElementSize);
		checkSlow(Serializer);
		checkSlow(Object->IsValid());
		checkSlow((SIZE_T)Array>=(SIZE_T)Object);
		checkSlow((SIZE_T)Array+sizeof(FArray)<=(SIZE_T)Object+Object->GetClass()->PropertiesSize);
		checkSlow(Index>=0);
		checkSlow(Count>=0);
		checkSlow(Index+Count<=Array->Num());

		// Save the array.
		new( Records )FObjectRecord( this, Object, Array, Index, Count, Oper, ElementSize, Serializer, Destructor );

		unguard;
	}
	void Apply()
	{
		guard(FTransaction::Apply);
		checkSlow(Inc==1||Inc==-1);

		// Figure out direction.
		INT Start = Inc==1 ? 0             : Records.Num()-1;
		INT End   = Inc==1 ? Records.Num() :              -1;
		INT i;

		// Init objects.
		for( INT i=Start; i!=End; i+=Inc )
		{
			Records(i).Restored = 0;
			Records(i).Object->SetFlags( RF_NeedPostLoad );
		}
		for( i=Start; i!=End; i+=Inc )
		{
			Records(i).Restore( this );
		}
		for( i=Start; i!=End; i+=Inc )
		{
			if( Records(i).Object->GetFlags() & RF_NeedPostLoad )
			{
				Records(i).Object->ConditionalPostLoad();
				UModel* Model = Cast<UModel>(Records(i).Object);
				if( Model )
					if( Model->Nodes.Num() )
						GEditor->bspBuildBounds( Model );
			}
		}

		// Flip it.
		if( Flip )
			Inc *= -1;

		unguard;
	}

	// FTransaction interface.
	SIZE_T DataSize()
	{
		guard(DataSize);
		SIZE_T Result=0;
		for( INT i=0; i<Records.Num(); i++ )
			Result += Records(i).Data.Num();
		return Result;
		unguard;
	}
	const TCHAR* GetTitle()
	{
		guard(FTransaction::GetTitle);
		return *Title;
		unguard;
	}
	friend FArchive& operator<<( FArchive& Ar, FTransaction& T )
	{
		guard(FTransaction<<);
		return Ar << T.Records << T.Title;
		unguard;
	}

	// Transaction friends.
	friend class FObjectRecord;
	friend class FObjectRecord::FReader;
	friend class FObjectRecord::FWriter;
};

/*-----------------------------------------------------------------------------
	Transaction tracking system.
-----------------------------------------------------------------------------*/

//
// Transaction tracking system, manages the undo and redo buffer.
//
class EDITOR_API UTransBuffer : public UTransactor
{
	DECLARE_CLASS(UTransBuffer,UObject,CLASS_Transient)
	NO_DEFAULT_CONSTRUCTOR(UTransBuffer)

	// Variables.
	TArray<FTransaction>	UndoBuffer;
	INT						UndoCount;
	FString					ResetReason;
	INT						ActiveCount;
	SIZE_T					MaxMemory;
	UBOOL					Overflow;

	// Constructor.
	UTransBuffer( SIZE_T InMaxMemory )
	:	MaxMemory( InMaxMemory )
	{
		guard(UTransBuffer::UTransBuffer);

		// Reset.
		Reset( TEXT("startup") );
		CheckState();

		debugf( NAME_Init, TEXT("Transaction tracking system initialized") );
		unguard;
	}

	// UObject interface.
	void Serialize( FArchive& Ar )
	{
		guard(UTransBuffer::Serialize);
		CheckState();

		// Handle garbage collection.
		Super::Serialize( Ar );
		Ar << UndoBuffer << ResetReason << UndoCount << ActiveCount << Overflow;

		CheckState();
		unguard;
	}
	void Destroy()
	{
		guard(UTransBuffer::Destroy);
		CheckState();
		debugf( NAME_Exit, TEXT("Transaction tracking system shut down") );
		Super::Destroy();
		unguard;
	}

	// UTransactor interface.
	void Reset( const TCHAR* Reason )
	{
		guard(UTransBuffer::Reset);
		CheckState();
		check(ActiveCount==0);

		// Reset all transactions.
		UndoBuffer.Empty();
		UndoCount    = 0;
		ResetReason  = Reason;
		ActiveCount  = 0;
		Overflow     = 0;

		CheckState();
		unguard;
	}
	void Begin( const TCHAR* SessionName )
	{
		guard(UTransBuffer::Begin);
		CheckState();
		if( ActiveCount++==0 )
		{
			// Cancel redo buffer.
			//debugf("BeginTrans %s",SessionName);
			if( UndoCount )
				UndoBuffer.Remove( UndoBuffer.Num()-UndoCount, UndoCount );
			UndoCount = 0;

			// Purge previous transactions if too much data occupied.
			while( UndoDataSize() > MaxMemory )
				UndoBuffer.Remove( 0 );

			// Begin a new transaction.
			GUndo = new(UndoBuffer)FTransaction( SessionName, 1 );
			Overflow = 0;
		}
		CheckState();
		unguard;
	}
	void End()
	{
		guard(UTransBuffer::End);
		CheckState();
		check(ActiveCount>=1);
		if( --ActiveCount==0 )
		{
			// End the current transaction.
			//debugf("EndTrans");
			//FTransaction& Trans = UndoBuffer.Last();
			GUndo = NULL;
		}
		CheckState();
		unguard;
	}
	void Continue()
	{
		guard(UTransBuffer::Continue);
		CheckState();
		if( ActiveCount==0 && UndoBuffer.Num()>0 && UndoCount==0 )
		{
			// Continue the previous transaction.
			ActiveCount++;
			GUndo = &UndoBuffer.Last();
		}
		CheckState();
		unguard;
	}
	UBOOL CanUndo( FString* Str=NULL )
	{
		guard(UTransBuffer::CanUndo);
		CheckState();
		if( ActiveCount )
		{
			if( Str )
				*Str = TEXT("Can't undo during a transaction");
			return 0;
		}
		if( UndoBuffer.Num()==UndoCount )
		{
			if( Str )
				*Str = US + TEXT("Can't undo after ") + ResetReason;
			return 0;
		}
		return 1;
		unguard;
	}
	UBOOL CanRedo( FString* Str=NULL )
	{
		guard(UTransBuffer::CanRedo);
		CheckState();
		if( ActiveCount )
		{
			if( Str )
				*Str = TEXT("Can't redo during a transaction");
			return 0;
		}
		if( UndoCount==0 )
		{
			if( Str )
				*Str = TEXT("Nothing to redo");
			return 0;
		}
		return 1;
		unguard;
	}
	UBOOL Undo()
	{
		guard(UTransBuffer::Undo);
		CheckState();
		if( !CanUndo() )
			return 0;

		// Apply the undo changes.
		FTransaction& Transaction = UndoBuffer( UndoBuffer.Num() - ++UndoCount );
		debugf( TEXT("Undo %s"), Transaction.GetTitle() );
		Transaction.Apply();
		FinishDo();

		CheckState();
		return 1;
		unguard;
	}
	UBOOL Redo()
	{
		guard(UTransBuffer::Redo);
		CheckState();
		if( !CanRedo() )
			return 0;

		// Apply the redo changes.
		FTransaction& Transaction = UndoBuffer( UndoBuffer.Num() - UndoCount-- );
		debugf( TEXT("Redo %s"), Transaction.GetTitle() );
		Transaction.Apply();
		FinishDo();

		CheckState();
		return 1;
		unguard;
	}
	FTransactionBase* CreateInternalTransaction()
	{
		guard(CreateInternalTransaction);
		return new FTransaction( TEXT("Internal"), 0 );
		unguard;
	}

	// Functions.
	void FinishDo()
	{
		guard(UTransBuffer::FinishDo);
		GEditor->NoteSelectionChange( GEditor->Level );
		unguard;
	}
	SIZE_T UndoDataSize()
	{
		guard(UTransBuffer::TotalDataSize);
		SIZE_T Result=0;
		for( INT i=0; i<UndoBuffer.Num(); i++ )
			Result += UndoBuffer(i).DataSize();
		return Result;
		unguard;
	}
	void CheckState()
	{
		guard(UTransBuffer::CheckState);

		// Validate the internal state.
		check(UndoBuffer.Num()>=UndoCount);
		check(ActiveCount>=0);

		unguard;
	}
};
IMPLEMENT_CLASS(UTransBuffer);
IMPLEMENT_CLASS(UTransactor);

/*-----------------------------------------------------------------------------
	Allocator.
-----------------------------------------------------------------------------*/

UTransactor* UEditorEngine::CreateTrans()
{
	guard(UEditorEngine::CreateTrans);
	return new UTransBuffer( 8*1024*1024 );
	unguard;
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
