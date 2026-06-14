/*=============================================================================
	UnScript.cpp: UnrealScript engine support code.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

Description:
	UnrealScript execution and support code.

Revision history:
	* Created by Tim Sweeney
=============================================================================*/

#include "EnginePrivate.h"
#include "UnRender.h"
#include "UnNet.h"

#if defined(__ANDROID__) || defined(PLATFORM_ANDROID)
#include <android/log.h>
#define UT99_ANDROID_MOVER_LOGI(...) __android_log_print(ANDROID_LOG_INFO, "UT99Mover", __VA_ARGS__)
#define UT99_ANDROID_AUDIO_LOGI(...) __android_log_print(ANDROID_LOG_INFO, "UT99Audio", __VA_ARGS__)

// UT99_ANDROID_V131_CITYINTRO_VOICE_MIDCAP:
// CityIntro uses PlayerPawn.ClientPlaySound() for the intro narration.  That
// script path plays the same sample four times at Volume=16 on different slots
// (None, Interface, Misc, Talk).  On Android handheld/TV speakers this easily
// clips the old UE1 mixer and sounds like an overdriven speaker.  Keep the fix
// native so it also works with retail Engine.u/Botpack.u data: only during
// CityIntro, only for the high-volume ClientPlaySound style calls, keep one
// Interface-slot voice at a moderate gain and suppress the three duplicate layers.
static INT GUT99AndroidIntroVoiceSoftcapLogCount = 0;

static UBOOL UT99AndroidIsCityIntroLevel( AActor* Actor )
{
	if( !Actor || !Actor->GetLevel() )
		return 0;

	const TCHAR* MapName = *Actor->GetLevel()->URL.Map;
	if( MapName && (appStricmp(MapName,TEXT("CityIntro"))==0 || appStricmp(MapName,TEXT("CityIntro.unr"))==0) )
		return 1;

	UObject* Outer = Actor->GetLevel()->GetOuter();
	return Outer && appStricmp(Outer->GetName(),TEXT("CityIntro"))==0;
}

static UBOOL UT99AndroidIsASGuardiaLevel( AActor* Actor )
{
	if( !Actor || !Actor->GetLevel() )
		return 0;

	const TCHAR* MapName = *Actor->GetLevel()->URL.Map;
	if( MapName && (appStricmp(MapName,TEXT("AS-Guardia"))==0 || appStricmp(MapName,TEXT("AS-Guardia.unr"))==0) )
		return 1;

	UObject* Outer = Actor->GetLevel()->GetOuter();
	return Outer && appStricmp(Outer->GetName(),TEXT("AS-Guardia"))==0;
}

static UBOOL UT99AndroidCallerLooksLikeClientPlaySound( UFunction* Caller )
{
	if( !Caller )
		return 0;
	return appStricmp(Caller->GetName(),TEXT("ClientPlaySound"))==0
		|| appStricmp(Caller->GetName(),TEXT("ClientReliablePlaySound"))==0;
}

static UBOOL UT99AndroidMaybeFilterCityIntroVoice( AActor* Actor, UFunction* Caller, USound* Sound, BYTE Slot, FLOAT& Volume )
{
	if( !Sound || !UT99AndroidIsCityIntroLevel(Actor) )
		return 0;

	if( Slot!=SLOT_None && Slot!=SLOT_Interface && Slot!=SLOT_Misc && Slot!=SLOT_Talk )
		return 0;

	// The intro narration path is Volume=16.  Do not touch normal quiet UI/gameplay sounds.
	if( Volume < 8.0f )
		return 0;

	// Prefer the exact caller check, but keep a high-volume fallback because some
	// retail bytecode/native combinations do not preserve Stack.Node reliably here.
	if( !UT99AndroidCallerLooksLikeClientPlaySound(Caller) && Volume < 15.0f )
		return 0;

	const FLOAT OldVolume = Volume;
	const UBOOL bKeepInterfaceLayer = Slot==SLOT_Interface;
	if( bKeepInterfaceLayer )
		Volume = 1.15f;

	if( GUT99AndroidIntroVoiceSoftcapLogCount < 8 )
	{
		UT99_ANDROID_AUDIO_LOGI(
			"UT99_ANDROID_V131_CITYINTRO_VOICE_MIDCAP %s map=%s actor=%s sound=%s slot=%d volume=%.2f->%.2f caller=%s",
			bKeepInterfaceLayer ? "keep" : "suppress",
			Actor && Actor->GetLevel() ? appToAnsi(*Actor->GetLevel()->URL.Map) : "None",
			Actor ? appToAnsi(Actor->GetName()) : "None",
			appToAnsi(Sound->GetFullName()),
			(INT)Slot,
			OldVolume,
			Volume,
			Caller ? appToAnsi(Caller->GetName()) : "None" );
		GUT99AndroidIntroVoiceSoftcapLogCount++;
	}

	return bKeepInterfaceLayer ? 0 : 1;
}


// UT99_ANDROID_V130_PRESSURE_RETRIGGER_NATIVE_TARGET_FIX + V153_STANDOPEN_FINISH_ONLY_FIX + V168Q_PRESSURE_SECOND_RETRIGGER_SOUND:
// V116 keeps the generic same-key mover finish path passive again. Pressure uses
// TriggerControl movers; forcing an extra UnTrigger after a same-key finish can
// re-enter the Close label forever, leaving pressure doors open or sound loops.
// V148/V151 proved the two bad no-op directions (1->1 while closing, 0->0
// while opening).  The remaining bug was not another single direction: after
// several rides StandOpenTimed could keep stale PrevKeyNum/ClientUpdate state,
// so the next Attach restarted only the sound or replayed the last interpolation.
// V153 keeps the proven same-key direction repairs, but removes the v152
// Attach suppressor entirely.  The stable fix is now only at movement finish:
// after every real StandOpenTimed open/close finish, normalize key/prev/client
// state so the next normal Attach can start a fresh full cycle.
static FName UT99AndroidMoverStateName( AMover* Mover )
{
	return (Mover && Mover->GetStateFrame() && Mover->GetStateFrame()->StateNode && Mover->GetStateFrame()->StateNode!=Mover->GetClass())
		? Mover->GetStateFrame()->StateNode->GetFName()
		: NAME_None;
}

static UBOOL UT99AndroidIsStandOpenTimedMover( AMover* Mover, FName* OutStateName=NULL )
{
	FName StateName = UT99AndroidMoverStateName( Mover );
	if( OutStateName )
		*OutStateName = StateName;
	return StateName == FName(TEXT("StandOpenTimed"), FNAME_Find);
}

static UBOOL UT99AndroidIsBumpOpenTimedMover( AMover* Mover, FName* OutStateName=NULL )
{
	FName StateName = UT99AndroidMoverStateName( Mover );
	if( OutStateName )
		*OutStateName = StateName;
	return StateName == FName(TEXT("BumpOpenTimed"), FNAME_Find);
}

static UBOOL UT99AndroidIsTriggerOpenTimedMover( AMover* Mover, FName* OutStateName=NULL )
{
	FName StateName = UT99AndroidMoverStateName( Mover );
	if( OutStateName )
		*OutStateName = StateName;
	return StateName == FName(TEXT("TriggerOpenTimed"), FNAME_Find);
}

static UBOOL UT99AndroidIsTriggerControlMover( AMover* Mover, FName* OutStateName=NULL )
{
	FName StateName = UT99AndroidMoverStateName( Mover );
	if( OutStateName )
		*OutStateName = StateName;
	return StateName == FName(TEXT("TriggerControl"), FNAME_Find);
}

static UBOOL UT99AndroidIsTriggerToggleMover( AMover* Mover, FName* OutStateName=NULL )
{
	FName StateName = UT99AndroidMoverStateName( Mover );
	if( OutStateName )
		*OutStateName = StateName;
	return StateName == FName(TEXT("TriggerToggle"), FNAME_Find);
}

static UBOOL UT99AndroidIsASGuardiaSlidingDoorName( AMover* Mover )
{
	if( !Mover )
		return 0;
	const TCHAR* Name = Mover->GetName();
	return appStricmp(Name,TEXT("Mover0"))==0
		|| appStricmp(Name,TEXT("Mover1"))==0;
}

static UBOOL UT99AndroidIsGuardiaScriptOwnedTriggerControlMover( AMover* Mover, FName StateName )
{
	return Mover
		&& StateName==FName(TEXT("TriggerControl"), FNAME_Find)
		&& UT99AndroidIsASGuardiaLevel(Mover)
		&& UT99AndroidIsASGuardiaSlidingDoorName(Mover);
}

static void UT99AndroidReplaceMoverInterpolation( AMover* Mover, BYTE NewKeyNum, FLOAT Seconds )
{
	if( !Mover )
		return;

	NewKeyNum = (BYTE)Clamp( (INT)NewKeyNum, 0, (INT)ARRAY_COUNT(Mover->KeyPos)-1 );

	if( NewKeyNum==Mover->PrevKeyNum && Mover->KeyNum!=Mover->PrevKeyNum )
	{
		// Same smooth reverse behavior as Mover.InterpolateTo().
		Mover->PhysAlpha = 1.0f - Mover->PhysAlpha;
		Mover->OldPos    = Mover->BasePos + Mover->KeyPos[Mover->KeyNum];
		Mover->OldRot    = Mover->BaseRot + Mover->KeyRot[Mover->KeyNum];
	}
	else
	{
		Mover->OldPos    = Mover->Location;
		Mover->OldRot    = Mover->Rotation;
		Mover->PhysAlpha = 0.0f;
	}

	Mover->setPhysics(PHYS_MovingBrush);
	Mover->bInterpolating = 1;
	Mover->PrevKeyNum     = Mover->KeyNum;
	Mover->KeyNum         = NewKeyNum;
	Mover->PhysRate       = 1.0f / ::Max(Seconds, 0.005f);

	// The bad no-op interpolation already incremented ClientUpdate in script.
	// Replace it in-place instead of adding another pending update.
	if( Mover->ClientUpdate <= 0 )
		Mover->ClientUpdate = 1;

	Mover->SimOldPos      = Mover->OldPos;
	Mover->SimOldRotYaw   = Mover->OldRot.Yaw;
	Mover->SimOldRotPitch = Mover->OldRot.Pitch;
	Mover->SimOldRotRoll  = Mover->OldRot.Roll;
	Mover->SimInterpolate.X = 100.0f * Mover->PhysAlpha;
	Mover->SimInterpolate.Y = 100.0f * ::Max(0.01f, Mover->PhysRate);
	Mover->SimInterpolate.Z = 256.0f * Mover->PrevKeyNum + Mover->KeyNum;
}

static void UT99AndroidNormalizeStandOpenFinish
(
	AMover* Mover,
	FName StateName,
	BYTE FinishKeyBefore,
	BYTE FinishPrevBefore,
	INT ClientBeforeEvent,
	BYTE KeyAfterEvent,
	BYTE PrevAfterEvent,
	INT ClientAfterEvent
)
{
	if( !Mover || StateName!=FName(TEXT("StandOpenTimed"), FNAME_Find) )
		return;

	if( Mover->Level && Mover->Level->NetMode==NM_Client )
		return;

	const UBOOL bFinishedOpening = FinishKeyBefore>FinishPrevBefore && FinishKeyBefore>0;
	const UBOOL bFinishedClosing = FinishKeyBefore==0 && FinishPrevBefore>0;
	const UBOOL bFinishedSameKey = FinishKeyBefore==FinishPrevBefore;

	if( !bFinishedOpening && !bFinishedClosing && !bFinishedSameKey )
		return;

	const BYTE KeyBeforeNormalize  = Mover->KeyNum;
	const BYTE PrevBeforeNormalize = Mover->PrevKeyNum;
	const INT ClientBeforeNormalize = Mover->ClientUpdate;

	if( bFinishedOpening )
	{
		// Stable top invariant: key 1, previous 1, no pending native update.
		// Keep bOpening=true; the StandOpenTimed script uses that while sleeping
		// before DoClose() clears it.
		Mover->KeyNum = FinishKeyBefore;
		Mover->PrevKeyNum = FinishKeyBefore;
	}
	else if( bFinishedClosing )
	{
		// Stable bottom invariant: key 0, previous 0, ready for a fresh Attach.
		Mover->KeyNum = 0;
		Mover->PrevKeyNum = 0;
		Mover->bOpening = 0;
	}
	else
	{
		// Harmless no-op that the script was waiting on.  Do not let it leave
		// stale PrevKeyNum/ClientUpdate behind.
		Mover->PrevKeyNum = Mover->KeyNum;
	}

	Mover->PhysAlpha = 0.0f;
	Mover->bInterpolating = 0;
	Mover->ClientUpdate = 0;
	Mover->AmbientSound = NULL;
	Mover->bDelaying = 0;
	Mover->RealPosition = Mover->Location;
	Mover->RealRotation = Mover->Rotation;

	UT99_ANDROID_MOVER_LOGI(
		"UT99_ANDROID_V153_STANDOPEN_FINISH_NORMALIZE name=%s state=%s kind=%s before=%d/%d client=%d event=%d/%d client=%d normalize=%d/%d client=%d -> %d/%d client=%d opening=%d ambient=%d",
		appToAnsi(Mover->GetName()),
		StateName!=NAME_None ? appToAnsi(*StateName) : "None",
		bFinishedOpening ? "open" : (bFinishedClosing ? "close" : "same"),
		(INT)FinishKeyBefore,
		(INT)FinishPrevBefore,
		ClientBeforeEvent,
		(INT)KeyAfterEvent,
		(INT)PrevAfterEvent,
		ClientAfterEvent,
		(INT)KeyBeforeNormalize,
		(INT)PrevBeforeNormalize,
		ClientBeforeNormalize,
		(INT)Mover->KeyNum,
		(INT)Mover->PrevKeyNum,
		(INT)Mover->ClientUpdate,
		(INT)Mover->bOpening,
		Mover->AmbientSound ? 1 : 0 );
}


static void UT99AndroidNormalizeBumpOpenFinish
(
	AMover* Mover,
	FName StateName,
	BYTE FinishKeyBefore,
	BYTE FinishPrevBefore,
	INT ClientBeforeEvent,
	BYTE KeyAfterEvent,
	BYTE PrevAfterEvent,
	INT ClientAfterEvent
)
{
	if( !Mover || StateName!=FName(TEXT("BumpOpenTimed"), FNAME_Find) )
		return;

	if( Mover->Level && Mover->Level->NetMode==NM_Client )
		return;

	const UBOOL bFinishedOpening = FinishKeyBefore>FinishPrevBefore && FinishKeyBefore>0;
	const UBOOL bFinishedClosing = FinishKeyBefore==0 && FinishPrevBefore>0;
	const UBOOL bFinishedSameKey = FinishKeyBefore==FinishPrevBefore;

	if( !bFinishedOpening && !bFinishedClosing && !bFinishedSameKey )
		return;

	const BYTE KeyBeforeNormalize  = Mover->KeyNum;
	const BYTE PrevBeforeNormalize = Mover->PrevKeyNum;
	const INT ClientBeforeNormalize = Mover->ClientUpdate;

	if( bFinishedOpening )
	{
		Mover->KeyNum = FinishKeyBefore;
		Mover->PrevKeyNum = FinishKeyBefore;
	}
	else if( bFinishedClosing )
	{
		Mover->KeyNum = 0;
		Mover->PrevKeyNum = 0;
		Mover->bOpening = 0;
	}
	else
	{
		Mover->PrevKeyNum = Mover->KeyNum;
	}

	Mover->PhysAlpha = 0.0f;
	Mover->bInterpolating = 0;
	Mover->ClientUpdate = 0;
	Mover->AmbientSound = NULL;
	Mover->bDelaying = 0;
	Mover->RealPosition = Mover->Location;
	Mover->RealRotation = Mover->Rotation;

	UT99_ANDROID_MOVER_LOGI(
		"UT99_ANDROID_V168M_BUMPOPENTIMED_FINISH_NORMALIZE name=%s state=%s kind=%s before=%d/%d client=%d event=%d/%d client=%d normalize=%d/%d client=%d -> %d/%d client=%d opening=%d ambient=%d",
		appToAnsi(Mover->GetName()),
		StateName!=NAME_None ? appToAnsi(*StateName) : "None",
		bFinishedOpening ? "open" : (bFinishedClosing ? "close" : "same"),
		(INT)FinishKeyBefore,
		(INT)FinishPrevBefore,
		ClientBeforeEvent,
		(INT)KeyAfterEvent,
		(INT)PrevAfterEvent,
		ClientAfterEvent,
		(INT)KeyBeforeNormalize,
		(INT)PrevBeforeNormalize,
		ClientBeforeNormalize,
		(INT)Mover->KeyNum,
		(INT)Mover->PrevKeyNum,
		(INT)Mover->ClientUpdate,
		(INT)Mover->bOpening,
		Mover->AmbientSound ? 1 : 0 );
}


static void UT99AndroidNormalizeEventTriggerFinish
(
	AMover* Mover,
	FName StateName,
	BYTE FinishKeyBefore,
	BYTE FinishPrevBefore,
	INT ClientBeforeEvent,
	BYTE KeyAfterEvent,
	BYTE PrevAfterEvent,
	INT ClientAfterEvent
)
{
	if( !Mover )
		return;
	if( StateName!=FName(TEXT("TriggerControl"), FNAME_Find) && StateName!=FName(TEXT("TriggerToggle"), FNAME_Find) )
		return;
	if( Mover->Level && Mover->Level->NetMode==NM_Client )
		return;
	if( UT99AndroidIsGuardiaScriptOwnedTriggerControlMover(Mover, StateName) )
		return;

	const UBOOL bFinishedOpening = FinishKeyBefore>FinishPrevBefore && FinishKeyBefore>0;
	const UBOOL bFinishedClosing = FinishKeyBefore==0 && FinishPrevBefore>0;
	const UBOOL bFinishedSameKey = FinishKeyBefore==FinishPrevBefore;
	if( !bFinishedOpening && !bFinishedClosing && !bFinishedSameKey )
		return;

	const BYTE KeyBeforeNormalize  = Mover->KeyNum;
	const BYTE PrevBeforeNormalize = Mover->PrevKeyNum;
	const INT ClientBeforeNormalize = Mover->ClientUpdate;

	if( bFinishedOpening )
	{
		Mover->KeyNum = FinishKeyBefore;
		Mover->PrevKeyNum = FinishKeyBefore;
	}
	else if( bFinishedClosing )
	{
		Mover->KeyNum = 0;
		Mover->PrevKeyNum = 0;
		Mover->bOpening = 0;
	}
	else
	{
		Mover->PrevKeyNum = Mover->KeyNum;
	}

	Mover->PhysAlpha = 0.0f;
	Mover->bInterpolating = 0;
	Mover->ClientUpdate = 0;
	Mover->AmbientSound = NULL;
	Mover->bDelaying = 0;
	Mover->RealPosition = Mover->Location;
	Mover->RealRotation = Mover->Rotation;

	UT99_ANDROID_MOVER_LOGI(
		"UT99_ANDROID_V170A_EVENT_TRIGGER_FINISH_NORMALIZE name=%s state=%s kind=%s before=%d/%d client=%d event=%d/%d client=%d normalize=%d/%d client=%d -> %d/%d client=%d opening=%d num=%d ambient=%d",
		appToAnsi(Mover->GetName()),
		StateName!=NAME_None ? appToAnsi(*StateName) : "None",
		bFinishedOpening ? "open" : (bFinishedClosing ? "close" : "same"),
		(INT)FinishKeyBefore,
		(INT)FinishPrevBefore,
		ClientBeforeEvent,
		(INT)KeyAfterEvent,
		(INT)PrevAfterEvent,
		ClientAfterEvent,
		(INT)KeyBeforeNormalize,
		(INT)PrevBeforeNormalize,
		ClientBeforeNormalize,
		(INT)Mover->KeyNum,
		(INT)Mover->PrevKeyNum,
		(INT)Mover->ClientUpdate,
		(INT)Mover->bOpening,
		(INT)Mover->numTriggerEvents,
		Mover->AmbientSound ? 1 : 0 );
}


struct FUT99AndroidTriggerOpenTimedCloseRecord
{
	AMover* Mover;
	FLOAT CloseAt;
	BYTE CloseTarget;
	UBOOL NotifiedOpening;
	BYTE NativeRetriggerSoundCount;
};

static FUT99AndroidTriggerOpenTimedCloseRecord GUT99AndroidTriggerOpenTimedCloseRecords[64];

static FLOAT UT99AndroidMoverTimeSeconds( AMover* Mover )
{
	return (Mover && Mover->Level) ? Mover->Level->TimeSeconds : 0.0f;
}

static void UT99AndroidEnableMoverProbe( AMover* Mover, FName ProbeName )
{
	if( !Mover || ProbeName.GetIndex()<NAME_PROBEMIN || ProbeName.GetIndex()>=NAME_PROBEMAX )
		return;
	if( !Mover->GetStateFrame() || !Mover->GetStateFrame()->StateNode )
		return;
	QWORD BaseProbeMask = (Mover->GetStateFrame()->StateNode->ProbeMask | Mover->GetClass()->ProbeMask) & Mover->GetStateFrame()->StateNode->IgnoreMask;
	Mover->GetStateFrame()->ProbeMask |= (BaseProbeMask & ((QWORD)1<<(ProbeName.GetIndex()-NAME_PROBEMIN)));
}

static void UT99AndroidClearTriggerOpenTimedCloseRecord( AMover* Mover )
{
	for( INT i=0; i<ARRAY_COUNT(GUT99AndroidTriggerOpenTimedCloseRecords); ++i )
	{
		if( GUT99AndroidTriggerOpenTimedCloseRecords[i].Mover==Mover )
		{
			GUT99AndroidTriggerOpenTimedCloseRecords[i].Mover = NULL;
			GUT99AndroidTriggerOpenTimedCloseRecords[i].CloseAt = 0.0f;
			GUT99AndroidTriggerOpenTimedCloseRecords[i].CloseTarget = 0;
			GUT99AndroidTriggerOpenTimedCloseRecords[i].NotifiedOpening = 0;
			GUT99AndroidTriggerOpenTimedCloseRecords[i].NativeRetriggerSoundCount = 0;
		}
	}
}

