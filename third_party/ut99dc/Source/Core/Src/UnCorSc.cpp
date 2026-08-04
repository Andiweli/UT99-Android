/*=============================================================================
	UnCorSc.cpp: UnrealScript execution and support code.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

Description:
	UnrealScript execution and support code.

Revision history:
	* Created by Tim Sweeney 

=============================================================================*/

#include "CorePrivate.h"

/*-----------------------------------------------------------------------------
	Globals.
-----------------------------------------------------------------------------*/

CORE_API void (UObject::*GNatives[EX_Max])( FFrame &Stack, RESULT_DECL );
CORE_API INT GNativeDuplicate=0;

#define RUNAWAY_LIMIT 10000000
#define RECURSE_LIMIT 250

#if DO_GUARD
	static INT Runaway=0;
	static INT Recurse=0;
	#define CHECK_RUNAWAY {if( ++Runaway > RUNAWAY_LIMIT ) {if(!ParseParam(appCmdLine(),TEXT("norunaway"))) Stack.Logf( NAME_Critical, TEXT("Runaway loop detected (over %i iterations)"), RUNAWAY_LIMIT ); Runaway=0;}}
	CORE_API void GInitRunaway() {Recurse=Runaway=0;}
#else
	#define CHECK_RUNAWAY
	CORE_API void GInitRunaway() {}
#endif

/*-----------------------------------------------------------------------------
	FFrame implementation.
-----------------------------------------------------------------------------*/

//
// Error or warning handler.
//
void FFrame::Serialize( const TCHAR* V, EName Event )
{
	guard(FFrame::Serialize);
	if( Event==NAME_Critical || GIsStrict ) appErrorf
	(
		TEXT("%s (%s:%04X) %s"),
		Object->GetFullName(),
		Node->GetFullName(),
		Code - &Node->Script(0),
		V
	);
	else debugf
	(
		NAME_ScriptWarning,
		TEXT("%s (%s:%04X) %s"),
		Object->GetFullName(),
		Node->GetFullName(),
		Code - &Node->Script(0),
		V
	);
	unguard;
}

/*-----------------------------------------------------------------------------
	Global script execution functions.
-----------------------------------------------------------------------------*/

//
// Have an object go to a named state, and idle at no label.
// If state is NAME_None or was not found, goes to no state.
// Returns 1 if we went to a state, 0 if went to no state.
//
EGotoState UObject::GotoState( FName NewState )
{
	guard(UObject::GotoState);
	if( !StateFrame )
		return GOTOSTATE_NotFound;

	StateFrame->LatentAction = 0;
	UState* StateNode = NULL;
	FName OldStateName = StateFrame->StateNode!=Class ? StateFrame->StateNode->GetFName() : NAME_None;
	if( NewState != NAME_Auto )
	{
		// Find regular state.
		StateNode = FindState( NewState );
	}
	else
	{
		// Find auto state.
		for( TFieldIterator<UState> It(GetClass()); It && !StateNode; ++It )
			if( It->StateFlags & STATE_Auto )
				StateNode = *It;
	}

	if( !StateNode )
	{
		// Going nowhere.
		NewState  = NAME_None;
		StateNode = GetClass();
	}
	else if( NewState == NAME_Auto )
	{
		// Going to auto state.
		NewState = StateNode->GetFName();
	}

	// Send EndState notification.
	if
	(	OldStateName!=NAME_None
	&&	NewState!=OldStateName
	&&	IsProbing(NAME_EndState) 
	&&	!(GetFlags() & RF_InEndState) )
	{
		ClearFlags( RF_StateChanged );
		SetFlags( RF_InEndState );
		eventEndState();
		ClearFlags( RF_InEndState );
		if( GetFlags() & RF_StateChanged )
			return GOTOSTATE_Preempted;
	}

	// Go there.
	StateFrame->Node	   = StateNode;
	StateFrame->StateNode  = StateNode;
	StateFrame->Code	   = NULL;
	StateFrame->ProbeMask  = (StateNode->ProbeMask | GetClass()->ProbeMask) & StateNode->IgnoreMask;

	// Send BeginState notification.
	if( NewState!=NAME_None && NewState!=OldStateName && IsProbing(NAME_BeginState) )
	{
		ClearFlags( RF_StateChanged );
		eventBeginState();
		if( GetFlags() & RF_StateChanged )
			return GOTOSTATE_Preempted;
	}

	// Return result.
	if( NewState != NAME_None )
	{
		SetFlags( RF_StateChanged );
		return GOTOSTATE_Success;
	}
	else return GOTOSTATE_NotFound;

	unguard;
}

//
// Goto a label in the current state.
// Returns 1 if went, 0 if not found.
//
UBOOL UObject::GotoLabel( FName FindLabel )
{
	guard(UObject::GotoLabel);
	if( StateFrame )
	{
		StateFrame->LatentAction = 0;
		if( FindLabel != NAME_None )
		{
			for( UState* SourceState=StateFrame->StateNode; SourceState; SourceState=SourceState->GetSuperState() )
			{
				if( SourceState->LabelTableOffset != MAXWORD )
				{
					for( FLabelEntry* Label = (FLabelEntry *)&SourceState->Script(SourceState->LabelTableOffset); Label->Name!=NAME_None; Label++ )
					{
						if( Label->Name==FindLabel )
						{
							StateFrame->Node = SourceState;
							StateFrame->Code = &SourceState->Script(Label->iCode);
							return 1;
						}
					}
				}
			}
		}
		StateFrame->Code = NULL;
	}
	return 0;
	unguard;
}

/*-----------------------------------------------------------------------------
	Natives.
-----------------------------------------------------------------------------*/

//////////////////////////////
// Undefined native handler //
//////////////////////////////

void UObject::execUndefined( FFrame& Stack, RESULT_DECL  )
{
	guardSlow(UObject::execUndefined);

	Stack.Logf( NAME_Critical, TEXT("Unknown code token %02X"), Stack.Code[-1] );

	unguardexecSlow;
}

///////////////
// Variables //
///////////////

void UObject::execLocalVariable( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execLocalVariable);

	checkSlow(Stack.Object==this);
	checkSlow(Stack.Locals!=NULL);
	GProperty = (UProperty*)Stack.ReadObject();
	GPropAddr = Stack.Locals + GProperty->Offset;
	if( Result )
		GProperty->CopyCompleteValue( Result, GPropAddr );

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_LocalVariable, execLocalVariable );

void UObject::execInstanceVariable( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execInstanceVariable);

	GProperty = (UProperty*)Stack.ReadObject();
	GPropAddr = (BYTE*)this + GProperty->Offset;
	if( Result )
		GProperty->CopyCompleteValue( Result, GPropAddr );

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_InstanceVariable, execInstanceVariable );

void UObject::execDefaultVariable( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execDefaultVariable);

	GProperty = (UProperty*)Stack.ReadObject();
	GPropAddr = &GetClass()->Defaults(GProperty->Offset);
	if( Result )
		GProperty->CopyCompleteValue( Result, GPropAddr );

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_DefaultVariable, execDefaultVariable );

void UObject::execClassContext( FFrame& Stack, RESULT_DECL )
{
	guard(UObject::execClassContext);

	// Get class expression.
	UClass* ClassContext=NULL;
	Stack.Step( Stack.Object, &ClassContext );

	// Execute expression in class context.
	if( ClassContext )
	{
		Stack.Code += 3;
		Stack.Step( ClassContext->GetDefaultObject(), Result );
	}
	else
	{
		Stack.Logf( TEXT("Accessed null class context") );
		INT wSkip = Stack.ReadWord();
		BYTE bSize = *Stack.Code++;
		Stack.Code += wSkip;
		GPropAddr = NULL;
		GProperty = NULL;
		if( Result )
			appMemzero( Result, bSize );
	}
	unguardexec;
}
IMPLEMENT_FUNCTION( UObject, EX_ClassContext, execClassContext );

void UObject::execArrayElement( FFrame& Stack, RESULT_DECL )
{
	guard(UObject::execArrayElement);//!!

	// Get array index expression.
	INT Index=0;
	Stack.Step( Stack.Object, &Index );

	// Get base element (must be a variable!!).
	GProperty = NULL;
	Stack.Step( this, NULL );

	// Add scaled offset to base pointer.
	if( GProperty && GPropAddr )
	{
		// Bounds check.
		if( Index>=GProperty->ArrayDim || Index<0 )
		{
			// Display out-of-bounds warning and continue on with index clamped to valid range.
			Stack.Logf( TEXT("Accessed array out of bounds (%i/%i)"), Index, GProperty->ArrayDim );
			Index = Clamp<INT>( Index, 0, (INT)GProperty->ArrayDim - 1 );
		}

		// Update address.
		GPropAddr += Index * GProperty->ElementSize;
		guard(ArrayElementCopySingleValue);//!!
		if( Result )//!!
			GProperty->CopySingleValue( Result, GPropAddr );
		unguard;
	}

	unguardexec;
}
IMPLEMENT_FUNCTION( UObject, EX_ArrayElement, execArrayElement );

void UObject::execDynArrayElement( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execArrayElement);

	// Get array index expression.
	INT Index=0;
	Stack.Step( Stack.Object, &Index );

	// Get FArray (only works with variables that have a GProperty!!) This is experimental and doesn't work yet.
	FArray* Array=NULL;
	Stack.Step( this, *(BYTE**)&Array );
	check(GProperty);
	check(GProperty->IsA(UArrayProperty::StaticClass()));
	check(Array);//!!
	check(Array->Num()!=0);//!! Needs to handle zero-element arrays gracefully.

	// Bounds check.
	if( Index>=Array->Num() || Index<0 )
	{
		// Display out-of-bounds warning and continue on with index clamped to valid range.
		//!!must work like null-context expressions.
		Stack.Logf( TEXT("Accessed array out of bounds (%i/%i)"), Index, GProperty->ArrayDim );
		Index = Clamp<INT>( Index, 0, (INT)Array->Num() - 1 );
	}

	// Add scaled offset to base pointer.
	//if( Result )
	//	Result = (BYTE*)Array->GetData() + *(INT*)Addr * ((UArrayProperty*)GProperty)->Inner->ElementSize;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_DynArrayElement, execDynArrayElement );

void UObject::execBoolVariable( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execBoolVariable);

	// The following variable opcode operand is a fixed 4-byte GObjObjects
	// index, not a native pointer.
	BYTE B = *Stack.Code++;
#if defined(PLATFORM_DREAMCAST) || defined(PLATFORM_ANDROID)
	INT PropIndex;
	__builtin_memcpy( &PropIndex, Stack.Code, sizeof( PropIndex ) );
