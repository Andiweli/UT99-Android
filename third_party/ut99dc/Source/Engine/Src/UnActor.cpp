/*=============================================================================
	UnActor.cpp: AActor implementation
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#include "EnginePrivate.h"
#include "UnNet.h"

/*-----------------------------------------------------------------------------
	AActor object implementations.
-----------------------------------------------------------------------------*/

IMPLEMENT_CLASS(AActor);
IMPLEMENT_CLASS(ALight);
IMPLEMENT_CLASS(AWeapon);
IMPLEMENT_CLASS(ALevelInfo);
IMPLEMENT_CLASS(AGameInfo);
IMPLEMENT_CLASS(ACamera);
IMPLEMENT_CLASS(AZoneInfo);
IMPLEMENT_CLASS(ASkyZoneInfo);
IMPLEMENT_CLASS(APathNode);
IMPLEMENT_CLASS(ANavigationPoint);
IMPLEMENT_CLASS(AScout);
IMPLEMENT_CLASS(AInterpolationPoint);
IMPLEMENT_CLASS(ADecoration);
IMPLEMENT_CLASS(AProjectile);
IMPLEMENT_CLASS(AWarpZoneInfo);
IMPLEMENT_CLASS(ATeleporter);
IMPLEMENT_CLASS(APlayerStart);
IMPLEMENT_CLASS(AKeypoint);
IMPLEMENT_CLASS(AInventory);
IMPLEMENT_CLASS(AInventorySpot);
IMPLEMENT_CLASS(ATriggers);
IMPLEMENT_CLASS(ATrigger);
IMPLEMENT_CLASS(ATriggerMarker);
IMPLEMENT_CLASS(AButtonMarker);
IMPLEMENT_CLASS(AWarpZoneMarker);
IMPLEMENT_CLASS(AHUD);
IMPLEMENT_CLASS(AMenu);
IMPLEMENT_CLASS(ASavedMove);
IMPLEMENT_CLASS(ACarcass);
IMPLEMENT_CLASS(ALiftCenter);
IMPLEMENT_CLASS(ALiftExit);
IMPLEMENT_CLASS(AInfo);
IMPLEMENT_CLASS(AReplicationInfo);
IMPLEMENT_CLASS(APlayerReplicationInfo);
IMPLEMENT_CLASS(AInternetInfo);
IMPLEMENT_CLASS(AStatLog);
IMPLEMENT_CLASS(AStatLogFile);
IMPLEMENT_CLASS(AGameReplicationInfo);
IMPLEMENT_CLASS(ULevelSummary);
IMPLEMENT_CLASS(Alocationid);
IMPLEMENT_CLASS(ADecal);
IMPLEMENT_CLASS(ASpawnNotify);

/*-----------------------------------------------------------------------------
	Replication.
-----------------------------------------------------------------------------*/

UBOOL NEQ(BYTE A,BYTE B,UPackageMap* Map) {return A!=B;}
UBOOL NEQ(INT A,INT B,UPackageMap* Map) {return A!=B;}
UBOOL NEQ(BITFIELD A,BITFIELD B,UPackageMap* Map) {return A!=B;}
UBOOL NEQ(FLOAT& A,FLOAT& B,UPackageMap* Map) {return *(INT*)&A!=*(INT*)&B;}
UBOOL NEQ(FVector& A,FVector& B,UPackageMap* Map) {return ((INT*)&A)[0]!=((INT*)&B)[0] || ((INT*)&A)[1]!=((INT*)&B)[1] || ((INT*)&A)[2]!=((INT*)&B)[2];}
UBOOL NEQ(FRotator& A,FRotator& B,UPackageMap* Map) {return A.Pitch!=B.Pitch || A.Yaw!=B.Yaw || A.Roll!=B.Roll;}
UBOOL NEQ(UObject* A,UObject* B,UPackageMap* Map) {return (Map->CanSerializeObject(A)?A:NULL)!=B;}
UBOOL NEQ(FName& A,FName B,UPackageMap* Map) {return *(INT*)&A!=*(INT*)&B;}
UBOOL NEQ(FColor& A,FColor& B,UPackageMap* Map) {return *(INT*)&A!=*(INT*)&B;}
UBOOL NEQ(FPlane& A,FPlane& B,UPackageMap* Map) {return
((INT*)&A)[0]!=((INT*)&B)[0] || ((INT*)&A)[1]!=((INT*)&B)[1] ||
((INT*)&A)[2]!=((INT*)&B)[2] || ((INT*)&A)[3]!=((INT*)&B)[3];}
UBOOL NEQ(FString A,FString B,UPackageMap* Map) {return A!=B;}