static void UT99AndroidPlayPressureNativeRetriggerSound( AMover* Mover, const char* Reason )
{
	if( !Mover || !Mover->GetLevel() || !Mover->GetLevel()->Engine || !Mover->GetLevel()->Engine->Audio )
		return;

	USound* Sound = Mover->OpeningSound ? Mover->OpeningSound : Mover->MoveAmbientSound;
	if( !Sound )
		return;

	const FLOAT Volume = Mover->TransientSoundVolume > 0.0f ? Mover->TransientSoundVolume : 1.0f;
	const FLOAT Radius = Mover->TransientSoundRadius > 0.0f ? Mover->TransientSoundRadius : 1600.0f;
	const INT Id = Mover->GetIndex()*16 + SLOT_None*2;
	Mover->GetLevel()->Engine->Audio->PlaySound( Mover, Id, Sound, Mover->Location, Volume, Radius, 1.0f );

	UT99_ANDROID_MOVER_LOGI(
		"UT99_ANDROID_V168Q_PRESSURE_NATIVE_RETRIGGER_SOUND name=%s reason=%s sound=%s key=%d prev=%d client=%d ambient=%d",
		appToAnsi(Mover->GetName()),
		Reason ? Reason : "native-retrigger",
		appToAnsi(Sound->GetName()),
		(INT)Mover->KeyNum,
		(INT)Mover->PrevKeyNum,
		(INT)Mover->ClientUpdate,
		Mover->AmbientSound ? 1 : 0 );
}

static UBOOL UT99AndroidIsPressureDoorTimedMover( AMover* Mover )
{
	if( !Mover )
		return 0;
	// In Pressure the chamber door movers are the long-stay multi-key
	// TriggerOpenTimed movers.  The short-stay movers are part of the chamber
	// machinery/animation and must keep the normal chained native close behavior.
	return Mover->StayOpenTime > 2.0f && Mover->KeyNum > 1;
}

static FUT99AndroidTriggerOpenTimedCloseRecord* UT99AndroidFindTriggerOpenTimedRecord( AMover* Mover, UBOOL bAllocate )
{
	FUT99AndroidTriggerOpenTimedCloseRecord* Empty = NULL;
	for( INT i=0; i<ARRAY_COUNT(GUT99AndroidTriggerOpenTimedCloseRecords); ++i )
	{
		FUT99AndroidTriggerOpenTimedCloseRecord& Rec = GUT99AndroidTriggerOpenTimedCloseRecords[i];
		if( Rec.Mover==Mover )
			return &Rec;
		if( !Rec.Mover && !Empty )
			Empty = &Rec;
	}
	return bAllocate ? Empty : NULL;
}

static void UT99AndroidScheduleTriggerOpenTimedCloseAfterOpen
(
	AMover* Mover,
	FName StateName,
	BYTE FinishKeyBefore,
	BYTE FinishPrevBefore,
	INT ClientBeforeEvent
)
{
	if( !Mover || StateName!=FName(TEXT("TriggerOpenTimed"), FNAME_Find) )
		return;

	if( Mover->Level && Mover->Level->NetMode==NM_Client )
		return;

	// Accept actual observed multi-key endpoints even when NumKeys is stale/low
	// on Android.  Pressure Mover6 can report KeyNum 2 with NumKeys-derived last=1.
	if( FinishKeyBefore<=FinishPrevBefore || FinishKeyBefore<=1 )
		return;

	if( Mover->bInterpolating || Mover->bTriggerOnceOnly )
		return;

	FUT99AndroidTriggerOpenTimedCloseRecord* ExistingSlot = UT99AndroidFindTriggerOpenTimedRecord( Mover, 0 );
	if( ExistingSlot && ExistingSlot->Mover==Mover
	&& (ExistingSlot->NotifiedOpening==4 || ExistingSlot->NotifiedOpening==5 || ExistingSlot->NotifiedOpening==6 || ExistingSlot->NotifiedOpening==7) )
	{
		UT99_ANDROID_MOVER_LOGI(
			"UT99_ANDROID_V168M_TRIGGEROPENTIMED_PRESSURE_SCHEDULE_SUPPRESS name=%s state=%s key=%d prev=%d client=%d phase=%d closeAt=%.3f ambient=%d",
			appToAnsi(Mover->GetName()),
			StateName!=NAME_None ? appToAnsi(*StateName) : "None",
			(INT)FinishKeyBefore,
			(INT)FinishPrevBefore,
			ClientBeforeEvent,
			(INT)ExistingSlot->NotifiedOpening,
			ExistingSlot->CloseAt,
			Mover->AmbientSound ? 1 : 0 );
		return;
	}

	FUT99AndroidTriggerOpenTimedCloseRecord* Slot = UT99AndroidFindTriggerOpenTimedRecord( Mover, 1 );
	if( !Slot )
		return;

	if( !Slot->Mover )
	{
		UFunction* FinishedOpening = Mover->FindFunction( FName(TEXT("FinishedOpening"), FNAME_Find) );
		if( FinishedOpening )
			Mover->ProcessEvent( FinishedOpening, NULL );
	}

	Slot->Mover = Mover;
	Slot->CloseTarget = 0;
	Slot->NativeRetriggerSoundCount = 0;

	const FLOAT Now = UT99AndroidMoverTimeSeconds(Mover);
	const FLOAT RequestedStayOpen = ::Max(0.01f, (FLOAT)Mover->StayOpenTime);
	const UBOOL bPressureDoor = (RequestedStayOpen > 2.0f);

	if( bPressureDoor )
	{
		// Pressure door workflow is visually reversed compared with the generic
		// TriggerOpenTimed fallback: after the button press the door must close now,
		// remain sealed while the pressure machinery runs, then reopen to key 0.
		// Phase 2 = start close-to-hold immediately; phase 4 = later reopen.
		Slot->CloseAt = Now;
		Slot->NotifiedOpening = 2;
	}
	else
	{
		// Non-door Pressure machinery / generic multi-key movers keep the v124
		// fallback: after their short authored stay, close stepwise to key 0.
		Slot->CloseAt = Now + RequestedStayOpen;
		Slot->NotifiedOpening = 1;
	}

	Mover->bInterpolating = 0;
	Mover->ClientUpdate = 0;
	Mover->PhysAlpha = 0.0f;
	Mover->PrevKeyNum = Mover->KeyNum;
	Mover->AmbientSound = NULL;
	Mover->RealPosition = Mover->Location;
	Mover->RealRotation = Mover->Rotation;

	UT99_ANDROID_MOVER_LOGI(
		"UT99_ANDROID_V130_TRIGGEROPENTIMED_SCHEDULE_PRESSURE_CLOSE name=%s state=%s key=%d prev=%d client=%d phase=%d target=%d stay=%.3f closeAt=%.3f opening=%d ambient=%d",
		appToAnsi(Mover->GetName()),
		StateName!=NAME_None ? appToAnsi(*StateName) : "None",
		(INT)FinishKeyBefore,
		(INT)FinishPrevBefore,
		ClientBeforeEvent,
		(INT)Slot->NotifiedOpening,
		(INT)Slot->CloseTarget,
		RequestedStayOpen,
		Slot->CloseAt,
		(INT)Mover->bOpening,
		Mover->AmbientSound ? 1 : 0 );
}


static void UT99AndroidScheduleTriggerOpenTimedSingleKeyClose
(
	AMover* Mover,
	FName StateName,
	BYTE FinishKeyBefore,
	BYTE FinishPrevBefore,
	INT ClientBeforeEvent
)
{
	if( !Mover || StateName!=FName(TEXT("TriggerOpenTimed"), FNAME_Find) )
		return;
	if( Mover->Level && Mover->Level->NetMode==NM_Client )
		return;
	if( Mover->bInterpolating || Mover->bTriggerOnceOnly )
		return;
	if( FinishKeyBefore!=1 || (FinishPrevBefore!=1 && FinishPrevBefore!=0) )
		return;

	FUT99AndroidTriggerOpenTimedCloseRecord* ExistingPressureSlot = UT99AndroidFindTriggerOpenTimedRecord( Mover, 0 );
	if( ExistingPressureSlot && ExistingPressureSlot->Mover==Mover
	&& (ExistingPressureSlot->NotifiedOpening==3 || ExistingPressureSlot->NotifiedOpening==4 || ExistingPressureSlot->NotifiedOpening==5 || ExistingPressureSlot->NotifiedOpening==6 || ExistingPressureSlot->NotifiedOpening==7) )
	{
		// UT99_ANDROID_V168O_PRESSURE_SINGLEKEY_SCHEDULE_SUPPRESS:
		// DM-Pressure Mover6 is a long-stay one-key-looking part of the pressure
		// sequence, not a DM-Conveyor style lift.  v168i/k accidentally reclassified
		// its key1 finish as a generic SingleKey lift and overwrote the Pressure
		// hold/reopen record.  Keep the existing Pressure record intact so the door
		// machinery can complete and reopen consistently.
		UT99_ANDROID_MOVER_LOGI(
			"UT99_ANDROID_V168O_PRESSURE_SINGLEKEY_SCHEDULE_SUPPRESS name=%s state=%s key=%d prev=%d client=%d phase=%d closeAt=%.3f ambient=%d",
			appToAnsi(Mover->GetName()),
			StateName!=NAME_None ? appToAnsi(*StateName) : "None",
			(INT)FinishKeyBefore,
			(INT)FinishPrevBefore,
			ClientBeforeEvent,
			(INT)ExistingPressureSlot->NotifiedOpening,
			ExistingPressureSlot->CloseAt,
			Mover->AmbientSound ? 1 : 0 );
		return;
	}

	FUT99AndroidTriggerOpenTimedCloseRecord* Slot = UT99AndroidFindTriggerOpenTimedRecord( Mover, 1 );
	if( !Slot )
		return;
	if( Slot->Mover==Mover && Slot->NotifiedOpening==8 )
	{
		// DM-Conveyor can report a second key=1/prev=1 finish while the lift is
		// already visually at the top.  v168h always refreshed the stay window;
		// that fixed bottom->top loops, but broke top->bottom use: the close sound
		// could be requested first, then the real down interpolation was delayed
		// and crawled near the bottom.  If the scheduled close time has already
		// arrived, treat this duplicate top finish as the close request and start
		// the down move immediately instead of extending the stay window again.
		const FLOAT Now = UT99AndroidMoverTimeSeconds(Mover);
		if( Now >= Slot->CloseAt - 0.02f )
		{
			Mover->bOpening = 0;
			Mover->bDelaying = 0;
			Mover->bInterpolating = 0;
			Mover->ClientUpdate = 0;
			Mover->PhysAlpha = 0.0f;
			Mover->PrevKeyNum = Mover->KeyNum;
			Mover->RealPosition = Mover->Location;
			Mover->RealRotation = Mover->Rotation;
			UT99AndroidReplaceMoverInterpolation( Mover, 0, Mover->MoveTime );
			Mover->AmbientSound = Mover->MoveAmbientSound;
			Slot->CloseAt = Now;
			Slot->CloseTarget = 0;
			Slot->NotifiedOpening = 9;
			UT99_ANDROID_MOVER_LOGI(
				"UT99_ANDROID_V168I_TRIGGEROPENTIMED_SINGLEKEY_DUP_TOP_CLOSE_NOW name=%s state=%s now=%.3f key=%d prev=%d client=%d interpolating=%d ambient=%d",
				appToAnsi(Mover->GetName()),
				StateName!=NAME_None ? appToAnsi(*StateName) : "None",
				Now,
				(INT)Mover->KeyNum,
				(INT)Mover->PrevKeyNum,
				(INT)Mover->ClientUpdate,
				(INT)Mover->bInterpolating,
				Mover->AmbientSound ? 1 : 0 );
			return;
		}

		Slot->CloseAt = Now + ::Max(0.05f, (FLOAT)Mover->StayOpenTime);
		Mover->bInterpolating = 0;
		Mover->ClientUpdate = 0;
		Mover->PhysAlpha = 0.0f;
		Mover->PrevKeyNum = Mover->KeyNum;
		Mover->AmbientSound = NULL;
		Mover->RealPosition = Mover->Location;
		Mover->RealRotation = Mover->Rotation;
		UT99_ANDROID_MOVER_LOGI(
			"UT99_ANDROID_V168I_TRIGGEROPENTIMED_SINGLEKEY_DUP_TOP_REFRESH name=%s state=%s key=%d prev=%d client=%d stay=%.3f closeAt=%.3f ambient=%d",
			appToAnsi(Mover->GetName()),
			StateName!=NAME_None ? appToAnsi(*StateName) : "None",
			(INT)FinishKeyBefore,
			(INT)FinishPrevBefore,
			ClientBeforeEvent,
			(FLOAT)::Max(0.05f, (FLOAT)Mover->StayOpenTime),
			Slot->CloseAt,
			Mover->AmbientSound ? 1 : 0 );
		return;
	}
	if( Slot->Mover==Mover && Slot->NotifiedOpening==9 )
		return;

	UFunction* FinishedOpening = Mover->FindFunction( FName(TEXT("FinishedOpening"), FNAME_Find) );
	if( FinishedOpening )
		Mover->ProcessEvent( FinishedOpening, NULL );

	Slot->Mover = Mover;
	Slot->CloseTarget = 0;
	Slot->CloseAt = UT99AndroidMoverTimeSeconds(Mover) + ::Max(0.05f, (FLOAT)Mover->StayOpenTime);
	Slot->NotifiedOpening = 8;

	Mover->bInterpolating = 0;
	Mover->ClientUpdate = 0;
	Mover->PhysAlpha = 0.0f;
	Mover->PrevKeyNum = Mover->KeyNum;
	Mover->AmbientSound = NULL;
	Mover->RealPosition = Mover->Location;
	Mover->RealRotation = Mover->Rotation;

	UT99_ANDROID_MOVER_LOGI(
		"UT99_ANDROID_V168I_TRIGGEROPENTIMED_SINGLEKEY_SCHEDULE_CLOSE name=%s state=%s key=%d prev=%d client=%d stay=%.3f closeAt=%.3f opening=%d ambient=%d",
		appToAnsi(Mover->GetName()),
		StateName!=NAME_None ? appToAnsi(*StateName) : "None",
		(INT)FinishKeyBefore,
		(INT)FinishPrevBefore,
		ClientBeforeEvent,
		(FLOAT)::Max(0.05f, (FLOAT)Mover->StayOpenTime),
		Slot->CloseAt,
		(INT)Mover->bOpening,
		Mover->AmbientSound ? 1 : 0 );
}

