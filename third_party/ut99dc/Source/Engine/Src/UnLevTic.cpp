/*=============================================================================
	UnLevTic.cpp: Level timer tick function
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#include "EnginePrivate.h"
#include "UnNet.h"

#if defined(__ANDROID__) || defined(PLATFORM_ANDROID)
#include <android/log.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
static void AndroidNetLogV115( const TCHAR* Text )
{
	if( Text )
	{
		debugf( NAME_Log, TEXT("%s"), Text );
		__android_log_print( ANDROID_LOG_INFO, "UT99LAN", "%s", appToAnsi(Text) );
	}
}


#define UT99_ANDROID_V167T_DOOR_LOGI(...) __android_log_print(ANDROID_LOG_INFO, "UT99Mover", __VA_ARGS__)

struct FUT99AndroidASGuardiaDoorRecord
{
	AMover* Mover;
	FLOAT LastPawnNearTime;
	UBOOL WasPawnNear;
};

static FUT99AndroidASGuardiaDoorRecord GUT99AndroidASGuardiaDoorRecords[4];

static void UT99AndroidV167PPlayDoorSound( AMover* Mover, USound* Sound, const char* Reason )
{
	if( !Mover || !Sound || !Mover->GetLevel() || !Mover->GetLevel()->Engine || !Mover->GetLevel()->Engine->Audio )
		return;

	const FLOAT Volume = Mover->TransientSoundVolume > 0.0f ? Mover->TransientSoundVolume : 1.0f;
	const FLOAT Radius = Mover->TransientSoundRadius > 0.0f ? Mover->TransientSoundRadius : 1600.0f;
	const INT Id = Mover->GetIndex()*16 + SLOT_None*2;
	Mover->GetLevel()->Engine->Audio->PlaySound( Mover, Id, Sound, Mover->Location, Volume, Radius, 1.0f );
	UT99_ANDROID_V167T_DOOR_LOGI(
		"UT99_ANDROID_V167T_ASGUARDIA_SLIDING_DOOR_SOUND name=%s reason=%s sound=%s loc=%.1f,%.1f,%.1f",
		appToAnsi(Mover->GetName()),
		Reason ? Reason : "door",
		appToAnsi(Sound->GetName()),
		Mover->Location.X,
		Mover->Location.Y,
		Mover->Location.Z );
}


static FName UT99AndroidV167OMoverStateName( AMover* Mover )
{
	return (Mover && Mover->GetStateFrame() && Mover->GetStateFrame()->StateNode && Mover->GetStateFrame()->StateNode!=Mover->GetClass())
		? Mover->GetStateFrame()->StateNode->GetFName()
		: NAME_None;
}

static UBOOL UT99AndroidV167OIsASGuardiaLevel( AActor* Actor )
{
	if( !Actor || !Actor->GetLevel() )
		return 0;

	const TCHAR* MapName = *Actor->GetLevel()->URL.Map;
	if( MapName && (appStricmp(MapName,TEXT("AS-Guardia"))==0 || appStricmp(MapName,TEXT("AS-Guardia.unr"))==0) )
		return 1;

	UObject* Outer = Actor->GetLevel()->GetOuter();
	return Outer && appStricmp(Outer->GetName(),TEXT("AS-Guardia"))==0;
}

static UBOOL UT99AndroidV167OIsASGuardiaSlidingDoorMover( AMover* Mover, FName StateName )
{
	if( !Mover || StateName!=FName(TEXT("TriggerControl"), FNAME_Find) )
		return 0;
	if( !UT99AndroidV167OIsASGuardiaLevel(Mover) )
		return 0;

	const TCHAR* Name = Mover->GetName();
	return appStricmp(Name,TEXT("Mover0"))==0
		|| appStricmp(Name,TEXT("Mover1"))==0;
}

static UBOOL UT99AndroidV167OASGuardiaDoorHasNearbyPawn( AMover* Mover )
{
	if( !Mover || !Mover->GetLevel() )
		return 0;

	// UT99_ANDROID_V167O_ASGUARDIA_SLIDING_DOOR_SIMPLE_PAWN_ZONE:
	// Stable authored doorway center between Mover0 and Mover1.  Use a compact
	// cylinder so enemies, bots and the player open the door, but actors outside
	// the doorway do not keep it alive forever.
	FVector DoorCenter;
	DoorCenter.X = 18.0f;
	DoorCenter.Y = -282.0f;
	DoorCenter.Z = 0.0f;

	const FLOAT NearXY = 256.0f;
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

static FUT99AndroidASGuardiaDoorRecord* UT99AndroidV167OFindDoorRecord( AMover* Mover )
{
	FUT99AndroidASGuardiaDoorRecord* Empty = NULL;
	for( INT i=0; i<ARRAY_COUNT(GUT99AndroidASGuardiaDoorRecords); ++i )
	{
		FUT99AndroidASGuardiaDoorRecord& Rec = GUT99AndroidASGuardiaDoorRecords[i];
		if( Rec.Mover==Mover )
			return &Rec;
		if( !Rec.Mover && !Empty )
			Empty = &Rec;
	}
	if( Empty )
	{
		Empty->Mover = Mover;
		Empty->LastPawnNearTime = -100000.0f;
		Empty->WasPawnNear = 0;
	}
	return Empty;
}

static void UT99AndroidV167OReplaceDoorInterpolation( AMover* Mover, BYTE NewKeyNum, FLOAT Seconds )
{
	if( !Mover )
		return;

	NewKeyNum = (BYTE)Clamp( (INT)NewKeyNum, 0, (INT)ARRAY_COUNT(Mover->KeyPos)-1 );

	if( NewKeyNum==Mover->PrevKeyNum && Mover->KeyNum!=Mover->PrevKeyNum )
	{
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
	Mover->PhysRate       = 1.0f / ::Max(Seconds, 0.25f);
	// UT99_ANDROID_V167O_ASGUARDIA_SLIDING_DOOR_CLIENTUPDATE_RESET:
	// This native controller owns the AS-Guardia door move. Keep one pending
	// interpolation update only; old TriggerControl leftovers could leave this at
	// 2+ and made finished moves look/sound stale.
	Mover->ClientUpdate  = 1;

	Mover->SimOldPos      = Mover->OldPos;
	Mover->SimOldRotYaw   = Mover->OldRot.Yaw;
	Mover->SimOldRotPitch = Mover->OldRot.Pitch;
	Mover->SimOldRotRoll  = Mover->OldRot.Roll;
	Mover->SimInterpolate.X = 100.0f * Mover->PhysAlpha;
	Mover->SimInterpolate.Y = 100.0f * ::Max(0.01f, Mover->PhysRate);
	Mover->SimInterpolate.Z = 256.0f * Mover->PrevKeyNum + Mover->KeyNum;
}

static void UT99AndroidV167OTickASGuardiaSlidingDoor( AActor* Actor, FLOAT DeltaSeconds )
{
	if( !Actor || !Actor->IsA(AMover::StaticClass()) )
		return;

	AMover* Mover = (AMover*)Actor;
	FName StateName = UT99AndroidV167OMoverStateName(Mover);
	if( !UT99AndroidV167OIsASGuardiaSlidingDoorMover(Mover, StateName) )
		return;
	if( Mover->Level && Mover->Level->NetMode==NM_Client )
		return;
	if( Mover->bDeleteMe )
		return;

	const FLOAT Now = Mover->GetLevel() ? Mover->GetLevel()->TimeSeconds : 0.0f;
	const FLOAT HoldSecondsAfterEmpty = 1.35f;
	const FLOAT DoorMoveSeconds = 0.34f;
	const UBOOL bPawnNear = UT99AndroidV167OASGuardiaDoorHasNearbyPawn(Mover);
	FUT99AndroidASGuardiaDoorRecord* Rec = UT99AndroidV167OFindDoorRecord(Mover);
	if( !Rec )
		return;

	if( bPawnNear )
	{
		Rec->LastPawnNearTime = Now;
		Rec->WasPawnNear = 1;
	}
	else if( Rec->WasPawnNear && Rec->LastPawnNearTime < -99999.0f )
	{
		Rec->LastPawnNearTime = Now;
	}

	const FVector ClosedPos = Mover->BasePos + Mover->KeyPos[0];
	const FVector OpenPos   = Mover->BasePos + Mover->KeyPos[1];
	const FLOAT DistClosedSq = (Mover->Location - ClosedPos).SizeSquared();
	const FLOAT DistOpenSq   = (Mover->Location - OpenPos).SizeSquared();
	const UBOOL bVisuallyOpen = DistOpenSq < DistClosedSq;
	const UBOOL bTargetingOpen = Mover->KeyNum!=0;
	const UBOOL bTargetingClosed = Mover->KeyNum==0;
	const INT OldNum = Mover->numTriggerEvents;
	const BYTE OldKey = Mover->KeyNum;
	const BYTE OldPrev = Mover->PrevKeyNum;
	const INT OldClient = Mover->ClientUpdate;
	const UBOOL OldInterp = Mover->bInterpolating;

	if( bPawnNear )
	{
		Mover->numTriggerEvents = 1;
		if( bTargetingClosed || (!Mover->bInterpolating && !bVisuallyOpen) )
		{
			Mover->bOpening = 1;
			Mover->bDelaying = 0;
			Mover->AmbientSound = Mover->MoveAmbientSound;
			Mover->ClientUpdate = 0;
			Mover->RealPosition = Mover->Location;
			Mover->RealRotation = Mover->Rotation;
			UT99AndroidV167OReplaceDoorInterpolation(Mover, 1, DoorMoveSeconds);
			UT99AndroidV167PPlayDoorSound(Mover, Mover->OpeningSound, "open-start");
			UT99_ANDROID_V167T_DOOR_LOGI(
				"UT99_ANDROID_V167T_ASGUARDIA_SLIDING_DOOR_SIMPLE_OPEN name=%s state=%s pawnNear=%d hold=%.2f key=%d/%d->%d/%d num=%d->%d client=%d->%d interp=%d->%d loc=%.1f,%.1f,%.1f",
				appToAnsi(Mover->GetName()),
				StateName!=NAME_None ? appToAnsi(*StateName) : "None",
				(INT)bPawnNear,
				(FLOAT)HoldSecondsAfterEmpty,
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
				Mover->Location.X,
				Mover->Location.Y,
				Mover->Location.Z );
		}
		return;
	}

	Mover->numTriggerEvents = 0;
	const FLOAT EmptyFor = (Rec->LastPawnNearTime > -99999.0f) ? (Now - Rec->LastPawnNearTime) : HoldSecondsAfterEmpty;
	if( EmptyFor < HoldSecondsAfterEmpty )
		return;

	if( bTargetingOpen || (!Mover->bInterpolating && bVisuallyOpen) )
	{
		Mover->bOpening = 0;
		Mover->bDelaying = 0;
		Mover->AmbientSound = Mover->MoveAmbientSound;
		Mover->ClientUpdate = 0;
		Mover->RealPosition = Mover->Location;
		Mover->RealRotation = Mover->Rotation;
		UT99AndroidV167OReplaceDoorInterpolation(Mover, 0, DoorMoveSeconds);
		UT99AndroidV167PPlayDoorSound(Mover, Mover->ClosingSound ? Mover->ClosingSound : Mover->ClosedSound, "close-start");
		UT99_ANDROID_V167T_DOOR_LOGI(
			"UT99_ANDROID_V167T_ASGUARDIA_SLIDING_DOOR_SIMPLE_CLOSE name=%s state=%s pawnNear=%d emptyFor=%.2f key=%d/%d->%d/%d num=%d->%d client=%d->%d interp=%d->%d loc=%.1f,%.1f,%.1f",
			appToAnsi(Mover->GetName()),
			StateName!=NAME_None ? appToAnsi(*StateName) : "None",
			(INT)bPawnNear,
			(FLOAT)EmptyFor,
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
			Mover->Location.X,
			Mover->Location.Y,
			Mover->Location.Z );
	}
}

// UT99_ANDROID_V126_NET_SELECTIVE_WORLD_REPL:
// v125 proved that the safe whitelist is stable, but it was too conservative:
// effect budget stayed at zero, self-fired projectiles/shells were suppressed and
// movers were still skipped as brushes.  That explains invisible bullets/casings,
// missing impact effects and lift animation jumps.  Keep the crash-safe whitelist,
// but reopen movers plus one-shot projectile/effect replication without bringing
// back the chatty owner weapon/inventory updates that caused v124 fire bursts.
static UBOOL AndroidV126ReplicateOne( UNetConnection* Connection, AActor* Actor )
{
	if( !Connection || !Connection->Driver || !Connection->PackageMap || Connection->State!=USOCK_Open )
		return 0;
	if( !Actor || Actor->bDeleteMe || !Actor->GetClass() )
		return 0;
	if( Actor->IsA(AZoneInfo::StaticClass()) )
		return 0;

	UClass* ActorClass = Actor->GetClass();
	if( Connection->PackageMap->ObjectToIndex(ActorClass)==INDEX_NONE )
		return 0;

	UActorChannel* Channel = Connection->ActorChannels.FindRef( Actor );
	if( !Channel )
	{
		Channel = (UActorChannel*)Connection->CreateChannel( CHTYPE_Actor, 1 );
		if( Channel )
			Channel->SetChannelActor( Actor );
	}
	if( !Channel || Channel->Connection!=Connection || Channel->Closing )
		return 0;

	Channel->RelevantTime = Connection->Driver->Time;
	if( !Channel->IsNetReady(0) )
		return 0;

	Channel->ReplicateActor();
	return 1;
}

static UBOOL AndroidV126BadWorldActor( AActor* Actor )
{
	if( !Actor || Actor->bDeleteMe || !Actor->GetClass() )
		return 1;
	if( Actor->IsA(AZoneInfo::StaticClass()) )
		return 1;
	// Movers are brushes, but they are gameplay-critical.  If they are filtered here,
	// clients only receive corrected pawn positions and see lifts/doors teleport.
	if( Actor->IsA(AMover::StaticClass()) )
		return 0;
	if( Actor->IsA(ABrush::StaticClass()) )
		return 1;
	if( Actor->IsA(ANavigationPoint::StaticClass()) )
		return 1;
	if( Actor->IsA(AMenu::StaticClass()) || Actor->IsA(AHUD::StaticClass()) || Actor->IsA(ALight::StaticClass()) )
		return 1;
	return 0;
}

static UBOOL AndroidV126NearClient( APlayerPawn* Viewer, AActor* Actor, FLOAT MaxDistSq )
{
	if( !Viewer || !Actor )
		return 0;
	return (Actor->Location - Viewer->Location).SizeSquared() <= MaxDistSq;
}

static UBOOL AndroidV138MoverNearClient( APlayerPawn* Viewer, AMover* Mover, FLOAT MaxDistSq )
{
	if( !Viewer || !Mover )
		return 0;

	// UT99_ANDROID_V138_MOVER_RELEVANCY_FIX:
	// Some lifts have brush pivots/base locations that are nowhere near the
	// visible platform.  The old selective Android net path tested only
	// Actor->Location and could therefore skip exactly the lifts the client was
	// standing on.  Check all keyframe positions plus the replicated real/old
	// positions, and always keep the player's current base relevant.
	if( Viewer->Base == Mover )
		return 1;

	if( (Mover->Location - Viewer->Location).SizeSquared() <= MaxDistSq )
		return 1;
	if( (Mover->RealPosition - Viewer->Location).SizeSquared() <= MaxDistSq )
		return 1;
	if( (Mover->OldPos - Viewer->Location).SizeSquared() <= MaxDistSq )
		return 1;

	INT Count = Clamp<INT>( (INT)Mover->NumKeys, 1, ARRAY_COUNT(Mover->KeyPos) );
	for( INT i=0; i<Count; i++ )
	{
		FVector KeyLocation = Mover->BasePos + Mover->KeyPos[i];
		if( (KeyLocation - Viewer->Location).SizeSquared() <= MaxDistSq )
			return 1;
	}
	return 0;
}

static UBOOL AndroidV126AlreadySentTemporary( UNetConnection* Connection, AActor* Actor )
{
	return Connection && Actor && Actor->bNetTemporary && Connection->SentTemporaries.FindItemIndex(Actor)!=INDEX_NONE;
}

static UBOOL AndroidV126OwnedOrInstigatedByViewer( APlayerPawn* Viewer, AActor* Actor )
{
	if( !Viewer || !Actor )
		return 0;
	if( Actor==Viewer || Actor->Owner==Viewer || Actor->Instigator==Viewer )
		return 1;
	AActor* Top = Actor;
	for( INT Depth=0; Top && Top->Owner && Depth<12; Depth++ )
		Top = Top->Owner;
	return Top==Viewer;
}

// UT99_ANDROID_V127_NET_REMOTE_SKIN_PACKAGE_PRIME:
// The selective Android listen-server path bypasses UE1's full relevancy pass.
// Gameplay is now stable, but the remote host pawn can remain green/unskinned on
// OUYA if its skin package is not present in the connection package map before
// the first pawn channel update.  Prime visual packages explicitly and resend
// the package map only when a new linker was added.
static INT AndroidV127EnsureVisualPackage( UNetConnection* Connection, UObject* Obj )
{
	if( !Connection || !Connection->PackageMap || !Obj )
		return 0;
	if( Connection->PackageMap->ObjectToIndex(Obj) != INDEX_NONE )
		return 0;
	ULinkerLoad* Linker = Obj->GetLinker();
	if( !Linker )
		return 0;
	if( Connection->PackageMap->AddLinker(Linker) == INDEX_NONE )
		return 0;
	Connection->PackageMap->Compute();
	Connection->SendPackageMap();
	return 1;
}

static INT AndroidV127PrimeActorVisualPackages( UNetConnection* Connection, AActor* Actor )
{
	if( !Actor )
		return 0;
	INT Added = 0;
	Added += AndroidV127EnsureVisualPackage( Connection, Actor->Mesh );
	Added += AndroidV127EnsureVisualPackage( Connection, Actor->Skin );
	Added += AndroidV127EnsureVisualPackage( Connection, Actor->Texture );
	Added += AndroidV127EnsureVisualPackage( Connection, Actor->Sprite );
	for( INT i=0; i<ARRAY_COUNT(Actor->MultiSkins); i++ )
		Added += AndroidV127EnsureVisualPackage( Connection, Actor->MultiSkins[i] );
	return Added;
}

static INT AndroidV127PrimePawnVisualPackages( UNetConnection* Connection, APawn* Pawn, INT Tick )
{
	if( !Pawn || Tick>1024 )
		return 0;
	INT Added = AndroidV127PrimeActorVisualPackages( Connection, Pawn );
	if( Pawn->Weapon )
		Added += AndroidV127PrimeActorVisualPackages( Connection, Pawn->Weapon );
	if( Pawn->PendingWeapon && Pawn->PendingWeapon!=Pawn->Weapon )
		Added += AndroidV127PrimeActorVisualPackages( Connection, Pawn->PendingWeapon );
	if( Pawn->PlayerReplicationInfo )
		Added += AndroidV127PrimeActorVisualPackages( Connection, Pawn->PlayerReplicationInfo );
	if( Added && (Tick<=64 || ((Tick&255)==0)) )
	{
		FString Msg = FString::Printf(TEXT("UT99_ANDROID_V127_NET primed visual packages pawn=%s added=%i tick=%i"), Pawn->GetName(), Added, Tick);
		AndroidNetLogV115( *Msg );
	}
	return Added;
}

static INT AndroidV126ReplicateClientCore( UNetConnection* Connection, INT Tick )
{
	if( !Connection || !Connection->Actor || Connection->Actor->bDeleteMe )
		return 0;

	INT Updated = 0;
	APlayerPawn* PlayerPawn = Connection->Actor;
	APawn* Pawn = PlayerPawn;

	// Client-owned pawn: keep corrections/HUD alive, but avoid hammering OUYA every
	// single frame after the join settles.
	if( Tick<=64 || ((Tick&1)==0) )
		Updated += AndroidV126ReplicateOne( Connection, PlayerPawn );

	// PRI changes slowly; frequent enough for score/death state, cheap enough for LAN.
	if( Pawn->PlayerReplicationInfo && (Tick<=64 || ((Tick&15)==0)) )
		Updated += AndroidV126ReplicateOne( Connection, Pawn->PlayerReplicationInfo );

	// Local weapon/inventory was too chatty in v124 and caused visible burst/ammo
	// feedback on the OUYA client. Send it during join/pickup windows and then only
	// as a slow resync; the client predicts its own firing locally.
	UBOOL bOwnerInventoryPulse = (Tick<=48) || ((Tick&63)==0);
	if( bOwnerInventoryPulse )
	{
		if( Pawn->Weapon )
			Updated += AndroidV126ReplicateOne( Connection, Pawn->Weapon );
		if( Pawn->PendingWeapon && Pawn->PendingWeapon!=Pawn->Weapon )
			Updated += AndroidV126ReplicateOne( Connection, Pawn->PendingWeapon );
		if( Pawn->SelectedItem )
			Updated += AndroidV126ReplicateOne( Connection, Pawn->SelectedItem );

		AInventory* Inv = Pawn->Inventory;
		for( INT i=0; Inv && i<10; i++ )
		{
			Updated += AndroidV126ReplicateOne( Connection, Inv );
			Inv = Inv->Inventory;
		}
	}
	return Updated;
}

static UBOOL AndroidV126ShouldReplicateEffect( AActor* Actor )
{
	if( !Actor || Actor->bDeleteMe || !Actor->GetClass() )
		return 0;
	// Projectiles plus net-temporary effect actors cover rockets, shock balls,
	// bullet puffs, wall decals, shell casings and spark/impact bursts.
	if( Actor->IsA(AProjectile::StaticClass()) || Actor->bNetTemporary )
		return 1;
	return 0;
}

static INT AndroidV126ReplicateSelectedWorldActors( ULevel* Level, UNetConnection* Connection, INT Tick, INT& OutPawns, INT& OutEffects, INT& OutMovers )
{
	OutPawns = 0;
	OutEffects = 0;
	OutMovers = 0;
	if( !Level || !Connection || !Connection->Actor )
		return 0;

	APlayerPawn* Viewer = Connection->Actor;
	INT Updated = 0;
	// Keep these budgets intentionally small.  They are per-server-tick and only for
	// the safe Android direct-join path, not a replacement for full UE1 relevancy.
	INT PawnBudget   = (Tick<=512) ? 8 : 4;
	INT EffectBudget = (Tick<=256) ? 12 : 8;
	INT MoverBudget  = (Tick<=512) ? 32 : 16;
	const FLOAT PawnDistSq   = 16000.0f * 16000.0f;
	const FLOAT EffectDistSq = 16000.0f * 16000.0f;
	const FLOAT MoverDistSq  = 22000.0f * 22000.0f;

	for( INT i=0; i<Level->Actors.Num(); i++ )
	{
		AActor* Actor = Level->Actors(i);
		if( AndroidV126BadWorldActor(Actor) || Actor==Viewer )
			continue;
		if( Actor->RemoteRole==ROLE_None )
			continue;

		// Moving brushes: doors, lifts and elevators.  These must be sent before the
		// generic brush filter path; otherwise the client pawn snaps to corrected
		// positions while the platform itself never animates.
		AMover* Mover = Cast<AMover>(Actor);
		if( Mover )
		{
			UBOOL bActiveMover = Mover->bInterpolating || (Viewer->Base == Mover);
			UBOOL bMoverPulse = (Tick<=512) || bActiveMover || ((Tick&7)==0);
			UBOOL bMoverRelevant = bActiveMover || AndroidV138MoverNearClient(Viewer, Mover, MoverDistSq);
			if( bMoverPulse && bMoverRelevant && (bActiveMover || MoverBudget>0) )
			{
				Updated += AndroidV126ReplicateOne( Connection, Actor );
				OutMovers++;
				if( !bActiveMover && MoverBudget>0 )
					MoverBudget--;
			}
			continue;
		}

		// Other player/bot pawns: update every tick for nearby pawns.  The previous
		// every-other-tick throttle made the host player look visibly laggy on OUYA.
		APawn* OtherPawn = Cast<APawn>(Actor);
		if( OtherPawn )
		{
			UBOOL bRemotePawnPulse = (Tick<=512) || ((Tick&1)==0);
			if( bRemotePawnPulse && PawnBudget>0 && AndroidV126NearClient(Viewer, Actor, PawnDistSq) )
			{
				AndroidV127PrimePawnVisualPackages( Connection, OtherPawn, Tick );
				Updated += AndroidV126ReplicateOne( Connection, Actor );
				OutPawns++;
				PawnBudget--;
				// Force visual identity/weapon state during join so remote players do not
				// stay green/unskinned or weaponless on OUYA.  After the join settles,
				// keep it light to shave a little WLAN lag.
				if( OtherPawn->PlayerReplicationInfo && (Tick<=512 || ((Tick&31)==0)) )
					Updated += AndroidV126ReplicateOne( Connection, OtherPawn->PlayerReplicationInfo );
				if( OtherPawn->Weapon && (Tick<=512 || ((Tick&7)==0)) )
					Updated += AndroidV126ReplicateOne( Connection, OtherPawn->Weapon );
			}
			continue;
		}

		// Projectiles / one-shot effects.  v125 skipped self-instigated effects and
		// therefore hid the client's own shots, casings and impacts.  Do not mirror
		// owner weapon/inventory state here; just replicate the spawned actor once.
		if( AndroidV126AlreadySentTemporary(Connection, Actor) )
			continue;
		if( AndroidV126ShouldReplicateEffect(Actor) && EffectBudget>0 && AndroidV126NearClient(Viewer, Actor, EffectDistSq) )
		{
			Updated += AndroidV126ReplicateOne( Connection, Actor );
			OutEffects++;
			EffectBudget--;
			continue;
		}
	}
	return Updated;
}

static INT AndroidV126ReplicateCoreActors( ULevel* Level, UNetConnection* Connection, INT Tick )
{
	INT CoreUpdated = AndroidV126ReplicateClientCore( Connection, Tick );
	INT PawnUpdated = 0;
	INT EffectUpdated = 0;
	INT MoverUpdated = 0;
	INT WorldUpdated = AndroidV126ReplicateSelectedWorldActors( Level, Connection, Tick, PawnUpdated, EffectUpdated, MoverUpdated );
	INT Updated = CoreUpdated + WorldUpdated;

	if( Tick<=32 || ((Tick&127)==0) )
	{
		FString Msg = FString::Printf(TEXT("UT99_ANDROID_V127_NET selective pulse tick=%i core=%i pawns=%i movers=%i effects=%i total=%i"), Tick, CoreUpdated, PawnUpdated, MoverUpdated, EffectUpdated, Updated);
		AndroidNetLogV115( *Msg );
	}
	return Updated;
}


// UT99_ANDROID_V128_NATIVE_LAN_BROWSER_BEACON:
// In the current Android/OUYA path the listen server itself is stable, but the
// stock LAN Servers tab can stay empty because the scripted IpDrv.UdpBeacon does
// not visibly bind/tick on the Android build.  Keep the gameplay replication path
// untouched and provide a tiny native LAN beacon responder for the stock
// UBrowserLocalLink protocol:
//   client sends  REPORTQUERY  to 8777..8786
//   server replies "ut <gameport>" to the client's source port
// This also sends periodic old-style beacons to 9777 for ClientBeaconReceiver.
static INT    GAndroidV128LanSock       = -1;
static INT    GAndroidV128LanPort       = 0;
static INT    GAndroidV128QuerySock     = -1;
static INT    GAndroidV128QueryPort     = 0;
static DOUBLE GAndroidV128LastBeacon    = 0.0;
static UBOOL  GAndroidV128BindLogged    = 0;
static UBOOL  GAndroidV128QueryLogged   = 0;

static void AndroidV128LanSendTo( INT Sock, DWORD AddrHost, INT Port, const char* Text )
{
	if( Sock < 0 || !Text || Port <= 0 )
		return;
	sockaddr_in To;
	appMemzero( &To, sizeof(To) );
	To.sin_family      = AF_INET;
	To.sin_port        = htons(Port);
	To.sin_addr.s_addr = htonl(AddrHost);
	sendto( Sock, Text, strlen(Text), 0, (sockaddr*)&To, sizeof(To) );
}

static INT AndroidV128LanEnsureSocket()
{
	if( GAndroidV128LanSock >= 0 )
		return GAndroidV128LanSock;

	INT Sock = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
	if( Sock < 0 )
		return -1;

	INT One = 1;
	setsockopt( Sock, SOL_SOCKET, SO_REUSEADDR, &One, sizeof(One) );
	setsockopt( Sock, SOL_SOCKET, SO_BROADCAST, &One, sizeof(One) );

	INT Flags = fcntl( Sock, F_GETFL, 0 );
	if( Flags >= 0 )
		fcntl( Sock, F_SETFL, Flags | O_NONBLOCK );

	for( INT Port=8777; Port<8797; Port++ )
	{
		sockaddr_in Addr;
		appMemzero( &Addr, sizeof(Addr) );
		Addr.sin_family      = AF_INET;
		Addr.sin_addr.s_addr = htonl(INADDR_ANY);
		Addr.sin_port        = htons(Port);
		if( bind( Sock, (sockaddr*)&Addr, sizeof(Addr) ) == 0 )
		{
			GAndroidV128LanSock = Sock;
			GAndroidV128LanPort = Port;
			if( !GAndroidV128BindLogged )
			{
				GAndroidV128BindLogged = 1;
				AndroidNetLogV115( *FString::Printf(TEXT("UT99_ANDROID_V128_LAN native beacon bound port=%i"), Port) );
			}
			return GAndroidV128LanSock;
		}
	}

	if( !GAndroidV128BindLogged )
	{
		GAndroidV128BindLogged = 1;
		AndroidNetLogV115( TEXT("UT99_ANDROID_V128_LAN native beacon bind failed ports=8777-8796") );
	}
	close( Sock );
	return -1;
}


static INT AndroidV128BindUdpPortRange( INT FirstPort, INT LastPort, INT& OutPort )
{
	INT Sock = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
	if( Sock < 0 )
		return -1;
	INT One = 1;
	setsockopt( Sock, SOL_SOCKET, SO_REUSEADDR, &One, sizeof(One) );
	setsockopt( Sock, SOL_SOCKET, SO_BROADCAST, &One, sizeof(One) );
	INT Flags = fcntl( Sock, F_GETFL, 0 );
	if( Flags >= 0 )
		fcntl( Sock, F_SETFL, Flags | O_NONBLOCK );
	for( INT Port=FirstPort; Port<=LastPort; Port++ )
	{
		sockaddr_in Addr;
		appMemzero( &Addr, sizeof(Addr) );
		Addr.sin_family      = AF_INET;
		Addr.sin_addr.s_addr = htonl(INADDR_ANY);
		Addr.sin_port        = htons(Port);
		if( bind( Sock, (sockaddr*)&Addr, sizeof(Addr) ) == 0 )
		{
			OutPort = Port;
			return Sock;
		}
	}
	close( Sock );
	return -1;
}

static INT AndroidV128LanEnsureQuerySocket( INT GamePort )
{
	if( GAndroidV128QuerySock >= 0 )
		return GAndroidV128QuerySock;
	INT Preferred = GamePort > 0 ? GamePort + 1 : 7778;
	GAndroidV128QuerySock = AndroidV128BindUdpPortRange( Preferred, Preferred, GAndroidV128QueryPort );
	if( GAndroidV128QuerySock < 0 )
		GAndroidV128QuerySock = AndroidV128BindUdpPortRange( 7778, 7787, GAndroidV128QueryPort );
	if( !GAndroidV128QueryLogged )
	{
		GAndroidV128QueryLogged = 1;
		if( GAndroidV128QuerySock >= 0 )
			AndroidNetLogV115( *FString::Printf(TEXT("UT99_ANDROID_V128_LAN native query bound port=%i"), GAndroidV128QueryPort) );
		else
			AndroidNetLogV115( TEXT("UT99_ANDROID_V128_LAN native query bind skipped ports=7778-7787") );
	}
	return GAndroidV128QuerySock;
}

static void AndroidV128LanTickQuerySocket( ULevel* Level, INT GamePort )
{
	INT Sock = AndroidV128LanEnsureQuerySocket( GamePort );
	if( Sock < 0 || !Level )
		return;

	char MapName[128];
	char Title[128];
	strncpy( MapName, appToAnsi(*Level->URL.Map), sizeof(MapName)-1 );
	MapName[sizeof(MapName)-1] = 0;
	strncpy( Title, "UT99 Android LAN", sizeof(Title)-1 );
	Title[sizeof(Title)-1] = 0;
	ALevelInfo* Info = Level->GetLevelInfo();
	if( Info && Info->Title.Len() > 0 )
	{
		strncpy( Title, appToAnsi(*Info->Title), sizeof(Title)-1 );
		Title[sizeof(Title)-1] = 0;
	}

	for( INT n=0; n<12; n++ )
	{
		char Buf[512];
		sockaddr_in From;
		socklen_t FromLen = sizeof(From);
		INT Count = recvfrom( Sock, Buf, sizeof(Buf)-1, 0, (sockaddr*)&From, &FromLen );
		if( Count <= 0 )
			break;
		Buf[Count] = 0;
		if( Buf[0] != '\\' )
			continue;

		char Reply[1024];
		snprintf( Reply, sizeof(Reply),
			"\\hostname\\UT99 Android LAN\\hostport\\%d\\mapname\\%s\\maptitle\\%s\\gametype\\DeathMatchPlus\\gamemode\\openplaying\\numplayers\\1\\maxplayers\\16\\gamever\\400\\minnetver\\400\\listenserver\\True\\gamename\\ut\\final\\",
			GamePort, MapName[0] ? MapName : "DM", Title[0] ? Title : "UT99 Android LAN" );
		sendto( Sock, Reply, strlen(Reply), 0, (sockaddr*)&From, FromLen );

		DWORD FromHost = ntohl(From.sin_addr.s_addr);
		INT FromPort = ntohs(From.sin_port);
		AndroidNetLogV115( *FString::Printf(TEXT("UT99_ANDROID_V128_LAN native query reply from=%u.%u.%u.%u:%i queryport=%i game=%i"),
			(FromHost>>24)&255, (FromHost>>16)&255, (FromHost>>8)&255, FromHost&255, FromPort, GAndroidV128QueryPort, GamePort) );
	}
}

static void AndroidV128LanTick( ULevel* Level )
{
	if( !Level || !Level->NetDriver )
		return;

	INT Sock = AndroidV128LanEnsureSocket();
	if( Sock < 0 )
		return;

	INT GamePort = Level->URL.Port > 0 ? Level->URL.Port : 7777;
	AndroidV128LanTickQuerySocket( Level, GamePort );
	INT QueryPort = GAndroidV128QueryPort > 0 ? GAndroidV128QueryPort : GamePort + 1;
	char ReplyUt[96];
	char ReplyUnreal[96];
	snprintf( ReplyUt, sizeof(ReplyUt), "ut %d", QueryPort );
	snprintf( ReplyUnreal, sizeof(ReplyUnreal), "unreal %d", QueryPort );

	for( INT n=0; n<12; n++ )
	{
		char Buf[256];
		sockaddr_in From;
		socklen_t FromLen = sizeof(From);
		INT Count = recvfrom( Sock, Buf, sizeof(Buf)-1, 0, (sockaddr*)&From, &FromLen );
		if( Count <= 0 )
			break;
		Buf[Count] = 0;

		INT FromPort = ntohs(From.sin_port);
		DWORD FromHost = ntohl(From.sin_addr.s_addr);
		FString Incoming = appFromAnsi((ANSICHAR*)Buf);
		if( appStricmp(*Incoming, TEXT("REPORTQUERY")) == 0 )
		{
			AndroidV128LanSendTo( Sock, FromHost, FromPort, ReplyUt );
			AndroidNetLogV115( *FString::Printf(TEXT("UT99_ANDROID_V128_LAN replied REPORTQUERY from=%u.%u.%u.%u:%i game=%i query=%i listen=%i"),
				(FromHost>>24)&255, (FromHost>>16)&255, (FromHost>>8)&255, FromHost&255, FromPort, GamePort, QueryPort, GAndroidV128LanPort) );
		}
		else if( appStricmp(*Incoming, TEXT("REPORT")) == 0 )
		{
			AndroidV128LanSendTo( Sock, FromHost, FromPort, ReplyUt );
			AndroidV128LanSendTo( Sock, FromHost, FromPort, ReplyUnreal );
			AndroidNetLogV115( *FString::Printf(TEXT("UT99_ANDROID_V128_LAN replied REPORT from=%u.%u.%u.%u:%i game=%i"),
				(FromHost>>24)&255, (FromHost>>16)&255, (FromHost>>8)&255, FromHost&255, FromPort, GamePort) );
		}
	}

	DOUBLE Now = appSeconds();
	if( Now - GAndroidV128LastBeacon >= 1.0 )
	{
		GAndroidV128LastBeacon = Now;
		AndroidV128LanSendTo( Sock, 0xffffffff, 9777, ReplyUt );
		AndroidV128LanSendTo( Sock, 0xffffffff, 9777, ReplyUnreal );
		if( ((INT)Now & 7) == 0 )
			AndroidNetLogV115( *FString::Printf(TEXT("UT99_ANDROID_V128_LAN broadcast beacon game=%i query=%i listen=%i"), GamePort, QueryPort, GAndroidV128LanPort) );
	}
}

#endif

/*-----------------------------------------------------------------------------
	Helper classes.
-----------------------------------------------------------------------------*/