#else
	INT PropIndex = *(INT*)Stack.Code;
#endif
	UBoolProperty* Property = (UBoolProperty*)UObject::GetIndexedObject( PropIndex );
	(this->*GNatives[B])( Stack, NULL );
	GProperty = Property;

	// Note that we're not returning an in-place pointer to to the bool, so EX_Let 
	// must take special precautions with bools.
	if( Result )
		*(BITFIELD*)Result = (GPropAddr && (*(BITFIELD*)GPropAddr & ((UBoolProperty*)GProperty)->BitMask)) ? 1 : 0;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_BoolVariable, execBoolVariable );

void UObject::execStructMember( FFrame& Stack, RESULT_DECL )
{
	guard(UObject::execStructMember);//!!

	// Get structure element.
	UProperty* Property = (UProperty*)Stack.ReadObject();

	// Get struct expression.
	UStruct* Struct = CastChecked<UStruct>(Property->GetOuter());
	BYTE* Buffer = (BYTE*)appAlloca(Struct->PropertiesSize);
	appMemzero( Buffer, Struct->PropertiesSize );
	GPropAddr = NULL;
	Stack.Step( this, Buffer );

	// Set result.
	GProperty = Property;
	if( GPropAddr )
		GPropAddr += Property->Offset;
	guard(StructMemberCopyComplete);//!!
	if( Result )
		Property->CopyCompleteValue( Result, Buffer+Property->Offset );
	unguard;
	guard(StructMemberDestroyValue);//!!
	for( UProperty* P=Struct->ConstructorLink; P; P=P->ConstructorLinkNext )
		P->DestroyValue( Buffer + P->Offset );
	unguard;

	unguardexec;
}
IMPLEMENT_FUNCTION( UObject, EX_StructMember, execStructMember );

/////////////
// Nothing //
/////////////

void UObject::execNothing( FFrame& Stack, RESULT_DECL )
{
	// Do nothing.
}
IMPLEMENT_FUNCTION( UObject, EX_Nothing, execNothing );

void UObject::execNativeParm( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execNativeParm);

	UProperty* Property = (UProperty*)Stack.ReadObject();
	if( Result )
	{
		GPropAddr = Stack.Locals + Property->Offset;
		Property->CopyCompleteValue( Result, Stack.Locals + Property->Offset );
	}

	unguardSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_NativeParm, execNativeParm );

void UObject::execEndFunctionParms( FFrame& Stack, RESULT_DECL )
{
	// For skipping over optional function parms without values specified.
	Stack.Code--;
}
IMPLEMENT_FUNCTION( UObject, EX_EndFunctionParms, execEndFunctionParms );

//////////////
// Commands //
//////////////

void UObject::execStop( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execStop);
	Stack.Code = NULL;
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_Stop, execStop );

//!!warning: Does not support UProperty's fully, will break
// when TArray's are supported in UnrealScript!
void UObject::execSwitch( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execSwitch);

	// Get switch size.
	BYTE bSize = *Stack.Code++;

	// Get switch expression.
	BYTE SwitchBuffer[1024], Buffer[1024];
	appMemzero( Buffer,       sizeof(FString) );
	appMemzero( SwitchBuffer, sizeof(FString) );
	Stack.Step( Stack.Object, SwitchBuffer );

	// Check each case clause till we find a match.
	for( ; ; )
	{
		// Skip over case token.
		checkSlow(*Stack.Code==EX_Case);
		Stack.Code++;

		// Get address of next handler.
		INT wNext = Stack.ReadWord();
		if( wNext == MAXWORD ) // Default case or end of cases.
			break;

		// Get case expression.
		Stack.Step( Stack.Object, Buffer );

		// Compare.
		if( bSize ? (appMemcmp(SwitchBuffer,Buffer,bSize)==0) : (*(FString*)SwitchBuffer==*(FString*)Buffer) )
			break;

		// Jump to next handler.
		Stack.Code = &Stack.Node->Script(wNext);
	}
	if( !bSize )
	{
		(*(FString*)SwitchBuffer).~FString();
		(*(FString*)Buffer      ).~FString();
	}
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_Switch, execSwitch );

void UObject::execCase( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execCase);
	INT wNext = Stack.ReadWord();
	if( wNext != MAXWORD )
	{
		// Skip expression.
		BYTE Buffer[1024];
		appMemzero( Buffer, sizeof(FString) );
		Stack.Step( Stack.Object, Buffer );
	}
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_Case, execCase );

void UObject::execJump( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execJump);
	CHECK_RUNAWAY;

	// Jump immediate.
	Stack.Code = &Stack.Node->Script(Stack.ReadWord() );

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_Jump, execJump );

void UObject::execJumpIfNot( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execJumpIfNot);
	CHECK_RUNAWAY;

	// Get code offset.
	INT wOffset = Stack.ReadWord();

	// Get boolean test value.
	UBOOL Value=0;
	Stack.Step( Stack.Object, &Value );

	// Jump if false.
	if( !Value )
		Stack.Code = &Stack.Node->Script( wOffset );

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_JumpIfNot, execJumpIfNot );

void UObject::execAssert( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execAssert);

	// Get line number.
	INT wLine = Stack.ReadWord();

	// Get boolean assert value.
	DWORD Value=0;
	Stack.Step( Stack.Object, &Value );

	// Check it.
	if( !Value )
		Stack.Logf( NAME_Critical, TEXT("Assertion failed, line %i"), wLine );

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_Assert, execAssert );

void UObject::execGotoLabel( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execGotoLabel);

	P_GET_NAME(N);
	if( !GotoLabel( N ) )
		Stack.Logf( TEXT("GotoLabel (%s): Label not found"), N );

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_GotoLabel, execGotoLabel );

////////////////
// Assignment //
////////////////

void UObject::execLet( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execLet);
	checkSlow(!IsA(UBoolProperty::StaticClass()));

	// Get variable address.
	GPropAddr = NULL;
	Stack.Step( Stack.Object, NULL ); // Evaluate variable.
	if( !GPropAddr )
	{
		Stack.Logf( NAME_ScriptWarning, TEXT("Attempt to assigned variable through None") );
		static BYTE Crud[1024];//!!temp
		GPropAddr = Crud;
		appMemzero( GPropAddr, sizeof(FString) );
	}
	Stack.Step( Stack.Object, GPropAddr ); // Evaluate expression into variable.

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_Let, execLet );

void UObject::execLetBool( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execLetBool);

	// Get variable address.
	GPropAddr = NULL;
	GProperty = NULL;
	Stack.Step( Stack.Object, NULL ); // Variable.
	BITFIELD*      BoolAddr     = (BITFIELD*)GPropAddr;
	UBoolProperty* BoolProperty = (UBoolProperty*)GProperty;
	BITFIELD Value=0;
	Stack.Step( Stack.Object, &Value );
	if( BoolAddr )
	{
		check(BoolProperty->IsA(UBoolProperty::StaticClass()));
		if( Value ) *BoolAddr |=  BoolProperty->BitMask;
		else        *BoolAddr &= ~BoolProperty->BitMask;
	}

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_LetBool, execLetBool );

/////////////////////////
// Context expressions //
/////////////////////////

void UObject::execSelf( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execSelf);

	// Get Self actor for this context.
	*(UObject**)Result = this;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_Self, execSelf );

void UObject::execContext( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execContext);

	// Get actor variable.
	UObject* NewContext=NULL;
	Stack.Step( this, &NewContext );

	// Execute or skip the following expression in the actor's context.
	if( NewContext != NULL )
	{
		Stack.Code += 3;
		Stack.Step( NewContext, Result );
	}
	else
	{
		Stack.Logf( TEXT("Accessed None") );
		INT wSkip = Stack.ReadWord();
		BYTE bSize = *Stack.Code++;
		Stack.Code += wSkip;
		GPropAddr = NULL;
		GProperty = NULL;
		if( Result )
			appMemzero( Result, bSize );
	}
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_Context, execContext );

////////////////////
// Function calls //
////////////////////

void UObject::execVirtualFunction( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execVirtualFunction);

	// Call the virtual function.
	CallFunction( Stack, Result, FindFunctionChecked(Stack.ReadName()) );

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_VirtualFunction, execVirtualFunction );

void UObject::execFinalFunction( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execFinalFunction);

	// Call the final function.
	CallFunction( Stack, Result, (UFunction*)Stack.ReadObject() );

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_FinalFunction, execFinalFunction );

void UObject::execGlobalFunction( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execGlobalFunction);

	// Call global version of virtual function.
	CallFunction( Stack, Result, FindFunctionChecked(Stack.ReadName(),1) );

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_GlobalFunction, execGlobalFunction );

///////////////////////
// Struct comparison //
///////////////////////

void UObject::execStructCmpEq( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execStructCmpEq);

	UStruct* Struct  = (UStruct*)Stack.ReadObject();
	BYTE*    Buffer1 = (BYTE*)appAlloca(Struct->PropertiesSize);
	BYTE*    Buffer2 = (BYTE*)appAlloca(Struct->PropertiesSize);
	appMemzero( Buffer1, Struct->PropertiesSize );
	appMemzero( Buffer2, Struct->PropertiesSize );
	Stack.Step( this, Buffer1 );
	Stack.Step( this, Buffer2 );
	*(DWORD*)Result  = Struct->StructCompare( Buffer1, Buffer2 );

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_StructCmpEq, execStructCmpEq );

void UObject::execStructCmpNe( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execStructCmpNe);

	UStruct* Struct = (UStruct*)Stack.ReadObject();
	BYTE*    Buffer1 = (BYTE*)appAlloca(Struct->PropertiesSize);
	BYTE*    Buffer2 = (BYTE*)appAlloca(Struct->PropertiesSize);
	appMemzero( Buffer1, Struct->PropertiesSize );
	appMemzero( Buffer2, Struct->PropertiesSize );
	Stack.Step( this, Buffer1 );
	Stack.Step( this, Buffer2 );
	*(DWORD*)Result = !Struct->StructCompare(Buffer1,Buffer2);

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_StructCmpNe, execStructCmpNe );

///////////////
// Constants //
///////////////

void UObject::execIntConst( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execIntConst);
	*(INT*)Result = Stack.ReadInt();
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_IntConst, execIntConst );

void UObject::execFloatConst( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execFloatConst);
	*(FLOAT*)Result = Stack.ReadFloat();
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_FloatConst, execFloatConst );