static void UT99AndroidPumpTriggerOpenTimedNativeClose( AMover* Mover )
{
	if( !Mover )
		return;
	FName StateName = UT99AndroidMoverStateName( Mover );
	if( StateName!=FName(TEXT("TriggerOpenTimed"), FNAME_Find) )
		return;

	// UT99_ANDROID_V168N_PRESSURE_INITIAL_DOOR_FAST_HOLD:
	// DM-Pressure exposed one remaining startup asymmetry: the first button press
	// could still run the authored key0->key3 path to completion before our
	// Pressure-door hold/reopen record existed.  Later presses were already fixed
	// by v168m and went directly to the remembered hold key.  Seed the same native
	// hold cycle when a long-stay multi-key TriggerOpenTimed mover starts from
	// key0 toward a high key, so the first press behaves like all later presses.
	// Do not touch AS-Guardia here; its explosive-wall sequence is intentionally
	// handled by the dedicated Guardia path below.
	if( (!Mover->Level || Mover->Level->NetMode!=NM_Client)
	&& !UT99AndroidIsASGuardiaLevel(Mover)
	&& !UT99AndroidFindTriggerOpenTimedRecord(Mover,0)
	&& Mover->StayOpenTime>2.0f
	&& Mover->PrevKeyNum==0
	&& Mover->KeyNum>2
	&& Mover->bInterpolating
	&& !Mover->bTriggerOnceOnly )
	{
		FUT99AndroidTriggerOpenTimedCloseRecord* Slot = UT99AndroidFindTriggerOpenTimedRecord( Mover, 1 );
		if( Slot )
		{
			const FLOAT Now = UT99AndroidMoverTimeSeconds(Mover);
			const BYTE OldKey = Mover->KeyNum;
			const BYTE OldPrev = Mover->PrevKeyNum;
			BYTE TargetKey = (BYTE)::Max(1, (INT)OldKey - 1);
			TargetKey = (BYTE)Clamp( (INT)TargetKey, 1, (INT)ARRAY_COUNT(Mover->KeyPos)-1 );

			Slot->Mover = Mover;
			Slot->CloseAt = Now;
			Slot->CloseTarget = TargetKey;
			Slot->NotifiedOpening = 3;
			Slot->NativeRetriggerSoundCount = 0;

			Mover->bOpening = 0;
			Mover->bDelaying = 0;
			Mover->ClientUpdate = 0;
			Mover->PhysAlpha = 0.0f;
			Mover->AmbientSound = Mover->MoveAmbientSound;
			Mover->RealPosition = Mover->Location;
			Mover->RealRotation = Mover->Rotation;
			Mover->bInterpolating = 0;
			UT99AndroidReplaceMoverInterpolation( Mover, TargetKey, Mover->MoveTime );

			UT99_ANDROID_MOVER_LOGI(
				"UT99_ANDROID_V168N_PRESSURE_INITIAL_DOOR_FAST_HOLD name=%s state=%s now=%.3f key=%d prev=%d target=%d stay=%.3f client=%d interpolating=%d ambient=%d",
				appToAnsi(Mover->GetName()),
				StateName!=NAME_None ? appToAnsi(*StateName) : "None",
				Now,
				(INT)OldKey,
				(INT)OldPrev,
				(INT)TargetKey,
				(FLOAT)Mover->StayOpenTime,
				(INT)Mover->ClientUpdate,
				(INT)Mover->bInterpolating,
				Mover->AmbientSound ? 1 : 0 );
		}
		return;
	}

	for( INT i=0; i<ARRAY_COUNT(GUT99AndroidTriggerOpenTimedCloseRecords); ++i )
	{
		FUT99AndroidTriggerOpenTimedCloseRecord& Rec = GUT99AndroidTriggerOpenTimedCloseRecords[i];
		if( Rec.Mover && Rec.Mover->bDeleteMe )
		{
			Rec.Mover = NULL;
			Rec.CloseAt = 0.0f;
			Rec.CloseTarget = 0;
			Rec.NotifiedOpening = 0;
			Rec.NativeRetriggerSoundCount = 0;
			continue;
		}
		if( Rec.Mover!=Mover )
			continue;

		const UBOOL bSingleKeyRearmRecord = (Rec.NotifiedOpening==10 || Rec.NotifiedOpening==11);
		const UBOOL bBottomRetriggerPulse = bSingleKeyRearmRecord
			&& Mover->KeyNum==0
			&& Mover->PrevKeyNum==0
			&& (Mover->bInterpolating || Mover->AmbientSound!=NULL || Mover->bOpening || Mover->ClientUpdate!=0);
		const UBOOL bTopRetriggerPulse = (Rec.NotifiedOpening==8)
			&& Mover->KeyNum==1
			&& Mover->PrevKeyNum==1
			&& (Mover->bInterpolating || Mover->AmbientSound!=NULL || Mover->bOpening || Mover->ClientUpdate!=0);
		if( Mover->bInterpolating && !bBottomRetriggerPulse && !bTopRetriggerPulse )
			return;

		const FLOAT Now = UT99AndroidMoverTimeSeconds(Mover);
		if( Now < Rec.CloseAt && !bBottomRetriggerPulse && !bTopRetriggerPulse )
			return;

		if( Rec.NotifiedOpening==8 )
		{
			// Generic one-key TriggerOpenTimed lift: after the authored stay, close normally.
			// v168k: v168j correctly converted real top->bottom close pulses into an
			// immediate downward interpolation, but it also consumed the harmless top
			// sound/null pulse that DM-Conveyor emits right after reaching key 1.  That
			// skipped the intended top stop.  If the pulse arrives before CloseAt, park
			// the lift silently at key 1 and keep the existing stay window.  Only when
			// CloseAt has elapsed do we start the real down move.
			if( bTopRetriggerPulse && Now < Rec.CloseAt - 0.02f )
			{
				Mover->bOpening = 0;
				Mover->bDelaying = 0;
				Mover->bInterpolating = 0;
				Mover->ClientUpdate = 0;
				Mover->PhysAlpha = 0.0f;
				Mover->KeyNum = 1;
				Mover->PrevKeyNum = 1;
				Mover->AmbientSound = NULL;
				Mover->RealPosition = Mover->Location;
				Mover->RealRotation = Mover->Rotation;
				UT99_ANDROID_MOVER_LOGI(
					"UT99_ANDROID_V168K_TRIGGEROPENTIMED_SINGLEKEY_TOP_PULSE_HOLD name=%s state=%s now=%.3f closeAt=%.3f key=%d prev=%d client=%d interpolating=%d ambient=%d",
					appToAnsi(Mover->GetName()),
					StateName!=NAME_None ? appToAnsi(*StateName) : "None",
					Now,
					Rec.CloseAt,
					(INT)Mover->KeyNum,
					(INT)Mover->PrevKeyNum,
					(INT)Mover->ClientUpdate,
					(INT)Mover->bInterpolating,
					Mover->AmbientSound ? 1 : 0 );
				return;
			}
			if( Now < Rec.CloseAt && !bTopRetriggerPulse )
				return;
			Mover->bOpening = 0;
			Mover->bDelaying = 0;
			Mover->ClientUpdate = 0;
			Mover->PhysAlpha = 0.0f;
			Mover->PrevKeyNum = Mover->KeyNum;
			Mover->RealPosition = Mover->Location;
			Mover->RealRotation = Mover->Rotation;
			Mover->bInterpolating = 0;
			UT99AndroidReplaceMoverInterpolation( Mover, 0, Mover->MoveTime );
			Mover->AmbientSound = Mover->MoveAmbientSound;
			Rec.NotifiedOpening = 9;
			Rec.CloseAt = Now;
			UT99_ANDROID_MOVER_LOGI(
				"UT99_ANDROID_V168K_TRIGGEROPENTIMED_SINGLEKEY_CLOSE_STEP name=%s state=%s now=%.3f closeAt=%.3f key=%d prev=%d client=%d interpolating=%d ambient=%d topPulse=%d",
				appToAnsi(Mover->GetName()),
				StateName!=NAME_None ? appToAnsi(*StateName) : "None",
				Now,
				Rec.CloseAt,
				(INT)Mover->KeyNum,
				(INT)Mover->PrevKeyNum,
				(INT)Mover->ClientUpdate,
				(INT)Mover->bInterpolating,
				Mover->AmbientSound ? 1 : 0,
				bTopRetriggerPulse ? 1 : 0 );
			return;
		}

		if( Rec.NotifiedOpening==9 )
		{
			// v168g: Some Android standalone movers can visually reach key0 after
			// our single-key close interpolation without delivering the scripted
			// bottom InterpolateEnd callback.  Do not fall through into the old
			// generic v130 pressure step, because that clears the record and leaves
			// DM-Conveyor in a sound-only/no-retrigger state.  If the mover is now
			// parked at the bottom, run the same safe rearm that the normal finish
			// callback would have performed.
			if( Mover->KeyNum==0 )
			{
				UFunction* FinishedClosing = Mover->FindFunction( FName(TEXT("FinishedClosing"), FNAME_Find) );
				if( FinishedClosing )
					Mover->ProcessEvent( FinishedClosing, NULL );
				UT99AndroidEnableMoverProbe( Mover, FName(TEXT("Trigger"), FNAME_Find) );
				Mover->GotoState( FName(TEXT("TriggerOpenTimed"), FNAME_Find) );
				UT99AndroidEnableMoverProbe( Mover, FName(TEXT("Trigger"), FNAME_Find) );
				Mover->KeyNum = 0;
				Mover->PrevKeyNum = 0;
				Mover->PhysAlpha = 0.0f;
				Mover->ClientUpdate = 0;
				Mover->bOpening = 0;
				Mover->bDelaying = 0;
				Mover->AmbientSound = NULL;
				Mover->RealPosition = Mover->Location;
				Mover->RealRotation = Mover->Rotation;
				Rec.Mover = Mover;
				Rec.CloseAt = UT99AndroidMoverTimeSeconds(Mover) + 0.15f;
				Rec.CloseTarget = 1;
				Rec.NotifiedOpening = 10;
				UT99_ANDROID_MOVER_LOGI(
					"UT99_ANDROID_V168I_TRIGGEROPENTIMED_SINGLEKEY_PUMP_BOTTOM_REARM name=%s state=%s now=%.3f key=%d prev=%d client=%d rearmAt=%.3f ambient=%d",
					appToAnsi(Mover->GetName()),
					StateName!=NAME_None ? appToAnsi(*StateName) : "None",
					Now,
					(INT)Mover->KeyNum,
					(INT)Mover->PrevKeyNum,
					(INT)Mover->ClientUpdate,
					Rec.CloseAt,
					Mover->AmbientSound ? 1 : 0 );
			}
			return;
		}

		if( Rec.NotifiedOpening==10 || Rec.NotifiedOpening==11 )
		{
			// Re-armed one-key TriggerOpenTimed lift after a completed cycle.
			// v168h: catch the real trigger pulse before the engine's zero-distance
			// key0/key0 interpolation can play the lift sound by itself.  If we wait
			// until the later finish callback, DM-Conveyor starts audio first and only
			// moves after that bogus pulse ends.
			const UBOOL bLooksAtBottom = (Mover->KeyNum==0 && Mover->PrevKeyNum==0);
			const UBOOL bRealRetriggerPulse = bLooksAtBottom && (Mover->bInterpolating || Mover->AmbientSound!=NULL || Mover->bOpening || Mover->ClientUpdate!=0);
			if( bRealRetriggerPulse )
			{
				Rec.CloseAt = 0.0f;
				Rec.CloseTarget = 1;
				Rec.NotifiedOpening = 11;
				Mover->bOpening = 1;
				Mover->bDelaying = 0;
				Mover->ClientUpdate = 0;
				Mover->PhysAlpha = 0.0f;
				Mover->AmbientSound = Mover->MoveAmbientSound;
				Mover->RealPosition = Mover->Location;
				Mover->RealRotation = Mover->Rotation;
				Mover->bInterpolating = 0;
				UT99AndroidReplaceMoverInterpolation( Mover, 1, Mover->MoveTime );
				UT99_ANDROID_MOVER_LOGI(
					"UT99_ANDROID_V168I_TRIGGEROPENTIMED_SINGLEKEY_SOUND_RETRIGGER_OPEN name=%s state=%s now=%.3f key=%d prev=%d client=%d interpolating=%d ambient=%d",
					appToAnsi(Mover->GetName()),
					StateName!=NAME_None ? appToAnsi(*StateName) : "None",
					Now,
					(INT)Mover->KeyNum,
					(INT)Mover->PrevKeyNum,
					(INT)Mover->ClientUpdate,
					(INT)Mover->bInterpolating,
					Mover->AmbientSound ? 1 : 0 );
			}
			return;
		}

		if( Rec.NotifiedOpening==6 )
		{
			// Re-armed idle Pressure-door record after a completed cycle.
			// Do not let the generic v130 pump consume it while sitting at key 0.
			return;
		}

		if( Rec.NotifiedOpening==4 )
		{
			// Pressure chamber is done: reopen the sealed door to key 0.
			Mover->bOpening = 0;
			Mover->bDelaying = 0;
			UT99AndroidReplaceMoverInterpolation( Mover, 0, Mover->MoveTime );
			Mover->AmbientSound = Mover->MoveAmbientSound;
			Rec.NotifiedOpening = 5;
			UT99_ANDROID_MOVER_LOGI(
				"UT99_ANDROID_V130_TRIGGEROPENTIMED_NATIVE_REOPEN_AFTER_PRESSURE name=%s state=%s now=%.3f closeAt=%.3f key=%d prev=%d client=%d interpolating=%d ambient=%d",
				appToAnsi(Mover->GetName()),
				StateName!=NAME_None ? appToAnsi(*StateName) : "None",
				Now,
				Rec.CloseAt,
				(INT)Mover->KeyNum,
				(INT)Mover->PrevKeyNum,
				(INT)Mover->ClientUpdate,
				(INT)Mover->bInterpolating,
				Mover->AmbientSound ? 1 : 0 );
			return;
		}

		// Start either the immediate Pressure-door close-to-hold (phase 2) or the
		// generic v124 close chain (phase 1) with one step downward.
		const BYTE StepTargetKey = (BYTE)::Max(0, (INT)Mover->KeyNum - 1);
		Mover->bOpening = 0;
		Mover->bDelaying = 0;
		UT99AndroidReplaceMoverInterpolation( Mover, StepTargetKey, Mover->MoveTime );
		if( Rec.NotifiedOpening==2 )
		{
			Mover->AmbientSound = Mover->MoveAmbientSound;
			Rec.NotifiedOpening = 3;
		}
		else
		{
			// Pressure chamber machinery already has its authored loop/effect.
			// Do not add another MoveAmbientSound layer during native catch-up steps.
			Mover->AmbientSound = NULL;
			Rec.Mover = NULL;
			Rec.CloseAt = 0.0f;
			Rec.CloseTarget = 0;
			Rec.NotifiedOpening = 0;
			Rec.NativeRetriggerSoundCount = 0;
		}

		UT99_ANDROID_MOVER_LOGI(
			"UT99_ANDROID_V130_TRIGGEROPENTIMED_NATIVE_PRESSURE_STEP name=%s state=%s now=%.3f closeAt=%.3f phase=%d stepTarget=%d key=%d prev=%d client=%d interpolating=%d ambient=%d",
			appToAnsi(Mover->GetName()),
			StateName!=NAME_None ? appToAnsi(*StateName) : "None",
			Now,
			Rec.CloseAt,
			(INT)Rec.NotifiedOpening,
			(INT)StepTargetKey,
			(INT)Mover->KeyNum,
			(INT)Mover->PrevKeyNum,
			(INT)Mover->ClientUpdate,
			(INT)Mover->bInterpolating,
			Mover->AmbientSound ? 1 : 0 );
		return;
	}
}

static void UT99AndroidFinishTriggerOpenTimedNativeClose
(
	AMover* Mover,
	FName StateName,
	BYTE FinishKeyBefore,
	BYTE FinishPrevBefore,
	INT ClientBeforeEvent
)
{
	if( !Mover || StateName!=FName(TEXT("TriggerOpenTimed"), FNAME_Find) )
		return;
	if( Mover->Level && Mover->Level->NetMode==NM_Client )
		return;
	if( Mover->bInterpolating )
		return;

	FUT99AndroidTriggerOpenTimedCloseRecord* Rec = UT99AndroidFindTriggerOpenTimedRecord( Mover, 0 );
	if( FinishPrevBefore<=FinishKeyBefore
	&& !(Rec && Rec->NotifiedOpening==6 && FinishKeyBefore==0 && FinishPrevBefore==0)
	&& !(Rec && Rec->NotifiedOpening==7 && FinishKeyBefore>0 && FinishPrevBefore==0)
	&& !(Rec && Rec->NotifiedOpening==10 && FinishKeyBefore==0 && FinishPrevBefore==0) )
		return;
	if( Rec && Rec->NotifiedOpening==9 && FinishKeyBefore==0 && FinishPrevBefore>0 )
	{
		UFunction* FinishedClosing = Mover->FindFunction( FName(TEXT("FinishedClosing"), FNAME_Find) );
		if( FinishedClosing )
			Mover->ProcessEvent( FinishedClosing, NULL );
		UT99AndroidEnableMoverProbe( Mover, FName(TEXT("Trigger"), FNAME_Find) );
		Mover->GotoState( FName(TEXT("TriggerOpenTimed"), FNAME_Find) );
		UT99AndroidEnableMoverProbe( Mover, FName(TEXT("Trigger"), FNAME_Find) );
		Mover->KeyNum = 0;
		Mover->PrevKeyNum = 0;
		Mover->PhysAlpha = 0.0f;
		Mover->ClientUpdate = 0;
		Mover->bOpening = 0;
		Mover->bDelaying = 0;
		Mover->AmbientSound = NULL;
		Mover->RealPosition = Mover->Location;
		Mover->RealRotation = Mover->Rotation;
		Rec->Mover = Mover;
		Rec->CloseAt = UT99AndroidMoverTimeSeconds(Mover) + 0.15f;
		Rec->CloseTarget = 1;
		Rec->NotifiedOpening = 10;
		UT99_ANDROID_MOVER_LOGI(
			"UT99_ANDROID_V168I_TRIGGEROPENTIMED_SINGLEKEY_FINISHED_REARM name=%s state=%s key=%d prev=%d client=%d rearmAt=%.3f ambient=%d",
			appToAnsi(Mover->GetName()),
			StateName!=NAME_None ? appToAnsi(*StateName) : "None",
			(INT)FinishKeyBefore,
			(INT)FinishPrevBefore,
			ClientBeforeEvent,
			Rec->CloseAt,
			Mover->AmbientSound ? 1 : 0 );
		return;
	}

	if( Rec && Rec->NotifiedOpening==10 && FinishKeyBefore==0 && FinishPrevBefore==0 )
	{
		const FLOAT Now = UT99AndroidMoverTimeSeconds(Mover);
		if( Now >= Rec->CloseAt )
		{
			Rec->CloseAt = 0.0f;
			Rec->CloseTarget = 1;
			Rec->NotifiedOpening = 11;
			Mover->bOpening = 1;
			Mover->bDelaying = 0;
			Mover->ClientUpdate = 0;
			Mover->PhysAlpha = 0.0f;
			Mover->AmbientSound = Mover->MoveAmbientSound;
			Mover->RealPosition = Mover->Location;
			Mover->RealRotation = Mover->Rotation;
			UT99AndroidReplaceMoverInterpolation( Mover, 1, Mover->MoveTime );
			UT99_ANDROID_MOVER_LOGI(
				"UT99_ANDROID_V168I_TRIGGEROPENTIMED_SINGLEKEY_RETRIGGER_OPEN name=%s state=%s now=%.3f key=%d prev=%d client=%d interpolating=%d ambient=%d",
				appToAnsi(Mover->GetName()),
				StateName!=NAME_None ? appToAnsi(*StateName) : "None",
				Now,
				(INT)FinishKeyBefore,
				(INT)FinishPrevBefore,
				ClientBeforeEvent,
				(INT)Mover->bInterpolating,
				Mover->AmbientSound ? 1 : 0 );
		}
		return;
	}

	if( Rec && (Rec->NotifiedOpening==3 || Rec->NotifiedOpening==7) )
	{
		// Door has reached its sealed/key-hold position. Keep it closed while the
		// pressure sequence runs, then reopen. Do not chain to key 0 yet.
		// Remember this physical hold key: after a completed cycle the map can send
		// only a key=0/prev=0 retrigger pulse, so we must move back to the same
		// authored hold key directly instead of faking key 3 or jumping labels.
		const FLOAT HoldSeconds = ::Max(0.50f, (FLOAT)Mover->StayOpenTime);
		Rec->CloseAt = UT99AndroidMoverTimeSeconds(Mover) + HoldSeconds;
		Rec->CloseTarget = FinishKeyBefore;
		Rec->NotifiedOpening = 4;
		Mover->PrevKeyNum = Mover->KeyNum;
		Mover->PhysAlpha = 0.0f;
		Mover->ClientUpdate = 0;
		Mover->bOpening = 0;
		Mover->AmbientSound = NULL;
		Mover->RealPosition = Mover->Location;
		Mover->RealRotation = Mover->Rotation;
		UT99_ANDROID_MOVER_LOGI(
			"UT99_ANDROID_V130_TRIGGEROPENTIMED_PRESSURE_DOOR_HELD_CLOSED name=%s state=%s key=%d prev=%d hold=%.3f reopenAt=%.3f client=%d ambient=%d",
			appToAnsi(Mover->GetName()),
			StateName!=NAME_None ? appToAnsi(*StateName) : "None",
			(INT)FinishKeyBefore,
			(INT)FinishPrevBefore,
			HoldSeconds,
			Rec->CloseAt,
			ClientBeforeEvent,
			Mover->AmbientSound ? 1 : 0 );
		return;
	}

	if( Rec && Rec->NotifiedOpening==5 )
	{
		// Final reopen after pressure sequence: release the mover/trigger again.
		UFunction* FinishedClosing = Mover->FindFunction( FName(TEXT("FinishedClosing"), FNAME_Find) );
		if( FinishedClosing )
			Mover->ProcessEvent( FinishedClosing, NULL );
		UT99AndroidEnableMoverProbe( Mover, FName(TEXT("Trigger"), FNAME_Find) );
		// Fully re-arm the TriggerOpenTimed state after our native Pressure-door cycle.
		// Without this, the first cycle works but the mover can remain logically past
		// its scripted Close label and ignore later button presses.
		Mover->GotoState( FName(TEXT("TriggerOpenTimed"), FNAME_Find) );
		UT99AndroidEnableMoverProbe( Mover, FName(TEXT("Trigger"), FNAME_Find) );
		Mover->PrevKeyNum = Mover->KeyNum;
		Mover->PhysAlpha = 0.0f;
		Mover->ClientUpdate = 0;
		Mover->bOpening = 0;
		Mover->AmbientSound = NULL;
		Mover->RealPosition = Mover->Location;
		Mover->RealRotation = Mover->Rotation;
		UT99_ANDROID_MOVER_LOGI(
			"UT99_ANDROID_V130_TRIGGEROPENTIMED_PRESSURE_DOOR_REOPENED name=%s state=%s key=%d prev=%d client=%d ambient=%d",
			appToAnsi(Mover->GetName()),
			StateName!=NAME_None ? appToAnsi(*StateName) : "None",
			(INT)FinishKeyBefore,
			(INT)FinishPrevBefore,
			ClientBeforeEvent,
			Mover->AmbientSound ? 1 : 0 );
		// Keep a small armed record after the visual reopen.  On Android the script
		// state can stay past the normal Close label; a later button pulse then shows
		// up only as a key=0/prev=0 finish instead of running the authored Open
		// label again.  Phase 6 lets us treat that later pulse as a real retrigger.
		Rec->Mover = Mover;
		Rec->CloseAt = UT99AndroidMoverTimeSeconds(Mover) + 0.25f;
		Rec->CloseTarget = FinishPrevBefore>0 ? FinishPrevBefore : Rec->CloseTarget;
		Rec->NotifiedOpening = 6;
		return;
	}

	if( Rec && Rec->NotifiedOpening==6 && FinishKeyBefore==0 && FinishPrevBefore==0 )
	{
		const FLOAT Now = UT99AndroidMoverTimeSeconds(Mover);
		if( Now >= Rec->CloseAt )
		{
			// Pressure button was pressed again after a completed cycle.  The log shows
			// the second and later pulses arrive only as key=0/prev=0: the sound/event
			// fires, but the mover no longer traverses the authored Open label.
			// Therefore restart the visual door cycle natively, but to the exact hold
			// key remembered from the first good cycle (Mover4/5 normally key 2,
			// Mover6 normally key 1). This avoids the v128 fake-key3 path and the v129
			// GotoLabel path, both of which could leave the brush visually stale.
			BYTE TargetKey = Rec->CloseTarget>0 ? Rec->CloseTarget : 1;
			TargetKey = (BYTE)Clamp( (INT)TargetKey, 1, (INT)ARRAY_COUNT(Mover->KeyPos)-1 );
			Rec->CloseAt = 0.0f;
			Rec->NotifiedOpening = 7;
			Mover->bOpening = 0;
			Mover->bDelaying = 0;
			Mover->ClientUpdate = 0;
			Mover->PhysAlpha = 0.0f;
			Mover->AmbientSound = Mover->MoveAmbientSound;
			Mover->RealPosition = Mover->Location;
			Mover->RealRotation = Mover->Rotation;
			UT99AndroidReplaceMoverInterpolation( Mover, TargetKey, Mover->MoveTime );
			if( Rec->NativeRetriggerSoundCount==0 )
			{
				UT99AndroidPlayPressureNativeRetriggerSound( Mover, "second-pressure-pulse" );
				Rec->NativeRetriggerSoundCount = 1;
			}
			UT99_ANDROID_MOVER_LOGI(
				"UT99_ANDROID_V130_TRIGGEROPENTIMED_PRESSURE_RETRIGGER_NATIVE_TARGET name=%s state=%s key=%d prev=%d target=%d now=%.3f client=%d interpolating=%d ambient=%d",
				appToAnsi(Mover->GetName()),
				StateName!=NAME_None ? appToAnsi(*StateName) : "None",
				(INT)FinishKeyBefore,
				(INT)FinishPrevBefore,
				(INT)TargetKey,
				Now,
				ClientBeforeEvent,
				(INT)Mover->bInterpolating,
				Mover->AmbientSound ? 1 : 0 );
		}
		return;
	}

	// Generic v124 fallback for non-door multi-key TriggerOpenTimed movers.
	if( FinishKeyBefore>0 )
	{
		const BYTE StepTargetKey = (BYTE)::Max(0, (INT)FinishKeyBefore - 1);
		Mover->bOpening = 0;
		Mover->bDelaying = 0;
		UT99AndroidReplaceMoverInterpolation( Mover, StepTargetKey, Mover->MoveTime );
		// Avoid duplicated chamber loop audio on the short pressure animation movers.
		Mover->AmbientSound = NULL;
		UT99_ANDROID_MOVER_LOGI(
			"UT99_ANDROID_V130_TRIGGEROPENTIMED_NATIVE_CONTINUE name=%s state=%s key=%d prev=%d next=%d client=%d interpolating=%d ambient=%d",
			appToAnsi(Mover->GetName()),
			StateName!=NAME_None ? appToAnsi(*StateName) : "None",
			(INT)FinishKeyBefore,
			(INT)FinishPrevBefore,
			(INT)StepTargetKey,
			ClientBeforeEvent,
			(INT)Mover->bInterpolating,
			Mover->AmbientSound ? 1 : 0 );
		return;
	}

	UFunction* FinishedClosing = Mover->FindFunction( FName(TEXT("FinishedClosing"), FNAME_Find) );
	if( FinishedClosing )
		Mover->ProcessEvent( FinishedClosing, NULL );
	UT99AndroidEnableMoverProbe( Mover, FName(TEXT("Trigger"), FNAME_Find) );

	Mover->PrevKeyNum = Mover->KeyNum;
	Mover->PhysAlpha = 0.0f;
	Mover->ClientUpdate = 0;
	Mover->bOpening = 0;
	Mover->AmbientSound = NULL;
	Mover->RealPosition = Mover->Location;
	Mover->RealRotation = Mover->Rotation;

	UT99_ANDROID_MOVER_LOGI(
		"UT99_ANDROID_V130_TRIGGEROPENTIMED_NATIVE_FINISHED name=%s state=%s key=%d prev=%d client=%d ambient=%d",
		appToAnsi(Mover->GetName()),
		StateName!=NAME_None ? appToAnsi(*StateName) : "None",
		(INT)FinishKeyBefore,
		(INT)FinishPrevBefore,
		ClientBeforeEvent,
		Mover->AmbientSound ? 1 : 0 );
}

static void UT99AndroidNormalizeTriggerOpenTimedFinish
(
	AMover* Mover,
	FName StateName,
	BYTE FinishKeyBefore,
	BYTE FinishPrevBefore,
	INT ClientBeforeEvent,
	BYTE KeyAfterEvent,
	BYTE PrevAfterEvent,
	INT ClientAfterEvent
)
{
	// UT99_ANDROID_V119_PRESSURE_TRIGGEROPENTIMED_SAFE_RESTORE:
	// TriggerOpenTimed movers, including the Pressure chamber doors, must be
	// driven by their UnrealScript Open/Close chain.  V117/V118 normalized
	// KeyNum/PrevKeyNum and cleared AmbientSound here, which stopped the pressure
	// chamber sound and left the doors at the open endpoint.  Keep this function
	// as a no-op so existing call sites compile, but do not modify these movers.
	return;
}