//
// Priority sortable list.
//
struct FActorPriority
{
	INT			    Priority;	// Update priority, higher = more important.
	AActor*			Actor;		// Actor.
	UActorChannel*	Channel;	// Actor channel.
#if defined(__ANDROID__) || defined(PLATFORM_ANDROID)
	// UT99_ANDROID_V118_NET_CACHED_ACTOR_FLAGS:
	// The direct-join crash happens after the priority list is built but before
	// UActorChannel::ReplicateActor() is reached.  On ARM this points at a fragile
	// second dereference of AActor fields inside ServerTickClient().  Cache the
	// fields we need while MakeConsiderList is already safely touching the actor,
	// then use the cached values in UpdateRelevant.
	UClass*			ActorClass;
	ULevel*			ActorLevel;
	UBOOL			bAndroidValid;
	UBOOL			bAndroidZoneInfo;
	UBOOL			bAndroidAlwaysRelevant;
	UBOOL			bAndroidNetOwner;
#endif
	FActorPriority()
	{}
	FActorPriority( FVector& ViewPos, FVector& ViewDir, UNetConnection* InConnection, AActor* InActor )
	{
		guard(FActorPriority::FActorPriority);
		Actor       = InActor;
		Channel     = NULL;
		Priority    = -2147483647;
#if defined(__ANDROID__) || defined(PLATFORM_ANDROID)
		ActorClass  = NULL;
		ActorLevel  = NULL;
		bAndroidValid = 0;
		bAndroidZoneInfo = 0;
		bAndroidAlwaysRelevant = 0;
		bAndroidNetOwner = 0;
#endif

		// UT99_ANDROID_V115_NET_PRIORITY_SAFE:
		// A joining client can make the listen server consider actors while the
		// connection/channel state is still being built.  The original code assumes
		// every pointer used for priority sorting is valid and later Sort() calls
		// Compare(FActorPriority*,FActorPriority*).  On Android/ARM this showed up
		// as a hard SIGSEGV inside Compare() during direct IP join.
		if( InConnection && InConnection->Driver && InActor && !InActor->bDeleteMe )
		{
#if defined(__ANDROID__) || defined(PLATFORM_ANDROID)
			ActorClass  = InActor->GetClass();
			ActorLevel  = InActor->GetLevel();
			bAndroidValid = (ActorClass!=NULL) && (ActorLevel!=NULL);
			bAndroidZoneInfo = ActorClass && InActor->IsA(AZoneInfo::StaticClass());
			bAndroidAlwaysRelevant = InActor->bAlwaysRelevant;
			bAndroidNetOwner = InActor->bNetOwner;
#endif
			Channel     = InConnection->ActorChannels.FindRef(Actor);
			FLOAT Time  = Channel ? (InConnection->Driver->Time - Channel->LastUpdateTime) : InConnection->Driver->SpawnPrioritySeconds;
			FLOAT Dot   = ViewDir | (Actor->Location - ViewPos).SafeNormal();
			Priority    = appRound(65536.0 * (3.0+Dot) * Actor->GetNetPriority( (Channel && Channel->Recent.Num()) ? (AActor*)&Channel->Recent(0) : NULL, Time, InConnection->BestLag ));
			if( InActor->bNetOptional )
				Priority -= 100000;
		}
		unguard;
	}
	friend INT Compare( const FActorPriority* A, const FActorPriority* B )
	{
		if( !A && !B )
			return 0;
		if( !A )
			return -1;
		if( !B )
			return 1;
		return B->Priority - A->Priority;
	}
};