void UObject::execStringConst( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execStringConst);
	*(FString*)Result = appFromAnsi((ANSICHAR*)Stack.Code);
	while( *Stack.Code )
		Stack.Code++;
	Stack.Code++;
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_StringConst, execStringConst );

void UObject::execUnicodeStringConst( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execStringConst);
	*(FString*)Result = appFromUnicode((UNICHAR*)Stack.Code);
	for( ;; )
	{
		_WORD Ch;
		__builtin_memcpy( &Ch, Stack.Code, sizeof(Ch) );
		Stack.Code += sizeof(_WORD);
		if( !Ch )
			break;
	}
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_UnicodeStringConst, execUnicodeStringConst );

void UObject::execObjectConst( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execObjectConst);
	*(UObject**)Result = (UObject*)Stack.ReadObject();
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_ObjectConst, execObjectConst );

void UObject::execNameConst( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execNameConst);
	*(FName*)Result = Stack.ReadName();
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_NameConst, execNameConst );

void UObject::execByteConst( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execByteConst);
	*(BYTE*)Result = *Stack.Code++;
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_ByteConst, execByteConst );

void UObject::execIntZero( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execIntZero);
	*(INT*)Result = 0;
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_IntZero, execIntZero );

void UObject::execIntOne( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execIntOne);
	*(INT*)Result = 1;
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_IntOne, execIntOne );

void UObject::execTrue( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execTrue);
	*(INT*)Result = 1;
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_True, execTrue );

void UObject::execFalse( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execFalse);
	*(DWORD*)Result = 0;
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_False, execFalse );

void UObject::execNoObject( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execNoObject);
	*(UObject**)Result = NULL;
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_NoObject, execNoObject );

void UObject::execIntConstByte( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execIntConstByte);
	*(INT*)Result = *Stack.Code++;
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_IntConstByte, execIntConstByte );

/////////////////
// Conversions //
/////////////////

void UObject::execDynamicCast( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execDynamicCast);

	// Get destination class of dynamic actor class.
	UClass* Class = (UClass *)Stack.ReadObject();

	// Compile object expression.
	UObject* Castee = NULL;
	Stack.Step( Stack.Object, &Castee );
	*(UObject**)Result = (Castee && Castee->IsA(Class)) ? Castee : NULL;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_DynamicCast, execDynamicCast );

void UObject::execMetaCast( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execMetaCast);

	// Get destination class of dynamic actor class.
	UClass* MetaClass = (UClass*)Stack.ReadObject();

	// Compile actor expression.
	UObject* Castee=NULL;
	Stack.Step( Stack.Object, &Castee );
	*(UObject**)Result = (Castee && Castee->IsA(UClass::StaticClass()) && ((UClass*)Castee)->IsChildOf(MetaClass)) ? Castee : NULL;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_MetaCast, execMetaCast );

void UObject::execByteToInt( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execByteToInt);
	BYTE B=0;
	Stack.Step( Stack.Object, &B );
	*(INT*)Result = B;
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_ByteToInt, execByteToInt );

void UObject::execByteToBool( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execByteToBool);
	BYTE B=0;
	Stack.Step( Stack.Object, &B );
	*(DWORD*)Result = B ? 1 : 0;
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_ByteToBool, execByteToBool );

void UObject::execByteToFloat( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execByteToFloat);
	BYTE B=0;
	Stack.Step( Stack.Object, &B );
	*(FLOAT*)Result = B;
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_ByteToFloat, execByteToFloat );

void UObject::execByteToString( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execByteToString);
	P_GET_BYTE(B);
	*(FString*)Result = FString::Printf(TEXT("%i"),B);
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_ByteToString, execByteToString );

void UObject::execIntToByte( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execIntToByte);
	INT I=0;
	Stack.Step( Stack.Object, &I );
	*(BYTE*)Result = I;
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_IntToByte, execIntToByte );

void UObject::execIntToBool( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execIntToBool);
	INT I=0;
	Stack.Step( Stack.Object, &I );
	*(INT*)Result = I ? 1 : 0;
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_IntToBool, execIntToBool );

void UObject::execIntToFloat( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execIntToFloat);
	INT I=0;
	Stack.Step( Stack.Object, &I );
	*(FLOAT*)Result = I;
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_IntToFloat, execIntToFloat );

void UObject::execIntToString( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execIntToString);
	P_GET_INT(I);
	*(FString*)Result = FString::Printf(TEXT("%i"),I);
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_IntToString, execIntToString );

void UObject::execBoolToByte( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execBoolToByte);
	UBOOL B=0;
	Stack.Step( Stack.Object, &B );
	*(BYTE*)Result = B & 1;
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_BoolToByte, execBoolToByte );

void UObject::execBoolToInt( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execBoolToInt);
	UBOOL B=0;
	Stack.Step( Stack.Object, &B );
	*(INT*)Result = B & 1;
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_BoolToInt, execBoolToInt );

void UObject::execBoolToFloat( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execBoolToFloat);
	UBOOL B=0;
	Stack.Step( Stack.Object, &B );
	*(FLOAT*)Result = B & 1;
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_BoolToFloat, execBoolToFloat );

void UObject::execBoolToString( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execBoolToString);
	P_GET_UBOOL(B);
	*(FString*)Result = B ? GTrue : GFalse;
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_BoolToString, execBoolToString );

void UObject::execFloatToByte( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execFloatToByte);
	FLOAT F=0.f;
	Stack.Step( Stack.Object, &F );
	*(BYTE*)Result = (BYTE)F;
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_FloatToByte, execFloatToByte );

void UObject::execFloatToInt( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execFloatToInt);
	FLOAT F=0.f;
	Stack.Step( Stack.Object, &F );
	*(INT*)Result = (INT)F;
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_FloatToInt, execFloatToInt );

void UObject::execFloatToBool( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execFloatToBool);
	FLOAT F=0.f;
	Stack.Step( Stack.Object, &F );
	*(DWORD*)Result = F!=0.0 ? 1 : 0;
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_FloatToBool, execFloatToBool );

void UObject::execFloatToString( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execFloatToString);
	P_GET_FLOAT(F);
	*(FString*)Result = FString::Printf(TEXT("%f"),F);
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_FloatToString, execFloatToString );

void UObject::execObjectToBool( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execObjectToBool);
	UObject* Obj=NULL;
	Stack.Step( Stack.Object, &Obj );
	*(DWORD*)Result = Obj!=NULL;
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_ObjectToBool, execObjectToBool );

void UObject::execObjectToString( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execObjectToString);
	P_GET_OBJECT(UObject,Obj);
	*(FString*)Result = Obj ? Obj->GetPathName() : TEXT("None");
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_ObjectToString, execObjectToString );

void UObject::execNameToBool( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execNameToBool);
	FName N=NAME_None;
	Stack.Step( Stack.Object, &N );
	*(DWORD*)Result = N!=NAME_None ? 1 : 0;
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_NameToBool, execNameToBool );

void UObject::execNameToString( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execNameToString);
	P_GET_NAME(N);
	*(FString*)Result = *N;
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_NameToString, execNameToString );

void UObject::execStringToByte( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execStringToByte);
	P_GET_STR(Str);
	*(BYTE*)Result = appAtoi( *Str );
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_StringToByte, execStringToByte );

void UObject::execStringToInt( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execStringToInt);
	P_GET_STR(Str);
	*(INT*)Result = appAtoi( *Str );
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_StringToInt, execStringToInt );

void UObject::execStringToBool( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execStringToBool);
	P_GET_STR(Str);
	if( appStricmp(*Str,TEXT("True") )==0 || appStricmp(*Str,GTrue)==0 )
	{
		*(INT*)Result = 1;
	}
	else if( appStricmp(*Str,TEXT("False"))==0 || appStricmp(*Str,GFalse)==0 )
	{
		*(INT*)Result = 0;
	}
	else
	{
		*(INT*)Result = appAtoi(*Str) ? 1 : 0;
	}
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_StringToBool, execStringToBool );

void UObject::execStringToFloat( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execStringToFloat);
	P_GET_STR(Str);
	*(FLOAT*)Result = appAtof( *Str );
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_StringToFloat, execStringToFloat );

/////////////////////////////////////////
// Native bool operators and functions //
/////////////////////////////////////////

void UObject::execNot_PreBool( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execNot_PreBool);

	P_GET_UBOOL(A);
	P_FINISH;

	*(DWORD*)Result = !A;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 129, execNot_PreBool );

void UObject::execEqualEqual_BoolBool( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execEqualEqual_BoolBool);

	P_GET_UBOOL(A);
	P_GET_UBOOL(B);
	P_FINISH;

	*(DWORD*)Result = ((!A) == (!B));
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 242, execEqualEqual_BoolBool );

void UObject::execNotEqual_BoolBool( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execNotEqual_BoolBool);

	P_GET_UBOOL(A);
	P_GET_UBOOL(B);
	P_FINISH;

	*(DWORD*)Result = ((!A) != (!B));

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 243, execNotEqual_BoolBool );

void UObject::execAndAnd_BoolBool( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execAndAnd_BoolBool);

	P_GET_UBOOL(A);
	P_GET_SKIP_OFFSET(W);

	if( A )
	{
		P_GET_UBOOL(B);
		*(DWORD*)Result = A && B;
		P_FINISH;
	}
	else
	{
		*(DWORD*)Result = 0;
		Stack.Code += W;
	}
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 130, execAndAnd_BoolBool );

void UObject::execXorXor_BoolBool( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execXorXor_BoolBool);

	P_GET_UBOOL(A);
	P_GET_UBOOL(B);
	P_FINISH;

	*(DWORD*)Result = !A ^ !B;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 131, execXorXor_BoolBool );

void UObject::execOrOr_BoolBool( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execOrOr_BoolBool);
	P_GET_UBOOL(A);
	P_GET_SKIP_OFFSET(W);
	if( !A )
	{
		P_GET_UBOOL(B);
		*(DWORD*)Result = A || B;
		P_FINISH;
	}
	else
	{
		*(DWORD*)Result = 1;
		Stack.Code += W;
	}
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 132, execOrOr_BoolBool );

/////////////////////////////////////////
// Native byte operators and functions //
/////////////////////////////////////////

void UObject::execMultiplyEqual_ByteByte( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execMultiplyEqual_ByteByte);

	P_GET_BYTE_REF(A);
	P_GET_BYTE(B);
	P_FINISH;

	*(BYTE*)Result = (*A *= B);

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 133, execMultiplyEqual_ByteByte );