static UBOOL UT99AndroidIsASGuardiaExplosiveWallMover( AMover* Mover, FName StateName, BYTE FinishKeyBefore, BYTE FinishPrevBefore )
{
	if( !Mover || StateName!=FName(TEXT("TriggerOpenTimed"), FNAME_Find) )
		return 0;

	if( !UT99AndroidIsASGuardiaLevel(Mover) )
		return 0;

	if( FinishKeyBefore!=3 || FinishPrevBefore!=0 )
		return 0;

	const TCHAR* Name = Mover->GetName();
	return appStricmp(Name,TEXT("Mover7"))==0
		|| appStricmp(Name,TEXT("Mover8"))==0
		|| appStricmp(Name,TEXT("Mover9"))==0;
}

static UBOOL UT99AndroidIsASGuardiaSlidingDoorMover( AMover* Mover, FName StateName )
{
	if( !Mover || StateName!=FName(TEXT("TriggerControl"), FNAME_Find) )
		return 0;

	if( !UT99AndroidIsASGuardiaLevel(Mover) )
		return 0;

	const TCHAR* Name = Mover->GetName();
	return appStricmp(Name,TEXT("Mover0"))==0
		|| appStricmp(Name,TEXT("Mover1"))==0;
}

static UBOOL UT99AndroidASGuardiaSlidingDoorHasNearbyPawn( AMover* Mover )
{
	if( !Mover || !Mover->GetLevel() )
		return 0;

	// UT99_ANDROID_V167M_ASGUARDIA_SLIDING_DOOR_FIXED_PAWN_ZONE:
	// v167l checked around the moving door leaf itself.  Once the two leaves are
	// open, that moving reference plus every Pawn in the larger 384uu cylinder can
	// keep pawnNear=true forever and the close pulse is immediately undone.  This
	// door is AS-Guardia-specific, so use the stable doorway midpoint from the
	// closed Mover0/Mover1 positions and count only live, colliding Pawns.
	FVector DoorCenter;
	DoorCenter.X = 18.0f;
	DoorCenter.Y = -282.0f;
	DoorCenter.Z = 0.0f;

	const FLOAT NearXY = 224.0f;
	const FLOAT NearZ  = 192.0f;
	const FLOAT NearXYSq = NearXY * NearXY;
	for( INT iActor=0; iActor<Mover->GetLevel()->Actors.Num(); ++iActor )
	{
		AActor* TestActor = Mover->GetLevel()->Actors(iActor);
		if( !TestActor || TestActor->bDeleteMe || !TestActor->IsA(APawn::StaticClass()) )
			continue;

		APawn* Pawn = (APawn*)TestActor;
		if( Pawn->Health<=0 || Pawn->bHidden )
			continue;
		if( !Pawn->bCollideActors && !Pawn->bBlockActors )
			continue;

		FLOAT Dz = Pawn->Location.Z - DoorCenter.Z;
		if( Dz < 0.0f )
			Dz = -Dz;
		if( Dz > NearZ )
			continue;

		const FLOAT Dx = Pawn->Location.X - DoorCenter.X;
		const FLOAT Dy = Pawn->Location.Y - DoorCenter.Y;
		if( Dx*Dx + Dy*Dy <= NearXYSq )
			return 1;
	}
	return 0;
}


static void UT99AndroidV167TReassertASGuardiaSlidingDoorNativeMove( AMover* Mover, UFunction* Caller )
{
	// UT99_ANDROID_V167T_ASGUARDIA_SLIDING_DOOR_SCRIPT_MOVE_REASSERT:
	// UnrealScript final functions are executed inside the VM, so the v167s
	// AActor::ProcessEvent InterpolateTo suppression cannot catch DoOpen()'s
	// internal InterpolateTo() call.  By the time the script tries to play its
	// door sound we are immediately after that stale InterpolateTo(), still within
	// the same VM step.  Reassert the native pawn-zone movement here so the old
	// TriggerControl state cannot restart the brush mid-open and cause the visible
	// open-close-open hiccup.
	if( !Mover || !Caller )
		return;

	const FName CallerName = Caller->GetFName();
	const UBOOL bScriptOpen  = CallerName==FName(TEXT("DoOpen"), FNAME_Find);
	const UBOOL bScriptClose = CallerName==FName(TEXT("DoClose"), FNAME_Find);
	if( !bScriptOpen && !bScriptClose )
		return;

	const BYTE DesiredKey = bScriptOpen ? 1 : 0;
	const BYTE DesiredPrev = bScriptOpen ? 0 : 1;
	const BYTE OldKey = Mover->KeyNum;
	const BYTE OldPrev = Mover->PrevKeyNum;
	const INT OldNum = Mover->numTriggerEvents;
	const INT OldClient = Mover->ClientUpdate;
	const UBOOL OldInterp = Mover->bInterpolating;
	const FLOAT OldAlpha = Mover->PhysAlpha;

	const FVector TargetPos = Mover->BasePos + Mover->KeyPos[DesiredKey];
	const FVector OtherPos  = Mover->BasePos + Mover->KeyPos[DesiredPrev];
	const FLOAT TotalDistSq = (TargetPos - OtherPos).SizeSquared();
	const FLOAT RemainingDistSq = (TargetPos - Mover->Location).SizeSquared();
	FLOAT RemainingSeconds = 0.12f;
	if( TotalDistSq > 1.0f )
	{
		const FLOAT Ratio = appSqrt( Clamp(RemainingDistSq / TotalDistSq, 0.0f, 1.0f) );
		RemainingSeconds = Clamp( 0.34f * Ratio, 0.06f, 0.18f );
	}

	Mover->OldPos = Mover->Location;
	Mover->OldRot = Mover->Rotation;
	Mover->PhysAlpha = 0.0f;
	Mover->setPhysics(PHYS_MovingBrush);
	Mover->bInterpolating = 1;
	Mover->bOpening = bScriptOpen;
	Mover->bDelaying = 0;
	Mover->PrevKeyNum = DesiredPrev;
	Mover->KeyNum = DesiredKey;
	Mover->PhysRate = 1.0f / ::Max(RemainingSeconds, 0.005f);
	Mover->ClientUpdate = 1;
	Mover->numTriggerEvents = bScriptOpen ? 1 : 0;
	Mover->AmbientSound = Mover->MoveAmbientSound;

	Mover->SimOldPos = Mover->OldPos;
	Mover->SimOldRotYaw = Mover->OldRot.Yaw;
	Mover->SimOldRotPitch = Mover->OldRot.Pitch;
	Mover->SimOldRotRoll = Mover->OldRot.Roll;
	Mover->SimInterpolate.X = 100.0f * Mover->PhysAlpha;
	Mover->SimInterpolate.Y = 100.0f * ::Max(0.01f, Mover->PhysRate);
	Mover->SimInterpolate.Z = 256.0f * Mover->PrevKeyNum + Mover->KeyNum;

	if( Mover->Level && Mover->Level->NetMode!=NM_Client )
	{
		Mover->RealPosition = Mover->Location;
		Mover->RealRotation = Mover->Rotation;
	}

	UT99_ANDROID_MOVER_LOGI(
		"UT99_ANDROID_V167T_ASGUARDIA_SLIDING_DOOR_SCRIPT_MOVE_REASSERT name=%s caller=%s key=%d/%d->%d/%d num=%d->%d client=%d->%d interp=%d->%d alpha=%.3f->%.3f remain=%.3f loc=%.1f,%.1f,%.1f",
		appToAnsi(Mover->GetName()),
		appToAnsi(Caller->GetName()),
		(INT)OldKey,
		(INT)OldPrev,
		(INT)Mover->KeyNum,
		(INT)Mover->PrevKeyNum,
		OldNum,
		(INT)Mover->numTriggerEvents,
		OldClient,
		(INT)Mover->ClientUpdate,
		(INT)OldInterp,
		(INT)Mover->bInterpolating,
		OldAlpha,
		Mover->PhysAlpha,
		RemainingSeconds,
		Mover->Location.X,
		Mover->Location.Y,
		Mover->Location.Z );
}


static UBOOL UT99AndroidV167TSuppressASGuardiaSlidingDoorScriptSound( AActor* Actor, UFunction* Caller, USound* Sound )
{
	// UT99_ANDROID_V167T_ASGUARDIA_SLIDING_DOOR_SCRIPT_SOUND_SUPPRESS:
	// The AS-Guardia door is now driven by the small native pawn-zone controller in
	// UnLevTic.cpp.  The original TriggerControl script can still wake up from stale
	// touch counters and play md2start/mlift1end without a matching visual move.
	// Suppress only those script sounds for Mover0/Mover1; the native controller
	// plays the same sounds directly at the actual movement start.
	if( !Actor || !Sound || !Actor->IsA(AMover::StaticClass()) )
		return 0;

	AMover* Mover = (AMover*)Actor;
	FName StateName = UT99AndroidMoverStateName( Mover );
	if( !UT99AndroidIsASGuardiaSlidingDoorMover(Mover, StateName) )
		return 0;

	const TCHAR* SoundName = Sound->GetName();
	const UBOOL bDoorSound =
		appStricmp(SoundName,TEXT("md2start"))==0 ||
		appStricmp(SoundName,TEXT("mlift1end"))==0 ||
		(Sound==Mover->OpeningSound) ||
		(Sound==Mover->ClosingSound) ||
		(Sound==Mover->OpenedSound) ||
		(Sound==Mover->ClosedSound);
	if( !bDoorSound )
		return 0;

	UT99AndroidV167TReassertASGuardiaSlidingDoorNativeMove( Mover, Caller );

	UT99_ANDROID_MOVER_LOGI(
		"UT99_ANDROID_V167T_ASGUARDIA_SLIDING_DOOR_SCRIPT_SOUND_SUPPRESS name=%s state=%s caller=%s sound=%s key=%d prev=%d num=%d interp=%d client=%d loc=%.1f,%.1f,%.1f",
		appToAnsi(Mover->GetName()),
		StateName!=NAME_None ? appToAnsi(*StateName) : "None",
		Caller ? appToAnsi(Caller->GetName()) : "None",
		appToAnsi(Sound->GetName()),
		(INT)Mover->KeyNum,
		(INT)Mover->PrevKeyNum,
		(INT)Mover->numTriggerEvents,
		(INT)Mover->bInterpolating,
		(INT)Mover->ClientUpdate,
		Mover->Location.X,
		Mover->Location.Y,
		Mover->Location.Z );
	return 1;
}


static void UT99AndroidV167PPlayASGuardiaDoorFinishSound( AMover* Mover, USound* Sound, const char* Reason )
{
	if( !Mover || !Sound || !Mover->GetLevel() || !Mover->GetLevel()->Engine || !Mover->GetLevel()->Engine->Audio )
		return;

	const FLOAT Volume = Mover->TransientSoundVolume > 0.0f ? Mover->TransientSoundVolume : 1.0f;
	const FLOAT Radius = Mover->TransientSoundRadius > 0.0f ? Mover->TransientSoundRadius : 1600.0f;
	const INT Id = Mover->GetIndex()*16 + SLOT_None*2;
	Mover->GetLevel()->Engine->Audio->PlaySound( Mover, Id, Sound, Mover->Location, Volume, Radius, 1.0f );
	UT99_ANDROID_MOVER_LOGI(
		"UT99_ANDROID_V167T_ASGUARDIA_SLIDING_DOOR_FINISH_SOUND name=%s reason=%s sound=%s loc=%.1f,%.1f,%.1f",
		appToAnsi(Mover->GetName()),
		Reason ? Reason : "finish",
		appToAnsi(Sound->GetName()),
		Mover->Location.X,
		Mover->Location.Y,
		Mover->Location.Z );
}

static UBOOL UT99AndroidConsumeASGuardiaSlidingDoorFinish
(
	AMover* Mover,
	FName StateName,
	BYTE FinishKeyBefore,
	BYTE FinishPrevBefore,
	INT ClientBeforeEvent
)
{
	if( !UT99AndroidIsASGuardiaSlidingDoorMover(Mover, StateName) )
		return 0;

	// UT99_ANDROID_V167T_ASGUARDIA_SLIDING_DOOR_FINISH_CONSUME:
	// Mover0/1 are now driven by the simple native pawn-zone controller in
	// UnLevTic.cpp. Letting the original TriggerControl latent state resume here
	// replays FinishedOpening()/FinishedClosing() late. Do not play any finish
	// sound here; the native controller plays the appropriate door sound exactly
	// when the visible movement starts.

	Mover->PhysAlpha = 0.0f;
	Mover->bInterpolating = 0;
	Mover->PrevKeyNum = Mover->KeyNum;
	Mover->ClientUpdate = 0;
	Mover->AmbientSound = NULL;
	if( Mover->Level && Mover->Level->NetMode!=NM_Client )
	{
		Mover->RealPosition = Mover->Location;
		Mover->RealRotation = Mover->Rotation;
	}

	UT99_ANDROID_MOVER_LOGI(
		"UT99_ANDROID_V167T_ASGUARDIA_SLIDING_DOOR_FINISH_CONSUME name=%s state=%s finish=%d/%d key=%d prev=%d client=%d->%d num=%d loc=%.1f,%.1f,%.1f",
		appToAnsi(Mover->GetName()),
		StateName!=NAME_None ? appToAnsi(*StateName) : "None",
		(INT)FinishKeyBefore,
		(INT)FinishPrevBefore,
		(INT)Mover->KeyNum,
		(INT)Mover->PrevKeyNum,
		ClientBeforeEvent,
		(INT)Mover->ClientUpdate,
		(INT)Mover->numTriggerEvents,
		Mover->Location.X,
		Mover->Location.Y,
		Mover->Location.Z );
	return 1;
}

static void UT99AndroidRepairASGuardiaSlidingDoorState
(
	AMover* Mover,
	FName StateName,
	BYTE FinishKeyBefore,
	BYTE FinishPrevBefore,
	INT ClientBeforeEvent,
	BYTE KeyAfterEvent,
	BYTE PrevAfterEvent,
	INT ClientAfterEvent
)
{
	if( !UT99AndroidIsASGuardiaSlidingDoorMover(Mover, StateName) )
		return;

	// UT99_ANDROID_V167T_ASGUARDIA_SLIDING_DOOR_FINISH_PASSIVE:
	// v167h-l tried to repair this TriggerControl mover at FinishInterpolation().
	// That fixed one symptom at a time but also created new stale states: open but
	// never closing, closed but never reopening, or sound-only transitions.  v167o
	// moves the actual rule to the post-physics actor tick in UnLevTic.cpp: live Pawn in the
	// doorway opens/holds; empty doorway closes after a short grace period.  Keep
	// this finish hook passive so it can no longer fight that simple rule.
	return;
}


static void UT99AndroidReleaseFinishedASGuardiaExplosiveWallMover
(
	AMover* Mover,
	FName StateName,
	BYTE FinishKeyBefore,
	BYTE FinishPrevBefore,
	INT ClientBeforeEvent,
	BYTE KeyAfterEvent,
	BYTE PrevAfterEvent,
	INT ClientAfterEvent
)
{
	if( !Mover || StateName!=FName(TEXT("TriggerOpenTimed"), FNAME_Find) )
		return;

	if( Mover->Level && Mover->Level->NetMode==NM_Client )
		return;

	const UBOOL bASGuardiaWall = UT99AndroidIsASGuardiaExplosiveWallMover( Mover, StateName, FinishKeyBefore, FinishPrevBefore );
	const UBOOL bGenericDamageOneShot = Mover->bDamageTriggered && Mover->bTriggerOnceOnly && FinishKeyBefore>FinishPrevBefore && FinishKeyBefore!=0;

	// v167a only handled the generic flag combination.  AS-Guardia's visible
	// destructible wall chunks reach the exact final endpoint logged on OUYA
	// (Mover7/8/9, TriggerOpenTimed, key=3, prev=0) but do not report through
	// that generic flag pair, so keep a map-and-mover-name scoped release path.
	if( !bASGuardiaWall && !bGenericDamageOneShot )
		return;

	if( Mover->bInterpolating || Mover->bDeleteMe )
		return;

	const UBOOL HadCollideActors = Mover->bCollideActors;
	const UBOOL HadCollideWorld  = Mover->bCollideWorld;
	const UBOOL HadBlockActors   = Mover->bBlockActors;
	const UBOOL HadBlockPlayers  = Mover->bBlockPlayers;
	const UBOOL HadProjTarget    = Mover->bProjTarget;
	const BYTE  OldDrawType      = Mover->DrawType;
	const UBOOL WasHidden        = Mover->bHidden;
	const UBOOL WasDamage        = Mover->bDamageTriggered;
	const UBOOL WasOnce          = Mover->bTriggerOnceOnly;
	const FVector OldLocation    = Mover->Location;

	if( Mover->GetLevel() && Mover->GetLevel()->BrushTracker )
		Mover->GetLevel()->BrushTracker->Flush( Mover );

	Mover->SetCollision( 0, 0, 0 );
	Mover->bCollideWorld = 0;
	Mover->bCollideWhenPlacing = 0;
	Mover->bProjTarget = 0;
	Mover->bHidden = 1;
	Mover->DrawType = DT_None;
	Mover->AmbientSound = NULL;
	Mover->bInterpolating = 0;
	Mover->bDelaying = 0;
	Mover->PhysAlpha = 0.0f;
	Mover->ClientUpdate = 0;
	Mover->RealPosition = Mover->Location;
	Mover->RealRotation = Mover->Rotation;

	if( Mover->GetLevel() && Mover->GetLevel()->BrushTracker )
		Mover->GetLevel()->BrushTracker->Flush( Mover );

	UT99_ANDROID_MOVER_LOGI(
		"UT99_ANDROID_V167B_ASGUARDIA_EXPLOSIVE_WALL_RELEASE name=%s state=%s finish=%d/%d event=%d/%d client=%d/%d/%d asg=%d damage=%d once=%d collide=%d/%d/%d/%d proj=%d draw=%d hidden=%d loc=%.1f,%.1f,%.1f",
		appToAnsi(Mover->GetName()),
		StateName!=NAME_None ? appToAnsi(*StateName) : "None",
		(INT)FinishKeyBefore,
		(INT)FinishPrevBefore,
		(INT)KeyAfterEvent,
		(INT)PrevAfterEvent,
		ClientBeforeEvent,
		ClientAfterEvent,
		(INT)Mover->ClientUpdate,
		(INT)bASGuardiaWall,
		(INT)WasDamage,
		(INT)WasOnce,
		(INT)HadCollideActors,
		(INT)HadCollideWorld,
		(INT)HadBlockActors,
		(INT)HadBlockPlayers,
		(INT)HadProjTarget,
		(INT)OldDrawType,
		(INT)WasHidden,
		OldLocation.X,
		OldLocation.Y,
		OldLocation.Z );
}

#else
#define UT99_ANDROID_MOVER_LOGI(...)
#endif

/*-----------------------------------------------------------------------------
	Tim's physics modes.
-----------------------------------------------------------------------------*/

FLOAT Splerp( FLOAT F )
{
	FLOAT S = Square(F);
	return (1.0/16.0)*S*S - (1.0/2.0)*S + 1;
}

//
// Interpolating along a path.
//
void AActor::physPathing( FLOAT DeltaTime )
{
	guard(AActor::physPathing);

	// Linear interpolate from Target to Target.Next.
	while( PhysRate!=0.0 && bInterpolating && DeltaTime>0.0 )
	{
		// Find destination interpolation point, if any.
		AInterpolationPoint* Dest = Cast<AInterpolationPoint>( Target );

		// Compute rate modifier.
		FLOAT RateModifier = 1.0;
		if( Dest && Dest->Next )
			RateModifier = Dest->RateModifier * (1.0 - PhysAlpha) + Dest->Next->RateModifier * PhysAlpha;

		// Update level slomo.
		Level->TimeDilation = Dest->GameSpeedModifier * (1.0 - PhysAlpha) + Dest->Next->GameSpeedModifier * PhysAlpha;

		// Update screenflash and FOV.
		if( IsA(APlayerPawn::StaticClass()) )
		{
			((APlayerPawn*)this)->FlashScale = FVector(1,1,1)*(((APlayerPawn*)this)->DesiredFlashScale = (Dest->ScreenFlashScale * (1.0 - PhysAlpha) + Dest->Next->ScreenFlashScale * PhysAlpha));
			((APlayerPawn*)this)->FlashFog   = ((APlayerPawn*)this)->DesiredFlashFog   = (Dest->ScreenFlashFog   * (1.0 - PhysAlpha) + Dest->Next->ScreenFlashFog   * PhysAlpha);
			((APlayerPawn*)this)->FovAngle                                             = (Dest->FovModifier      * (1.0 - PhysAlpha) + Dest->Next->FovModifier      * PhysAlpha) * ((APlayerPawn*)GetClass()->GetDefaultObject())->FovAngle;
		}

		// Update alpha.
		FLOAT OldAlpha  = PhysAlpha;
		FLOAT DestAlpha = PhysAlpha + PhysRate * RateModifier * DeltaTime;
		PhysAlpha       = Clamp( DestAlpha, 0.f, 1.f );

		// Move and rotate.
		if( Dest && Dest->Next )
		{
			FCheckResult Hit;
			FVector NewLocation;
			FRotator NewRotation;
			if( Dest->Prev && Dest->Next->Next )
			{
				// Cubic spline interpolation.
				FLOAT W0 = Splerp(PhysAlpha+1.0);
				FLOAT W1 = Splerp(PhysAlpha+0.0);
				FLOAT W2 = Splerp(PhysAlpha-1.0);
				FLOAT W3 = Splerp(PhysAlpha-2.0);
				FLOAT RW = 1.0 / (W0 + W1 + W2 + W3);
				NewLocation = (W0*Dest->Prev->Location + W1*Dest->Location + W2*Dest->Next->Location + W3*Dest->Next->Next->Location)*RW;
				NewRotation = (W0*Dest->Prev->Rotation + W1*Dest->Rotation + W2*Dest->Next->Rotation + W3*Dest->Next->Next->Rotation)*RW;
			}
			else
			{
				// Linear interpolation.
				FLOAT W0 = 1.0 - PhysAlpha;
				FLOAT W1 = PhysAlpha;
				NewLocation = W0*Dest->Location + W1*Dest->Next->Location;
				NewRotation = W0*Dest->Rotation + W1*Dest->Next->Rotation;
			}
			GetLevel()->MoveActor( this, NewLocation - Location, NewRotation, Hit );
			if( IsA(APawn::StaticClass()) )
				((APawn*)this)->ViewRotation = Rotation;
		}

		// If overflowing, notify and go to next place.
		if( PhysRate>0.0 && DestAlpha>1.0 )
		{
			PhysAlpha = 0.0;
			DeltaTime *= (DestAlpha - 1.0) / (DestAlpha - OldAlpha);
			if( Target )
			{
				Target->eventInterpolateEnd(this);
				eventInterpolateEnd(Target);
				if( Dest )
				{
					do
					{
						Target = Dest->Next;
						Dest = Cast<AInterpolationPoint>( Target );
					} while( Dest && Dest->bSkipNextPath );
				}
			}
		}
		else if( PhysRate<0.0 && DestAlpha<0.0 )
		{
			PhysAlpha = 1.0;
			DeltaTime *= (0.0 - DestAlpha) / (OldAlpha - DestAlpha);
			if( Target )
			{
				Target->eventInterpolateEnd(this);
				eventInterpolateEnd(Target);
				if( Dest )
				{
					do
					{
						Target = Dest->Prev;
						Dest = Cast<AInterpolationPoint>( Target );
					} while( Dest && Dest->bSkipNextPath );
				}
			}
			eventInterpolateEnd(NULL);
		}
		else DeltaTime=0.0;
	};
	unguard;
}