/*-----------------------------------------------------------------------------
	Tick a single actor.
-----------------------------------------------------------------------------*/

UBOOL AActor::Tick( FLOAT DeltaSeconds, ELevelTick TickType )
{
	guard(AActor::Tick);

	// Ignore actors in stasis
	if
	(	bStasis 
	&&	(bForceStasis || (Physics==PHYS_None) || (Physics == PHYS_Rotating))
	&&	(GetLevel()->TimeSeconds - GetLevel()->Model->Zones[Region.ZoneNumber].LastRenderTime > 5)
	&&	(Level->NetMode == NM_Standalone) )
		return 1;

	// Handle owner-first updating.
	if( Owner && (INT)Owner->bTicked!=GetLevel()->Ticked )
	{
		GetLevel()->NewlySpawned = new(GEngineMem)FActorLink(this,GetLevel()->NewlySpawned);
		return 0;
	}
	bTicked = GetLevel()->Ticked;
	APawn* Pawn = NULL;
	if( bIsPawn )
		Pawn = Cast<APawn>(this);

	INT bSimulatedPawn = ( Pawn && (Role == ROLE_SimulatedProxy) );

	// Update all animation, including multiple passes if necessary.
	INT Iterations = 0;
	FLOAT Seconds = DeltaSeconds;
	//if ( bSimulatedPawn )
	//	debugf("Animation %s frame %f rate %f tween %f",*AnimSequence,AnimFrame, AnimRate, TweenRate);
	while
	(	IsAnimating()
	&&	(Seconds>0.0)
	&&	(++Iterations <= 4) )
	{
		// Remember the old frame.
		FLOAT OldAnimFrame = AnimFrame;

		// Update animation, and possibly overflow it.
		if( AnimFrame >= 0.0 )
		{
			// Update regular or velocity-scaled animation.
			if( AnimRate >= 0.0 )
				AnimFrame += AnimRate * Seconds;
			else
				AnimFrame += ::Max( AnimMinRate, Velocity.Size() * -AnimRate ) * Seconds;

			// Handle all animation sequence notifys.
			if( bAnimNotify && Mesh )
			{
				const FMeshAnimSeq* Seq = Mesh->GetAnimSeq( AnimSequence );
				if( Seq )
				{
					FLOAT BestElapsedFrames = 100000.0;
					const FMeshAnimNotify* BestNotify = NULL;
					for( INT i=0; i<Seq->Notifys.Num(); i++ )
					{
						const FMeshAnimNotify& Notify = Seq->Notifys(i);
						if( OldAnimFrame<Notify.Time && AnimFrame>=Notify.Time )
						{
							FLOAT ElapsedFrames = Notify.Time - OldAnimFrame;
							if( BestNotify==NULL || ElapsedFrames<BestElapsedFrames )
							{
								BestElapsedFrames = ElapsedFrames;
								BestNotify        = &Notify;
							}
						}
					}
					if( BestNotify )
					{
						Seconds   = Seconds * (AnimFrame - BestNotify->Time) / (AnimFrame - OldAnimFrame);
						AnimFrame = BestNotify->Time;
						UFunction* Function = FindFunction( BestNotify->Function );
						if( Function )
							ProcessEvent( Function, NULL );
						continue;
					}
				}
			}

			// Handle end of animation sequence.
			if( AnimFrame<AnimLast )
			{
				// We have finished the animation updating for this tick.
				break;
			}
			else if( bAnimLoop )
			{
				if( AnimFrame < 1.0 )
				{
					// Still looping.
					Seconds = 0.0;
				}
				else
				{
					// Just passed end, so loop it.
					Seconds = Seconds * (AnimFrame - 1.0) / (AnimFrame - OldAnimFrame);
					AnimFrame = 0.0;
				}
				if( OldAnimFrame < AnimLast )
				{
					if( GetStateFrame()->LatentAction == EPOLL_FinishAnim )
						bAnimFinished = 1;
					if( !bSimulatedPawn )
						eventAnimEnd();
				}
			}
			else 
			{
				// Just passed end-minus-one frame.
				Seconds = Seconds * (AnimFrame - AnimLast) / (AnimFrame - OldAnimFrame);
				AnimFrame	 = AnimLast;
				bAnimFinished = 1;
				AnimRate      = 0.0;
				if ( !bSimulatedPawn )
					eventAnimEnd();
				
				if ( (RemoteRole < ROLE_SimulatedProxy) && !IsA(AWeapon::StaticClass()) )
				{
					SimAnim.X = 10000 * AnimFrame;
					SimAnim.Y = 5000 * AnimRate;
					if ( SimAnim.Y > 32767 )
						SimAnim.Y = 32767;
				}
			}
		}
		else
		{
			// Update tweening.
			AnimFrame += TweenRate * Seconds;
			if( AnimFrame >= 0.0 )
			{
				// Finished tweening.
				Seconds          = Seconds * (AnimFrame-0) / (AnimFrame - OldAnimFrame);
				AnimFrame = 0.0;
				if( AnimRate == 0.0 )
				{
					bAnimFinished = 1;
					if ( !bSimulatedPawn )
						eventAnimEnd();
				}
			}
			else
			{
				// Finished tweening.
				break;
			}
		}
	}

	// This actor is tickable.
	if( bSimulatedPawn )
	{
		// FIXME - predict fall for all pawns (COOP) - but need
		// new replicated bool for pawns which don't fly but don't fall
		// (i.e. stuck on wall, PHYS_Spider, etc.)
		if ( Pawn->bIsPlayer && !Pawn->bCanFly && !Region.Zone->bWaterZone )
		{
			// only add gravity if pawn is not resting on valid floor
			FCheckResult Hit(1.0);
			GetLevel()->SingleLineCheck(Hit, this, Location - FVector(0,0,8), Location, TRACE_VisBlocking, GetCylinderExtent());
			if ( (Hit.Time == 1.0) || (Hit.Normal.Z < 0.7) )
				Velocity += 0.5 * Region.Zone->ZoneGravity * DeltaSeconds;
		}
		//simulated pawns just predict location, no script execution
		moveSmooth(Velocity * DeltaSeconds);

		// Tick the nonplayer.
		if ( IsProbing(NAME_Tick) )
			eventTick(DeltaSeconds);
	}
	else if( RemoteRole == ROLE_AutonomousProxy ) 
	{
		if( Role == ROLE_Authority )
		{
			// update viewtarget replicated info
			APlayerPawn* PlayerPawn = NULL;
			if( Pawn )
			{
				PlayerPawn = Cast<APlayerPawn>(this);
			}
			if( PlayerPawn && PlayerPawn->ViewTarget )
			{
				APawn* TargetPawn = Cast<APawn>(PlayerPawn->ViewTarget);
				if ( TargetPawn )
				{
					PlayerPawn->TargetViewRotation = TargetPawn->ViewRotation;
					PlayerPawn->TargetEyeHeight = TargetPawn->EyeHeight;
					if ( TargetPawn->Weapon )
						PlayerPawn->TargetWeaponViewOffset = TargetPawn->Weapon->PlayerViewOffset;
				}
			}

			// Server handles timers for autonomous proxy.
			if( (TimerRate>0.0) && (TimerCounter+=DeltaSeconds)>=TimerRate )
			{
				// Normalize the timer count.
				INT TimerTicksPassed = 1;
				if( TimerRate > 0.0 )
				{
					TimerTicksPassed     = (int)(TimerCounter/TimerRate);
					TimerCounter -= TimerRate * TimerTicksPassed;
					if( TimerTicksPassed && !bTimerLoop )
					{
						// Only want a one-shot timer message.
						TimerTicksPassed = 1;
						TimerRate = 0.0;
					}
				}

				// Call timer routine with count of timer events that have passed.
				eventTimer();
			}
		}
	}
	else if( Role>=ROLE_SimulatedProxy )
	{
		APlayerPawn* PlayerPawn = NULL;
		if ( Pawn )
			PlayerPawn = Cast<APlayerPawn>(this);
		if( !PlayerPawn || !PlayerPawn->Player )
		{
			// Non-player update.
			if( TickType==LEVELTICK_ViewportsOnly )
				return 1;

			// Tick the nonplayer.
			if ( IsProbing(NAME_Tick) )
				eventTick(DeltaSeconds);
		}
		else
		{
			// Player update.
			if( PlayerPawn->IsA(ACamera::StaticClass()) && !(PlayerPawn->ShowFlags & SHOW_PlayerCtrl) )
				return 1;

			// Process PlayerTick with input.
			PlayerPawn->Player->ReadInput( DeltaSeconds );
			PlayerPawn->eventPlayerInput( DeltaSeconds );
			PlayerPawn->eventPlayerTick( DeltaSeconds );
			PlayerPawn->Player->ReadInput( -1.0 );

			if( GetLevel()->DemoRecDriver && !GetLevel()->DemoRecDriver->ServerConnection )
			{
				PlayerPawn->DemoViewPitch = PlayerPawn->ViewRotation.Pitch;
				PlayerPawn->DemoViewYaw = PlayerPawn->ViewRotation.Yaw;
			}
		}

		// Update the actor's script state code.
		ProcessState( DeltaSeconds );

		// Update timers.
		if( TimerRate>0.0 && (TimerCounter+=DeltaSeconds)>=TimerRate )
		{
			// Normalize the timer count.
			INT TimerTicksPassed = 1;
			if( TimerRate > 0.0 )
			{
				TimerTicksPassed     = (int)(TimerCounter/TimerRate);
				TimerCounter -= TimerRate * TimerTicksPassed;
				if( TimerTicksPassed && !bTimerLoop )
				{
					// Only want a one-shot timer message.
					TimerTicksPassed = 1;
					TimerRate = 0.0;
				}
			}

			// Call timer routine with count of timer events that have passed.
			eventTimer();
		}

		// Update LifeSpan.
		if( LifeSpan!=0.f )
		{
			LifeSpan -= DeltaSeconds;
			if( LifeSpan <= 0.0001 )
			{
				// Actor's LifeSpan expired.
				eventExpired();
				GetLevel()->DestroyActor( this );
				return 1;
			}
		}

		// Perform physics.
		if( Physics!=PHYS_None && Role!=ROLE_AutonomousProxy )
			performPhysics( DeltaSeconds );
	}
	else if ( Physics == PHYS_Falling ) // dumbproxies simulate falling if client side physics set
		performPhysics( DeltaSeconds );

	// During demo playback, setup view offsets for viewtarget
	if( GetLevel()->DemoRecDriver && GetLevel()->DemoRecDriver->ServerConnection )
	{
		if( Role == ROLE_Authority )
		{
			// update viewtarget replicated info
			APlayerPawn* PlayerPawn = NULL;
			if( Pawn )
			{
				PlayerPawn = Cast<APlayerPawn>(this);
			}
			if( PlayerPawn && PlayerPawn->ViewTarget && !PlayerPawn->bBehindView )
			{
				APawn* TargetPawn = Cast<APawn>(PlayerPawn->ViewTarget);
				if ( TargetPawn )
				{
					PlayerPawn->TargetViewRotation = TargetPawn->ViewRotation;
					PlayerPawn->TargetEyeHeight = TargetPawn->EyeHeight;
					if ( TargetPawn->Weapon )
						PlayerPawn->TargetWeaponViewOffset = TargetPawn->Weapon->PlayerViewOffset;
				}
			}
		}
	}
	
	// Update eyeheight and send visibility updates
	// with PVS, monsters look for other monsters, rather than sending msgs
	// Also sends PainTimer messages if PainTime
	if( Pawn )
	{
		if( Pawn->bIsPlayer && Role>=ROLE_AutonomousProxy )
		{
			if ( Pawn->bViewTarget )
				Pawn->eventUpdateEyeHeight( DeltaSeconds );
			else
				Pawn->ViewRotation = Rotation;
		}

		// update weapon location (in case its playing sounds, etc.)
		if ( Pawn->Weapon )
		{
			GetLevel()->FarMoveActor( Pawn->Weapon, Location );
		}
		if( Role==ROLE_Authority && TickType==LEVELTICK_All )
		{
			if( Pawn->SightCounter < 0.0 )
			{
				Pawn->SightCounter += 0.2;
			}
			Pawn->SightCounter = Pawn->SightCounter - DeltaSeconds; 
			if( Pawn->bIsPlayer && !Pawn->bHidden )
			{
				Pawn->ShowSelf();
			}
			if( Pawn->SightCounter<0.0 && Pawn->IsProbing(NAME_EnemyNotVisible) )
			{
				Pawn->CheckEnemyVisible();
				Pawn->SightCounter = 0.1;
			}
			if( Pawn->PainTime > 0.0 )
			{
				Pawn->PainTime -= DeltaSeconds;
				if (Pawn->PainTime < 0.001)
				{
					Pawn->PainTime = 0.0;
					Pawn->eventPainTimer();
				}
			}
			if( Pawn->SpeechTime > 0.0 )
			{
				Pawn->SpeechTime -= DeltaSeconds;
				if (Pawn->SpeechTime < 0.001)
				{
					Pawn->SpeechTime = 0.0;
					Pawn->eventSpeechTimer();
				}
			}
			if ( Pawn->bAdvancedTactics )
				Pawn->eventUpdateTactics(DeltaSeconds);
		}
	}

	#if defined(__ANDROID__) || defined(PLATFORM_ANDROID)
	// UT99_ANDROID_V167T_ASGUARDIA_SLIDING_DOOR_POSTPHYS_TICK:
	// Start custom AS-Guardia door moves after the normal actor/script/physics
	// work for this frame. v167t uses a short visible move time and consumes
	// the AS-Guardia door finish natively while filtering original script sounds, so audio and movement share the native transition start.
	UT99AndroidV167OTickASGuardiaSlidingDoor( this, DeltaSeconds );
	#endif

	return 1;
	unguard;
}