void UObject::execDivideEqual_ByteByte( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execDivideEqual_ByteByte);

	P_GET_BYTE_REF(A);
	P_GET_BYTE(B);
	P_FINISH;

	*(BYTE*)Result = B ? (*A /= B) : 0;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 134, execDivideEqual_ByteByte );

void UObject::execAddEqual_ByteByte( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execAddEqual_ByteByte);

	P_GET_BYTE_REF(A);
	P_GET_BYTE(B);
	P_FINISH;

	*(BYTE*)Result = (*A += B);

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 135, execAddEqual_ByteByte );

void UObject::execSubtractEqual_ByteByte( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execSubtractEqual_ByteByte);

	P_GET_BYTE_REF(A);
	P_GET_BYTE(B);
	P_FINISH;

	*(BYTE*)Result = (*A -= B);

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 136, execSubtractEqual_ByteByte );

void UObject::execAddAdd_PreByte( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execAddAdd_PreByte);

	P_GET_BYTE_REF(A);
	P_FINISH;

	*(BYTE*)Result = ++(*A);

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 137, execAddAdd_PreByte );

void UObject::execSubtractSubtract_PreByte( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execSubtractSubtract_PreByte);

	P_GET_BYTE_REF(A);
	P_FINISH;

	*(BYTE*)Result = --(*A);

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 138, execSubtractSubtract_PreByte );

void UObject::execAddAdd_Byte( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execAddAdd_Byte);

	P_GET_BYTE_REF(A);
	P_FINISH;

	*(BYTE*)Result = (*A)++;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 139, execAddAdd_Byte );

void UObject::execSubtractSubtract_Byte( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execSubtractSubtract_Byte);

	P_GET_BYTE_REF(A);
	P_FINISH;

	*(BYTE*)Result = (*A)--;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 140, execSubtractSubtract_Byte );

/////////////////////////////////
// Int operators and functions //
/////////////////////////////////

void UObject::execComplement_PreInt( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execComplement_PreInt);

	P_GET_INT(A);
	P_FINISH;

	*(INT*)Result = ~A;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 141, execComplement_PreInt );

void UObject::execGreaterGreaterGreater_IntInt( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execGreaterGreaterGreater_IntInt);

	P_GET_INT(A);
	P_GET_INT(B);
	P_FINISH;

	*(INT*)Result = ((DWORD)A) >> B;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 196, execGreaterGreaterGreater_IntInt );

void UObject::execSubtract_PreInt( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execSubtract_PreInt);

	P_GET_INT(A);
	P_FINISH;

	*(INT*)Result = -A;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 143, execSubtract_PreInt );

void UObject::execMultiply_IntInt( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execMultiply_IntInt);

	P_GET_INT(A);
	P_GET_INT(B);
	P_FINISH;

	*(INT*)Result = A * B;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 144, execMultiply_IntInt );

void UObject::execDivide_IntInt( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execDivide_IntInt);

	P_GET_INT(A);
	P_GET_INT(B);
	P_FINISH;

	*(INT*)Result = B ? A / B : 0;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 145, execDivide_IntInt );

void UObject::execAdd_IntInt( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execAdd_IntInt);

	P_GET_INT(A);
	P_GET_INT(B);
	P_FINISH;

	*(INT*)Result = A + B;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 146, execAdd_IntInt );

void UObject::execSubtract_IntInt( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execSubtract_IntInt);

	P_GET_INT(A);
	P_GET_INT(B);
	P_FINISH;

	*(INT*)Result = A - B;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 147, execSubtract_IntInt );

void UObject::execLessLess_IntInt( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execLessLess_IntInt);

	P_GET_INT(A);
	P_GET_INT(B);
	P_FINISH;

	*(INT*)Result = A << B;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 148, execLessLess_IntInt );

void UObject::execGreaterGreater_IntInt( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execGreaterGreater_IntInt);

	P_GET_INT(A);
	P_GET_INT(B);
	P_FINISH;

	*(INT*)Result = A >> B;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 149, execGreaterGreater_IntInt );

void UObject::execLess_IntInt( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execLess_IntInt);

	P_GET_INT(A);
	P_GET_INT(B);
	P_FINISH;

	*(DWORD*)Result = A < B;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 150, execLess_IntInt );

void UObject::execGreater_IntInt( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execGreater_IntInt);

	P_GET_INT(A);
	P_GET_INT(B);
	P_FINISH;

	*(DWORD*)Result = A > B;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 151, execGreater_IntInt );

void UObject::execLessEqual_IntInt( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execLessEqual_IntInt);

	P_GET_INT(A);
	P_GET_INT(B);
	P_FINISH;

	*(DWORD*)Result = A <= B;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 152, execLessEqual_IntInt );

void UObject::execGreaterEqual_IntInt( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execGreaterEqual_IntInt);

	P_GET_INT(A);
	P_GET_INT(B);
	P_FINISH;

	*(DWORD*)Result = A >= B;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 153, execGreaterEqual_IntInt );

void UObject::execEqualEqual_IntInt( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execEqualEqual_IntInt);

	P_GET_INT(A);
	P_GET_INT(B);
	P_FINISH;

	*(DWORD*)Result = A == B;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 154, execEqualEqual_IntInt );

void UObject::execNotEqual_IntInt( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execNotEqual_IntInt);

	P_GET_INT(A);
	P_GET_INT(B);
	P_FINISH;

	*(DWORD*)Result = A != B;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 155, execNotEqual_IntInt );

void UObject::execAnd_IntInt( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execAnd_IntInt);

	P_GET_INT(A);
	P_GET_INT(B);
	P_FINISH;

	*(INT*)Result = A & B;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 156, execAnd_IntInt );

void UObject::execXor_IntInt( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execXor_IntInt);

	P_GET_INT(A);
	P_GET_INT(B);
	P_FINISH;

	*(INT*)Result = A ^ B;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 157, execXor_IntInt );

void UObject::execOr_IntInt( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execOr_IntInt);

	P_GET_INT(A);
	P_GET_INT(B);
	P_FINISH;

	*(INT*)Result = A | B;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 158, execOr_IntInt );

void UObject::execMultiplyEqual_IntFloat( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execMultiplyEqual_IntInt);

	P_GET_INT_REF(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(INT*)Result = *A = (INT)(*A * B);

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 159, execMultiplyEqual_IntFloat );

void UObject::execDivideEqual_IntFloat( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execDivideEqual_IntInt);

	P_GET_INT_REF(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(INT*)Result = *A = (INT)(B ? *A/B : 0.f);

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 160, execDivideEqual_IntFloat );

void UObject::execAddEqual_IntInt( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execAddEqual_IntInt);

	P_GET_INT_REF(A);
	P_GET_INT(B);
	P_FINISH;

	*(INT*)Result = (*A += B);

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 161, execAddEqual_IntInt );

void UObject::execSubtractEqual_IntInt( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execSubtractEqual_IntInt);

	P_GET_INT_REF(A);
	P_GET_INT(B);
	P_FINISH;

	*(INT*)Result = (*A -= B);

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 162, execSubtractEqual_IntInt );

void UObject::execAddAdd_PreInt( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execAddAdd_PreInt);

	P_GET_INT_REF(A);
	P_FINISH;

	*(INT*)Result = ++(*A);

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 163, execAddAdd_PreInt );

void UObject::execSubtractSubtract_PreInt( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execSubtractSubtract_PreInt);

	P_GET_INT_REF(A);
	P_FINISH;

	*(INT*)Result = --(*A);

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 164, execSubtractSubtract_PreInt );

void UObject::execAddAdd_Int( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execAddAdd_Int);

	P_GET_INT_REF(A);
	P_FINISH;

	*(INT*)Result = (*A)++;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 165, execAddAdd_Int );

void UObject::execSubtractSubtract_Int( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execSubtractSubtract_Int);

	P_GET_INT_REF(A);
	P_FINISH;

	*(INT*)Result = (*A)--;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 166, execSubtractSubtract_Int );

void UObject::execRand( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execRand);

	P_GET_INT(A);
	P_FINISH;

	*(INT*)Result = A>0 ? (appRand() % A) : 0;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 167, execRand );

void UObject::execMin( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execMin);

	P_GET_INT(A);
	P_GET_INT(B);
	P_FINISH;

	*(INT*)Result = Min(A,B);

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 249, execMin );

void UObject::execMax( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execMax);

	P_GET_INT(A);
	P_GET_INT(B);
	P_FINISH;

	*(INT*)Result = Max(A,B);

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 250, execMax );

void UObject::execClamp( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execClamp);

	P_GET_INT(V);
	P_GET_INT(A);
	P_GET_INT(B);
	P_FINISH;

	*(INT*)Result = Clamp(V,A,B);

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 251, execClamp );

///////////////////////////////////
// Float operators and functions //
///////////////////////////////////

void UObject::execSubtract_PreFloat( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execSubtract_PreFloat);

	P_GET_FLOAT(A);
	P_FINISH;

	*(FLOAT*)Result = -A;

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 169, execSubtract_PreFloat );

void UObject::execMultiplyMultiply_FloatFloat( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execMultiplyMultiply_FloatFloat);

	P_GET_FLOAT(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(FLOAT*)Result = appPow(A,B);

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 170, execMultiplyMultiply_FloatFloat );

void UObject::execMultiply_FloatFloat( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execMultiply_FloatFloat);

	P_GET_FLOAT(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(FLOAT*)Result = A * B;

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 171, execMultiply_FloatFloat );

void UObject::execDivide_FloatFloat( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execDivide_FloatFloat);

	P_GET_FLOAT(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(FLOAT*)Result = A / B;

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 172, execDivide_FloatFloat );

void UObject::execPercent_FloatFloat( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execPercent_FloatFloat);

	P_GET_FLOAT(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(FLOAT*)Result = appFmod(A,B);

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 173, execPercent_FloatFloat );

void UObject::execAdd_FloatFloat( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execAdd_FloatFloat);

	P_GET_FLOAT(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(FLOAT*)Result = A + B;

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 174, execAdd_FloatFloat );

void UObject::execSubtract_FloatFloat( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execSubtract_FloatFloat);

	P_GET_FLOAT(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(FLOAT*)Result = A - B;

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 175, execSubtract_FloatFloat );

void UObject::execLess_FloatFloat( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execLess_FloatFloat);

	P_GET_FLOAT(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(DWORD*)Result = A < B;

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 176, execLess_FloatFloat );

void UObject::execGreater_FloatFloat( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execGreater_FloatFloat);

	P_GET_FLOAT(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(DWORD*)Result = A > B;

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 177, execGreater_FloatFloat );