//
// Moving brush.
//
void AActor::physMovingBrush( FLOAT DeltaTime )
{
	guard(physMovingBrush);
	if( IsA(AMover::StaticClass()) )
	{
		AMover* Mover  = (AMover*)this;
		#if defined(__ANDROID__) || defined(PLATFORM_ANDROID)
		UT99AndroidPumpTriggerOpenTimedNativeClose( Mover );
		#endif
		while( Mover->bInterpolating && DeltaTime>0.0 )
		{
			// We are moving.
			FLOAT NewAlpha = Mover->PhysAlpha + DeltaTime * Mover->PhysRate;
			if( NewAlpha > 1.0 )
			{
				DeltaTime *= (NewAlpha - 1.0) / (NewAlpha - PhysAlpha);
				NewAlpha   = 1.0;
			}
			else DeltaTime = 0.0;

			// Compute alpha.
			FLOAT RenderAlpha;
			if( Mover->MoverGlideType == MV_GlideByTime )
			{
				// Make alpha time-smooth and time-continuous.
				// f(0)=0, f(1)=1, f'(0)=f'(1)=0.
				RenderAlpha = 3.0*NewAlpha*NewAlpha - 2.0*NewAlpha*NewAlpha*NewAlpha;
			}
			else RenderAlpha = NewAlpha;

			// Move.
			// UT99_ANDROID_V166L_MULTISTEP_MOVER_KEY_REFRESH:
			// InterpolateEnd() may start the next key of a chained mover while this
			// same physics tick still has leftover DeltaTime.  Keep the target key
			// fresh for every loop iteration; otherwise fast multi-key movers can
			// finish logically at their final key while the brush/collision remains
			// at an older key. AS-Guardia's collapsing wall uses exactly that path.
			INT KeyNum = Clamp( (INT)Mover->KeyNum, (INT)0, (INT)ARRAY_COUNT(Mover->KeyPos)-1 );
			FCheckResult Hit(1.0);
			FVector MoveTarget = Mover->BasePos + Mover->KeyPos[KeyNum];
			FRotator RotTarget = Mover->BaseRot + Mover->KeyRot[KeyNum];
			FVector MoveDelta = Mover->OldPos + (MoveTarget - Mover->OldPos) * RenderAlpha - Mover->Location;
			FRotator MoveRot = Mover->OldRot + (RotTarget - Mover->OldRot) * RenderAlpha;

			#if defined(__ANDROID__) || defined(PLATFORM_ANDROID)
			// UT99_ANDROID_V153_STANDOPEN_FINISH_ONLY_FIX:
			// A same-key interpolation on Android is never useful for StandOpenTimed
			// lifts.  At the top, key1->key1 during close becomes key1->key0.
			// At the bottom, key0->key0 during open becomes key0->key1.  Any
			// remaining no-op is finished silently and then normalized after the
			// script InterpolateEnd() returns.
			const UBOOL bAndroidSameKeyNoMove =
				Mover->KeyNum==Mover->PrevKeyNum
			&&	(MoveTarget - Mover->OldPos).SizeSquared() < 1.0f
			&&	(MoveDelta).SizeSquared() < 1.0f;
			if( bAndroidSameKeyNoMove && (UT99AndroidIsStandOpenTimedMover(Mover,NULL) || UT99AndroidIsBumpOpenTimedMover(Mover,NULL) || UT99AndroidIsTriggerControlMover(Mover,NULL) || UT99AndroidIsTriggerToggleMover(Mover,NULL)) )
			{
				FName StateName = NAME_None;
				const UBOOL bStandOpenTimed = UT99AndroidIsStandOpenTimedMover( Mover, &StateName );
				const UBOOL bBumpOpenTimed = StateName == FName(TEXT("BumpOpenTimed"), FNAME_Find);
				const UBOOL bTriggerControl = StateName == FName(TEXT("TriggerControl"), FNAME_Find);
				const UBOOL bTriggerToggle = StateName == FName(TEXT("TriggerToggle"), FNAME_Find);
				const UBOOL bTimedBumpOrStand = bStandOpenTimed || bBumpOpenTimed;
				const UBOOL bEventTrigger = (bTriggerControl || bTriggerToggle) && !UT99AndroidIsGuardiaScriptOwnedTriggerControlMover(Mover, StateName);
				const UBOOL bTriggerOpenTimed = StateName == FName(TEXT("TriggerOpenTimed"), FNAME_Find);
				const INT OldNumTriggerEvents = Mover->numTriggerEvents;
				const INT OldClientUpdate = Mover->ClientUpdate;

				if( bEventTrigger && Mover->bOpening && Mover->KeyNum==0 )
				{
					const BYTE OldKey = Mover->KeyNum;
					const BYTE OldPrev = Mover->PrevKeyNum;
					Mover->bDelaying = 0;
					UT99AndroidReplaceMoverInterpolation( Mover, 1, Mover->MoveTime );
					Mover->AmbientSound = Mover->MoveAmbientSound;
					UT99_ANDROID_MOVER_LOGI(
						"UT99_ANDROID_V170A_EVENT_TRIGGER_REPLACE_SAMEKEY_WITH_OPEN name=%s state=%s key=%d/%d->%d/%d num=%d clientUpdate=%d->%d opening=%d delaying=%d rate=%.3f",
						appToAnsi(Mover->GetName()),
						StateName!=NAME_None ? appToAnsi(*StateName) : "None",
						(INT)OldKey,
						(INT)OldPrev,
						(INT)Mover->KeyNum,
						(INT)Mover->PrevKeyNum,
						(INT)Mover->numTriggerEvents,
						OldClientUpdate,
						(INT)Mover->ClientUpdate,
						(INT)Mover->bOpening,
						(INT)Mover->bDelaying,
						(FLOAT)Mover->PhysRate );
					DeltaTime = 0.0f;
					continue;
				}

				if( bEventTrigger && !Mover->bOpening && Mover->KeyNum>0 )
				{
					const BYTE OldKey = Mover->KeyNum;
					const BYTE OldPrev = Mover->PrevKeyNum;
					Mover->bDelaying = 0;
					UT99AndroidReplaceMoverInterpolation( Mover, 0, Mover->MoveTime );
					Mover->AmbientSound = Mover->MoveAmbientSound;
					UT99_ANDROID_MOVER_LOGI(
						"UT99_ANDROID_V170A_EVENT_TRIGGER_REPLACE_SAMEKEY_WITH_CLOSE name=%s state=%s key=%d/%d->%d/%d num=%d clientUpdate=%d->%d opening=%d delaying=%d rate=%.3f",
						appToAnsi(Mover->GetName()),
						StateName!=NAME_None ? appToAnsi(*StateName) : "None",
						(INT)OldKey,
						(INT)OldPrev,
						(INT)Mover->KeyNum,
						(INT)Mover->PrevKeyNum,
						(INT)Mover->numTriggerEvents,
						OldClientUpdate,
						(INT)Mover->ClientUpdate,
						(INT)Mover->bOpening,
						(INT)Mover->bDelaying,
						(FLOAT)Mover->PhysRate );
					DeltaTime = 0.0f;
					continue;
				}

				if( bTimedBumpOrStand && Mover->bOpening && Mover->KeyNum==0 )
				{
					const BYTE OldKey = Mover->KeyNum;
					const BYTE OldPrev = Mover->PrevKeyNum;
					Mover->bDelaying = 0;
					UT99AndroidReplaceMoverInterpolation( Mover, 1, Mover->MoveTime );
					Mover->AmbientSound = Mover->MoveAmbientSound;
					UT99_ANDROID_MOVER_LOGI(
						"UT99_ANDROID_V168M_TIMED_BUMP_OR_STAND_REPLACE_SAMEKEY_WITH_OPEN name=%s state=%s key=%d/%d->%d/%d clientUpdate=%d->%d opening=%d delaying=%d rate=%.3f",
						appToAnsi(Mover->GetName()),
						StateName!=NAME_None ? appToAnsi(*StateName) : "None",
						(INT)OldKey,
						(INT)OldPrev,
						(INT)Mover->KeyNum,
						(INT)Mover->PrevKeyNum,
						OldClientUpdate,
						(INT)Mover->ClientUpdate,
						(INT)Mover->bOpening,
						(INT)Mover->bDelaying,
						(FLOAT)Mover->PhysRate );
					DeltaTime = 0.0f;
					continue;
				}

				if( bTimedBumpOrStand && !Mover->bOpening && Mover->KeyNum>0 )
				{
					const BYTE OldKey = Mover->KeyNum;
					const BYTE OldPrev = Mover->PrevKeyNum;
					Mover->bDelaying = 0;
					UT99AndroidReplaceMoverInterpolation( Mover, 0, Mover->MoveTime );
					Mover->AmbientSound = Mover->MoveAmbientSound;
					UT99_ANDROID_MOVER_LOGI(
						"UT99_ANDROID_V168M_TIMED_BUMP_OR_STAND_REPLACE_SAMEKEY_WITH_CLOSE name=%s state=%s key=%d/%d->%d/%d clientUpdate=%d->%d opening=%d delaying=%d rate=%.3f",
						appToAnsi(Mover->GetName()),
						StateName!=NAME_None ? appToAnsi(*StateName) : "None",
						(INT)OldKey,
						(INT)OldPrev,
						(INT)Mover->KeyNum,
						(INT)Mover->PrevKeyNum,
						OldClientUpdate,
						(INT)Mover->ClientUpdate,
						(INT)Mover->bOpening,
						(INT)Mover->bDelaying,
						(FLOAT)Mover->PhysRate );
					DeltaTime = 0.0f;
					continue;
				}

				Mover->PhysAlpha = 1.0f;
				Mover->bInterpolating = 0;
				Mover->AmbientSound = NULL;
				UT99_ANDROID_MOVER_LOGI(
					"UT99_ANDROID_V119_STANDOPEN_SAMEKEY_FINISH_ONLY name=%s state=%s key=%d prev=%d num=%d->%d clientUpdate=%d standopen=%d opening=%d",
					appToAnsi(Mover->GetName()),
					StateName!=NAME_None ? appToAnsi(*StateName) : "None",
					(INT)Mover->KeyNum,
					(INT)Mover->PrevKeyNum,
					OldNumTriggerEvents,
					(INT)Mover->numTriggerEvents,
					OldClientUpdate,
					(INT)bTimedBumpOrStand,
					(INT)Mover->bOpening );
				const BYTE AndroidSameKeyFinishKeyBefore = Mover->KeyNum;
				const BYTE AndroidSameKeyFinishPrevBefore = Mover->PrevKeyNum;
				const INT AndroidSameKeyFinishClientBefore = Mover->ClientUpdate;
				Mover->eventInterpolateEnd(NULL);
				if( bStandOpenTimed )
				{
					UT99AndroidNormalizeStandOpenFinish(
						Mover,
						StateName,
						AndroidSameKeyFinishKeyBefore,
						AndroidSameKeyFinishPrevBefore,
						AndroidSameKeyFinishClientBefore,
						Mover->KeyNum,
						Mover->PrevKeyNum,
						Mover->ClientUpdate );
				}
				else if( bBumpOpenTimed )
				{
					UT99AndroidNormalizeBumpOpenFinish(
						Mover,
						StateName,
						AndroidSameKeyFinishKeyBefore,
						AndroidSameKeyFinishPrevBefore,
						AndroidSameKeyFinishClientBefore,
						Mover->KeyNum,
						Mover->PrevKeyNum,
						Mover->ClientUpdate );
				}
				else if( bEventTrigger )
				{
					UT99AndroidNormalizeEventTriggerFinish(
						Mover,
						StateName,
						AndroidSameKeyFinishKeyBefore,
						AndroidSameKeyFinishPrevBefore,
						AndroidSameKeyFinishClientBefore,
						Mover->KeyNum,
						Mover->PrevKeyNum,
						Mover->ClientUpdate );
				}
				else if( bTriggerOpenTimed )
				{
					UT99AndroidNormalizeTriggerOpenTimedFinish(
						Mover,
						StateName,
						AndroidSameKeyFinishKeyBefore,
						AndroidSameKeyFinishPrevBefore,
						AndroidSameKeyFinishClientBefore,
						Mover->KeyNum,
						Mover->PrevKeyNum,
						Mover->ClientUpdate );
				}
				DeltaTime = 0.0f;
				continue;
			}
			#endif

			// UT99_ANDROID_V141_MOVER_STANDALONE_NOFAIL_FIX:
			// Retroid/Android builds were still able to wedge lifts in standalone play:
			// the brush visually reaches the destination key, but a late encroachment
			// reject keeps bInterpolating set, so the scripted FinishInterpolation() never
			// resumes and MoveAmbientSound loops forever.  First keep the original UE1
			// encroachment-aware path.  Only if that path rejects the move do we retry the
			// same mover step with bNoFail on Android, letting the mover finish and hand
			// control back to the Mover state machine so timed lifts can close again.
			UBOOL bMoved = GetLevel()->MoveActor( Mover, MoveDelta, MoveRot, Hit );
			#if defined(__ANDROID__) || defined(PLATFORM_ANDROID)
			if( !bMoved )
			{
				UT99_ANDROID_MOVER_LOGI(
					"UT99_ANDROID_V141_MOVER_RETRY name=%s net=%d role=%d key=%d prev=%d alpha=%.3f new=%.3f hit=%s",
					appToAnsi(Mover->GetName()),
					Level ? (INT)Level->NetMode : -1,
					(INT)Mover->Role,
					(INT)Mover->KeyNum,
					(INT)Mover->PrevKeyNum,
					(FLOAT)Mover->PhysAlpha,
					(FLOAT)NewAlpha,
					Hit.Actor ? appToAnsi(Hit.Actor->GetName()) : "None" );
				Hit = FCheckResult(1.0);
				bMoved = GetLevel()->MoveActor( Mover, MoveDelta, MoveRot, Hit, 0, 0, 0, 1 );
				if( bMoved )
				{
					UT99_ANDROID_MOVER_LOGI(
						"UT99_ANDROID_V141_MOVER_RETRY_OK name=%s key=%d alpha=%.3f",
						appToAnsi(Mover->GetName()),
						(INT)Mover->KeyNum,
						(FLOAT)NewAlpha );
				}
			}
			#endif

			if( bMoved )
			{
				// Successfully moved.
				Mover->PhysAlpha = NewAlpha;
				if( NewAlpha == 1.0 )
				{
					// Just finished moving.
					Mover->bInterpolating = 0;
					#if defined(__ANDROID__) || defined(PLATFORM_ANDROID)
					FName StateName = (Mover->GetStateFrame() && Mover->GetStateFrame()->StateNode && Mover->GetStateFrame()->StateNode!=Mover->GetClass())
						? Mover->GetStateFrame()->StateNode->GetFName()
						: NAME_None;
					UT99_ANDROID_MOVER_LOGI(
						"UT99_ANDROID_V142_MOVER_FINISH name=%s state=%s net=%d role=%d key=%d prev=%d num=%d clientUpdate=%d",
						appToAnsi(Mover->GetName()),
						StateName!=NAME_None ? appToAnsi(*StateName) : "None",
						Level ? (INT)Level->NetMode : -1,
						(INT)Mover->Role,
						(INT)Mover->KeyNum,
						(INT)Mover->PrevKeyNum,
						(INT)Mover->numTriggerEvents,
						(INT)Mover->ClientUpdate );
					#endif
					#if defined(__ANDROID__) || defined(PLATFORM_ANDROID)
					const BYTE AndroidFinishKeyBefore = Mover->KeyNum;
					const BYTE AndroidFinishPrevBefore = Mover->PrevKeyNum;
					const INT AndroidFinishClientBefore = Mover->ClientUpdate;
					if( UT99AndroidConsumeASGuardiaSlidingDoorFinish( Mover, StateName, AndroidFinishKeyBefore, AndroidFinishPrevBefore, AndroidFinishClientBefore ) )
					{
						DeltaTime = 0.0f;
						continue;
					}
					#endif

					Mover->eventInterpolateEnd(NULL);

					#if defined(__ANDROID__) || defined(PLATFORM_ANDROID)
					UT99AndroidReleaseFinishedASGuardiaExplosiveWallMover(
						Mover,
						StateName,
						AndroidFinishKeyBefore,
						AndroidFinishPrevBefore,
						AndroidFinishClientBefore,
						Mover->KeyNum,
						Mover->PrevKeyNum,
						Mover->ClientUpdate );
					UT99AndroidRepairASGuardiaSlidingDoorState(
						Mover,
						StateName,
						AndroidFinishKeyBefore,
						AndroidFinishPrevBefore,
						AndroidFinishClientBefore,
						Mover->KeyNum,
						Mover->PrevKeyNum,
						Mover->ClientUpdate );
					UT99AndroidNormalizeStandOpenFinish(
						Mover,
						StateName,
						AndroidFinishKeyBefore,
						AndroidFinishPrevBefore,
						AndroidFinishClientBefore,
						Mover->KeyNum,
						Mover->PrevKeyNum,
						Mover->ClientUpdate );
					UT99AndroidNormalizeBumpOpenFinish(
						Mover,
						StateName,
						AndroidFinishKeyBefore,
						AndroidFinishPrevBefore,
						AndroidFinishClientBefore,
						Mover->KeyNum,
						Mover->PrevKeyNum,
						Mover->ClientUpdate );
					UT99AndroidNormalizeEventTriggerFinish(
						Mover,
						StateName,
						AndroidFinishKeyBefore,
						AndroidFinishPrevBefore,
						AndroidFinishClientBefore,
						Mover->KeyNum,
						Mover->PrevKeyNum,
						Mover->ClientUpdate );
					UT99AndroidNormalizeTriggerOpenTimedFinish(
						Mover,
						StateName,
						AndroidFinishKeyBefore,
						AndroidFinishPrevBefore,
						AndroidFinishClientBefore,
						Mover->KeyNum,
						Mover->PrevKeyNum,
						Mover->ClientUpdate );
					UT99AndroidFinishTriggerOpenTimedNativeClose(
						Mover,
						StateName,
						AndroidFinishKeyBefore,
						AndroidFinishPrevBefore,
						AndroidFinishClientBefore );
					UT99AndroidScheduleTriggerOpenTimedSingleKeyClose(
						Mover,
						StateName,
						AndroidFinishKeyBefore,
						AndroidFinishPrevBefore,
						AndroidFinishClientBefore );
					UT99AndroidScheduleTriggerOpenTimedCloseAfterOpen(
						Mover,
						StateName,
						AndroidFinishKeyBefore,
						AndroidFinishPrevBefore,
						AndroidFinishClientBefore );
					#endif

					if( Level && Level->NetMode==NM_Client && Mover->Role<ROLE_Authority )
					{
						// The scripted InterpolateEnd() decrements ClientUpdate, but on
						// clients the move was started by PostNetReceive(), not by
						// InterpolateTo().  Clamp it back to idle so Timer() can accept
						// the next replicated RealPosition/RealRotation correction.
						Mover->ClientUpdate = 0;
						Mover->RealPosition = Mover->Location;
						Mover->RealRotation = Mover->Rotation;
						Mover->AmbientSound = NULL;
					}
				}
			}
		}
	}
	unguard;
}

//
// Initialize execution.
//
void AActor::InitExecution()
{
	guard(AActor::InitExecution);

	UObject::InitExecution();

	check(GetStateFrame());
	check(GetStateFrame()->Object==this);
	check(GetLevel()!=NULL);
	check(GetLevel()->Actors(0)!=NULL);
	check(GetLevel()->Actors(0)==Level);
	check(Level!=NULL);

	unguardobj;
}

/*-----------------------------------------------------------------------------
	Natives.
-----------------------------------------------------------------------------*/

//////////////////////
// Console Commands //
//////////////////////

void AActor::execConsoleCommand( FFrame& Stack, RESULT_DECL )
{
	guard(UObject::execConsoleCommand);

	P_GET_STR(Command);
	P_FINISH;

	FStringOutputDevice StrOut;
	GetLevel()->Engine->Exec( *Command, StrOut );
	*(FString*)Result = *StrOut;

	unguard;
}