#define DOREP(c,v) \
	if( NEQ(v,((A##c*)Recent)->v,Map) ) \
	{ \
		static UProperty* sp##v = FindObjectChecked<UProperty>(A##c::StaticClass(),TEXT(#v)); \
		*Ptr++ = sp##v->RepIndex; \
	}
#define DOREPARRAY(c,v) \
	static UProperty* sp##v = FindObjectChecked<UProperty>(A##c::StaticClass(),TEXT(#v)); \
	for( INT i=0; i<ARRAY_COUNT(v); i++ ) \
		if( NEQ(v[i],((A##c*)Recent)->v[i],Map) ) \
			*Ptr++ = sp##v->RepIndex+i;

INT* AActor::GetOptimizedRepList( BYTE* Recent, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map )
{
	guard(AActor::GetOptimizedRepList);
	if( StaticClass()->ClassFlags & CLASS_NativeReplication )
	{
		if( Role==ROLE_Authority )
		{
			DOREP(Actor,Owner);
			DOREP(Actor,Role);
			DOREP(Actor,RemoteRole);
			DOREP(Actor,bCollideActors);
			DOREP(Actor,bCollideWorld);
			DOREP(Actor,LightType);
			DOREP(Actor,bHidden);
			DOREP(Actor,bOnlyOwnerSee);
			DOREP(Actor,Texture);
			DOREP(Actor,DrawScale);
			DOREP(Actor,PrePivot);
			DOREP(Actor,DrawType);
			DOREP(Actor,AmbientGlow);
			DOREP(Actor,Fatness);
			DOREP(Actor,ScaleGlow);
			DOREP(Actor,bUnlit);
			DOREP(Actor,Style);
			if( bNetOwner )
			{
				DOREP(Actor,bNetOwner);
				DOREP(Actor,Inventory);
			}
			if( bReplicateInstigator && RemoteRole>=ROLE_SimulatedProxy )
			{
				DOREP(Actor,Instigator);
			}
			if  ( !bNetOwner || !bClientAnim )
			{
				DOREP(Actor,AmbientSound);
			}

			if( (AmbientSound!=NULL) && (!bNetOwner || !bClientAnim) )
			{
				DOREP(Actor,SoundRadius);
				DOREP(Actor,SoundVolume);
				DOREP(Actor,SoundPitch);
			}
			if( bCollideActors || bCollideWorld )
			{
				DOREP(Actor,bProjTarget);
				DOREP(Actor,bBlockActors);
				DOREP(Actor,bBlockPlayers);
				DOREP(Actor,CollisionRadius);
				DOREP(Actor,CollisionHeight);
			}
			if( !bCarriedItem && (bNetInitial || bSimulatedPawn || RemoteRole<ROLE_SimulatedProxy) )
			{
				DOREP(Actor,Location);
			}
			if( !bCarriedItem && (DrawType==DT_Mesh || DrawType==DT_Brush) && (bNetInitial || bSimulatedPawn || RemoteRole<ROLE_SimulatedProxy) )
			{
				DOREP(Actor,Rotation);
			}
			if( DrawType==DT_Mesh )
			{
				DOREP(Actor,Mesh);
				DOREP(Actor,bMeshEnviroMap);
				DOREP(Actor,Skin);
				DOREPARRAY(Actor,MultiSkins);
				if( ((RemoteRole<=ROLE_SimulatedProxy) && (!bNetOwner || !bClientAnim)) || bDemoRecording )
				{
					DOREP(Actor,AnimSequence);
					DOREP(Actor,SimAnim);
					DOREP(Actor,AnimMinRate);
					DOREP(Actor,bAnimNotify);
				}
			}
			else if( DrawType==DT_Sprite )
			{
				if( !bHidden && (!bOnlyOwnerSee || bNetOwner) )
				{
					DOREP(Actor,Sprite);
				}
			}
			if( DrawType==DT_Brush )
			{
				DOREP(Actor,Brush);
			}
			if( LightType!=LT_None )
			{
				DOREP(Actor,LightEffect);
				DOREP(Actor,LightBrightness);
				DOREP(Actor,LightHue);
				DOREP(Actor,LightSaturation);
				DOREP(Actor,LightRadius);
				DOREP(Actor,LightPeriod);
				DOREP(Actor,LightPhase);
				DOREP(Actor,VolumeBrightness);
				DOREP(Actor,VolumeRadius);
				DOREP(Actor,bSpecialLit);
			}
			if( RemoteRole==ROLE_SimulatedProxy )
			{
				DOREP(Actor,Base);
				if( bNetInitial )
				{
					if( !bSimulatedPawn )
					{
						DOREP(Actor,Physics);
						DOREP(Actor,Acceleration);
						DOREP(Actor,bBounce);
					}
					if( Physics==PHYS_Rotating )
					{
						DOREP(Actor,bFixedRotationDir);
						DOREP(Actor,bRotateToDesired);
						DOREP(Actor,RotationRate);
						DOREP(Actor,DesiredRotation);
					}
				}
			}
			else if ( bSimFall )
			{
				DOREP(Actor,Physics);
				DOREP(Actor,Acceleration);
				DOREP(Actor,bBounce);
			}

			if( bSimFall || bIsMover || (RemoteRole==ROLE_SimulatedProxy && (bNetInitial || bSimulatedPawn)) )
			{
				DOREP(Actor,Velocity);
			}
		}
	}
	return Ptr;
	unguard;
}
INT* APawn::GetOptimizedRepList( BYTE* Recent, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map )
{
	guard(APawn::GetOptimizedRepList);
	Ptr = Super::GetOptimizedRepList(Recent,Retire,Ptr,Map);
	if( StaticClass()->ClassFlags & CLASS_NativeReplication )
	{
		if( Role==ROLE_Authority )
		{
			DOREP(Pawn,Weapon);
			DOREP(Pawn,PlayerReplicationInfo);
			DOREP(Pawn,Health);
			DOREP(Pawn,bCanFly);
			if( bNetOwner )
			{
				 DOREP(Pawn,bIsPlayer);
				 DOREP(Pawn,carriedDecoration);
				 DOREP(Pawn,SelectedItem);
				 DOREP(Pawn,GroundSpeed);
				 DOREP(Pawn,WaterSpeed);
				 DOREP(Pawn,AirSpeed);
				 DOREP(Pawn,AccelRate);
				 DOREP(Pawn,JumpZ);
				 DOREP(Pawn,AirControl);
				 DOREP(Pawn,bBehindView);
				 DOREP(Pawn,MoveTarget);
			}
		}
		if( (bNetOwner && bIsPlayer && bNetInitial) || bDemoRecording )
		{
			DOREP(Pawn,ViewRotation);
		}
		if( bDemoRecording )
		{
			DOREP(Pawn,EyeHeight);
		}
	}
	return Ptr;
	unguard;
}
INT* APlayerPawn::GetOptimizedRepList( BYTE* Recent, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map )
{
	guard(APlayerPawn::GetOptimizedRepList);
	Ptr = Super::GetOptimizedRepList(Recent,Retire,Ptr,Map);
	if( StaticClass()->ClassFlags & CLASS_NativeReplication )
	{
		if( Role==ROLE_Authority )
		{
			if( bNetOwner )
			{
				DOREP(PlayerPawn,ViewTarget);
				DOREP(PlayerPawn,ScoringType);
				DOREP(PlayerPawn,HUDType);
				DOREP(PlayerPawn,GameReplicationInfo);
				DOREP(PlayerPawn,bFixedCamera);
				DOREP(PlayerPawn,bNeverAutoSwitch);
				DOREP(PlayerPawn,bCheatsEnabled);
				{
					FRotator TempTargetViewRotation = TargetViewRotation;
					FRotator TempRecentTargetViewRotation = ((APlayerPawn*)Recent)->TargetViewRotation;
					if( NEQ(TempTargetViewRotation,TempRecentTargetViewRotation,Map) )
					{
						static UProperty* spTargetViewRotation = FindObjectChecked<UProperty>(APlayerPawn::StaticClass(),TEXT("TargetViewRotation"));
						*Ptr++ = spTargetViewRotation->RepIndex;
					}
				}
				DOREP(PlayerPawn,TargetEyeHeight);
				DOREP(PlayerPawn,TargetWeaponViewOffset);
			}
			if( bDemoRecording )
			{
				DOREP(PlayerPawn,DemoViewPitch);
				DOREP(PlayerPawn,DemoViewYaw);
			}
		}
		else
		{
			DOREP(PlayerPawn,Password);
			DOREP(PlayerPawn,bReadyToPlay);
		}
	}
	return Ptr;
	unguard;
}
INT* AMover::GetOptimizedRepList( BYTE* Recent, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map )
{
	guard(AMover::GetOptimizedRepList);
	Ptr = Super::GetOptimizedRepList(Recent,Retire,Ptr,Map);
	if( StaticClass()->ClassFlags & CLASS_NativeReplication )
	{
		if( Role==ROLE_Authority )
		{
			DOREP(Mover,SimOldPos);
			DOREP(Mover,SimOldRotPitch);
			DOREP(Mover,SimOldRotYaw);
			DOREP(Mover,SimOldRotRoll);
			DOREP(Mover,SimInterpolate);
			DOREP(Mover,RealPosition);
			DOREP(Mover,RealRotation);
		}
	}
	return Ptr;
	unguard;
}
INT* AZoneInfo::GetOptimizedRepList( BYTE* Recent, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map )
{
	guard(AZoneInfo::GetOptimizedRepList);
	Ptr = Super::GetOptimizedRepList(Recent,Retire,Ptr,Map);
	if( StaticClass()->ClassFlags & CLASS_NativeReplication )
	{
		if( Role==ROLE_Authority )
		{
			DOREP(ZoneInfo,ZoneGravity);
			DOREP(ZoneInfo,ZoneVelocity);
			DOREP(ZoneInfo,AmbientBrightness);
			DOREP(ZoneInfo,AmbientHue);
			DOREP(ZoneInfo,AmbientSaturation);
			DOREP(ZoneInfo,TexUPanSpeed);
			DOREP(ZoneInfo,TexVPanSpeed);
			DOREP(ZoneInfo,bReverbZone);
			DOREP(ZoneInfo,FogColor);
		}
	}
	return Ptr;
	unguard;
}
INT* APlayerReplicationInfo::GetOptimizedRepList( BYTE* Recent, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map )
{
	guard(APlayerReplicationInfo::GetOptimizedRepList);
	if ( bNetInitial )
		Ptr = Super::GetOptimizedRepList(Recent,Retire,Ptr,Map);
	if( StaticClass()->ClassFlags & CLASS_NativeReplication )
	{
		if( Role==ROLE_Authority )
		{
			DOREP(PlayerReplicationInfo,PlayerName);
			DOREP(PlayerReplicationInfo,OldName);
			DOREP(PlayerReplicationInfo,PlayerID);
			DOREP(PlayerReplicationInfo,TeamName);
			DOREP(PlayerReplicationInfo,Team);
			DOREP(PlayerReplicationInfo,TeamID);
			DOREP(PlayerReplicationInfo,Score);
			DOREP(PlayerReplicationInfo,Deaths);
			DOREP(PlayerReplicationInfo,VoiceType);
			DOREP(PlayerReplicationInfo,HasFlag);
			DOREP(PlayerReplicationInfo,Ping);
			DOREP(PlayerReplicationInfo,PacketLoss);
			DOREP(PlayerReplicationInfo,bIsFemale);
			DOREP(PlayerReplicationInfo,bIsABot);
			DOREP(PlayerReplicationInfo,bFeigningDeath);
			DOREP(PlayerReplicationInfo,bIsSpectator);
			DOREP(PlayerReplicationInfo,bWaitingPlayer);
			DOREP(PlayerReplicationInfo,bAdmin);
			DOREP(PlayerReplicationInfo,TalkTexture);
			DOREP(PlayerReplicationInfo,PlayerZone);
			DOREP(PlayerReplicationInfo,PlayerLocation);
			DOREP(PlayerReplicationInfo,StartTime);
		}
	}
	return Ptr;
	unguard;
}
INT* AGameReplicationInfo::GetOptimizedRepList( BYTE* Recent, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map )
{
	guard(AGameReplicationInfo::GetOptimizedRepList);
	Ptr = Super::GetOptimizedRepList(Recent,Retire,Ptr,Map);
	if( StaticClass()->ClassFlags & CLASS_NativeReplication )
	{
		if( Role==ROLE_Authority )
		{
			DOREP(GameReplicationInfo,GameName);
			DOREP(GameReplicationInfo,GameClass);
			DOREP(GameReplicationInfo,bTeamGame);
			DOREP(GameReplicationInfo,ServerName);
			DOREP(GameReplicationInfo,ShortName);
			DOREP(GameReplicationInfo,AdminName);
			DOREP(GameReplicationInfo,AdminEmail);
			DOREP(GameReplicationInfo,Region);
			DOREP(GameReplicationInfo,MOTDLine1);
			DOREP(GameReplicationInfo,MOTDLine2);
			DOREP(GameReplicationInfo,MOTDLine3);
			DOREP(GameReplicationInfo,MOTDLine4);
			DOREP(GameReplicationInfo,RemainingMinute);
			DOREP(GameReplicationInfo,NumPlayers);
			DOREP(GameReplicationInfo,bStopCountDown);
			DOREP(GameReplicationInfo,GameEndedComments);
			if ( bNetInitial )
			{
				DOREP(GameReplicationInfo,RemainingTime);
				DOREP(GameReplicationInfo,ElapsedTime);
			}
		}
	}
	return Ptr;
	unguard;
}
INT* AInventory::GetOptimizedRepList( BYTE* Recent, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map )
{
	guard(AInventory::GetOptimizedRepList);

	if ( bAlwaysRelevant && !bNetInitial ) // only inventory pickups should be like this
	{
			DOREP(Actor,bHidden);
			return Ptr;
	}
	Ptr = Super::GetOptimizedRepList(Recent,Retire,Ptr,Map);
	if( StaticClass()->ClassFlags & CLASS_NativeReplication )
	{
		if( Role==ROLE_Authority )
		{
			if( bNetOwner )
			{
				DOREP(Inventory,bIsAnArmor);
				DOREP(Inventory,Charge);
				DOREP(Inventory,bActivatable);
				DOREP(Inventory,bActive);
				DOREP(Inventory,PlayerViewOffset);
				DOREP(Inventory,PlayerViewMesh);
				DOREP(Inventory,PlayerViewScale);
			}
			else if ( (RemoteRole == ROLE_SimulatedProxy) && AmbientSound )
				DOREP(Actor,Location);
			DOREP(Inventory,FlashCount);
			DOREP(Inventory,bSteadyFlash3rd);
			DOREP(Inventory,ThirdPersonMesh);
			DOREP(Inventory,ThirdPersonScale);
		}
	}
	return Ptr;
	unguard;
}
UBOOL AInventory::ShouldDoScriptReplication()
{
	return !bAlwaysRelevant || bNetInitial;
}

/*-----------------------------------------------------------------------------
	AActor networking implementation.
-----------------------------------------------------------------------------*/

//
// Static variables for networking.
//
static FVector   SavedLocation;
static FRotator  SavedRotation;
static AActor*   SavedBase;
static DWORD     SavedCollision;
static FLOAT	 SavedRadius;
static FLOAT     SavedHeight;
static FPlane    SavedSimAnim;
static FVector	 SavedSimInterpolate;

//
// Skins.
//
UTexture* AActor::GetSkin( INT Index )
{
	if( Index < ARRAY_COUNT(MultiSkins) )
		return MultiSkins[Index];
	return NULL;
}

//
// Net priority.
//
FLOAT AActor::GetNetPriority( AActor* Sent, FLOAT Time, FLOAT Lag )
{
	guardSlow(AActor::GetNetPriority);
	return NetPriority * Time;
	unguardSlow;
}

//
// Always called immediately before properties are received from the remote.
//
void AActor::PreNetReceive()
{
	guard(AActor::PreNetReceive);
	SavedLocation   = Location;
	SavedRotation   = Rotation;
	SavedBase       = Base;
	SavedCollision  = bCollideActors;
	SavedRadius		= CollisionRadius;
	SavedHeight     = CollisionHeight;
	SavedSimAnim    = SimAnim;
	if( IsA(AMover::StaticClass()) )
		SavedSimInterpolate = ((AMover*)this)->SimInterpolate;
	if( bCollideActors )
		GetLevel()->Hash->RemoveActor( this );
	unguard;
}

//
// Always called immediately after properties are received from the remote.
//
void AActor::PostNetReceive()
{
	guard(AActor::PostNetReceive);
	Exchange ( Location,        SavedLocation  );
	Exchange ( Rotation,        SavedRotation  );
	Exchange ( Base,            SavedBase      );
	ExchangeB( bCollideActors,  SavedCollision );
	Exchange ( CollisionRadius, SavedRadius    );
	Exchange ( CollisionHeight, SavedHeight    );
	if( bCollideActors )
		GetLevel()->Hash->AddActor( this );
	if( IsA(AMover::StaticClass()) )
	{
		AMover* Mover = Cast<AMover>( this );
		if( SavedSimInterpolate != Mover->SimInterpolate )
		{
			Mover->OldPos = Mover->SimOldPos;
			Mover->OldRot.Yaw = Mover->SimOldRotYaw;
			Mover->OldRot.Pitch = Mover->SimOldRotPitch;
			Mover->OldRot.Roll = Mover->SimOldRotRoll;
			Mover->PhysAlpha = Mover->SimInterpolate.X * 0.01;
			Mover->PhysRate = Mover->SimInterpolate.Y * 0.01;
			INT keynums = (INT) Mover->SimInterpolate.Z;
			Mover->KeyNum = keynums & 255;
			Mover->PrevKeyNum = keynums >> 8;
			Mover->setPhysics(PHYS_MovingBrush);
			Mover->bInterpolating   = true;
			/*FRotator ApproxRot = Mover->BaseRot + Mover->KeyRot[Mover->KeyNum];
			ApproxRot.Roll = ApproxRot.Roll & 65280;
			ApproxRot.Pitch = ApproxRot.Pitch & 65280;
			ApproxRot.Yaw = ApproxRot.Yaw & 65280;
			if ( ApproxRot == Mover->OldRot )
				Mover->OldRot = Mover->BaseRot + Mover->KeyRot[Mover->KeyNum];*/
		}
	}
	if( IsA(APlayerPawn::StaticClass()) && GetLevel()->DemoRecDriver && GetLevel()->DemoRecDriver->ServerConnection )
	{
		APlayerPawn* PlayerPawn = Cast<APlayerPawn>( this );
		PlayerPawn->ViewRotation.Pitch = PlayerPawn->DemoViewPitch;
		PlayerPawn->ViewRotation.Yaw = PlayerPawn->DemoViewYaw;
	}
	if( SimAnim != SavedSimAnim )
	{
		AnimFrame = SimAnim.X * 0.0001;
		AnimRate  = SimAnim.Y * 0.0002;
		TweenRate = SimAnim.Z * 0.001;
		AnimLast  = SimAnim.W * 0.0001;
		if( AnimLast < 0 )
		{
			AnimLast *= -1;
			bAnimLoop = 1;
			if( IsA(APawn::StaticClass()) && AnimMinRate<0.5 )
				AnimMinRate = 0.5;
		}
		else bAnimLoop = 0;
	}
	if( Location!=SavedLocation )
	{
		if( IsA(APawn::StaticClass()) && (Role == ROLE_SimulatedProxy) 
			&& !Velocity.IsNearlyZero() && ((Location - SavedLocation).SizeSquared() < 10000) )
		{
			// smooth out movement of other players to account for frame rate induced jitter
			// look at whether location is a reasonable approximation already (<100 error)
			// if so only partially correct
			
			FLOAT StartError = (Location - SavedLocation).SizeSquared();
			if ( StartError > 1600 )
			{
				// if error > 40 try moving smoothly closer
				moveSmooth(0.35 * (SavedLocation - Location));
				// if error not reduced enough, set to new loc
				if ( (Location - SavedLocation).SizeSquared() > 0.75 * StartError )
					GetLevel()->FarMoveActor( this, Location + 0.5 * (SavedLocation - Location), 0, 1 );
			}
			else
				GetLevel()->FarMoveActor( this, Location + 0.15 * (SavedLocation - Location), 0, 1 );
		}
		else
			GetLevel()->FarMoveActor( this, SavedLocation, 0, 1 );
	}
	if( Rotation!=SavedRotation )
	{
		FCheckResult Hit;
		GetLevel()->MoveActor( this, FVector(0,0,0), SavedRotation, Hit, 0, 0, 0, 1 );
	}
	if( CollisionRadius!=SavedRadius || CollisionHeight!=SavedHeight )
	{
		SetCollisionSize( SavedRadius, SavedHeight );
	}
	if( bCollideActors!=SavedCollision )
	{
		SetCollision( SavedCollision, bBlockActors, bBlockPlayers );
	}
	if( Base!=SavedBase )
	{
		// Base changed.
		eventBump( SavedBase );
		if( SavedBase )
			SavedBase->eventBump( this );
		SetBase( SavedBase );
	}
	bJustTeleported = 0;
	if (IsA(APlayerReplicationInfo::StaticClass()) && Level->NetMode == NM_Client)
	{
		APlayerReplicationInfo* PRI = Cast<APlayerReplicationInfo>( this );
		if( GetLevel()->NetDriver &&
			GetLevel()->Engine->Client->Viewports(0)->Actor &&
			GetLevel()->Engine->Client->Viewports(0)->Actor->PlayerReplicationInfo == this
		)
			PRI->Ping -= (INT) ((GetLevel()->NetDriver->ServerConnection->AverageFrameTime * 1000) / 2);
		if( PRI->Ping < 0 )
			PRI->Ping = 0;
	}
	unguard;
}

/*-----------------------------------------------------------------------------
	APlayerPawn implementation.
-----------------------------------------------------------------------------*/

//
// Set the player.
//
void APlayerPawn::SetPlayer( UPlayer* InPlayer )
{
	guard(APlayerPawn::SetPlayer);
	check(InPlayer!=NULL);

	// Detach old player.
	if( InPlayer->Actor )
	{
		InPlayer->Actor->Player = NULL;
		InPlayer->Actor = NULL;
	}

	// Set the viewport.
	Player = InPlayer;
	InPlayer->Actor = this;

	// Send possess message to script.
	eventPossess();

	// Debug message.
	debugf( NAME_Log, TEXT("Possessed PlayerPawn: %s"), GetFullName() );

	unguard;
}

bool APlayerPawn::ClearScreen()
{
	return false;
}
bool APlayerPawn::RecomputeLighting()
{
	return false;
}
bool APlayerPawn::CanSee( const AActor* Actor )
{
	return false;
}
INT APlayerPawn::GetViewZone( INT iViewZone, const UModel* Model )
{
	return iViewZone;
}
bool APlayerPawn::IsZoneVisible( INT iZone )
{
	return true;
}
bool APlayerPawn::IsSurfVisible( const FBspNode* Node, INT iZone, const FBspSurf* Poly )
{
	return true;
}
bool APlayerPawn::IsActorVisible( const AActor* Actor )
{
	return true;
}

/*-----------------------------------------------------------------------------
	AZoneInfo.
-----------------------------------------------------------------------------*/

void AZoneInfo::PostEditChange()
{
	guard(AZoneInfo::PostEditChange);
	Super::PostEditChange();
	if( GIsEditor )
		GCache.Flush();
	unguard;
}

/*-----------------------------------------------------------------------------
	AActor.
-----------------------------------------------------------------------------*/

void AActor::Destroy()
{
	guard(AActor::Destroy);
	if( RenderInterface )
	{
		RenderInterface->RemoveFromRoot();
		RenderInterface->ConditionalDestroy();
		RenderInterface = NULL;
	}
	UObject::Destroy();
	unguard;
}

void AActor::PostLoad()
{
	guard(AActor::PostLoad);
	Super::PostLoad();
	if( GetClass()->ClassFlags & CLASS_Localized )
		LoadLocalized();
	if( Brush )
		Brush->SetFlags( RF_Transactional );
	if( Brush && Brush->Polys )
		Brush->Polys->SetFlags( RF_Transactional );
	unguard;
}

void AActor::ProcessEvent( UFunction* Function, void* Parms, void* Result )
{
	guardSlow(AActor::ProcessEvent);
	if( Level->bBegunPlay )
		Super::ProcessEvent( Function, Parms, Result );
	unguardSlow;
}

void AActor::PostEditChange()
{
	guard(AActor::PostEditChange);
	Super::PostEditChange();
	if( GIsEditor )
		bLightChanged = 1;
	unguard;
}

//
// Set the actor's collision properties.
//
void AActor::SetCollision
(
	UBOOL NewCollideActors,
	UBOOL NewBlockActors,
	UBOOL NewBlockPlayers
)
{
	guard(AActor::SetCollision);

	// Untouch this actor.
	if( bCollideActors && GetLevel()->Hash )
		GetLevel()->Hash->RemoveActor( this );

	// Set properties.
	bCollideActors = NewCollideActors;
	bBlockActors   = NewBlockActors;
	bBlockPlayers  = NewBlockPlayers;

	// Touch this actor.
	if( bCollideActors && GetLevel()->Hash )
		GetLevel()->Hash->AddActor( this );

	unguard;
}

//
// Set collision size.
//
void AActor::SetCollisionSize( FLOAT NewRadius, FLOAT NewHeight )
{
	guard(AActor::SetCollisionSize);

	// Untouch this actor.
	if( bCollideActors && GetLevel()->Hash )
		GetLevel()->Hash->RemoveActor( this );

	// Set properties.
	CollisionRadius = NewRadius;
	CollisionHeight = NewHeight;

	// Touch this actor.
	if( bCollideActors && GetLevel()->Hash )
		GetLevel()->Hash->AddActor( this );

	unguard;
}

//
// Return whether this actor overlaps another.
//
UBOOL AActor::IsOverlapping( const AActor* Other ) const
{
	guardSlow(AActor::IsOverlapping);
	checkSlow(Other!=NULL);

	if( !IsBrush() && !Other->IsBrush() && Other!=Level )
	{
		// See if cylinder actors are overlapping.
		return
			Square(Location.X      - Other->Location.X)
		+	Square(Location.Y      - Other->Location.Y)
		<	Square(CollisionRadius + Other->CollisionRadius) 
		&&	Square(Location.Z      - Other->Location.Z)
		<	Square(CollisionHeight + Other->CollisionHeight);
	}
	else
	{
		// We cannot detect whether these actors are overlapping so we say they aren't.
		return 0;
	}
	unguardSlow;
}

/*-----------------------------------------------------------------------------
	Actor touch minions.
-----------------------------------------------------------------------------*/

static UBOOL TouchTo( AActor* Actor, AActor* Other )
{
	guard(TouchTo);
	check(Actor);
	check(Other);
	check(Actor!=Other);

	INT Available=-1;
	for( INT i=0; i<ARRAY_COUNT(Actor->Touching); i++ )
	{
		if( Actor->Touching[i] == NULL )
		{
			// Found an available slot.
			Available = i;
		}
		else if( Actor->Touching[i] == Other )
		{
			// Already touching.
			return 1;
		}
	}
	if( Available == -1 )
	{
		// Try to prune touches.
		for( INT i=0; i<ARRAY_COUNT(Actor->Touching); i++ )
		{
			check(Actor->Touching[i]->IsValid());
			if( Actor->Touching[i]->Physics == PHYS_None )
			{
				Actor->EndTouch( Actor->Touching[i], 0 );
				Available = i;
			}
		}
		if ( (Available == -1) && Other->IsA(APawn::StaticClass()) )
		{
			// try to prune in favor of 1. players, 2. other pawns
			for( INT i=0; i<ARRAY_COUNT(Actor->Touching); i++ )
			{
				if( !Actor->Touching[i]->IsA(APawn::StaticClass()) )
				{
					Actor->EndTouch( Actor->Touching[i], 0 );
					Available = i;
					break;
				}
			}
			if ( (Available == -1) && ((APawn *)Other)->bIsPlayer )
				for( INT i=0; i<ARRAY_COUNT(Actor->Touching); i++ )
				{
					if( !Actor->Touching[i]->IsA(APawn::StaticClass()) || !((APawn *)Actor->Touching[i])->bIsPlayer )
					{
						Actor->EndTouch( Actor->Touching[i], 0 );
						Available = i;
						break;
					}
				}
		}
	}

	if( Available >= 0 )
	{
		// Make Actor touch TouchActor.
		Actor->Touching[Available] = Other;
		Actor->eventTouch( Other );

		// See if first actor did something that caused an UnTouch.
		if( Actor->Touching[Available] != Other )
			return 0;
	}

	return 1;
	unguard;
}

//
// Note that TouchActor has begun touching Actor.
//
// If an actor's touch list overflows, neither actor receives the
// touch messages, as if they are not touching.
//
// This routine is reflexive.
//
// Handles the case of the first-notified actor changing its touch status.
//
void AActor::BeginTouch( AActor* Other )
{
	guard(AActor::BeginTouch);

	// Perform reflective touch.
	if( TouchTo( this, Other ) )
		TouchTo( Other, this );

	unguard;
}

//
// Note that TouchActor is no longer touching Actor.
//
// If NoNotifyActor is specified, Actor is not notified but
// TouchActor is (this happens during actor destruction).
//
void AActor::EndTouch( AActor* Other, UBOOL NoNotifySelf )
{
	guard(AActor::EndTouch);
	check(Other!=this);

	// Notify Actor.
	for( int i=0; i<ARRAY_COUNT(Touching); i++ )
	{
		if( Touching[i] == Other )
		{
			Touching[i] = NULL;
			if( !NoNotifySelf )
				eventUnTouch( Other );
			break;
		}
	}

	// Notify TouchActor.
	for( INT i=0; i<ARRAY_COUNT(Other->Touching); i++ )
	{
		if( Other->Touching[i] == this )
		{
			Other->Touching[i] = NULL;
			Other->eventUnTouch( this );
			break;
		}
	}
	unguard;
}

/*-----------------------------------------------------------------------------
	AActor member functions.
-----------------------------------------------------------------------------*/

//
// Destroy the actor.
//
void AActor::Serialize( FArchive& Ar )
{
	guard(AActor::Serialize);
	Super::Serialize( Ar );
	unguard;
}

/*-----------------------------------------------------------------------------
	Relations.
-----------------------------------------------------------------------------*/

//
// Change the actor's owner.
//
void AActor::SetOwner( AActor *NewOwner )
{
	guard(AActor::SetOwner);

	// Sets this actor's parent to the specified actor.
	if( Owner != NULL )
		Owner->eventLostChild( this );

	Owner = NewOwner;

	if( Owner != NULL )
		Owner->eventGainedChild( this );

	unguard;
}

//
// Change the actor's base.
//
void AActor::SetBase( AActor* NewBase, int bNotifyActor )
{
	guard(AActor::SetBase);
	//debugf("SetBase %s -> %s",GetName(),NewBase ? NewBase->GetName() : TEXT("NULL"));

	// Verify no recursion.
	for( AActor* Loop=NewBase; Loop!=NULL; Loop=Loop->Base )
		if ( Loop == this ) 
			return;

	if( NewBase != Base )
	{
		// Notify old base, unless it's the level.
		if( Base && Base!=Level )
		{
			Base->StandingCount--;
			Base->eventDetach( this );
		}

		// Set base.
		Base = NewBase;

		// Notify new base, unless it's the level.
		if( Base && Base!=Level )
		{
			Base->StandingCount++;
			Base->eventAttach( this );
		}

		// Notify this actor of his new floor.
		if ( bNotifyActor )
			eventBaseChange();
	}
	unguard;
}

/*-----------------------------------------------------------------------------
	The end.
-----------------------------------------------------------------------------*/