/*-----------------------------------------------------------------------------
	Network client tick.
-----------------------------------------------------------------------------*/

void ULevel::TickNetClient( FLOAT DeltaSeconds )
{
	guard(ULevel::TickNetClient);
	clock(NetTickCycles);
	if( NetDriver->ServerConnection->State==USOCK_Open )
	{
		for( TMap<AActor*,UActorChannel*>::TIterator ItC(NetDriver->ServerConnection->ActorChannels); ItC; ++ItC )
		{
			guard(UpdateLocalActors);
			UActorChannel* It = ItC.Value();
			APlayerPawn* PlayerPawn = Cast<APlayerPawn>(It->GetActor());
			if( PlayerPawn && PlayerPawn->Player )
				It->ReplicateActor();
			unguard;
		}
	}
	else if( NetDriver->ServerConnection->State==USOCK_Closed )
	{
		// Server disconnected.
		check(Engine->Client->Viewports.Num());
		Engine->SetClientTravel( Engine->Client->Viewports(0), TEXT("?failed"), 0, TRAVEL_Absolute );
	}
	unclock(NetTickCycles);
	unguard;
}

/*-----------------------------------------------------------------------------
	Network server ticking individual client.
-----------------------------------------------------------------------------*/

UBOOL ActorCanSee( AActor* Actor, APlayerPawn* RealViewer, AActor* Viewer, FVector SrcLocation )
{
	guardSlow(ActorCanSee);
#if defined(__ANDROID__) || defined(PLATFORM_ANDROID)
	// UT99_ANDROID_V115_NET_ACTORCANSEE_SAFE:
	// The original UE1 relevancy test walks Actor->Owner recursively via
	// IsOwnedBy() and again for pawn weapons.  On Android listen servers this
	// can dereference a stale owner pointer during the first client join and
	// crash in ActorCanSee before normal guard/error handling can run.
	//
	// Keep the normal visibility behavior for standalone/desktop builds.  On
	// Android, avoid recursive owner-chain dereferences and only use direct
	// ownership comparisons, then fall back to the normal FastLineCheck.
	if( !Actor || !RealViewer || !Viewer )
		return 0;
	if( Actor->bDeleteMe || Viewer->bDeleteMe || RealViewer->bDeleteMe )
		return 0;

	AActor* DirectOwner = Actor->Owner;
	if( Actor->bAlwaysRelevant || Actor==Viewer || Actor==RealViewer || DirectOwner==Viewer || DirectOwner==RealViewer )
		return 1;
	else if( Actor->AmbientSound
			&& ((Actor->Location-Viewer->Location).SizeSquared() < 0.3*Actor->WorldSoundRadius()*Actor->WorldSoundRadius()) )
		return 1;
	else if( (Actor->bHidden || Actor->bOnlyOwnerSee) && !Actor->bBlockPlayers && !Actor->AmbientSound )
		return 0;
	else
	{
		ULevel* ActorLevel = Actor->GetLevel();
		if( !ActorLevel || !ActorLevel->Model )
			return 0;
		return ActorLevel->Model->FastLineCheck(Actor->Location,SrcLocation);
	}
#else
	if( Actor->bAlwaysRelevant || Actor->IsOwnedBy(Viewer) || Actor->IsOwnedBy(RealViewer) || Actor==Viewer || Actor==RealViewer )
		return 1;
	else if( Actor->AmbientSound 
			&& ((Actor->Location-Viewer->Location).SizeSquared() < 0.3*Actor->WorldSoundRadius()*Actor->WorldSoundRadius()) )
		return 1;
	else if( Actor->Owner && Actor->Owner->bIsPawn && Actor==((APawn*)Actor->Owner)->Weapon )
		return ActorCanSee( Actor->Owner, RealViewer, Viewer, SrcLocation );
	else if( (Actor->bHidden || Actor->bOnlyOwnerSee) && !Actor->bBlockPlayers && !Actor->AmbientSound )
		return 0;
	else
		return Actor->GetLevel()->Model->FastLineCheck(Actor->Location,SrcLocation);
#endif
	unguardSlow;
}