/////////////////////////////
// Log and error functions //
/////////////////////////////

void AActor::execError( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execError);

	P_GET_STR(S);
	P_FINISH;

	Stack.Log( *S );
	GetLevel()->DestroyActor( this );

	unguardexecSlow;
}

//////////////////////////
// Clientside functions //
//////////////////////////

void APlayerPawn::execClientTravel( FFrame& Stack, RESULT_DECL )
{
	guardSlow(APlayerPawn::execClientTravel);

	P_GET_STR(URL);
	P_GET_BYTE(TravelType);
	P_GET_UBOOL(bItems);
	P_FINISH;

	if( Player )
	{
		// Warn the client.
		eventPreClientTravel();

		// Do the travel.
		GetLevel()->Engine->SetClientTravel( Player, *URL, bItems, (ETravelType)TravelType );
	}

	unguardexecSlow;
}

void APlayerPawn::execGetPlayerNetworkAddress( FFrame& Stack, RESULT_DECL )
{
	guard(APlayerPawn::execGetPlayerNetworkAddress);
	P_FINISH;

	if( Player && Player->IsA(UNetConnection::StaticClass()) )
		*(FString*)Result = Cast<UNetConnection>(Player)->LowLevelGetRemoteAddress();
	else
		*(FString*)Result = TEXT("");
	unguard;
}

void APlayerPawn::execCopyToClipboard( FFrame& Stack, RESULT_DECL )
{
	guard(APlayerPawn::execCopyToClipboard);
	P_GET_STR(Text);
	P_FINISH;
	appClipboardCopy(*Text);
	unguard;
}

void APlayerPawn::execPasteFromClipboard( FFrame& Stack, RESULT_DECL )
{
	guard(APlayerPawn::execCopyToClipboard);
	P_GET_STR(Text);
	P_FINISH;
	*(FString*)Result = appClipboardPaste();
	unguard;
}

void ALevelInfo::execGetLocalURL( FFrame& Stack, RESULT_DECL )
{
	guardSlow(ALevelInfo::execGetLocalURL);

	P_FINISH;

	*(FString*)Result = GetLevel()->URL.String();

	unguardexecSlow;
}

void ALevelInfo::execGetAddressURL( FFrame& Stack, RESULT_DECL )
{
	guardSlow(ALevelInfo::execGetAddressURL);

	P_FINISH;

	*(FString*)Result = FString::Printf( TEXT("%s:%i"), *GetLevel()->URL.Host, GetLevel()->URL.Port );

	unguardexecSlow;
}

///////////////////////////
// Client-side functions //
///////////////////////////

void APawn::execClientHearSound( FFrame& Stack, RESULT_DECL )
{
	guard(APawn::execClientHearSound);

	P_GET_OBJECT(AActor,Actor);
	P_GET_INT(Id);
	P_GET_OBJECT(USound,Sound);
	P_GET_VECTOR(SoundLocation);
	P_GET_VECTOR(Parameters);
	P_FINISH;

	FLOAT Volume = 0.01 * Parameters.X;
	FLOAT Radius = Parameters.Y;
	FLOAT Pitch  = 0.01 * Parameters.Z;
	if
	(	IsA(APlayerPawn::StaticClass()) 
	&&	((APlayerPawn*)this)->Player
	&&	((APlayerPawn*)this)->Player->IsA(UViewport::StaticClass())
	&&	GetLevel()->Engine->Audio )
	{
		if( Actor && Actor->bDeleteMe )
			Actor = NULL;
		GetLevel()->Engine->Audio->PlaySound( Actor, Id, Sound, SoundLocation, Volume, Radius ? Radius : 1600.f, Pitch );
	}
	unguardexec;
}

////////////////////////////////
// Latent function initiators //
////////////////////////////////

void AActor::execSleep( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execSleep);

	P_GET_FLOAT(Seconds);
	P_FINISH;

	GetStateFrame()->LatentAction = EPOLL_Sleep;
	LatentFloat  = Seconds;

	unguardexecSlow;
}

void AActor::execFinishAnim( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execFinishAnim);

	P_FINISH;

	// If we are looping, finish at the next sequence end.
	if( bAnimLoop )
	{
		bAnimLoop     = 0;
		bAnimFinished = 0;
	}

	// If animation is playing, wait for it to finish.
	if( IsAnimating() && AnimFrame<AnimLast )
		GetStateFrame()->LatentAction = EPOLL_FinishAnim;

	unguardexecSlow;
}

void AActor::execFinishInterpolation( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execFinishInterpolation);

	P_FINISH;

	GetStateFrame()->LatentAction = EPOLL_FinishInterpolation;

	unguardexecSlow;
}

///////////////////////////
// Slow function pollers //
///////////////////////////

void AActor::execPollSleep( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execPollSleep);

#ifdef PLATFORM_DREAMCAST
	// try to avoid potential unaligned accesses
	FLOAT DeltaSeconds = 0.0f;
	__builtin_memcpy( (void*)&DeltaSeconds, (void*)Result, sizeof(FLOAT) );
#else
	FLOAT DeltaSeconds = *(FLOAT*)Result;
#endif
	if( (LatentFloat-=DeltaSeconds) < 0.5 * DeltaSeconds )
	{
		// Awaken.
		GetStateFrame()->LatentAction = 0;
	}
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( AActor, EPOLL_Sleep, execPollSleep );

void AActor::execPollFinishAnim( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execPollFinishAnim);

	if( bAnimFinished )
		GetStateFrame()->LatentAction = 0;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( AActor, EPOLL_FinishAnim, execPollFinishAnim );

void AActor::execPollFinishInterpolation( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execPollFinishInterpolation);

	if( !bInterpolating )
		GetStateFrame()->LatentAction = 0;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( AActor, EPOLL_FinishInterpolation, execPollFinishInterpolation );

/////////////////////////
// Animation functions //
/////////////////////////

void AActor::execPlayAnim( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execPlayAnim);

	P_GET_NAME(SequenceName);
	P_GET_FLOAT_OPTX(PlayAnimRate,1.0);
	P_GET_FLOAT_OPTX(TweenTime,-1.0);
	P_FINISH;

	// Set one-shot animation.
	if( Mesh )
	{
		const FMeshAnimSeq* Seq = Mesh->GetAnimSeq( SequenceName );
		if( Seq )
		{
			if( AnimSequence == NAME_None )
				TweenTime = 0.0;
			AnimSequence  = SequenceName;
			AnimRate      = PlayAnimRate * Seq->Rate / Seq->NumFrames;
			AnimLast      = 1.0 - 1.0 / Seq->NumFrames;
			bAnimNotify   = Seq->Notifys.Num()!=0;
			bAnimFinished = 0;
			bAnimLoop     = 0;
			if( AnimLast == 0.0 )
			{
				AnimMinRate   = 0.0;
				bAnimNotify   = 0;
				OldAnimRate   = 0;
				if( TweenTime > 0.0 )
					TweenRate = 1.0 / TweenTime;
				else
					TweenRate = 10.0; //tween in 0.1 sec
				AnimFrame = -1.0/Seq->NumFrames;
				AnimRate = 0;
			}
			else if( TweenTime>0.0 )
			{
				TweenRate = 1.0 / (TweenTime * Seq->NumFrames);
				AnimFrame = -1.0/Seq->NumFrames;
			}
			else if ( TweenTime == -1.0 )
			{
				AnimFrame = -1.0/Seq->NumFrames;
				if ( OldAnimRate > 0 )
					TweenRate = OldAnimRate;
				else if ( OldAnimRate < 0 ) //was velocity based looping
					TweenRate = ::Max(0.5f * AnimRate, -1 * Velocity.Size() * OldAnimRate );
				else
					TweenRate =  1.0/(0.025 * Seq->NumFrames);
			}
			else
			{
				TweenRate = 0.0;
				AnimFrame = 0.001;
			}
			FPlane OldSimAnim = SimAnim;
			SimAnim.X = 10000 * AnimFrame;
			SimAnim.Y = 5000 * AnimRate;
			if ( SimAnim.Y > 32767 )
				SimAnim.Y = 32767;
			SimAnim.Z = 1000 * TweenRate;
			SimAnim.W = 10000 * AnimLast;
			/*
			if ( IsA(AWeapon::StaticClass())
				&& (PlayAnimRate * Seq->Rate < 0.21) )
			{
				SimAnim.X = 0;
				SimAnim.Z = 0;
			} */
				
			if ( OldSimAnim == SimAnim )
				SimAnim.W = SimAnim.W + 1;
			OldAnimRate = AnimRate;
			//debugf("%s PlayAnim %f %f %f %f", GetName(), SimAnim.X, SimAnim.Y, SimAnim.Z, SimAnim.W);
		}
		else Stack.Logf( TEXT("PlayAnim: Sequence '%s' not found in Mesh '%s'"), *SequenceName, Mesh->GetName() );
	} else Stack.Logf( TEXT("PlayAnim: No mesh") );
	unguardexecSlow;
}

void AActor::execLoopAnim( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execLoopAnim);

	P_GET_NAME(SequenceName);
	P_GET_FLOAT_OPTX(PlayAnimRate,1.0);
	P_GET_FLOAT_OPTX(TweenTime,-1.0);
	P_GET_FLOAT_OPTX(MinRate,0.0);
	P_FINISH;

	// Set looping animation.
	if( Mesh )
	{
		const FMeshAnimSeq* Seq = Mesh->GetAnimSeq( SequenceName );
		if( Seq )
		{
			if ( (AnimSequence == SequenceName) && bAnimLoop && IsAnimating() )
			{
				AnimRate      = PlayAnimRate * Seq->Rate / Seq->NumFrames;
				bAnimFinished = 0;
				AnimMinRate   = MinRate!=0.0 ? MinRate * (Seq->Rate / Seq->NumFrames) : 0.0;
				FPlane OldSimAnim = SimAnim;
				OldAnimRate   = AnimRate;		
				SimAnim.Y = 5000 * AnimRate;
				SimAnim.W = -10000 * (1.0 - 1.0 / Seq->NumFrames);
				if ( OldSimAnim == SimAnim )
					SimAnim.W = SimAnim.W + 1;
				return;
			}
			if( AnimSequence == NAME_None )
				TweenTime = 0.0;
			AnimSequence  = SequenceName;
			AnimRate      = PlayAnimRate * Seq->Rate / Seq->NumFrames;
			AnimLast      = 1.0 - 1.0 / Seq->NumFrames;
			AnimMinRate   = MinRate!=0.0 ? MinRate * (Seq->Rate / Seq->NumFrames) : 0.0;
			bAnimNotify   = Seq->Notifys.Num()!=0;
			bAnimFinished = 0;
			bAnimLoop     = 1;
			if ( AnimLast == 0.0 )
			{
				AnimMinRate   = 0.0;
				bAnimNotify   = 0;
				OldAnimRate   = 0;
				if ( TweenTime > 0.0 )
					TweenRate = 1.0 / TweenTime;
				else
					TweenRate = 10.0; //tween in 0.1 sec
				AnimFrame = -1.0/Seq->NumFrames;
				AnimRate = 0;
			}
			else if( TweenTime>0.0 )
			{
				TweenRate = 1.0 / (TweenTime * Seq->NumFrames);
				AnimFrame = -1.0/Seq->NumFrames;
			}
			else if ( TweenTime == -1.0 )
			{
				AnimFrame = -1.0/Seq->NumFrames;
				if ( OldAnimRate > 0 )
					TweenRate = OldAnimRate;
				else if ( OldAnimRate < 0 ) //was velocity based looping
					TweenRate = ::Max(0.5f * AnimRate, -1 * Velocity.Size() * OldAnimRate );
				else
					TweenRate =  1.0/(0.025 * Seq->NumFrames);
			}
			else
			{
				TweenRate = 0.0;
				AnimFrame = 0.0001;
			}
			OldAnimRate = AnimRate;
			SimAnim.X = 10000 * AnimFrame;
			SimAnim.Y = 5000 * AnimRate;
			if ( SimAnim.Y > 32767 )
				SimAnim.Y = 32767;
			SimAnim.Z = 1000 * TweenRate;
			SimAnim.W = -10000 * AnimLast;
			//debugf("%s LoopAnim %f %f %f %f", GetName(), SimAnim.X, SimAnim.Y, SimAnim.Z, SimAnim.W);
		}
		else Stack.Logf( TEXT("LoopAnim: Sequence '%s' not found in Mesh '%s'"), *SequenceName, Mesh->GetName() );
	} else Stack.Logf( TEXT("LoopAnim: No mesh") );
	unguardexecSlow;
}

void AActor::execTweenAnim( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execTweenAnim);

	P_GET_NAME(SequenceName);
	P_GET_FLOAT(TweenTime);
	P_FINISH;

	// Tweening an animation from wherever it is, to the start of a specified sequence.
	if( Mesh )
	{
		const FMeshAnimSeq* Seq = Mesh->GetAnimSeq( SequenceName );
		if( Seq )
		{
			AnimSequence  = SequenceName;
			AnimLast      = 0.0;
			AnimMinRate   = 0.0;
			bAnimNotify   = 0;
			bAnimFinished = 0;
			bAnimLoop     = 0;
			AnimRate      = 0;
			OldAnimRate   = 0;
			if( TweenTime>0.0 )
			{
				TweenRate =  1.0/(TweenTime * Seq->NumFrames);
				AnimFrame = -1.0/Seq->NumFrames;
			}
			else
			{
				TweenRate = 0.0;
				AnimFrame = 0.0;
			}
			SimAnim.X = 10000 * AnimFrame;
			SimAnim.Y = 5000 * AnimRate;
			if ( SimAnim.Y > 32767 )
				SimAnim.Y = 32767;
			SimAnim.Z = 1000 * TweenRate;
			SimAnim.W = 10000 * AnimLast;
			//debugf("%s TweenAnim %f %f %f %f", GetName(), SimAnim.X, SimAnim.Y, SimAnim.Z, SimAnim.W);
		}
		else Stack.Logf( TEXT("TweenAnim: Sequence '%s' not found in Mesh '%s'"), *SequenceName, Mesh->GetName() );
	} else Stack.Logf( TEXT("TweenAnim: No mesh") );
	unguardexecSlow;
}

void AActor::execIsAnimating( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execIsAnimating);

	P_FINISH;

	*(DWORD*)Result = IsAnimating();

	unguardexecSlow;
}

void AActor::execGetAnimGroup( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execGetAnimGroup);

	P_GET_NAME(SequenceName);
	P_FINISH;

	// Return the animation group.
	*(FName*)Result = NAME_None;
	if( Mesh )
	{
		const FMeshAnimSeq* Seq = Mesh->GetAnimSeq( SequenceName );
		if( Seq )
		{
			*(FName*)Result = Seq->Group;
		}
		else Stack.Logf( TEXT("GetAnimGroup: Sequence '%s' not found in Mesh '%s'"), *SequenceName, Mesh->GetName() );
	} else Stack.Logf( TEXT("GetAnimGroup: No mesh") );

	unguardexecSlow;
}

void AActor::execHasAnim( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execHasAnim);

	P_GET_NAME(SequenceName);
	P_FINISH;

	// Check for a certain anim sequence.
	if( Mesh )
	{
		const FMeshAnimSeq* Seq = Mesh->GetAnimSeq( SequenceName );
		if( Seq )
		{
			*(DWORD*)Result = 1;
		} else
			*(DWORD*)Result = 0;
	} else Stack.Logf( TEXT("HasAnim: No mesh") );
	unguardexecSlow;
}

///////////////
// Collision //
///////////////

void AActor::execSetCollision( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execSetCollision);

	P_GET_UBOOL_OPTX(NewCollideActors,bCollideActors);
	P_GET_UBOOL_OPTX(NewBlockActors,  bBlockActors  );
	P_GET_UBOOL_OPTX(NewBlockPlayers, bBlockPlayers );
	P_FINISH;

	SetCollision( NewCollideActors, NewBlockActors, NewBlockPlayers );

	unguardexecSlow;
}

void AActor::execSetCollisionSize( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execSetCollisionSize);

	P_GET_FLOAT(NewRadius);
	P_GET_FLOAT(NewHeight);
	P_FINISH;

	SetCollisionSize( NewRadius, NewHeight );

	// Return boolean success or failure.
	*(DWORD*)Result = 1;

	unguardexecSlow;
}

void AActor::execSetBase( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execSetFloor);

	P_GET_OBJECT(AActor,NewBase);
	P_FINISH;

	SetBase( NewBase );

	unguardSlow;
}

///////////
// Audio //
///////////
void AActor::CheckHearSound(APawn* Hearer, INT Id, USound* Sound, FVector Parameters, FLOAT RadiusSquared)
{
	guardSlow(AActor::CheckHearSound);

	FVector HearSource;
	if ( Hearer->IsA(APlayerPawn::StaticClass()) && ((APlayerPawn *)Hearer)->ViewTarget )
		HearSource = ((APlayerPawn *)Hearer)->ViewTarget->Location;
	else
		HearSource = Hearer->Location;

	FLOAT NewRadiusSquared = RadiusSquared/1.3f;
	FLOAT DistSq = (HearSource-Location).SizeSquared();
	if( DistSq < NewRadiusSquared )
	{
		if ( !GetLevel()->Model->FastLineCheck(HearSource,Location) )
		{
			// if no line of sight, reduce radius and volume
			if ( Instigator != Hearer )
				NewRadiusSquared *= 0.6f;

			Parameters.X *= 0.35f;
			if ( DistSq > NewRadiusSquared )
				return;
		}
		Hearer->eventClientHearSound( this, Id, Sound, Location, Parameters );
	}

	unguardexecSlow;
}

void AActor::execDemoPlaySound( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execDemoPlaySound);

	// Get parameters.
	P_GET_OBJECT(USound,Sound);
	P_GET_BYTE_OPTX(Slot,SLOT_Misc);
	P_GET_FLOAT_OPTX(Volume,TransientSoundVolume);
	P_GET_UBOOL_OPTX(bNoOverride, 0);
	P_GET_FLOAT_OPTX(Radius,TransientSoundRadius);
	P_GET_FLOAT_OPTX(Pitch,1.0);
	P_FINISH;

	if( !Sound )
		return;

	// Play the sound locally
	INT Id = GetIndex()*16 + Slot*2 + bNoOverride;
	FLOAT RadiusSquared = Square( Radius ? Radius : 1600.f );
	FVector Parameters = FVector(100 * Volume, Radius, 100 * Pitch);

	UClient* Client = GetLevel()->Engine->Client;
	if( Client )
	{
		for( INT i=0; i<Client->Viewports.Num(); i++ )
		{
			APlayerPawn* Hearer = Client->Viewports(i)->Actor;
			if( Hearer && Hearer->GetLevel()==GetLevel() )
				CheckHearSound(Hearer, Id, Sound, Parameters,RadiusSquared);
		}
	}
	unguardexecSlow;
}

#pragma DISABLE_OPTIMIZATION
void AActor::execPlaySound( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execPlaySound);

	// Get parameters.
	P_GET_OBJECT(USound,Sound);
	P_GET_BYTE_OPTX(Slot,SLOT_Misc);
	P_GET_FLOAT_OPTX(Volume,TransientSoundVolume);
	P_GET_UBOOL_OPTX(bNoOverride, 0);
	P_GET_FLOAT_OPTX(Radius,TransientSoundRadius);
	P_GET_FLOAT_OPTX(Pitch,1.0);
	P_FINISH;

	if( !Sound )
		return;

	UFunction* Caller = Cast<UFunction>( Stack.Node );

#if defined(__ANDROID__) || defined(PLATFORM_ANDROID)
	if( UT99AndroidMaybeFilterCityIntroVoice( this, Caller, Sound, Slot, Volume ) )
		return;
	if( UT99AndroidV167TSuppressASGuardiaSlidingDoorScriptSound( this, Caller, Sound ) )
		return;
#endif

	// Server-side demo needs a call to execDemoPlaySound for the DemoRecSpectator
	if(		GetLevel() && GetLevel()->DemoRecDriver
		&&	!GetLevel()->DemoRecDriver->ServerConnection
		&&	GetLevel()->GetLevelInfo()->NetMode != NM_Client )
		eventDemoPlaySound(Sound, Slot, Volume, bNoOverride, Radius, Pitch);

	INT Id = GetIndex()*16 + Slot*2 + bNoOverride;
	FLOAT RadiusSquared = Square( Radius ? Radius : 1600.f );
	FVector Parameters = FVector(100 * Volume, Radius, 100 * Pitch);

	// See if the function is simulated.
	if( (GetLevel()->GetLevelInfo()->NetMode == NM_Client) || (Caller && (Caller->FunctionFlags & FUNC_Simulated)) )
	{
		// Called from a simulated function, so propagate locally only.
		UClient* Client = GetLevel()->Engine->Client;
		if( Client )
		{
			for( INT i=0; i<Client->Viewports.Num(); i++ )
			{
				APlayerPawn* Hearer = Client->Viewports(i)->Actor;
				if( Hearer && Hearer->GetLevel()==GetLevel() )
					CheckHearSound(Hearer, Id, Sound, Parameters,RadiusSquared);
			}
		}
	}
	else
	{
		// Propagate to all player actors.
		for( APawn* Hearer=Level->PawnList; Hearer; Hearer=Hearer->nextPawn )
		{
			if( Hearer->bIsPlayer )
				CheckHearSound(Hearer, Id, Sound, Parameters,RadiusSquared);
		}
	}
	unguardexecSlow;
}
#pragma ENABLE_OPTIMIZATION