void UObject::execLessEqual_FloatFloat( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execLessEqual_FloatFloat);

	P_GET_FLOAT(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(DWORD*)Result = A <= B;

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 178, execLessEqual_FloatFloat );

void UObject::execGreaterEqual_FloatFloat( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execGreaterEqual_FloatFloat);

	P_GET_FLOAT(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(DWORD*)Result = A >= B;

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 179, execGreaterEqual_FloatFloat );

void UObject::execEqualEqual_FloatFloat( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execEqualEqual_FloatFloat);

	P_GET_FLOAT(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(DWORD*)Result = A == B;

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 180, execEqualEqual_FloatFloat );

void UObject::execNotEqual_FloatFloat( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execNotEqual_FloatFloat);

	P_GET_FLOAT(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(DWORD*)Result = A != B;

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 181, execNotEqual_FloatFloat );

void UObject::execComplementEqual_FloatFloat( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execComplementEqual_FloatFloat);

	P_GET_FLOAT(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(DWORD*)Result = Abs(A - B) < (1.e-4);

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 210, execComplementEqual_FloatFloat );

void UObject::execMultiplyEqual_FloatFloat( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execMultiplyEqual_FloatFloat);

	P_GET_FLOAT_REF(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(FLOAT*)Result = (*A *= B);

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 182, execMultiplyEqual_FloatFloat );

void UObject::execDivideEqual_FloatFloat( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execDivideEqual_FloatFloat);

	P_GET_FLOAT_REF(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(FLOAT*)Result = (*A /= B);

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 183, execDivideEqual_FloatFloat );

void UObject::execAddEqual_FloatFloat( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execAddEqual_FloatFloat);

	P_GET_FLOAT_REF(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(FLOAT*)Result = (*A += B);

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 184, execAddEqual_FloatFloat );

void UObject::execSubtractEqual_FloatFloat( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execSubtractEqual_FloatFloat);

	P_GET_FLOAT_REF(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(FLOAT*)Result = (*A -= B);

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 185, execSubtractEqual_FloatFloat );

void UObject::execAbs( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execAbs);

	P_GET_FLOAT(A);
	P_FINISH;

	*(FLOAT*)Result = Abs(A);

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 186, execAbs );

void UObject::execSin( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execSin);

	P_GET_FLOAT(A);
	P_FINISH;

	*(FLOAT*)Result = appSin(A);

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 187, execSin );

void UObject::execCos( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execCos);

	P_GET_FLOAT(A);
	P_FINISH;

	*(FLOAT*)Result = appCos(A);

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 188, execCos );

void UObject::execTan( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execTan);

	P_GET_FLOAT(A);
	P_FINISH;

	*(FLOAT*)Result = appTan(A);

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 189, execTan );

void UObject::execAtan( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execAtan);

	P_GET_FLOAT(A);
	P_FINISH;

	*(FLOAT*)Result = appAtan(A);

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 190, execAtan );

void UObject::execExp( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execExp);

	P_GET_FLOAT(A);
	P_FINISH;

	*(FLOAT*)Result = appExp(A);

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 191, execExp );

void UObject::execLoge( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execLoge);

	P_GET_FLOAT(A);
	P_FINISH;

	*(FLOAT*)Result = appLoge(A);

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 192, execLoge );

void UObject::execSqrt( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execSqrt);

	P_GET_FLOAT(A);
	P_FINISH;

	*(FLOAT*)Result = appSqrt(A);

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 193, execSqrt );

void UObject::execSquare( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execSquare);

	P_GET_FLOAT(A);
	P_FINISH;

	*(FLOAT*)Result = Square(A);

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 194, execSquare );

void UObject::execFRand( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execFRand);

	P_FINISH;

	*(FLOAT*)Result = appFrand();

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 195, execFRand );

void UObject::execFMin( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execFMin);

	P_GET_FLOAT(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(FLOAT*)Result = Min(A,B);

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 244, execFMin );

void UObject::execFMax( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execFMax);

	P_GET_FLOAT(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(FLOAT*)Result = Max(A,B);

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 245, execFMax );

void UObject::execFClamp( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execFClamp);

	P_GET_FLOAT(V);
	P_GET_FLOAT(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(FLOAT*)Result = Clamp(V,A,B);

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 246, execFClamp );

void UObject::execLerp( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execLerp);

	P_GET_FLOAT(V);
	P_GET_FLOAT(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(FLOAT*)Result = A + V*(B-A);

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 247, execLerp );

void UObject::execSmerp( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execSmerp);

	P_GET_FLOAT(V);
	P_GET_FLOAT(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(FLOAT*)Result = A + (3.0*V*V - 2.0*V*V*V)*(B-A);

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 248, execSmerp );

void UObject::execRotationConst( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execRotationConst);
	((FRotator*)Result)->Pitch = Stack.ReadInt();
	((FRotator*)Result)->Yaw   = Stack.ReadInt();
	((FRotator*)Result)->Roll  = Stack.ReadInt();
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_RotationConst, execRotationConst );

void UObject::execVectorConst( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execVectorConst);
#if defined(PLATFORM_DREAMCAST) || defined(PLATFORM_ANDROID)
	FVector Temp;
	__builtin_memcpy(&Temp, Stack.Code, sizeof(FVector));
	*(FVector*)Result = Temp;
#else
	__builtin_memcpy( Result, Stack.Code, sizeof(FVector) );
#endif
	Stack.Code += sizeof(FVector);
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_VectorConst, execVectorConst );

/////////////////
// Conversions //
/////////////////

void UObject::execStringToVector( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execStringToVector);
	P_GET_STR(Str);

	const TCHAR* Stream = *Str;
	FVector Value(0,0,0);
	Value.X = appAtof(Stream);
	Stream = appStrchr(Stream,',');
	if( Stream )
	{
		Value.Y = appAtof(++Stream);
		Stream = appStrchr(Stream,',');
		if( Stream )
			Value.Z = appAtof(++Stream);
	}
	*(FVector*)Result = Value;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_StringToVector, execStringToVector );

void UObject::execStringToRotator( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execStringToRotator);
	P_GET_STR(Str);

	const TCHAR* Stream = *Str;
	FRotator Rotation(0,0,0);
	Rotation.Pitch = appAtoi(Stream);
	Stream = appStrchr(Stream,',');
	if( Stream )
	{
		Rotation.Yaw = appAtoi(++Stream);
		Stream = appStrchr(Stream,',');
		if( Stream )
			Rotation.Roll = appAtoi(++Stream);
	}
	*(FRotator*)Result = Rotation;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_StringToRotator, execStringToRotator );

void UObject::execVectorToBool( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execVectorToBool);
	FVector V(0,0,0);
	Stack.Step( Stack.Object, &V );
	*(DWORD*)Result = V.IsZero() ? 0 : 1;
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_VectorToBool, execVectorToBool );

void UObject::execVectorToString( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execVectorToString);
	P_GET_VECTOR(V);
	*(FString*)Result = FString::Printf( TEXT("%f,%f,%f"), V.X, V.Y, V.Z );
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_VectorToString, execVectorToString );

void UObject::execVectorToRotator( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execVectorToRotator);
	FVector V(0,0,0);
	Stack.Step( Stack.Object, &V );
	*(FRotator*)Result = V.Rotation();
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_VectorToRotator, execVectorToRotator );

void UObject::execRotatorToBool( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execRotatorToBool);
	FRotator R(0,0,0);
	Stack.Step( Stack.Object, &R );
	*(DWORD*)Result = R.IsZero() ? 0 : 1;
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_RotatorToBool, execRotatorToBool );

void UObject::execRotatorToVector( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execRotatorToVector);
	FRotator R(0,0,0);
	Stack.Step( Stack.Object, &R );
	*(FVector*)Result = R.Vector();
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_RotatorToVector, execRotatorToVector );

void UObject::execRotatorToString( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execRotatorToString);
	P_GET_ROTATOR(R);
	*(FString*)Result = FString::Printf( TEXT("%i,%i,%i"), R.Pitch&65535, R.Yaw&65535, R.Roll&65535 );
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_RotatorToString, execRotatorToString );

////////////////////////////////////
// Vector operators and functions //
////////////////////////////////////

void UObject::execSubtract_PreVector( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execSubtract_PreVector);

	P_GET_VECTOR(A);
	P_FINISH;

	*(FVector*)Result = -A;

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 0x80 + 83, execSubtract_PreVector );

void UObject::execMultiply_VectorFloat( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execMultiply_VectorFloat);

	P_GET_VECTOR(A);
	P_GET_FLOAT (B);
	P_FINISH;

	*(FVector*)Result = A*B;

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 0x80 + 84, execMultiply_VectorFloat );

void UObject::execMultiply_FloatVector( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execMultiply_FloatVector);

	P_GET_FLOAT (A);
	P_GET_VECTOR(B);
	P_FINISH;

	*(FVector*)Result = A*B;

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 0x80 + 85, execMultiply_FloatVector );

void UObject::execMultiply_VectorVector( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execMultiply_VectorVector);

	P_GET_VECTOR(A);
	P_GET_VECTOR(B);
	P_FINISH;

	*(FVector*)Result = A*B;

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 296, execMultiply_VectorVector );

void UObject::execDivide_VectorFloat( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execDivide_VectorFloat);

	P_GET_VECTOR(A);
	P_GET_FLOAT (B);
	P_FINISH;

	*(FVector*)Result = A/B;

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 0x80 + 86, execDivide_VectorFloat );

void UObject::execAdd_VectorVector( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execAdd_VectorVector);

	P_GET_VECTOR(A);
	P_GET_VECTOR(B);
	P_FINISH;

	*(FVector*)Result = A+B;

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 0x80 + 87, execAdd_VectorVector );

void UObject::execSubtract_VectorVector( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execSubtract_VectorVector);

	P_GET_VECTOR(A);
	P_GET_VECTOR(B);
	P_FINISH;

	*(FVector*)Result = A-B;

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 0x80 + 88, execSubtract_VectorVector );

void UObject::execLessLess_VectorRotator( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execLessLess_VectorRotator);

	P_GET_VECTOR(A);
	P_GET_ROTATOR(B);
	P_FINISH;

	*(FVector*)Result = A.TransformVectorBy(GMath.UnitCoords / B);

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 275, execLessLess_VectorRotator );

void UObject::execGreaterGreater_VectorRotator( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execGreaterGreater_VectorRotator);

	P_GET_VECTOR(A);
	P_GET_ROTATOR(B);
	P_FINISH;

	*(FVector*)Result = A.TransformVectorBy(GMath.UnitCoords * B);

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 276, execGreaterGreater_VectorRotator );