INT ULevel::ServerTickClient( UNetConnection* Connection, FLOAT DeltaSeconds )
{
	guard(ULevel::ServerTickClient);
	check(Connection);
	check(Connection->State==USOCK_Pending || Connection->State==USOCK_Open || Connection->State==USOCK_Closed);
	DOUBLE CullTime=0.0, TraceTime=0.0, RepTime=0.0; INT CullCount=0, RepCount=0;

	// Handle not ready channels.
	INT Updated=0;
	if( Connection->Actor && Connection->IsNetReady(0) && Connection->State==USOCK_Open )
	{
#if defined(__ANDROID__) || defined(PLATFORM_ANDROID)
		if( Connection->Actor->bDeleteMe )
			return 0;
		if( !NetDriver || !Connection->Driver || !Connection->PackageMap )
		{
			AndroidNetLogV115( TEXT("UT99_ANDROID_V115_NET skip client tick: missing netdriver/driver/packagemap") );
			return 0;
		}
#endif
		// Get list of visible/relevant actors.
		FMemMark Mark(GMem);
		NetTag++;
		Connection->TickCount++;

#if defined(__ANDROID__) || defined(PLATFORM_ANDROID)
		// UT99_ANDROID_V127_NET_SELECTIVE_HOST_SAFE:
		// Keep the crash-safe whitelist, but reopen the missing gameplay rings:
		// nearby movers for lifts/doors, remote pawns every tick, and one-shot
		// projectile/effect actors.  Owner weapon/inventory remains throttled so
		// v124's fire/ammo feedback loop does not return.
		if( Connection->TickCount <= 16 )
		{
			if( Connection->TickCount<=4 || ((Connection->TickCount&7)==0) )
			{
				FString Msg = FString::Printf(TEXT("UT99_ANDROID_V127_NET defer initial relevancy tick=%i"), Connection->TickCount);
				AndroidNetLogV115( *Msg );
			}
			Mark.Pop();
			return 0;
		}
		if( Connection->TickCount >= 17 )
		{
			INT CoreUpdated = AndroidV126ReplicateCoreActors( this, Connection, Connection->TickCount );
			if( ((Connection->TickCount&127)==0) )
			{
				FString Msg = FString::Printf(TEXT("UT99_ANDROID_V127_NET bypass global relevancy tick=%i updated=%i"), Connection->TickCount, CoreUpdated);
				AndroidNetLogV115( *Msg );
			}
			Mark.Pop();
			return CoreUpdated;
		}
#endif

		// Set up to skip all sent temporary actors.
		guard(SkipSentTemporaries);
		for( INT i=0; i<Connection->SentTemporaries.Num(); i++ )
		{
			AActor* SentTemp = Connection->SentTemporaries(i);
#if defined(__ANDROID__) || defined(PLATFORM_ANDROID)
			if( !SentTemp || SentTemp->bDeleteMe )
				continue;
#endif
			SentTemp->NetTag = NetTag;
		}
		unguard;

		// Get viewer coordinates.
		AActor*      Viewer    = Connection->Actor;
		APlayerPawn* InViewer  = Connection->Actor;
		FVector      Location  = InViewer->Location;
		FRotator     Rotation  = InViewer->ViewRotation;
		InViewer->eventPlayerCalcView( Viewer, Location, Rotation );
#if defined(__ANDROID__) || defined(PLATFORM_ANDROID)
		if( !Viewer || Viewer->bDeleteMe )
		{
			AndroidNetLogV115( TEXT("UT99_ANDROID_V115_NET skip client tick: invalid viewer after PlayerCalcView") );
			Mark.Pop();
			return 0;
		}
#endif
		check(Viewer);

		// Compute ahead-vectors for prediction.
		FVector Ahead = FVector(0,0,0);
		if( Connection->TickCount & 1 )
		{
			FLOAT PredictSeconds = (Connection->TickCount&2) ? 0.4 : 0.9;
			Ahead = PredictSeconds * Viewer->Velocity;
#if defined(__ANDROID__) || defined(PLATFORM_ANDROID)
			// UT99_ANDROID_V115_NET_BASE_SAFE:
			// During the first join frames the client's view actor can still reference
			// transient bases. Avoid dereferencing Base here; the prediction offset is
			// only an optimisation for relevancy, not required for correctness.
#else
			if( Viewer->Base )
				Ahead += PredictSeconds * Viewer->Base->Velocity;
#endif
			FCheckResult Hit(1.0);
			Hit.Location = Location + Ahead;
			if( Viewer->GetLevel() && Viewer->GetLevel()->Model )
				Viewer->GetLevel()->Model->LineCheck(Hit,NULL,Hit.Location,Location,FVector(0,0,0),NF_NotVisBlocking);
			Location = Hit.Location;
		}

		// Make list of all actors to consider.
		CullTime-=appSeconds();
		INT              ConsiderCount  = 0;
		FActorPriority*  PriorityList   = new(GMem,Actors.Num())FActorPriority;
		FActorPriority** PriorityActors = new(GMem,Actors.Num())FActorPriority*;
#if defined(__ANDROID__) || defined(PLATFORM_ANDROID)
		if( !PriorityList || !PriorityActors )
		{
			AndroidNetLogV115( TEXT("UT99_ANDROID_V115_NET skip client tick: priority allocation failed") );
			Mark.Pop();
			return 0;
		}
#endif
		FVector          ViewPos        = Viewer->Location;
		FVector          ViewDir        = InViewer->ViewRotation.Vector();
		DOUBLE			 LastTime		= Connection->LastRepTime;
		DOUBLE           ThisTime       = Connection->Driver->Time;
		guard(MakeConsiderList);
		for( INT i=0; i<Actors.Num(); i++ )
		{
			AActor* Actor = Actors(i);
			if( Actor )
			{
#if defined(__ANDROID__) || defined(PLATFORM_ANDROID)
				if( Actor->bDeleteMe || !Actor->GetClass() )
					continue;
#endif
				if
				(	(i>=iFirstDynamicActor || Actor->bAlwaysRelevant)
				&&	(Actor->NetTag!=NetTag)
				&&	(Actor->RemoteRole!=ROLE_None)
				&&	(appRound(LastTime*Actor->NetUpdateFrequency)!=appRound(ThisTime*Actor->NetUpdateFrequency)) )
				{
					CullCount++;
					Actor->NetTag                 = NetTag;
					PriorityList  [ConsiderCount] = FActorPriority( ViewPos, ViewDir, Connection, Actor );
					PriorityActors[ConsiderCount] = PriorityList + ConsiderCount++;
				}
				LastTime += 0.023;
				ThisTime += 0.023;
			}
		}
		Connection->LastRepTime = Connection->Driver->Time;
		CullTime+=appSeconds();
		unguard;

		// Sort by priority.
		guard(SortConsiderList);
#if defined(__ANDROID__) || defined(PLATFORM_ANDROID)
		// UT99_ANDROID_V115_NET_SORT_SAFE:
		// Direct IP join reached the real replication path, but Android/ARM crashed
		// in Compare(FActorPriority*,FActorPriority*) while sorting this temporary
		// pointer list.  For small local listen-server games the priority sort is an
		// optimisation, not a correctness requirement.  Keep the list in collection
		// order on Android to remove the fragile pointer-sort step completely.
		if( ConsiderCount > 0 )
			{ FString Msg = FString::Printf(TEXT("UT99_ANDROID_V122_NET skip priority sort count=%i tick=%i"), ConsiderCount, Connection->TickCount); AndroidNetLogV115( *Msg ); }
#else
		Sort( PriorityActors, ConsiderCount );
#endif
		unguard;

		// Update all relevant actors in sorted order.
		guard(UpdateRelevant);
#if defined(__ANDROID__) || defined(PLATFORM_ANDROID)
		// UT99_ANDROID_V119_NET_JOIN_DEFER:
		// v118 still crashed after the priority-list marker but before the first
		// update-safe marker.  That places the fault at the very entrance of the
		// first UpdateRelevant pass (loop readiness / first actor-channel touch),
		// not in ReplicateActor().  Give a freshly joined Android listen-server
		// client a tiny replication warmup window; the control/login path keeps
		// running, but the fragile actor relevancy pass is delayed until the
		// connection and package map have settled.
		if( Connection->TickCount <= 16 )
		{
			FString Msg = FString::Printf(TEXT("UT99_ANDROID_V122_NET defer initial relevancy tick=%i count=%i"), Connection->TickCount, ConsiderCount);
			AndroidNetLogV115( *Msg );
			Mark.Pop();
			return 0;
		}

		// UT99_ANDROID_V122_NET_DIRECT_JOIN_HOST_SAFE:
		// The v121 test proved two important things:
		//  1) the OUYA can already enter gameplay through the login/control path, and
		//  2) the Retroid host crashes as soon as the first real UpdateRelevant actor
		//     loop starts at tick 17, before any per-actor V121 update-safe marker is
		//     printed.
		// Therefore the immediate stability fix is to keep the server connection alive
		// but completely suppress the fragile actor relevancy loop on Android listen
		// servers.  This deliberately favors a stable direct-join session over full
		// actor replication for now; once the host survives, we can re-enable selected
		// actors one class at a time.
		if( Connection->TickCount >= 17 )
		{
			if( Connection->TickCount <= 24 || ((Connection->TickCount & 63)==0) )
			{
				FString Msg = FString::Printf(TEXT("UT99_ANDROID_V122_NET defer host-safe relevancy tick=%i count=%i"), Connection->TickCount, ConsiderCount);
				AndroidNetLogV115( *Msg );
			}
			Mark.Pop();
			return 0;
		}
		FString UpdateStartMsg = FString::Printf(TEXT("UT99_ANDROID_V122_NET update loop start tick=%i count=%i"), Connection->TickCount, ConsiderCount);
		AndroidNetLogV115( *UpdateStartMsg );
		UBOOL bAndroidLoopNetReady = (Connection && Connection->State==USOCK_Open && Connection->Driver && Connection->PackageMap);
		for( INT j=0; j<ConsiderCount && bAndroidLoopNetReady; j++ )
#else
		for( INT j=0; j<ConsiderCount && Connection->IsNetReady(0); j++ )
#endif
		{
			if( !PriorityActors[j] )
				continue;
			FActorPriority* AndroidPriority = PriorityActors[j];
			AActor*        Actor       = AndroidPriority->Actor;
			UActorChannel* Channel     = AndroidPriority->Channel;
#if defined(__ANDROID__) || defined(PLATFORM_ANDROID)
			// UT99_ANDROID_V119_NET_UPDATE_SAFE:
			// Do not call Connection->IsNetReady() from the loop condition on Android;
			// the first direct-join crash happens before any per-actor marker.  Use a
			// once-per-loop readiness snapshot and keep the first relevant actors very
			// conservative until the connection has survived a few replication frames.
			if( !Actor || !AndroidPriority->bAndroidValid || !AndroidPriority->ActorClass || !AndroidPriority->ActorLevel )
				continue;
			UBOOL bAndroidJoinWarmup = Connection->TickCount <= 96;
			UBOOL bAndroidCoreActor = (Actor==Connection->Actor || Actor==Viewer || Actor==InViewer);
			if( AndroidPriority->bAndroidZoneInfo )
				continue;
			if( bAndroidJoinWarmup && !bAndroidCoreActor )
				continue;
			if( Connection->TickCount <= 20 || j < 4 )
			{
				FString Msg = FString::Printf(TEXT("UT99_ANDROID_V122_NET update-safe tick=%i j=%i count=%i channel=%i core=%i"), Connection->TickCount, j, ConsiderCount, Channel ? 1 : 0, bAndroidCoreActor ? 1 : 0);
				AndroidNetLogV115( *Msg );
			}
			// UT99_ANDROID_V115_NET_CHANNEL_SAFE:
			// v114 proved the priority sort was no longer the crash site.  The crash now
			// happens immediately after the sort, inside ServerTickClient(), while the
			// first joining client receives its initial relevancy update.  The most fragile
			// dereferences here are stale/existing UActorChannel pointers returned from the
			// per-connection ActorChannels map.  For the first join frames, do not touch
			// existing channel internals from this hot path; only open missing channels.
			UBOOL bAndroidHasExistingChannel = Channel != NULL;
			if( bAndroidHasExistingChannel && Connection->TickCount <= 96 )
			{
				if( j<4 )
				{
					FString Msg = FString::Printf(TEXT("UT99_ANDROID_V115_NET defer existing channel tick=%i j=%i count=%i"), Connection->TickCount, j, ConsiderCount);
					AndroidNetLogV115( *Msg );
				}
				continue;
			}
#endif
			TraceTime-=appSeconds();
#if defined(__ANDROID__) || defined(PLATFORM_ANDROID)
			UBOOL          CanSee      = bAndroidJoinWarmup ? 1 : ActorCanSee( Actor, InViewer, Viewer, Location );
#else
			UBOOL          CanSee      = ActorCanSee( Actor, InViewer, Viewer, Location );
#endif
			TraceTime+=appSeconds();
#if defined(__ANDROID__) || defined(PLATFORM_ANDROID)
			UBOOL RecentlyRelevant = 0;
			if( CanSee || RecentlyRelevant )
#else
			if( CanSee || (Channel && NetDriver->Time-Channel->RelevantTime<NetDriver->RelevantTimeout) )
#endif
			{
				// Find or create the channel for this actor.
#if defined(__ANDROID__) || defined(PLATFORM_ANDROID)
				AndroidPriority->ActorLevel->NumPV++;
#else
				Actor->GetLevel()->NumPV++;
#endif
#if defined(__ANDROID__) || defined(PLATFORM_ANDROID)
				if( !Connection->PackageMap )
					continue;
#endif
				if( !Channel && Connection->PackageMap->ObjectToIndex(AndroidPriority->ActorClass)!=INDEX_NONE )
				{
					// Create a new channel for this actor.
					Channel = (UActorChannel*)Connection->CreateChannel( CHTYPE_Actor, 1 );
					if( Channel )
						Channel->SetChannelActor( Actor );
				}
				if( Channel )
				{
#if defined(__ANDROID__) || defined(PLATFORM_ANDROID)
					if( Channel->Connection != Connection || Channel->Closing )
					{
						AndroidNetLogV115( TEXT("UT99_ANDROID_V115_NET skip channel: wrong connection or closing") );
						continue;
					}
#endif
					if( CanSee )
						Channel->RelevantTime = NetDriver->Time;
					if( Channel->IsNetReady(0) )
					{
						RepTime-=appSeconds();
						RepCount++;
						Channel->ReplicateActor();
						RepTime+=appSeconds();
						Updated++;
					}
				}
			}
			else if( Channel )
#if defined(__ANDROID__) || defined(PLATFORM_ANDROID)
			{
				// Do not close possibly stale existing channels during the Android join
				// warmup. Closing is an optimisation and can wait until the connection is
				// fully settled.
				if( Connection->TickCount > 96 )
					Channel->Close();
			}
#else
				Channel->Close();
#endif
		}
		unguard;
		Mark.Pop();
	}
	if( NetDriver->ProfileStats )
		debugf(TEXT("Cull=%01.4f (%03i) Trace=%01.4f Rep=%01.4f (%03i)"),CullTime*1000,CullCount,TraceTime*1000,RepTime*1000,RepCount);
	return Updated;
	unguard;
}