void AActor::execPlayOwnedSound( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execPlayOwnedSound);

	// Get parameters.
	P_GET_OBJECT(USound,Sound);
	P_GET_BYTE_OPTX(Slot,SLOT_Misc);
	P_GET_FLOAT_OPTX(Volume,TransientSoundVolume);
	P_GET_UBOOL_OPTX(bNoOverride, 0);
	P_GET_FLOAT_OPTX(Radius,TransientSoundRadius);
	P_GET_FLOAT_OPTX(Pitch,1.0);
	P_FINISH;

	if( !Sound )
		return;
	// if we're recording a demo, make a call to execDemoPlaySound()
	if( (GetLevel() && GetLevel()->DemoRecDriver && !GetLevel()->DemoRecDriver->ServerConnection) )
		eventDemoPlaySound(Sound, Slot, Volume, bNoOverride, Radius, Pitch);

	INT Id = GetIndex()*16 + Slot*2 + bNoOverride;
	FLOAT RadiusSquared = Square( Radius ? Radius : 1600.f );
	FVector Parameters = FVector(100 * Volume, Radius, 100 * Pitch);

	if( GetLevel()->GetLevelInfo()->NetMode == NM_Client )
	{
		UClient* Client = GetLevel()->Engine->Client;
		if( Client )
		{
			for( INT i=0; i<Client->Viewports.Num(); i++ )
			{
				APlayerPawn* Hearer = Client->Viewports(i)->Actor;
				if( Hearer && Hearer->GetLevel()==GetLevel() )
					CheckHearSound(Hearer, Id, Sound, Parameters,RadiusSquared);
			}
		}
	}
	else
	{
		AActor *RemoteOwner = NULL;
		if( GetLevel()->GetLevelInfo()->NetMode != NM_Standalone )
		{
			if ( IsA(APlayerPawn::StaticClass()) )
			{
				if ( ((APlayerPawn *)this)->Player
					&& !((APlayerPawn*)this)->Player->IsA(UViewport::StaticClass()) )
					RemoteOwner = this;
			}
			else if ( Owner && Owner->IsA(APlayerPawn::StaticClass()) && ((APlayerPawn *)Owner)->Player
					&& !((APlayerPawn*)Owner)->Player->IsA(UViewport::StaticClass()) )
				RemoteOwner = Owner;
		}

		for( APawn* Hearer=Level->PawnList; Hearer; Hearer=Hearer->nextPawn )
		{
			if( Hearer->bIsPlayer && (Hearer != RemoteOwner) )
				CheckHearSound(Hearer, Id, Sound, Parameters,RadiusSquared);
		}
	}
	unguardexecSlow;
}

void AActor::execGetSoundDuration( FFrame& Stack, RESULT_DECL )
{
	guard(AActor::execGetSoundDuration);

	// Get parameters.
	P_GET_OBJECT(USound,Sound);
	P_FINISH;

	*(FLOAT*)Result = Sound->GetDuration();

	unguardexec;
}

//////////////
// Movement //
//////////////

void AActor::execMove( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execMove);

	P_GET_VECTOR(Delta);
	P_FINISH;

	FCheckResult Hit(1.0);
	*(DWORD*)Result = GetLevel()->MoveActor( this, Delta, Rotation, Hit );

	unguardexecSlow;
}

void AActor::execSetLocation( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execSetLocation);

	P_GET_VECTOR(NewLocation);
	P_FINISH;

	*(DWORD*)Result = GetLevel()->FarMoveActor( this, NewLocation );

	unguardexecSlow;
}

void AActor::execSetRotation( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execSetRotation);

	P_GET_ROTATOR(NewRotation);
	P_FINISH;

	FCheckResult Hit(1.0);
	*(DWORD*)Result = GetLevel()->MoveActor( this, FVector(0,0,0), NewRotation, Hit );

	unguardexecSlow;
}

///////////////
// Relations //
///////////////

void AActor::execSetOwner( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execSetOwner);

	P_GET_ACTOR(NewOwner);
	P_FINISH;

	SetOwner( NewOwner );

	unguardexecSlow;
}

//////////////////
// Line tracing //
//////////////////

void AActor::execTrace( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execTrace);

	P_GET_VECTOR_REF(HitLocation);
	P_GET_VECTOR_REF(HitNormal);
	P_GET_VECTOR(TraceEnd);
	P_GET_VECTOR_OPTX(TraceStart,Location);
	P_GET_UBOOL_OPTX(bTraceActors,bCollideActors);
	P_GET_VECTOR_OPTX(TraceExtent,FVector(0,0,0));
	P_FINISH;

	// Trace the line.
	FCheckResult Hit(1.0);
	DWORD TraceFlags;
	if( bTraceActors )
		TraceFlags = TRACE_AllColliding | TRACE_ProjTargets;
	else
		TraceFlags = TRACE_VisBlocking;

	GetLevel()->SingleLineCheck( Hit, this, TraceEnd, TraceStart, TraceFlags, TraceExtent );
	/*if( Hit.Actor && Hit.Item!=INDEX_NONE )
	{
		UModel*  Model = Hit.Actor->IsA(ULevelInfo::StaticClass) ? XLevel->Model : Actor->Model;
		FBspNode& Node = Model->Nodes( Hit.Item );
		FBspSurf& Surf = Model->Surfs( Node.iSurf );
		UTexture* HitTexture = Surf->Texture;
		//do something with HitTexture
	}*/
	*(AActor**)Result = Hit.Actor;
	*HitLocation      = Hit.Location;
	*HitNormal        = Hit.Normal;

	unguardexecSlow;
}

void AActor::execFastTrace( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execTrace);

	P_GET_VECTOR(TraceEnd);
	P_GET_VECTOR_OPTX(TraceStart,Location);
	P_FINISH;

	// Trace the line.
	*(DWORD*)Result = GetLevel()->Model->FastLineCheck(TraceEnd, TraceStart);

	unguardexecSlow;
}

///////////////////////
// Spawn and Destroy //
///////////////////////

void AActor::execSpawn( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execSpawn);

	P_GET_OBJECT(UClass,SpawnClass);
	P_GET_OBJECT_OPTX(AActor,SpawnOwner,NULL); 
	P_GET_NAME_OPTX(SpawnName,NAME_None);
	P_GET_VECTOR_OPTX(SpawnLocation,Location);
	P_GET_ROTATOR_OPTX(SpawnRotation,Rotation);
	P_FINISH;

	// Spawn and return actor.
	AActor* Spawned = SpawnClass ? GetLevel()->SpawnActor
	(
		SpawnClass,
		NAME_None,
		SpawnOwner,
		Instigator,
		SpawnLocation,
		SpawnRotation
	) : NULL;
	if( Spawned )
		Spawned->Tag = SpawnName;
	*(AActor**)Result = Spawned;

	unguardexecSlow;
}

void AActor::execDestroy( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execDestroy);

	P_FINISH;
	
	*(DWORD*)Result = GetLevel()->DestroyActor( this );

	unguardexecSlow;
}

////////////
// Timing //
////////////

void AActor::execSetTimer( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execSetTimer);

	P_GET_FLOAT(NewTimerRate);
	P_GET_UBOOL(bLoop);
	P_FINISH;

	TimerCounter = 0.0;
	TimerRate    = NewTimerRate;
	bTimerLoop   = bLoop;

	unguardexecSlow;
}

////////////////
// Warp zones //
////////////////

void AWarpZoneInfo::execWarp( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AWarpZoneInfo::execWarp);

	P_GET_VECTOR_REF(WarpLocation);
	P_GET_VECTOR_REF(WarpVelocity);
	P_GET_ROTATOR_REF(WarpRotation);
	P_FINISH;

	// Perform warping.
	*WarpLocation = (*WarpLocation).TransformPointBy ( WarpCoords.Transpose() );
	*WarpVelocity = (*WarpVelocity).TransformVectorBy( WarpCoords.Transpose() );
	*WarpRotation = (GMath.UnitCoords / *WarpRotation * WarpCoords.Transpose()).OrthoRotation();

	unguardexecSlow;
}

void AWarpZoneInfo::execUnWarp( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AWarpZoneInfo::execUnWarp);

	P_GET_VECTOR_REF(WarpLocation);
	P_GET_VECTOR_REF(WarpVelocity);
	P_GET_ROTATOR_REF(WarpRotation);
	P_FINISH;

	// Perform unwarping.
	*WarpLocation = (*WarpLocation).TransformPointBy ( WarpCoords );
	*WarpVelocity = (*WarpVelocity).TransformVectorBy( WarpCoords );
	*WarpRotation = (GMath.UnitCoords / *WarpRotation * WarpCoords).OrthoRotation();

	unguardexecSlow;
}

/*-----------------------------------------------------------------------------
	Native iterator functions.
-----------------------------------------------------------------------------*/

void AActor::execAllActors( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execAllActors);

	// Get the parms.
	P_GET_OBJECT(UClass,BaseClass);
	P_GET_ACTOR_REF(OutActor);
	P_GET_NAME_OPTX(TagName,NAME_None);
	P_FINISH;

	BaseClass = BaseClass ? BaseClass : AActor::StaticClass();
	INT iActor=0;

	PRE_ITERATOR;
		// Fetch next actor in the iteration.
		*OutActor = NULL;
		while( iActor<GetLevel()->Actors.Num() && *OutActor==NULL )
		{
			AActor* TestActor = GetLevel()->Actors(iActor++);
			if(	TestActor && TestActor->IsA(BaseClass) && (TagName==NAME_None || TestActor->Tag==TagName) )
				*OutActor = TestActor;
		}
		if( *OutActor == NULL )
		{
			Stack.Code = &Stack.Node->Script(wEndOffset + 1);
			break;
		}
	POST_ITERATOR;

	unguardexecSlow;
}

void AActor::execChildActors( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execChildActors);

	P_GET_OBJECT(UClass,BaseClass);
	P_GET_ACTOR_REF(OutActor);
	P_FINISH;

	BaseClass = BaseClass ? BaseClass : AActor::StaticClass();
	INT iActor=0;

	PRE_ITERATOR;
		// Fetch next actor in the iteration.
		*OutActor = NULL;
		while( iActor<GetLevel()->Actors.Num() && *OutActor==NULL )
		{
			AActor* TestActor = GetLevel()->Actors(iActor++);
			if(	TestActor && TestActor->IsA(BaseClass) && TestActor->IsOwnedBy( this ) )
				*OutActor = TestActor;
		}
		if( *OutActor == NULL )
		{
			Stack.Code = &Stack.Node->Script(wEndOffset + 1);
			break;
		}
	POST_ITERATOR;

	unguardexecSlow;
}

void AActor::execBasedActors( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execBasedActors);

	P_GET_OBJECT(UClass,BaseClass);
	P_GET_ACTOR_REF(OutActor);
	P_FINISH;

	BaseClass = BaseClass ? BaseClass : AActor::StaticClass();
	INT iActor=0;

	PRE_ITERATOR;
		// Fetch next actor in the iteration.
		*OutActor = NULL;
		while( iActor<GetLevel()->Actors.Num() && *OutActor==NULL )
		{
			AActor* TestActor = GetLevel()->Actors(iActor++);
			if(	TestActor && TestActor->IsA(BaseClass) && TestActor->Base==this )
				*OutActor = TestActor;
		}
		if( *OutActor == NULL )
		{
			Stack.Code = &Stack.Node->Script(wEndOffset + 1);
			break;
		}
	POST_ITERATOR;

	unguardexecSlow;
}

void AActor::execTouchingActors( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execTouchingActors);

	P_GET_OBJECT(UClass,BaseClass);
	P_GET_ACTOR_REF(OutActor);
	P_FINISH;

	BaseClass = BaseClass ? BaseClass : AActor::StaticClass();
	INT iTouching=0;

	PRE_ITERATOR;
		// Fetch next actor in the iteration.
		*OutActor = NULL;
		for( iTouching; iTouching<ARRAY_COUNT(Touching) && *OutActor==NULL; iTouching++ )
		{
			AActor* TestActor = Touching[iTouching];
			if(	TestActor && TestActor->IsA(BaseClass) )
				*OutActor = TestActor;
		}
		if( *OutActor == NULL )
		{
			Stack.Code = &Stack.Node->Script(wEndOffset + 1);
			break;
		}
	POST_ITERATOR;

	unguardexecSlow;
}

void AActor::execTraceActors( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execTraceActors);

	P_GET_OBJECT(UClass,BaseClass);
	P_GET_ACTOR_REF(OutActor);
	P_GET_VECTOR_REF(HitLocation);
	P_GET_VECTOR_REF(HitNormal);
	P_GET_VECTOR(End);
	P_GET_VECTOR_OPTX(Start,Location);
	P_GET_VECTOR_OPTX(TraceExtent,FVector(0,0,0));
	P_FINISH;

	FMemMark Mark(GMem);
	BaseClass         = BaseClass ? BaseClass : AActor::StaticClass();
	FCheckResult* Hit = GetLevel()->MultiLineCheck( GMem, End, Start, TraceExtent, 1, Level, 0 );

	PRE_ITERATOR;
		if( Hit )
		{
			*OutActor    = Hit->Actor;
			*HitLocation = Hit->Location;
			*HitNormal   = Hit->Normal;
			Hit          = Hit->GetNext();
		}
		else
		{
			Stack.Code = &Stack.Node->Script(wEndOffset + 1);
			*OutActor = NULL;
			break;
		}
	POST_ITERATOR;
	Mark.Pop();

	unguardexecSlow;
}

void AActor::execRadiusActors( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execRadiusActors);

	P_GET_OBJECT(UClass,BaseClass);
	P_GET_ACTOR_REF(OutActor);
	P_GET_FLOAT(Radius);
	P_GET_VECTOR_OPTX(TraceLocation,Location);
	P_FINISH;

	BaseClass = BaseClass ? BaseClass : AActor::StaticClass();
	INT iActor=0;

	PRE_ITERATOR;
		// Fetch next actor in the iteration.
		*OutActor = NULL;
		while( iActor<GetLevel()->Actors.Num() && *OutActor==NULL )
		{
			AActor* TestActor = GetLevel()->Actors(iActor++);
			if
			(	TestActor
			&&	TestActor->IsA(BaseClass) 
			&&	(TestActor->Location - TraceLocation).SizeSquared() < Square(Radius + TestActor->CollisionRadius) )
				*OutActor = TestActor;
		}
		if( *OutActor == NULL )
		{
			Stack.Code = &Stack.Node->Script(wEndOffset + 1);
			break;
		}
	POST_ITERATOR;

	unguardexecSlow;
}

void AActor::execVisibleActors( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execVisibleActors);

	P_GET_OBJECT(UClass,BaseClass);
	P_GET_ACTOR_REF(OutActor);
	P_GET_FLOAT_OPTX(Radius,0.0);
	P_GET_VECTOR_OPTX(TraceLocation,Location);
	P_FINISH;

	BaseClass = BaseClass ? BaseClass : AActor::StaticClass();
	INT iActor=0;

	PRE_ITERATOR;
		// Fetch next actor in the iteration.
		*OutActor = NULL;
		while( iActor<GetLevel()->Actors.Num() && *OutActor==NULL )
		{
			AActor* TestActor = GetLevel()->Actors(iActor++);
			if
			(	TestActor
			&& !TestActor->bHidden
			&&	TestActor->IsA(BaseClass)
			&&	(Radius==0.0 || (TestActor->Location-TraceLocation).SizeSquared() < Square(Radius))
			&&	GetLevel()->Model->FastLineCheck(TestActor->Location, TraceLocation) )
				*OutActor = TestActor;
		}
		if( *OutActor == NULL )
		{
			Stack.Code = &Stack.Node->Script(wEndOffset + 1);
			break;
		}
	POST_ITERATOR;

	unguardexecSlow;
}

void AActor::execVisibleCollidingActors( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execVisibleCollidingActors);

	P_GET_OBJECT(UClass,BaseClass);
	P_GET_ACTOR_REF(OutActor);
	P_GET_FLOAT_OPTX(Radius,0.0);
	P_GET_VECTOR_OPTX(TraceLocation,Location);
	P_GET_UBOOL_OPTX(bIgnoreHidden, 0); 
	P_FINISH;

	Radius = Radius ? Radius : 1000;
	BaseClass = BaseClass ? BaseClass : AActor::StaticClass();
	FMemMark Mark(GMem);
	FCheckResult* Link=GetLevel()->Hash->ActorRadiusCheck( GMem, TraceLocation, Radius, 0 );
	
	PRE_ITERATOR;
		// Fetch next actor in the iteration.
		*OutActor = NULL;
		if ( Link )
		{
			while
			(	Link
			&&	(!Link->Actor
			||	!Link->Actor->IsA(BaseClass) 
			||  (bIgnoreHidden && Link->Actor->bHidden)
			||	!GetLevel()->Model->FastLineCheck(Link->Actor->Location, TraceLocation)) )
				Link=Link->GetNext();

			if ( Link )
			{
				*OutActor = Link->Actor;
				Link=Link->GetNext();
			}
		}
		if ( *OutActor == NULL ) 
		{
			Stack.Code = &Stack.Node->Script(wEndOffset + 1);
			break;
		}
	POST_ITERATOR;

	Mark.Pop();
	unguardexecSlow;
}

void AZoneInfo::execZoneActors( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AZoneInfo::execZoneActors);

	P_GET_OBJECT(UClass,BaseClass);
	P_GET_ACTOR_REF(OutActor);
	P_FINISH;

	BaseClass = BaseClass ? BaseClass : AActor::StaticClass();
	INT iActor=0;

	PRE_ITERATOR;
		// Fetch next actor in the iteration.
		*OutActor = NULL;
		while( iActor<GetLevel()->Actors.Num() && *OutActor==NULL )
		{
			AActor* TestActor = GetLevel()->Actors(iActor++);
			if
			(	TestActor
			&&	TestActor->IsA(BaseClass)
			&&	TestActor->IsInZone(this) )
				*OutActor = TestActor;
		}
		if( *OutActor == NULL )
		{
			Stack.Code = &Stack.Node->Script(wEndOffset + 1);
			break;
		}
	POST_ITERATOR;

	unguardexecSlow;
}

/*-----------------------------------------------------------------------------
	Script processing function.
-----------------------------------------------------------------------------*/

//
// Execute the state code of the actor.
//
void AActor::ProcessState( FLOAT DeltaSeconds )
{
	if
	(	GetStateFrame()
	&&	GetStateFrame()->Code
	&&	(Role>=ROLE_Authority || (GetStateFrame()->StateNode->StateFlags & STATE_Simulated))
	&&	!IsPendingKill() )
	{
		UState* OldStateNode = GetStateFrame()->StateNode;
		guard(AActor::ProcessState);
		if( ++GScriptEntryTag==1 )
			clock(GScriptCycles);

		// If a latent action is in progress, update it.
		if( GetStateFrame()->LatentAction )
			(this->*GNatives[GetStateFrame()->LatentAction])( *GetStateFrame(), (BYTE*)&DeltaSeconds );

		// Execute code.
		INT NumStates=0;
		while( !bDeleteMe && GetStateFrame()->Code && !GetStateFrame()->LatentAction )
		{
			BYTE Buffer[MAX_CONST_SIZE];
			GetStateFrame()->Step( this, Buffer );
			if( GetStateFrame()->StateNode!=OldStateNode )
			{
				OldStateNode = GetStateFrame()->StateNode;
				if( ++NumStates > 4 )
				{
					//GetStateFrame().Logf( "Pause going from %s to %s", xx, yy );
					break;
				}
			}
		}
		if( --GScriptEntryTag==0 )
			unclock(GScriptCycles);
		unguardf(( TEXT("Object %s, Old State %s, New State %s"), GetFullName(), OldStateNode->GetFullName(), GetStateFrame()->StateNode->GetFullName() ));
	}
}

//
// Internal RPC calling.
//
static inline void InternalProcessRemoteFunction
(
	AActor*			Actor,
	UNetConnection*	Connection,
	UFunction*		Function,
	void*			Parms,
	FFrame*			Stack,
	UBOOL			IsServer
)
{
	guardSlow(InternalProcessRemoteFunction);
	Actor->GetLevel()->NumRPC++;

	// Make sure this function exists for both parties.
	FClassNetCache* ClassCache = Connection->PackageMap->GetClassNetCache( Actor->GetClass() );
	if( !ClassCache )
		return;
	FFieldNetCache* FieldCache = ClassCache->GetFromField( Function );
	if( !FieldCache )
		return;

	// Get the actor channel.
	UActorChannel* Ch = Connection->ActorChannels.FindRef(Actor);
	if( !Ch )
	{
		if( IsServer )
			Ch = (UActorChannel *)Connection->CreateChannel( CHTYPE_Actor, 1 );
		if( !Ch )
			return;
		if( IsServer )
			Ch->SetChannelActor( Actor );
	}

	// Make sure initial channel-opening replication has taken place.
	if( Ch->OpenPacketId==INDEX_NONE )
	{
		if( !IsServer )
			return;
		Ch->ReplicateActor();
	}

	// Form the RPC preamble.
	FOutBunch Bunch( Ch, 0 );
	//debugf(TEXT("   Call %s"),Function->GetFullName());
	Bunch.WriteInt( FieldCache->FieldNetIndex, ClassCache->GetMaxIndex() );

	// Form the RPC parameters.
	if( Stack )
	{
		appMemzero( Parms, Function->ParmsSize );
		for( TFieldIterator<UProperty> It(Function); It && (It->PropertyFlags & (CPF_Parm|CPF_ReturnParm))==CPF_Parm; ++It )
			Stack->Step( Stack->Object, (BYTE*)Parms + It->Offset );
		checkSlow(*Stack->Code==EX_EndFunctionParms);
	}
	for( TFieldIterator<UProperty> It(Function); It && (It->PropertyFlags & (CPF_Parm|CPF_ReturnParm))==CPF_Parm; ++It )
	{
		if( Connection->PackageMap->ObjectToIndex(*It)!=INDEX_NONE )
		{
			UBOOL Send = 1;
			if( !It->IsA(UBoolProperty::StaticClass()) )
			{
				Send = !It->Matches(Parms,NULL,0);
				Bunch.WriteBit( Send );
			}
			if( Send )
				It->NetSerializeItem( Bunch, Connection->PackageMap, (BYTE*)Parms + It->Offset );
		}
	}

	// Reliability.
	//warning: RPC's might overflow, preventing reliable functions from getting thorough.
	if( Function->FunctionFlags & FUNC_NetReliable )
		Bunch.bReliable = 1;

	// Send the bunch.
	if( !Bunch.IsError() )
		Ch->SendBunch( &Bunch, 1 );
	else
		debugf( NAME_DevNet, TEXT("RPC bunch overflowed") );

	unguardSlow;
}