void UObject::execEqualEqual_VectorVector( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execEqualEqual_VectorVector);

	P_GET_VECTOR(A);
	P_GET_VECTOR(B);
	P_FINISH;

	*(DWORD*)Result = A.X==B.X && A.Y==B.Y && A.Z==B.Z;

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 0x80 + 89, execEqualEqual_VectorVector );

void UObject::execNotEqual_VectorVector( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execNotEqual_VectorVector);

	P_GET_VECTOR(A);
	P_GET_VECTOR(B);
	P_FINISH;

	*(DWORD*)Result = A.X!=B.X || A.Y!=B.Y || A.Z!=B.Z;

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 0x80 + 90, execNotEqual_VectorVector );

void UObject::execDot_VectorVector( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execDot_VectorVector);

	P_GET_VECTOR(A);
	P_GET_VECTOR(B);
	P_FINISH;

	*(FLOAT*)Result = A|B;

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 0x80 + 91, execDot_VectorVector );

void UObject::execCross_VectorVector( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execCross_VectorVector);

	P_GET_VECTOR(A);
	P_GET_VECTOR(B);
	P_FINISH;

	*(FVector*)Result = A^B;

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 0x80 + 92, execCross_VectorVector );

void UObject::execMultiplyEqual_VectorFloat( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execMultiplyEqual_VectorFloat);

	P_GET_VECTOR_REF(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(FVector*)Result = (*A *= B);

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 0x80 + 93, execMultiplyEqual_VectorFloat );

void UObject::execMultiplyEqual_VectorVector( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execMultiplyEqual_VectorVector);

	P_GET_VECTOR_REF(A);
	P_GET_VECTOR(B);
	P_FINISH;

	*(FVector*)Result = (*A *= B);

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 297, execMultiplyEqual_VectorVector );

void UObject::execDivideEqual_VectorFloat( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execDivideEqual_VectorFloat);

	P_GET_VECTOR_REF(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(FVector*)Result = (*A /= B);

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 0x80 + 94, execDivideEqual_VectorFloat );

void UObject::execAddEqual_VectorVector( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execAddEqual_VectorVector);

	P_GET_VECTOR_REF(A);
	P_GET_VECTOR(B);
	P_FINISH;

	*(FVector*)Result = (*A += B);

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 0x80 + 95, execAddEqual_VectorVector );

void UObject::execSubtractEqual_VectorVector( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execSubtractEqual_VectorVector);

	P_GET_VECTOR_REF(A);
	P_GET_VECTOR(B);
	P_FINISH;

	*(FVector*)Result = (*A -= B);

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 0x80 + 96, execSubtractEqual_VectorVector );

void UObject::execVSize( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execVSize);

	P_GET_VECTOR(A);
	P_FINISH;

	*(FLOAT*)Result = A.Size();

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 0x80 + 97, execVSize );

void UObject::execNormal( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execNormal);

	P_GET_VECTOR(A);
	P_FINISH;

	*(FVector*)Result = A.SafeNormal();

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 0x80 + 98, execNormal );

void UObject::execInvert( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execInvert);

	P_GET_VECTOR_REF(X);
	P_GET_VECTOR_REF(Y);
	P_GET_VECTOR_REF(Z);
	P_FINISH;

	FCoords Temp = FCoords( FVector(0,0,0), *X, *Y, *Z ).Inverse();
	*X           = Temp.XAxis;
	*Y           = Temp.YAxis;
	*Z           = Temp.ZAxis;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 0x80 + 99, execInvert );

void UObject::execVRand( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execVRand);
	P_FINISH;
	*((FVector*)Result) = VRand();
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 0x80 + 124, execVRand );

void UObject::execRotRand( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execRotRand);
	P_GET_UBOOL_OPTX(bRoll, 0);
	P_FINISH;

	FRotator RRot;
	RRot.Yaw = ((2 * appRand()) % 65535);
	RRot.Pitch = ((2 * appRand()) % 65535);
	if ( bRoll )
		RRot.Roll = ((2 * appRand()) % 65535);
	else
		RRot.Roll = 0;
	*((FRotator*)Result) = RRot;
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 320, execRotRand );

void UObject::execMirrorVectorByNormal( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execMirrorVectorByNormal);

	P_GET_VECTOR(A);
	P_GET_VECTOR(B);
	P_FINISH;

	B = B.SafeNormal();
	*(FVector*)Result = A - 2.f * B * (B | A);

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 300, execMirrorVectorByNormal );

//////////////////////////////////////
// Rotation operators and functions //
//////////////////////////////////////

void UObject::execEqualEqual_RotatorRotator( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execEqualEqual_RotatorRotator);

	P_GET_ROTATOR(A);
	P_GET_ROTATOR(B);
	P_FINISH;

	*(DWORD*)Result = A.Pitch==B.Pitch && A.Yaw==B.Yaw && A.Roll==B.Roll;

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 0x80 + 14, execEqualEqual_RotatorRotator );

void UObject::execNotEqual_RotatorRotator( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execNotEqual_RotatorRotator);

	P_GET_ROTATOR(A);
	P_GET_ROTATOR(B);
	P_FINISH;

	*(DWORD*)Result = A.Pitch!=B.Pitch || A.Yaw!=B.Yaw || A.Roll!=B.Roll;

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 0x80 + 75, execNotEqual_RotatorRotator );

void UObject::execMultiply_RotatorFloat( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execMultiply_RotatorFloat);

	P_GET_ROTATOR(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(FRotator*)Result = A * B;

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 287, execMultiply_RotatorFloat );

void UObject::execMultiply_FloatRotator( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execMultiply_FloatRotator);

	P_GET_FLOAT(A);
	P_GET_ROTATOR(B);
	P_FINISH;

	*(FRotator*)Result = B * A;

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 288, execMultiply_FloatRotator );

void UObject::execDivide_RotatorFloat( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execDivide_RotatorFloat);

	P_GET_ROTATOR(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(FRotator*)Result = A * (1.0/B);

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 289, execDivide_RotatorFloat );

void UObject::execMultiplyEqual_RotatorFloat( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execMultiplyEqual_RotatorFloat);

	P_GET_ROTATOR_REF(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(FRotator*)Result = (*A *= B);

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 290, execMultiplyEqual_RotatorFloat );

void UObject::execDivideEqual_RotatorFloat( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execDivideEqual_RotatorFloat);

	P_GET_ROTATOR_REF(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(FRotator*)Result = (*A *= (1.0/B));

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 291, execDivideEqual_RotatorFloat );

void UObject::execAdd_RotatorRotator( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execAdd_RotatorRotator);

	P_GET_ROTATOR(A);
	P_GET_ROTATOR(B);
	P_FINISH;

	*(FRotator*)Result = A + B;

	unguardSlow;
}
IMPLEMENT_FUNCTION( UObject, 316, execAdd_RotatorRotator );

void UObject::execSubtract_RotatorRotator( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execSubtract_RotatorRotator);

	P_GET_ROTATOR(A);
	P_GET_ROTATOR(B);
	P_FINISH;

	*(FRotator*)Result = A - B;

	unguardSlow;
}
IMPLEMENT_FUNCTION( UObject, 317, execSubtract_RotatorRotator );

void UObject::execAddEqual_RotatorRotator( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execAddEqual_RotatorRotator);

	P_GET_ROTATOR_REF(A);
	P_GET_ROTATOR(B);
	P_FINISH;

	*(FRotator*)Result = (*A += B);

	unguardSlow;
}
IMPLEMENT_FUNCTION( UObject, 318, execAddEqual_RotatorRotator );

void UObject::execSubtractEqual_RotatorRotator( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execSubtractEqual_RotatorRotator);

	P_GET_ROTATOR_REF(A);
	P_GET_ROTATOR(B);
	P_FINISH;

	*(FRotator*)Result = (*A -= B);

	unguardSlow;
}
IMPLEMENT_FUNCTION( UObject, 319, execSubtractEqual_RotatorRotator );

void UObject::execGetAxes( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execGetAxes);

	P_GET_ROTATOR(A);
	P_GET_VECTOR_REF(X);
	P_GET_VECTOR_REF(Y);
	P_GET_VECTOR_REF(Z);
	P_FINISH;

	FCoords Coords = GMath.UnitCoords / A;
	*X = Coords.XAxis;
	*Y = Coords.YAxis;
	*Z = Coords.ZAxis;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 0x80 + 101, execGetAxes );

void UObject::execGetUnAxes( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execGetUnAxes);

	P_GET_ROTATOR(A);
	P_GET_VECTOR_REF(X);
	P_GET_VECTOR_REF(Y);
	P_GET_VECTOR_REF(Z);
	P_FINISH;

	FCoords Coords = GMath.UnitCoords * A;
	*X = Coords.XAxis;
	*Y = Coords.YAxis;
	*Z = Coords.ZAxis;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 0x80 + 102, execGetUnAxes );

void UObject::execOrthoRotation( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execOrthoRotation);

	P_GET_VECTOR(X);
	P_GET_VECTOR(Y);
	P_GET_VECTOR(Z);
	P_FINISH;

	FCoords Coords( FVector(0,0,0), X, Y, Z );
	*(FRotator*)Result = Coords.OrthoRotation();

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, INDEX_NONE, execOrthoRotation );

void UObject::execNormalize( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execNormalize);

	P_GET_ROTATOR(Rot);
	P_FINISH;

	Rot.Pitch = Rot.Pitch & 0xFFFF; if( Rot.Pitch > 32767 ) Rot.Pitch -= 0x10000;
	Rot.Roll  = Rot.Roll  & 0xFFFF; if( Rot.Roll  > 32767 )	Rot.Roll  -= 0x10000;
	Rot.Yaw   = Rot.Yaw   & 0xFFFF; if( Rot.Yaw   > 32767 )	Rot.Yaw   -= 0x10000;
	*(FRotator*)Result = Rot;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, INDEX_NONE, execNormalize );

////////////////////////////////////
// Str operators and functions //
////////////////////////////////////

void UObject::execEatString( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execEatString);

	// Call function returning a string, then discard the result.
	FString String;
	Stack.Step( this, &String );

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_EatString, execEatString );

void UObject::execConcat_StringString( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execConcat_StringString);

	P_GET_STR(A);
	P_GET_STR(B);
	P_FINISH;

	*(FString*)Result = (A+B);

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 112, execConcat_StringString );

void UObject::execAt_StringString( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execAt_StringString);

	P_GET_STR(A);
	P_GET_STR(B);
	P_FINISH;

	*(FString*)Result = (A+TEXT(" ")+B);

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 168, execAt_StringString );