/*-----------------------------------------------------------------------------
	Network server tick.
-----------------------------------------------------------------------------*/

void ULevel::TickNetServer( FLOAT DeltaSeconds )
{
	guard(ULevel::TickNetServer);

#if defined(__ANDROID__) || defined(PLATFORM_ANDROID)
	AndroidV128LanTick( this );
#endif

	// Update all clients.
	clock(NetTickCycles);
	INT Updated=0;
	for( INT i=NetDriver->ClientConnections.Num()-1; i>=0; i-- )
		Updated += ServerTickClient( NetDriver->ClientConnections(i), DeltaSeconds );
	unclock(NetTickCycles);

	// Log message.
	if( (INT)(TimeSeconds-DeltaSeconds)!=(INT)(TimeSeconds) )
		debugf( NAME_Title, LocalizeProgress("RunningNet"), *GetLevelInfo()->Title, *URL.Map, NetDriver->ClientConnections.Num() );

	// Stats.
	if( Updated )
	{
		for( INT i=0; i<NetDriver->ClientConnections.Num(); i++ )
		{
			UNetConnection* Connection = NetDriver->ClientConnections(i);
			if( Connection->Actor && Connection->State==USOCK_Open )
			{
				if( Connection->UserFlags&1 )
				{
					// Send stats.
					INT NumActors=0;
					for( INT i=0; i<Actors.Num(); i++ )
						NumActors += Actors(i)!=NULL;
					FString Stats = FString::Printf
					(
						TEXT("r=%i cli=%i act=%03.1f (%i) net=%03.1f pv/c=%i rep/c=%i rpc/c=%i"),
						appRound(Engine->GetMaxTickRate()),
						NetDriver->ClientConnections.Num(),
						GSecondsPerCycle*1000*ActorTickCycles,
						NumActors,
						GSecondsPerCycle*1000*NetTickCycles,
						NumPV  /NetDriver->ClientConnections.Num(),
						NumReps/NetDriver->ClientConnections.Num(),
						NumRPC /NetDriver->ClientConnections.Num()
					);
					Connection->Actor->eventClientMessage( *Stats, NAME_None, 0 );
				}
				if( Connection->UserFlags&2 )
				{
					FString Stats = FString::Printf
					(
						TEXT("snd=%02.1f recv=%02.1f"),
						GSecondsPerCycle*1000*Connection->Driver->SendCycles,
						GSecondsPerCycle*1000*Connection->Driver->RecvCycles
					);
					Connection->Actor->eventClientMessage( *Stats, NAME_None, 0 );
				}
			}
		}
	}
	unguard;
}