//
// Return whether a function should be executed remotely.
//
UBOOL AActor::ProcessRemoteFunction( UFunction* Function, void* Parms, FFrame* Stack )
{
	guard(AActor::ProcessRemoteFunction);

	// Quick reject.
	if( (Function->FunctionFlags & FUNC_Static) || bDeleteMe )
		return 0;
	UBOOL Absorb = Role<=ROLE_SimulatedProxy && !(Function->FunctionFlags & FUNC_Simulated);
	if( GetLevel()->DemoRecDriver )
	{
		if( GetLevel()->DemoRecDriver->ServerConnection )
			return Absorb;
		ProcessDemoRecFunction( Function, Parms, Stack );
	}
	if( Level->NetMode==NM_Standalone )
		return 0;
	if( !(Function->FunctionFlags & FUNC_Net) )
		return Absorb;

	// Check if the actor can potentially call remote functions.
	APlayerPawn*    Top              = Cast<APlayerPawn>(GetTopOwner());
	UNetConnection* ClientConnection = NULL;
	if
	(	(Role==ROLE_Authority)
	&&	(Top==NULL || (ClientConnection=Cast<UNetConnection>(Top->Player))==NULL) )
		return Absorb;

	// See if UnrealScript replication condition is met.
	while( Function->GetSuperFunction() )
		Function = Function->GetSuperFunction();
	UBOOL Val=0;
	FFrame( this, Function->GetOwnerClass(), Function->RepOffset, NULL ).Step( this, &Val );
	if( !Val )
		return Absorb;

	// Get the connection.
	UBOOL           IsServer   = Level->NetMode==NM_DedicatedServer || Level->NetMode==NM_ListenServer;
	UNetConnection* Connection = IsServer ? ClientConnection : GetLevel()->NetDriver->ServerConnection;
	check(Connection);

	// If saturated and function is unimportant, skip it.
	if( !(Function->FunctionFlags & FUNC_NetReliable) && !Connection->IsNetReady(0) )
		return 1;

	// Send function data to remote.
	InternalProcessRemoteFunction( this, Connection, Function, Parms, Stack, IsServer );
	return 1;

	unguardf(( TEXT("(%s)"), Function->GetFullName() ));
}

// Replicate a function call to a demo recording file
void AActor::ProcessDemoRecFunction( UFunction* Function, void* Parms, FFrame* Stack )
{
	guard(AActor::ProcessDemoRecFunction);

	// Check if the function is replicatable
	if( (Function->FunctionFlags & (FUNC_Static|FUNC_Net))!=FUNC_Net || bNetTemporary )
		return;

	UBOOL IsNetClient = (GetLevel()->GetLevelInfo()->NetMode == NM_Client);

	// Check if actor was spawned locally in a client-side demo 
	if(IsNetClient && Role == ROLE_Authority)
		return;

	// See if UnrealScript replication condition is met.
	while( Function->GetSuperFunction() )
		Function = Function->GetSuperFunction();

	UBOOL Val=0;
	if(IsNetClient)
		Exchange(RemoteRole, Role);
	bDemoRecording = 1;
	bClientDemoRecording = IsNetClient;
	FFrame( this, Function->GetOwnerClass(), Function->RepOffset, NULL ).Step( this, &Val );
	bDemoRecording = 0;
	bClientDemoRecording = 0;
	if(IsNetClient)
		Exchange(RemoteRole, Role);
	bClientDemoNetFunc = 0;
	if( !Val )
		return;

	// Get the channel.
	UNetConnection* Connection = GetLevel()->DemoRecDriver->ClientConnections(0);
	check(Connection);

	// Send function data to remote.
	BYTE* SavedCode = Stack ? Stack->Code : NULL;
	InternalProcessRemoteFunction( this, Connection, Function, Parms, Stack, 1 );
	if( Stack )
		Stack->Code = SavedCode;

	unguardf(( TEXT("(%s/%s)"), GetName(), Function->GetFullName() ));
}

/*-----------------------------------------------------------------------------
	GameInfo
-----------------------------------------------------------------------------*/

//
// Network
//
void AGameInfo::execGetNetworkNumber( FFrame& Stack, RESULT_DECL )
{
	guard(AGameInfo::execNetworkNumber);
	P_FINISH;

	*(FString*)Result = XLevel->NetDriver ? XLevel->NetDriver->LowLevelGetNetworkNumber() : FString(TEXT(""));

	unguardexec;
}

//
// Deathmessage parsing.
//
void AGameInfo::execParseKillMessage( FFrame& Stack, RESULT_DECL )
{
	guard(AGameInfo::execParseKillMessage);
	P_GET_STR(KillerName);
	P_GET_STR(VictimName);
	P_GET_STR(WeaponName);
	P_GET_STR(KillMessage);
	P_FINISH;

	FString Message, Temp;
	INT Offset;

	Temp = KillMessage;

	Offset = Temp.InStr(TEXT("%k"));
	if (Offset != -1)
	{
		Message = Temp.Left(Offset);
		Message += KillerName;
		Message += Temp.Right(Temp.Len() - Offset - 2);
	}
	Temp = Message;

	Offset = Temp.InStr(TEXT("%o"));
	if (Offset != -1)
	{
		Message = Temp.Left(Offset);
		Message += VictimName;
		Message += Temp.Right(Temp.Len() - Offset - 2);
	}
	Temp = Message;

	Offset = Temp.InStr(TEXT("%w"));
	if (Offset != -1)
	{
		Message = Temp.Left(Offset);
		Message += WeaponName;
		Message += Temp.Right(Temp.Len() - Offset - 2);
	}

	*(FString*)Result = Message;

	unguardexec;
}

/*-----------------------------------------------------------------------------
	ADecal Implementation
-----------------------------------------------------------------------------*/

// Find the coplanar surface corresponding to this intersection point.
static INT FindCoplanarSurface( UModel* Model, INT iNode, FVector IntersectionPoint, INT Depth )
{
	guard(FindCoplanarSurface);
	if( iNode == INDEX_NONE )
		return INDEX_NONE;

	FBspNode* Node = &Model->Nodes( iNode );
	if( Node->NumVertices > 0)
	{
		// check if this intersection point lies inside this node.
		FVert* Verts = &Model->Verts( Node->iVertPool );
		FVector &SurfNormal = Model->Vectors( Model->Surfs( Node->iSurf).vNormal );

		FVector* PrevVertex = &Model->Points( Verts[Node->NumVertices - 1].pVertex );
		UBOOL Success = 1;
		FLOAT PrevDot = 0;
		for( INT i=0;i<Node->NumVertices;i++ )
		{
			FVector* Vertex = &Model->Points(Verts[i].pVertex);
			FVector ClipNorm = SurfNormal ^ (*Vertex - *PrevVertex);
			FPlane ClipPlane( *Vertex, ClipNorm );

			FLOAT Dot = ClipPlane.PlaneDot( IntersectionPoint );
			
			if( (Dot < 0 && PrevDot > 0) ||
				(Dot > 0 && PrevDot < 0) )
			{
				Success = 0;
				break;
			}
			PrevDot = Dot;
			PrevVertex = Vertex;
		}
		if( Success )
			return Node->iSurf;
	}

	// check next co-planars to see if it contains this intersection point.
	return FindCoplanarSurface( Model, Node->iPlane, IntersectionPoint, Depth + 1 );
	unguard;
}

static void CalcClippedNodes( UModel* Model, FBspSurf& Surf, FVector* DecalVerts, TArray<INT>& NodeArray )
{
	guard(CalcClippedNodes);

	for( INT n=0;n<Surf.Nodes.Num(); n++)
	{
		FBspNode* Node = &Model->Nodes( Surf.Nodes(n) );
		
		if( Node->NumVertices > 0)
		{
			static FVector	Pts[FBspNode::MAX_FINAL_VERTICES];
			static FLOAT	Dots[FBspNode::MAX_FINAL_VERTICES];
			int NumPts;

			for( INT i=0;i<4;i++ )
				Pts[i] = DecalVerts[i];
			NumPts = 4;
			INT i;

			// check if this node contains any of the decal
			FVert* Verts = &Model->Verts( Node->iVertPool );
			FVector &SurfNormal = Model->Vectors( Surf.vNormal );

			FVector* PrevVertex = &Model->Points( Verts[Node->NumVertices - 1].pVertex );
			UBOOL Success = 1;
			for( i=0;i<Node->NumVertices;i++ )
			{
				FVector* Vertex = &Model->Points(Verts[i].pVertex);
				FVector ClipNorm = SurfNormal ^ (*Vertex - *PrevVertex);
				FPlane ClipPlane( *Vertex, ClipNorm );

				INT j;
				for(j=0;j<NumPts;j++)
					Dots[j] = ClipPlane.PlaneDot( Pts[j] );
				for(j=0;j<NumPts;j++)
				{
					if(		(Dots[j] > 0 && Dots[(j+1)%NumPts] < 0) 
						||	(Dots[j] < 0 && Dots[(j+1)%NumPts] > 0))
					{
						guard(InsertClippingPoint);
						FVector NewPoint = FLinePlaneIntersection( Pts[j], Pts[(j+1)%NumPts], ClipPlane );
						if(j < NumPts-1)
						{	
							// move Dots[] and Pts[] arrays along
							appMemmove( &Dots[j+2], &Dots[j+1], sizeof(FLOAT) * (NumPts - j - 1));
							appMemmove( &Pts[j+2], &Pts[j+1], sizeof(FVector) * (NumPts - j - 1));
						}
						Pts[j+1] = NewPoint;
						Dots[j+1] = 0; 
						NumPts++;
						j++;
						check(NumPts < FBspNode::MAX_FINAL_VERTICES);
						unguard;
					}			
				}
				guard(DeleteClippedPoints);
				for(j=0;j<NumPts;j++)
				{
					if( Dots[j] < 0 )
					{
						appMemmove( &Dots[j], &Dots[j+1], sizeof(FLOAT) * (NumPts - j - 1));
						appMemmove( &Pts[j], &Pts[j+1], sizeof(FVector) * (NumPts - j - 1) );
						j--;
						NumPts--;
					}
				}
				unguard;
				if( NumPts == 0 )
				{
					Success = 0;
					break;
				}
				PrevVertex = Vertex;
			}
			if( Success )
				NodeArray.AddItem( Surf.Nodes(n) );
		}
	}
	unguard;
}

void ADecal::execAttachDecal( FFrame& Stack, RESULT_DECL )
{
	guard(ADecal::execAttachDecal);
	P_GET_FLOAT(TraceDistance);
	P_GET_VECTOR_OPTX(DecalDir,FVector(0,0,0));
	P_FINISH;

	*(INT*)Result = 0;
	if( !GetLevel()->Engine->Client || !GetLevel()->Engine->Client->Decals )
		return;

#ifndef NODECALS
	if( Region.Zone->bFogZone )
		return;
	if(!Texture)
	{
		debugf(TEXT("AttachDecal: No Texture"));
		return;
	}
	MultiDecalLevel = Min<INT>(MultiDecalLevel, 4);

	UModel *Model = Level->XLevel->Model;
	FCheckResult Hit(1.0);
	FVector EndVect = -1 * Rotation.Vector(); // assume rotation oriented in direction of hitnormal
	EndVect.Normalize(); // FIXME - Jack, no need to do this - EndVect already normalized
	EndVect *= TraceDistance;

	INT RandDir = 0;
	if ( DecalDir.IsZero() )
	{
		DecalDir = VRand();
		RandDir = 1;
	}

	if( Model->LineCheck( Hit, NULL, Location + EndVect, Location, FVector(0, 0, 0), TRACE_VisBlocking ) != 0 ||
	    Hit.Item == INDEX_NONE )
	{
		return;
	}
	else
	{
		FBspSurf &Surf = Model->Surfs( Model->Nodes(Hit.Item).iSurf );
		FVector &SurfNormal = Model->Vectors(Surf.vNormal);
		FVector &SurfBase = Model->Points(Surf.pBase);
		FVector Intersection = FLinePlaneIntersection( Location,  Location + EndVect, SurfBase, SurfNormal );
		INT SurfIndex = FindCoplanarSurface( Model, Hit.Item, Intersection, 0 );
	
		if( SurfIndex == INDEX_NONE )
			return;
	
		// setup vertices for main decal surface.
		guard(FindMainSurfaceVertices);
		FBspSurf &Surf = Model->Surfs(SurfIndex);
		FVector &SurfNormal = Model->Vectors(Surf.vNormal);
		FVector &SurfBase = Model->Points(Surf.pBase);
		FVector DecalCenter = FLinePlaneIntersection( Location, Location + EndVect, SurfBase, SurfNormal );

		FLOAT d = Rotation.Vector() | SurfNormal;
		if( !d )
		{
			//debugf(TEXT("AttachDecal: decal ray is parallel to surface"));
			return;
		}
		if(Abs(((SurfBase - DecalCenter) | SurfNormal)) > 0.001f )
		{
			//debugf(TEXT("AttachDecal: Couldn't place decal: dot product is %f"), ((SurfBase - DecalCenter) | SurfNormal));
			return;
		}

		if( Surf.PolyFlags & (PF_AutoUPan|PF_AutoVPan) )
			return;

		// attach decal to new surface
		FDecal* MainDecal = NULL;
		for( INT j=0;j<Surf.Decals.Num();j++)
			if(Surf.Decals(j).Actor->Texture == Texture)
			{
				Surf.Decals.InsertZeroed(j);
				MainDecal = &Surf.Decals(j);					
				break;
			}
		if(!MainDecal) 
			MainDecal = &Surf.Decals(Surf.Decals.AddZeroed());		
		MainDecal->Actor = this;
		SurfList.AddItem(SurfIndex);

		guard(SetVertices);
		FLOAT diag = appSqrt( DrawScale * DrawScale * Texture->USize * Texture->USize / 2.f );
		// calculate decal co-ordinates - ASSUME DECALS ARE SQUARE

		if ( !RandDir )
		{
			// Project DecalDir onto the surface
			FVector MainAxis = DecalDir - (DecalDir | SurfNormal) * SurfNormal;

			if ( MainAxis.IsNearlyZero() )
			{
				MainAxis = DecalDir = ( SurfBase - DecalCenter );
				RandDir = 1;
			}
			else
			{
				// then we cross with the normal to get the other axis.
				FVector OtherAxis = MainAxis ^ SurfNormal;
				MainAxis.Normalize();
				OtherAxis.Normalize();

				// calculate the vector from the center to the diagonal.
				MainDecal->Vertices[0] = MainAxis + OtherAxis;
				MainDecal->Vertices[1] = MainAxis - OtherAxis;
			}
		}
		if ( RandDir )
		{
			// calculate the vector from the center to the diagonal.
			MainDecal->Vertices[0] = DecalDir - (DecalDir | SurfNormal) * SurfNormal;
			MainDecal->Vertices[1] = MainDecal->Vertices[0] ^ SurfNormal;
		}

		MainDecal->Vertices[0].Normalize();
		MainDecal->Vertices[1].Normalize();
		MainDecal->Vertices[0] *= diag;
		MainDecal->Vertices[1] *= diag;
		MainDecal->Vertices[2] = -MainDecal->Vertices[0];
		MainDecal->Vertices[3] = -MainDecal->Vertices[1];
		MainDecal->Vertices[0] += DecalCenter;
		MainDecal->Vertices[1] += DecalCenter;
		MainDecal->Vertices[2] += DecalCenter;
		MainDecal->Vertices[3] += DecalCenter;
		CalcClippedNodes( Model, Surf, MainDecal->Vertices, MainDecal->Nodes );
		unguard;

		guard(FindSecondarySurface);
		FLOAT NormSize = SurfNormal.Size();
		FVector TraceVect = -50*(SurfNormal / NormSize);
		FVector XVect = MainDecal->Vertices[1] - MainDecal->Vertices[0];
		FVector YVect = MainDecal->Vertices[3] - MainDecal->Vertices[0];

		for( INT X=0; X < MultiDecalLevel; X++ )
		{
			for( INT Y=0; Y < MultiDecalLevel; Y++ )
			{
				FVector TracePoint = MainDecal->Vertices[0] + (((FLOAT)(X+1.))/MultiDecalLevel)*XVect + (((FLOAT)(Y+1.))/MultiDecalLevel)*YVect;
				if( Model->LineCheck( Hit, NULL, TracePoint + TraceVect, TracePoint - TraceVect, FVector(0, 0, 0), TRACE_VisBlocking ) == 0 && Hit.Item != INDEX_NONE )
				{
					FBspSurf &SecSurf = Model->Surfs( Model->Nodes(Hit.Item).iSurf );
					FVector &SecNormal = Model->Vectors(SecSurf.vNormal);
					FVector &SecBase = Model->Points(SecSurf.pBase);
					FVector SecInt = FLinePlaneIntersection( TracePoint - TraceVect,  TracePoint + TraceVect, SecBase, SecNormal );
					SurfIndex = FindCoplanarSurface( Model, Hit.Item, SecInt, 0 );
				}
				else
					continue;

				if( SurfIndex == INDEX_NONE )
					continue;

				FBspSurf &SecSurf = Model->Surfs(SurfIndex);
				FVector &SecNormal = Model->Vectors(SecSurf.vNormal);
				FVector &SecBase = Model->Points(SecSurf.pBase);

				INT Found;
				if(SurfList.FindItem(SurfIndex, Found))
					continue;

				if( SecSurf.PolyFlags & (PF_AutoUPan|PF_AutoVPan) )
					continue;

				FLOAT costheta = (SurfNormal | SecNormal) / (SurfNormal.Size() * SecNormal.Size());
				if( Abs(costheta) <= 0.7 ) 
					continue;	// angle is too close to 90 degrees	

				// attach decal to seondary surface
				FDecal* SecDecal = NULL;
				INT j;
				for( j=0;j<SecSurf.Decals.Num();j++)
					if(SecSurf.Decals(j).Actor->Texture == Texture)
					{
						SecSurf.Decals.InsertZeroed(j);
						SecDecal = &SecSurf.Decals(j);					
						break;
					}
				if(!SecDecal) 
					SecDecal = &SecSurf.Decals(SecSurf.Decals.AddZeroed());
				SecDecal->Actor = this;
				SurfList.AddItem(SurfIndex);

				for( j=0;j<4;j++)
				{
					// Locate texture-wrapped point on secondary surface, for each vertex
					FVector A = FLinePlaneIntersection( MainDecal->Vertices[j]-SurfNormal, MainDecal->Vertices[j], SecBase, SecNormal );
					FVector B = FLinePlaneIntersection( MainDecal->Vertices[j]-SecNormal, MainDecal->Vertices[j], SecBase, SecNormal );
					FLOAT X = (MainDecal->Vertices[j] - B).Size() / costheta;
					FLOAT H = (MainDecal->Vertices[j] - A).Size() / costheta;
					FVector AB = B - A;
					AB.Normalize();
					SecDecal->Vertices[j] = B - (H-X)*AB;
				}
				CalcClippedNodes( Model, SecSurf, SecDecal->Vertices, SecDecal->Nodes );
				SecDecal->Vertices[0] -= SecBase;
				SecDecal->Vertices[1] -= SecBase;
				SecDecal->Vertices[2] -= SecBase;
				SecDecal->Vertices[3] -= SecBase;
			}
		}
		unguard;

		MainDecal->Vertices[0] -= SurfBase;
		MainDecal->Vertices[1] -= SurfBase;
		MainDecal->Vertices[2] -= SurfBase;
		MainDecal->Vertices[3] -= SurfBase;
		unguard;
	}
	*(INT*)Result = 1;
#endif
	unguard;
}

void ADecal::execDetachDecal( FFrame& Stack, RESULT_DECL )
{
	guard(ADecal::execDetachDecal);
	P_FINISH;

#ifndef NODECALS
	while( SurfList.Num() > 0 )
	{
		// detach decal from old surface
		FBspSurf& Surf = Level->XLevel->Model->Surfs(SurfList(SurfList.Num()-1));
		UBOOL RemovedDecal = 0;
		for( INT i=0; i<Surf.Decals.Num(); i++ )
			if( Surf.Decals(i).Actor == this )
			{
				Surf.Decals.Remove(i);
				RemovedDecal = 1;
				break;
			}

		//!! check(RemovedDecal);  // caused a crash with shadows during GC...
		SurfList.Remove(SurfList.Num()-1);
	}
#endif
	unguard;
}

// Color functions
#define P_GET_COLOR(var)            P_GET_STRUCT(FColor,var)

void AActor::execMultiply_ColorFloat( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execMultiply_ColorFloat);

	P_GET_COLOR(A);
	P_GET_FLOAT(B);
	P_FINISH;

	A.R = (BYTE) (A.R * B);
	A.G = (BYTE) (A.G * B);
	A.B = (BYTE) (A.B * B);
	*(FColor*)Result = A;

	unguardexecSlow;
}	

void AActor::execMultiply_FloatColor( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execMultiply_FloatColor);

	P_GET_FLOAT (A);
	P_GET_COLOR(B);
	P_FINISH;

	B.R = (BYTE) (B.R * A);
	B.G = (BYTE) (B.G * A);
	B.B = (BYTE) (B.B * A);
	*(FColor*)Result = B;

	unguardexecSlow;
}	

void AActor::execAdd_ColorColor( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execAdd_ColorColor);

	P_GET_COLOR(A);
	P_GET_COLOR(B);
	P_FINISH;

	A.R = A.R + B.R;
	A.G = A.G + B.G;
	A.B = A.B + B.B;
	*(FColor*)Result = A;

	unguardexecSlow;
}

void AActor::execSubtract_ColorColor( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execSubtract_ColorColor);

	P_GET_COLOR(A);
	P_GET_COLOR(B);
	P_FINISH;

	A.R = A.R - B.R;
	A.G = A.G - B.G;
	A.B = A.B - B.B;
	*(FColor*)Result = A;

	unguardexecSlow;
}
/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