void UObject::execLess_StringString( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execLess_StringString);

	P_GET_STR(A);
	P_GET_STR(B);
	P_FINISH;

	*(DWORD*)Result = appStrcmp(*A,*B)<0;;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 115, execLess_StringString );

void UObject::execGreater_StringString( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execGreater_StringString);

	P_GET_STR(A);
	P_GET_STR(B);
	P_FINISH;

	*(DWORD*)Result = appStrcmp(*A,*B)>0;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 116, execGreater_StringString );

void UObject::execLessEqual_StringString( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execLessEqual_StringString);

	P_GET_STR(A);
	P_GET_STR(B);
	P_FINISH;

	*(DWORD*)Result = appStrcmp(*A,*B)<=0;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 120, execLessEqual_StringString );

void UObject::execGreaterEqual_StringString( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execGreaterEqual_StringString);

	P_GET_STR(A);
	P_GET_STR(B);
	P_FINISH;

	*(DWORD*)Result = appStrcmp(*A,*B)>=0;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 121, execGreaterEqual_StringString );

void UObject::execEqualEqual_StringString( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execEqualEqual_StringString);

	P_GET_STR(A);
	P_GET_STR(B);
	P_FINISH;

	*(DWORD*)Result = appStrcmp(*A,*B)==0;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 122, execEqualEqual_StringString );

void UObject::execNotEqual_StringString( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execNotEqual_StringString);

	P_GET_STR(A);
	P_GET_STR(B);
	P_FINISH;

	*(DWORD*)Result = appStrcmp(*A,*B)!=0;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 123, execNotEqual_StringString );

void UObject::execComplementEqual_StringString( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execComplementEqual_StringString);

	P_GET_STR(A);
	P_GET_STR(B);
	P_FINISH;

	*(DWORD*)Result = appStricmp(*A,*B)==0;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 124, execComplementEqual_StringString );

void UObject::execLen( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execLen);

	P_GET_STR(S);
	P_FINISH;

	*(INT*)Result = S.Len();

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 125, execLen );

void UObject::execInStr( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execInStr);

	P_GET_STR(S);
	P_GET_STR(A);
	P_FINISH;

	*(INT*)Result = S.InStr(A);

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 126, execInStr );

void UObject::execMid( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execMid);

	P_GET_STR(A);
	P_GET_INT(I);
	P_GET_INT_OPTX(C,65535);
	P_FINISH;

	*(FString*)Result = A.Mid(I,C);

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 127, execMid );

void UObject::execLeft( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execLeft);

	P_GET_STR(A);
	P_GET_INT(N);
	P_FINISH;

	*(FString*)Result = A.Left(N);

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 128, execLeft );

void UObject::execRight( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execRight);

	P_GET_STR(A);
	P_GET_INT(N);
	P_FINISH;

	*(FString*)Result = A.Right(N);

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 234, execRight );

void UObject::execCaps( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execCaps);

	P_GET_STR(A);
	P_FINISH;

	*(FString*)Result = A.Caps();

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 235, execCaps );

void UObject::execChr( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execChr);

	P_GET_INT(i);
	P_FINISH;

	TCHAR Temp[2];
	Temp[0] = i;
	Temp[1] = 0;
	*(FString*)Result = Temp;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 236, execChr );

void UObject::execAsc( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execAsc);

	P_GET_STR(S);
	P_FINISH;

	*(INT*)Result = **S;	

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 237, execAsc );

/////////////////////////////////////////
// Native name operators and functions //
/////////////////////////////////////////

void UObject::execEqualEqual_NameName( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execEqualEqual_NameName);

	P_GET_NAME(A);
	P_GET_NAME(B);
	P_FINISH;

	*(DWORD*)Result = A == B;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 254, execEqualEqual_NameName );

void UObject::execNotEqual_NameName( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execNotEqual_NameName);

	P_GET_NAME(A);
	P_GET_NAME(B);
	P_FINISH;

	*(DWORD*)Result = A != B;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 255, execNotEqual_NameName );

////////////////////////////////////
// Object operators and functions //
////////////////////////////////////

void UObject::execEqualEqual_ObjectObject( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execEqualEqual_ObjectObject);

	P_GET_OBJECT(UObject,A);
	P_GET_OBJECT(UObject,B);
	P_FINISH;

	*(DWORD*)Result = A == B;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 114, execEqualEqual_ObjectObject );

void UObject::execNotEqual_ObjectObject( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execNotEqual_ObjectObject);

	P_GET_OBJECT(UObject,A);
	P_GET_OBJECT(UObject,B);
	P_FINISH;

	*(DWORD*)Result = A != B;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 119, execNotEqual_ObjectObject );

/////////////////////////////
// Log and error functions //
/////////////////////////////

void UObject::execLog( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execLog);

	P_GET_STR(S);
	P_GET_NAME_OPTX(N,NAME_ScriptLog);
	P_FINISH;

	debugf( (EName)N.GetIndex(), TEXT("%s"), *S );

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 231, execLog );

void UObject::execWarn( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execWarn);

	P_GET_STR(S);
	P_FINISH;

	Stack.Logf( TEXT("%s"), *S );

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 232, execWarn );

void UObject::execLocalize( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execLocalize);

	P_GET_STR(SectionName);
	P_GET_STR(KeyName);
	P_GET_STR(PackageName);
	P_FINISH;

	*(FString*)Result = Localize( *SectionName, *KeyName, *PackageName );

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, INDEX_NONE, execLocalize );

//////////////////
// High natives //
//////////////////

#define HIGH_NATIVE(n) \
void UObject::execHighNative##n( FFrame& Stack, RESULT_DECL ) \
{ \
	guardSlow(UObject::execHighNative##n); \
	BYTE B = *Stack.Code++; \
	(this->*GNatives[ n*0x100 + B ])( Stack, Result ); \
	unguardexecSlow; \
} \
IMPLEMENT_FUNCTION( UObject, 0x60 + n, execHighNative##n );

HIGH_NATIVE(0);
HIGH_NATIVE(1);
HIGH_NATIVE(2);
HIGH_NATIVE(3);
HIGH_NATIVE(4);
HIGH_NATIVE(5);
HIGH_NATIVE(6);
HIGH_NATIVE(7);
HIGH_NATIVE(8);
HIGH_NATIVE(9);
HIGH_NATIVE(10);
HIGH_NATIVE(11);
HIGH_NATIVE(12);
HIGH_NATIVE(13);
HIGH_NATIVE(14);
HIGH_NATIVE(15);
#undef HIGH_NATIVE

/////////////////////////
// Object construction //
/////////////////////////

void UObject::execNew( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execNew);

	// Get parameters.
	P_GET_OBJECT_OPTX(UObject,Outer,GetIndex()!=INDEX_NONE ? this : NULL);
	P_GET_NAME_OPTX(Name,NAME_None);
	P_GET_INT_OPTX(Flags,0);
	P_GET_OBJECT_OPTX(UClass,Cls,NULL);

	// Validate parameters.
	if( Flags & ~RF_ScriptMask )
		Stack.Logf( TEXT("new: Flags %08X not allowed"), Flags & ~RF_ScriptMask );

	// Construct new object.
	if( !Outer )
		Outer = GetTransientPackage();
	*(UObject**)Result = StaticConstructObject( Cls, Outer, Name, Flags&RF_ScriptMask, NULL, &Stack );

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_New, execNew );

/////////////////////////////
// Class related functions //
/////////////////////////////

void UObject::execClassIsChildOf( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execClassIsChildOf);

	P_GET_OBJECT(UClass,K);
	P_GET_OBJECT(UClass,C);
	P_FINISH;

	*(DWORD*)Result = (C && K) ? K->IsChildOf(C) : 0;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 258, execClassIsChildOf );

///////////////////////////////
// State and label functions //
///////////////////////////////

void UObject::execGotoState( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execGotoState);

	// Get parameters.
	FName CurrentStateName = (StateFrame && StateFrame->StateNode!=Class) ? StateFrame->StateNode->GetFName() : NAME_None;
	P_GET_NAME_OPTX( S, CurrentStateName );
	P_GET_NAME_OPTX( L, NAME_None );
	P_FINISH;

	// Go to the state.
	EGotoState Result = GOTOSTATE_Success;
	if( S!=CurrentStateName )
		Result = GotoState( S );

	// Handle success.
	if( Result==GOTOSTATE_Success )
	{
		// Now go to the label.
		if( !GotoLabel( L==NAME_None ? NAME_Begin : L ) && L!=NAME_None )
			Stack.Logf( TEXT("GotoState (%s %s): Label not found"), *S, *L );
	}
	else if( Result==GOTOSTATE_NotFound )
	{
		// Warning.
		if( S!=NAME_None && S!=NAME_Auto )
			Stack.Logf( TEXT("GotoState (%s %s): State not found"), *S, *L );
	}
	else
	{
		// Safely preempted by another GotoState.
	}
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 113, execGotoState );

void UObject::execEnable( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execEnable);

	P_GET_NAME(N);
	if( N.GetIndex()>=NAME_PROBEMIN && N.GetIndex()<NAME_PROBEMAX && StateFrame )
	{
		QWORD BaseProbeMask = (GetStateFrame()->StateNode->ProbeMask | GetClass()->ProbeMask) & GetStateFrame()->StateNode->IgnoreMask;
		GetStateFrame()->ProbeMask |= (BaseProbeMask & ((QWORD)1<<(N.GetIndex()-NAME_PROBEMIN)));
	}
	else Stack.Logf( TEXT("Enable: '%s' is not a probe function"), *N );
	P_FINISH;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 117, execEnable );

void UObject::execDisable( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execDiable);

	P_GET_NAME(N);
	P_FINISH;

	if( N.GetIndex()>=NAME_PROBEMIN && N.GetIndex()<NAME_PROBEMAX && StateFrame )
		GetStateFrame()->ProbeMask &= ~((QWORD)1<<(N.GetIndex()-NAME_PROBEMIN));
	else
		Stack.Logf( TEXT("Enable: '%s' is not a probe function"), *N );

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 118, execDisable );

///////////////////
// Property text //
///////////////////

void UObject::execGetPropertyText( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execGetPropertyText);

	P_GET_STR(PropName);
	P_FINISH;

	UProperty* Property=FindField<UProperty>( Class, *PropName );
	if( Property && (Property->GetFlags() & RF_Public) )
	{
		TCHAR Temp[1024]=TEXT("");//!!
		Property->ExportText( 0, Temp, (BYTE*)this, (BYTE*)this, PPF_Localized );
		*(FString*)Result = Temp;
	}
	else *(FString*)Result = TEXT("");

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, INDEX_NONE, execGetPropertyText );