/*-----------------------------------------------------------------------------
	Demo Recording tick.
-----------------------------------------------------------------------------*/

INT ULevel::TickDemoRecord( FLOAT DeltaSeconds )
{
	guard(ULevel::TickDemo);

	// All replicatable actors are assumed to be relevant for demo recording.
	UNetConnection* Connection = DemoRecDriver->ClientConnections(0);
	for( INT i=0; i<Actors.Num(); i++ )
	{
		AActor* Actor = Actors(i);
		UBOOL IsNetClient = (GetLevelInfo()->NetMode == NM_Client);
		if
		(	Actor
		&&	(Actor->RemoteRole!=ROLE_None || (IsNetClient && Actor->Role!=ROLE_None && Actor->Role != ROLE_Authority))
		&&  (i>=iFirstDynamicActor || Actor->IsA(AZoneInfo::StaticClass()))
		&&  (!Actor->bNetTemporary || Connection->SentTemporaries.FindItemIndex(Actor)==INDEX_NONE)
		&&  (Actor->bStatic || !Actor->GetClass()->GetDefaultActor()->bStatic))
		{
			// Create a new channel for this actor.
			UActorChannel* Channel = Connection->ActorChannels.FindRef( Actor );
			if( !Channel && Connection->PackageMap->ObjectToIndex(Actor->GetClass())!=INDEX_NONE )
			{
				// Check we haven't run out of actor channels.
				Channel = (UActorChannel*)Connection->CreateChannel( CHTYPE_Actor, 1 );
				check(Channel);
				Channel->SetChannelActor( Actor );
			}
			if( Channel )
			{
				// Send it out!
				check(!Channel->Closing);
				if( Channel->IsNetReady(0) )
				{
					Actor->bDemoRecording = 1;
					Actor->bClientDemoRecording = IsNetClient;
					if(IsNetClient)
						Exchange(Actor->RemoteRole, Actor->Role);
					Channel->ReplicateActor();
					if(IsNetClient)
						Exchange(Actor->RemoteRole, Actor->Role);
					Actor->bDemoRecording = 0;
					Actor->bClientDemoRecording = 0;
				}
			}
		}
	}
	return 1;
	unguard;
}
INT ULevel::TickDemoPlayback( FLOAT DeltaSeconds )
{
	guard(ULevel::TickDemoPlayback);
	if
	(	GetLevelInfo()->LevelAction==LEVACT_Connecting 
	&&	DemoRecDriver->ServerConnection->State!=USOCK_Pending )
	{
		GetLevelInfo()->LevelAction = LEVACT_None;
		Engine->SetProgress( TEXT(""), TEXT(""), 0.0 );
	} 
	if( DemoRecDriver->ServerConnection->State==USOCK_Closed )
	{
		// Demo stopped playing
		check(Engine->Client->Viewports.Num());
		Engine->SetClientTravel( Engine->Client->Viewports(0), TEXT("?entry"), 0, TRAVEL_Absolute );
	}
	return 1;
	unguard;
}