void UObject::execSetPropertyText( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execSetPropertyText);

	P_GET_STR(PropName);
	P_GET_STR(PropValue);
	P_FINISH;

	UProperty* Property=FindField<UProperty>( Class, *PropName );
	if
	(	(Property)
	&&	(Property->GetFlags() & RF_Public)
	&&	!(Property->PropertyFlags & CPF_Const) )
		Property->ImportText( *PropValue, (BYTE*)this + Property->Offset, PPF_Localized );

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, INDEX_NONE, execSetPropertyText );

void UObject::execSaveConfig( FFrame& Stack, RESULT_DECL )
{
	guard(UObject::execSaveConfig);
	P_FINISH;
	SaveConfig();
	unguard;
}
IMPLEMENT_FUNCTION( UObject, 536, execSaveConfig);

void UObject::execStaticSaveConfig( FFrame& Stack, RESULT_DECL )
{
	guard(UObject::execStaticSaveConfig);
	P_FINISH;
	Class->GetDefaultObject()->SaveConfig();
	unguard;
}
IMPLEMENT_FUNCTION( UObject, INDEX_NONE, execStaticSaveConfig);

void UObject::execResetConfig( FFrame& Stack, RESULT_DECL )
{
	guard(UObject::execResetConfig);
	P_FINISH;
	ResetConfig(GetClass());
	unguard;
}
IMPLEMENT_FUNCTION( UObject, 543, execResetConfig);

void UObject::execGetEnum( FFrame& Stack, RESULT_DECL )
{
	guard(UObject::execGetEnum);

	P_GET_OBJECT(UObject,E);
	P_GET_INT(i);
	P_FINISH;

	*(FName*)Result = NAME_None;
	if( Cast<UEnum>(E) && i>=0 && i<Cast<UEnum>(E)->Names.Num() )
		*(FName*)Result = Cast<UEnum>(E)->Names(i);

	unguard;
}
IMPLEMENT_FUNCTION( UObject, INDEX_NONE, execGetEnum);

void UObject::execDynamicLoadObject( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execDynamicLoadObject);

	P_GET_STR(Name);
	P_GET_OBJECT(UClass,Class);
	P_GET_UBOOL_OPTX(bMayFail,0);
	P_FINISH;

	*(UObject**)Result = StaticLoadObject( Class, NULL, *Name, NULL, LOAD_NoWarn | (bMayFail?LOAD_Quiet:0), NULL );

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, INDEX_NONE, execDynamicLoadObject );

void UObject::execIsInState( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execIsInState);

	P_GET_NAME(StateName);
	P_FINISH;

	if( StateFrame )
		for( UState* Test=StateFrame->StateNode; Test; Test=Test->GetSuperState() )
			if( Test->GetFName()==StateName )
				{*(DWORD*)Result=1; return;}
	*(DWORD*)Result = 0;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 281, execIsInState );

void UObject::execGetStateName( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execGetStateName);
	P_FINISH;
	*(FName*)Result = (StateFrame && StateFrame->StateNode) ? StateFrame->StateNode->GetFName() : NAME_None;
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 284, execGetStateName );

void UObject::execIsA( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execIsA);

	P_GET_NAME(ClassName);
	P_FINISH;

	UClass* TempClass;
	for( TempClass=GetClass(); TempClass; TempClass=TempClass->GetSuperClass() )
		if( TempClass->GetFName() == ClassName )
			break;
	*(DWORD*)Result = (TempClass!=NULL);

	unguardexecSlow;
}
IMPLEMENT_FUNCTION(UObject,303,execIsA);

/*-----------------------------------------------------------------------------
	Native iterator functions.
-----------------------------------------------------------------------------*/

void UObject::execIterator( FFrame& Stack, RESULT_DECL )
{}
IMPLEMENT_FUNCTION( UObject, EX_Iterator, execIterator );

/*-----------------------------------------------------------------------------
	Native registry.
-----------------------------------------------------------------------------*/

//
// Register a native function.
// Warning: Called at startup time, before engine initialization.
//
BYTE CORE_API GRegisterNative( INT iNative, const Native& Func )
{
	static int Initialized = 0;
	if( !Initialized )
	{
		Initialized = 1;
		for( int i=0; i<ARRAY_COUNT(GNatives); i++ )
			GNatives[i] = &UObject::execUndefined;
	}
	if( iNative != INDEX_NONE )
	{
		if( iNative<0 || iNative>ARRAY_COUNT(GNatives) || GNatives[iNative]!=&UObject::execUndefined) 
			GNativeDuplicate = iNative;
		GNatives[iNative] = Func;
	}
	return 0;
}

/*-----------------------------------------------------------------------------
	Script processing function.
-----------------------------------------------------------------------------*/

//
// Information remembered about an Out parameter.
//
struct FOutParmRec
{
	UProperty* Property;
	BYTE*      PropAddr;
};

//
// Call a function.
//
void UObject::CallFunction( FFrame& Stack, RESULT_DECL, UFunction* Function )
{
	guardSlow(UObject::CallFunction);
#if DO_GUARD_SLOW
	DWORD Cycles=0; clock(Cycles);
#endif
	// Found it.
	UBOOL SkipIt = 0;
	if( Function->iNative )
	{
		// Call native final function.
		(this->*Function->Func)( Stack, Result );
	}
	else if( Function->FunctionFlags & FUNC_Native )
	{
		// Call native networkable function.
		BYTE Buffer[1024];
		if( !ProcessRemoteFunction( Function, Buffer, &Stack ) )
		{
			// Call regular native function.
			(this->*Function->Func)( Stack, Result );
		}
		else
		{
			// Eat up the remaining parameters in the stream.
			SkipIt = 1;
			goto Temporary;
		}
	}
	else
	{
		// Make new stack frame in the current context.
		Temporary:
		BYTE* Frame = (BYTE*)appAlloca(Function->PropertiesSize);
		appMemzero( Frame, Function->PropertiesSize );
		FFrame NewStack( this, Function, 0, Frame );
		FOutParmRec Outs[MAX_FUNC_PARMS], *Out = Outs;
		for( UProperty* Property=(UProperty*)Function->Children; *Stack.Code!=EX_EndFunctionParms; Property=(UProperty*)Property->Next )
		{
			GPropAddr = NULL;
			Stack.Step( Stack.Object, NewStack.Locals + Property->Offset );
			if( (Property->PropertyFlags & CPF_OutParm) && GPropAddr )
			{
				Out->PropAddr = GPropAddr;
				Out->Property = Property;
				Out++;
			}
		}
		Stack.Code++;

		// Execute the code.
		if( !SkipIt )
			ProcessInternal( NewStack, Result );

		// Copy back outparms.
		while( --Out >= Outs )
			Out->Property->CopyCompleteValue( Out->PropAddr, NewStack.Locals + Out->Property->Offset );

		// Destruct properties on the stack.
		for( UProperty* Destruct=Function->ConstructorLink; Destruct; Destruct=Destruct->ConstructorLinkNext )
			Destruct->DestroyValue( NewStack.Locals + Destruct->Offset );
	}
#if DO_GUARD_SLOW
	unclock(Cycles);
	Function->Cycles += Cycles;
	Function->Calls++;
#endif
	unguardSlow;
}

//
// Internal function call processing.
//!!might not write anything to Result if singular or proper type isn't returned.
//
void UObject::ProcessInternal( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::ProcessInternal);
	DWORD SingularFlag = ((UFunction*)Stack.Node)->FunctionFlags & FUNC_Singular;
	if
	(	!ProcessRemoteFunction( (UFunction*)Stack.Node, Stack.Locals, NULL )
	&&	IsProbing( Stack.Node->GetFName() )
	&&	!(ObjectFlags & SingularFlag) )
	{
		ObjectFlags |= SingularFlag;
		BYTE Buffer[1024];//!!hardcoded size
		appMemzero( Buffer, sizeof(FString) );//!!
#if DO_GUARD
		if( ++Recurse > RECURSE_LIMIT )
			Stack.Logf( NAME_Critical, TEXT("Infinite script recursion (%i calls) detected"), RECURSE_LIMIT );
#endif
		while( *Stack.Code != EX_Return )
			Stack.Step( Stack.Object, Buffer );
		Stack.Code++;
		Stack.Step( Stack.Object, Result );
		ObjectFlags &= ~SingularFlag;
#if DO_GUARD
		--Recurse;
#endif
	}
	unguardSlow;
}

//
// Script processing function.
//
void UObject::ProcessEvent( UFunction* Function, void* Parms, void* UnusedResult )
{
	guard(UObject::ProcessEvent);

	// Reject.
	if
	(	!GIsScriptable
	||	!IsProbing( Function->GetFName() )
	||	IsPendingKill()
	||	Function->iNative
	||	((Function->FunctionFlags & FUNC_Native) && ProcessRemoteFunction( Function, Parms, NULL )) )
		return;
	checkSlow(Function->ParmsSize==0 || Parms!=NULL);

	// Start timer.
	if( ++GScriptEntryTag == 1 )
		clock(GScriptCycles);

	// Create a new local execution stack.
	FFrame NewStack( this, Function, 0, appAlloca(Function->PropertiesSize) );
	appMemcpy( NewStack.Locals, Parms, Function->ParmsSize );
	appMemzero( NewStack.Locals+Function->ParmsSize, Function->PropertiesSize-Function->ParmsSize );

	// Call native function or UObject::ProcessInternal.
	(this->*Function->Func)( NewStack, NewStack.Locals+Function->ReturnValueOffset );

	// Copy everything back.
	appMemcpy( Parms, NewStack.Locals, Function->ParmsSize );

	// Destroy local variables except function parameters.!! see also UObject::ScriptConsoleExec
	for( UProperty* P=Function->ConstructorLink; P; P=P->ConstructorLinkNext )
		if( P->Offset >= Function->ParmsSize )
			P->DestroyValue( NewStack.Locals + P->Offset );

	// Stop timer.
	if( --GScriptEntryTag == 0 )
		unclock(GScriptCycles);

	unguardf(( TEXT("(%s, %s)"), GetFullName(), Function->GetFullName() ));
}

//
// Execute the state code of the object.
//
void UObject::ProcessState( FLOAT DeltaSeconds )
{}

//
// Process a remote function; returns 1 if remote, 0 if local.
//
UBOOL UObject::ProcessRemoteFunction( UFunction* Function, void* Parms, FFrame* Stack )
{
	return 0;
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