/*-----------------------------------------------------------------------------
	Main level timer tick handler.
-----------------------------------------------------------------------------*/

//
// Update the level after a variable amount of time, DeltaSeconds, has passed.
// All child actors are ticked after their owners have been ticked.
//
void ULevel::Tick( ELevelTick TickType, FLOAT DeltaSeconds )
{
	guard(ULevel::Tick);
	ALevelInfo* Info = GetLevelInfo();
	InitStats();
	FMemMark Mark(GMem);
	FMemMark EngineMark(GEngineMem);
	GInitRunaway();
	InTick=1;

	//Keep actor time profile FIXME TEMP!!!
	Info->AvgAITime = 0.95 * GetLevelInfo()->AvgAITime + 0.05 * 1000 * GSecondsPerCycle * ActorTickCycles;
	FLOAT ratio = GSecondsPerCycle * ActorTickCycles/DeltaSeconds;
	INT offset = (INT)(10 * ratio);
	if ( offset > 7 )
		offset = 7;
	else if ( offset < 0 )
		offset = 0;
	//debugf("ratio is %f, offset is %d",ratio,offset);
	Info->AIProfile[offset] += 1;

	// Update the net code and fetch all incoming packets.
	guard(UpdatePreNet);
	if( NetDriver )
	{
		NetDriver->TickDispatch( DeltaSeconds );
		if( NetDriver->ServerConnection )
			TickNetClient( DeltaSeconds );
	}
	unguard;

	// Fetch demo playback packets from demo file.
	guard(UpdatePreDemoRec);
	if( DemoRecDriver )
	{
		DemoRecDriver->TickDispatch( DeltaSeconds );
		if( DemoRecDriver->ServerConnection )
			TickDemoPlayback( DeltaSeconds );
	}
	unguard;

	// Update collision.
	guard(UpdateCollision);
	if( Hash )
		Hash->Tick();
	unguard;

	// Update time.
	guard(UpdateTime);
	DeltaSeconds *= Info->TimeDilation;
	TimeSeconds += DeltaSeconds;
	Info->TimeSeconds = TimeSeconds;
	UpdateTime(Info);
	if( Info->bPlayersOnly )
		TickType = LEVELTICK_ViewportsOnly;
	unguard;

	// Clamp time between 200 fps and 2.5 fps.
	DeltaSeconds = Clamp(DeltaSeconds,0.005f,0.40f);

	// If caller wants time update only, or we are paused, skip the rest.
	clock(ActorTickCycles);
	if
	(	(TickType!=LEVELTICK_TimeOnly)
	&&	Info->Pauser==TEXT("")
	&&	(!NetDriver || !NetDriver->ServerConnection || NetDriver->ServerConnection->State==USOCK_Open) )
	{
		// Tick all actors, owners before owned.
		guard(TickAllActors);
		NewlySpawned = NULL;
		INT Updated  = 0;
		for( INT iActor=iFirstDynamicActor; iActor<Actors.Num(); iActor++ )
			if( Actors( iActor ) )
				Updated += Actors( iActor )->Tick(DeltaSeconds,TickType);
		while( NewlySpawned && Updated )
		{
			FActorLink* Link = NewlySpawned;
			NewlySpawned     = NULL;
			Updated          = 0;
			for( Link; Link; Link=Link->Next )
				if( Link->Actor->bTicked!=(DWORD)Ticked )
					Updated += Link->Actor->Tick( DeltaSeconds, TickType );
		}
		unguard;
	}
	else if( Info->Pauser!=TEXT("") )
	{
		// Absorb input if paused.
		guard(AbsorbedPaused);
		for( INT iActor=iFirstDynamicActor; iActor<Actors.Num(); iActor++ )
		{
			APlayerPawn* PlayerPawn=Cast<APlayerPawn>(Actors(iActor));
			if( PlayerPawn && PlayerPawn->Player )
			{
				PlayerPawn->Player->ReadInput( DeltaSeconds );
				PlayerPawn->eventPlayerInput( DeltaSeconds );
				for( TFieldIterator<UFloatProperty> It(PlayerPawn->GetClass()); It; ++It )
					if( It->PropertyFlags & CPF_Input )
						*(FLOAT*)((BYTE*)PlayerPawn + It->Offset) = 0.f;
			}
			else if( Actors(iActor) && Actors(iActor)->bAlwaysTick )
				Actors(iActor)->Tick(DeltaSeconds,TickType);
		}
		unguard;
	}
	unclock(ActorTickCycles);

	// Update net server and flush networking.
	guard(UpdateNetServer);
	if( NetDriver )
	{
		if( !NetDriver->ServerConnection )
			TickNetServer( DeltaSeconds );
		NetDriver->TickFlush();
	}
	unguard;

	// Demo Recording.
	guard(UpdatePostDemoRec);
	if( DemoRecDriver )
	{
		if( !DemoRecDriver->ServerConnection )
			TickDemoRecord( DeltaSeconds );
		DemoRecDriver->TickFlush();
	}
	unguard;

	// Finish up.
	Ticked = !Ticked;
	InTick = 0;
	Mark.Pop();
	EngineMark.Pop();
	CleanupDestroyed( 0 );

	unguardf(( TEXT("(NetMode=%i)"), GetLevelInfo()->NetMode ));
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
