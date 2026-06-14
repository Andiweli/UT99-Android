/*=============================================================================
	UnPhysic.cpp: Actor physics implementation

	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Steven Polge 3/97
=============================================================================*/

#include "EnginePrivate.h"

void AActor::execMoveSmooth( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execMoveSmooth);

	P_GET_VECTOR(Delta);
	P_FINISH;

	bJustTeleported = 0;
	int didHit = moveSmooth(Delta);

	*(DWORD*)Result = didHit;
	unguardexecSlow;
}

void AActor::execSetPhysics( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execSetPhysics);

	P_GET_BYTE(NewPhysics);
	P_FINISH;

	setPhysics(NewPhysics);

	unguardSlow;
}

void AActor::execAutonomousPhysics( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execAutonomousPhysics);

	P_GET_FLOAT(DeltaSeconds);
	P_FINISH;

	// round acceleration to be consistent with replicated acceleration
	Acceleration.X = 0.1 * int(10 * Acceleration.X);
	Acceleration.Y = 0.1 * int(10 * Acceleration.Y);
	Acceleration.Z = 0.1 * int(10 * Acceleration.Z);

	// Perform physics.
	if( Physics!=PHYS_None )
		performPhysics( DeltaSeconds );

	unguardSlow;
}

//======================================================================================

int AActor::moveSmooth(FVector Delta)
{
	guard(AActor::moveSmooth);

	FCheckResult Hit(1.0);
	int didHit = GetLevel()->MoveActor( this, Delta, Rotation, Hit );
	if (Hit.Time < 1.0)
	{
		FVector Adjusted = (Delta - Hit.Normal * (Delta | Hit.Normal)) * (1.0 - Hit.Time);
		if( (Delta | Adjusted) >= 0 )
		{
			FVector OldHitNormal = Hit.Normal;
			FVector DesiredDir = Delta.SafeNormal();
			GetLevel()->MoveActor(this, Adjusted, Rotation, Hit);
			if (Hit.Time < 1.0)
			{
				eventHitWall(Hit.Normal, Hit.Actor);
				TwoWallAdjust(DesiredDir, Adjusted, Hit.Normal, OldHitNormal, Hit.Time);
				GetLevel()->MoveActor(this, Adjusted, Rotation, Hit);
			}
		}
	}

	return didHit;
	unguard;
}

void AActor::FindBase()
{
	guard(AActor::findBase);

	FCheckResult Hit(1.0);
	GetLevel()->SingleLineCheck( Hit, this, Location + FVector(0,0,-8), Location, TRACE_AllColliding, GetCylinderExtent() );
	if (Base != Hit.Actor)
		SetBase(Hit.Actor);

	unguard;
}

void AActor::setPhysics(BYTE NewPhysics, AActor *NewFloor)
{
	guard(AActor::setPhysics);

	if (Physics == NewPhysics)
		return;
	Physics = NewPhysics;

	if ((Physics == PHYS_Walking) || (Physics == PHYS_None) || (Physics == PHYS_Rolling) 
			|| (Physics == PHYS_Rotating) || (Physics == PHYS_Spider) )
	{
		if (NewFloor != NULL)
		{
			if (Base != NewFloor)
				SetBase(NewFloor);
		}
		else
			FindBase();
	}
	else if (Base != NULL)
		SetBase(NULL);

	if ( (Physics == PHYS_None) || (Physics == PHYS_Rotating) )
	{
		Velocity = FVector(0,0,0);
		Acceleration = FVector(0,0,0);
	}
	unguard;
}

void AActor::performPhysics(FLOAT DeltaSeconds)
{
	guard(AActor::performPhysics);

	FVector OldVelocity = Velocity;

	// change position
	switch (Physics)
	{
		case PHYS_Projectile: physProjectile(DeltaSeconds, 0); break;
		case PHYS_Falling: physFalling(DeltaSeconds, 0); break;
		case PHYS_Rotating: break;
		case PHYS_Interpolating: 
			{
				OldLocation = Location;
				physPathing(DeltaSeconds); 
				Velocity = (Location - OldLocation)/DeltaSeconds;
				break;
			}
		case PHYS_MovingBrush: 
			{
				OldLocation = Location;
				physMovingBrush(DeltaSeconds); 
				Velocity = (Location - OldLocation)/DeltaSeconds;
				break;
			}
		case PHYS_Trailer: physTrailer(DeltaSeconds); break;
		case PHYS_Rolling: physRolling(DeltaSeconds, 0); break;
	}

	// rotate
	if ( !RotationRate.IsZero() ) 
		physicsRotation(DeltaSeconds);

	// allow touched actors to impact physics
	if ( PendingTouch )
	{
		PendingTouch->eventPostTouch(this);
		AActor *OldTouch = PendingTouch;
		PendingTouch = PendingTouch->PendingTouch;
		OldTouch->PendingTouch = NULL;
	}
	unguard;
}

void APawn::performPhysics(FLOAT DeltaSeconds)
{
	guard(APawn::performPhysics);

	FVector OldVelocity = Velocity;

	// change position
	switch (Physics)
	{
		case PHYS_Walking: physWalking(DeltaSeconds, 0); break;
		case PHYS_Falling: physFalling(DeltaSeconds, 0); break;
		case PHYS_Flying: physFlying(DeltaSeconds, 0); break;
		case PHYS_Swimming: physSwimming(DeltaSeconds, 0); break;
		case PHYS_Spider: physSpider(DeltaSeconds, 0); break;
		case PHYS_Interpolating: 
			{
				OldLocation = Location;
				physPathing(DeltaSeconds); 
				Velocity = (Location - OldLocation)/DeltaSeconds;
				break;
			}
	}

	// rotate
	if ( (Physics != PHYS_Spider) 
			&& (IsA(APlayerPawn::StaticClass()) || (Rotation != DesiredRotation) || (RotationRate.Roll > 0)) ) 
		physicsRotation(DeltaSeconds, OldVelocity);

	MoveTimer -= DeltaSeconds;
	AvgPhysicsTime = 0.8 * AvgPhysicsTime + 0.2 * DeltaSeconds;

	if ( PendingTouch )
	{
		PendingTouch->eventPostTouch(this);
		if ( PendingTouch )
		{
			AActor *OldTouch = PendingTouch;
			PendingTouch = PendingTouch->PendingTouch;
			OldTouch->PendingTouch = NULL;
		}
	}

	unguard;
}

int AActor::fixedTurn(int current, int desired, int deltaRate)
{
	guard(AActor::fixedTurn);

	if (deltaRate == 0)
		return (current & 65535);

	int result = current & 65535;
	current = result;
	desired = desired & 65535;

	if (bFixedRotationDir)
	{
		if (bRotateToDesired)
		{
			if (deltaRate > 0)
			{
				if (current > desired)
					desired += 65536;
				result += Min(deltaRate, desired - current);
			}
			else 
			{
				if (current < desired)
					current += 65536;
				result += ::Max(deltaRate, desired - current);
			}
		}
		else
			result += deltaRate;
	}
	else if (bRotateToDesired)
	{
		if (current > desired)
		{
			if (current - desired < 32768)
				result -= Min((current - desired), Abs(deltaRate));
			else
				result += Min((desired + 65536 - current), Abs(deltaRate));
		}
		else
		{
			if (desired - current < 32768)
				result += Min((desired - current), Abs(deltaRate));
			else
				result -= Min((current + 65536 - desired), Abs(deltaRate));
		}
	}

	return (result & 65535);
	unguard;
}

void APawn::physicsRotation(FLOAT deltaTime, FVector OldVelocity)
{
	guard(APawn::physicsRotation);

	// Accumulate a desired new rotation.
	FRotator NewRotation = Rotation;	

	if (!IsA(APlayerPawn::StaticClass())) //don't pitch or yaw player
	{
		int deltaYaw = (INT) (RotationRate.Yaw * deltaTime);
		bRotateToDesired = 1; //Pawns always have a "desired" rotation
		bFixedRotationDir = 0;
	
		//YAW 
		if ( DesiredRotation.Yaw != NewRotation.Yaw )
			NewRotation.Yaw = fixedTurn(NewRotation.Yaw, DesiredRotation.Yaw, deltaYaw);

		//PITCH
		if ( DesiredRotation.Pitch != NewRotation.Pitch )
		{
			//pawns pitch instantly
			NewRotation.Pitch = DesiredRotation.Pitch & 65535;
			//debugf("desired pitch %f actual pitch %f",DesiredRot.Pitch, NewRotation.Pitch);
			if ( NewRotation.Pitch < 32768 )
			{
				if (NewRotation.Pitch > RotationRate.Pitch) //bound pitch
					NewRotation.Pitch = RotationRate.Pitch;
			}
			else if (NewRotation.Pitch < 65536 - RotationRate.Pitch)
				NewRotation.Pitch = 65536 - RotationRate.Pitch;
		}

	}

	//ROLL
	if (RotationRate.Roll > 0) 
	{
		//pawns roll based on physics
		if ((Physics == PHYS_Walking) && Velocity.SizeSquared() < 40000.f)
		{
			FLOAT SmoothRoll = Min(1.f, 8.f * deltaTime);
			if (NewRotation.Roll < 32768)
				NewRotation.Roll = (INT) (NewRotation.Roll * (1 - SmoothRoll));
			else
				NewRotation.Roll = (INT) (NewRotation.Roll + (65536 - NewRotation.Roll) * SmoothRoll);
		}
		else
		{
			FVector RealAcceleration = (Velocity - OldVelocity)/deltaTime;
			if (RealAcceleration.SizeSquared() > 10000.f) 
			{
				FLOAT MaxRoll = 28000.f;
				if ( Physics == PHYS_Walking )
					MaxRoll = 4096.f;
				NewRotation.Roll = 0;

				RealAcceleration = RealAcceleration.TransformVectorBy(GMath.UnitCoords/NewRotation); //y component will affect roll

				if (RealAcceleration.Y > 0) 
					NewRotation.Roll = Min<INT>(RotationRate.Roll, (int)(RealAcceleration.Y * MaxRoll/AccelRate)); 
				else
					NewRotation.Roll = ::Max<INT>(65536 - RotationRate.Roll, (int)(65536.f + RealAcceleration.Y * MaxRoll/AccelRate));

				//smoothly change rotation
				Rotation.Roll = Rotation.Roll & 65535;
				if (NewRotation.Roll > 32768)
				{
					if (Rotation.Roll < 32768)
						Rotation.Roll += 65536;
				}
				else if (Rotation.Roll > 32768)
					Rotation.Roll -= 65536;
	
				FLOAT SmoothRoll = Min(1.f, 5.f * deltaTime);
				NewRotation.Roll = (INT) (NewRotation.Roll * SmoothRoll + Rotation.Roll * (1 - SmoothRoll));

				//if ((NewRotation.Roll > MaxRoll) && (NewRotation.Roll < (65536 - MaxRoll)))
				//	debugf("Illegal roll for %f", RealAcceleration.Y);
			}
			else
			{
				FLOAT SmoothRoll = Min(1.f, 8.f * deltaTime);
				if (NewRotation.Roll < 32768)
					NewRotation.Roll = (INT) (NewRotation.Roll * (1 - SmoothRoll));
				else
					NewRotation.Roll = (INT) (NewRotation.Roll + (65536 - NewRotation.Roll) * SmoothRoll);
			}
		}
	}
	else
		NewRotation.Roll = 0;

	// Set the new rotation.
	if( NewRotation != Rotation )
	{
		FCheckResult Hit(1.0);
		GetLevel()->MoveActor( this, FVector(0,0,0), NewRotation, Hit );
	}

	unguard;
}

void AActor::physicsRotation(FLOAT deltaTime)
{
	guard(AActor::physicsRotation);
	
	if ( (!bRotateToDesired && !bFixedRotationDir)
		|| (bRotateToDesired && (Rotation == DesiredRotation)) )
		return;

	// Accumulate a desired new rotation.
	FRotator NewRotation = Rotation;	
	FRotator deltaRotation = RotationRate * deltaTime;

	//YAW
	if ( (deltaRotation.Yaw != 0) && (!bRotateToDesired || (DesiredRotation.Yaw != NewRotation.Yaw)) )
		NewRotation.Yaw = fixedTurn(NewRotation.Yaw, DesiredRotation.Yaw, deltaRotation.Yaw);
	//PITCH
	if ( (deltaRotation.Pitch != 0) && (!bRotateToDesired || (DesiredRotation.Pitch != NewRotation.Pitch)) )
		NewRotation.Pitch = fixedTurn(NewRotation.Pitch, DesiredRotation.Pitch, deltaRotation.Pitch);
	//ROLL
	if ( (deltaRotation.Roll != 0) && (!bRotateToDesired || (DesiredRotation.Roll != NewRotation.Roll)) )
		NewRotation.Roll = fixedTurn(NewRotation.Roll, DesiredRotation.Roll, deltaRotation.Roll);	

	// Set the new rotation.
	if( NewRotation != Rotation )
	{
		FCheckResult Hit(1.0);
		GetLevel()->MoveActor( this, FVector(0,0,0), NewRotation, Hit );
	}

	if ( bRotateToDesired && (Rotation == DesiredRotation) && IsProbing(NAME_EndedRotation) )
		eventEndedRotation(); //tell thing rotation ended

	unguard;
}

/*
physWalking()

*/
#if defined(LEGEND) //LEGEND
//-----------------------------------------------------------------------------
// climable and frictionless texture support routines 
//-----------------------------------------------------------------------------
//const float FRICTION_SLIPPERY  = 0.0;
const float FRICTION_CLIMBABLE = 10.0;

static UTexture* TraceTexture
(
	AActor*			Actor,
	FCheckResult&	Hit,
	FVector			TraceEnd,
	FVector			TraceStart = FVector(0,0,0),
	FVector			TraceExtent = FVector(0,0,0)
)
{
	UModel* Model = Actor->XLevel->Model;
	UTexture* Texture = NULL;

	// Trace the line.
	Actor->GetLevel()->SingleLineCheck( Hit, Actor, TraceEnd, TraceStart, TRACE_VisBlocking, TraceExtent );

	// attempt to locate the surface/texture associated with the BSP Node (Hit.Item)
	if( Hit.Actor != NULL && Hit.Actor->IsA( ALevelInfo::StaticClass() ) )
	{
		const FBspNode*	Node = &Model->Nodes( Hit.Item );
		if( Node != NULL )
		{
			const FBspSurf* Surf = &Model->Surfs( Node->iSurf );
			Texture = Surf->Texture;
		}
	}

	return Texture;
}

static UTexture* CheckClimbSurface( APawn* Pawn, FCheckResult& Hit )
{
    FVector StartTrace, EndTrace;
	FRotator Rot;
	UTexture* Texture;

	Rot = Pawn->Rotation;
	Rot.Pitch = 0;

	// trace forward from just above the player's feet
    StartTrace = Pawn->Location - FVector(0,0,0.7) * Pawn->CollisionHeight;
    EndTrace = StartTrace + 2*Pawn->CollisionRadius * Rot.Vector();
    Texture = TraceTexture( Pawn, Hit, EndTrace, StartTrace );

	if( Texture == NULL || Texture->Friction != FRICTION_CLIMBABLE )
	{
		// check to the player's left
		Rot.Yaw -= 16384;
		EndTrace = StartTrace + 2*Pawn->CollisionRadius * Rot.Vector();
	    Texture = TraceTexture( Pawn, Hit, EndTrace, StartTrace );
	}
	if( Texture == NULL || Texture->Friction != FRICTION_CLIMBABLE )
	{
		// check to the player's right
		Rot.Yaw += 32768;
		EndTrace = StartTrace + 2*Pawn->CollisionRadius * Rot.Vector();
	    Texture = TraceTexture( Pawn, Hit, EndTrace, StartTrace );
	}
	if( Texture == NULL || Texture->Friction != FRICTION_CLIMBABLE )
	{
		// check behind the player
		Rot.Yaw += 16384;
		EndTrace = StartTrace + 2*Pawn->CollisionRadius * Rot.Vector();
	    Texture = TraceTexture( Pawn, Hit, EndTrace, StartTrace );
	}

	return Texture;
}

static UTexture* CheckWalkSurface( APawn* Pawn, FCheckResult& Hit )
{
    FVector StartTrace, EndTrace;
    UTexture* Texture;

	// trace from player origin to radius*2 below the collision cylinder
    StartTrace = Pawn->Location;
    EndTrace = StartTrace - FVector(0,0,1) * ( Pawn->CollisionHeight + Pawn->CollisionRadius * 2 );
    Texture = TraceTexture( Pawn, Hit, EndTrace, StartTrace );

	return Texture;
}

static bool CheckSurfaces( APawn* Pawn, FLOAT deltaTime, INT Iterations )
{
	FCheckResult Hit(1.0);
	UTexture* Texture;
	
	if( ! Pawn->Level->bCheckWalkSurfaces )
		return false;

	Texture = CheckClimbSurface( Pawn, Hit );
	if( Texture != NULL && Texture->Friction == FRICTION_CLIMBABLE )
	{
		Pawn->eventWalkTexture( Texture, Hit.Location, Hit.Normal );

		if( !Pawn->Acceleration.IsZero() )
		{
			// bias facing view up (so that moving directly forward causes pawns to climb)
			FRotator Rot = Pawn->ViewRotation;
			Rot.Pitch += 4096;
			Pawn->Velocity.Z = Pawn->GroundSpeed * Rot.Vector().Z;
		}
		Pawn->physFlying( deltaTime, Iterations );
		return true;
	}

	Texture = CheckWalkSurface( Pawn, Hit );
	Pawn->eventWalkTexture( Texture, Hit.Location, Hit.Normal );

	if( Pawn->Physics == PHYS_Walking && Texture != NULL && Texture->Friction < 1.0 )
	{
		// compute slip direction
		FVector Slide = (deltaTime * Pawn->Region.Zone->ZoneGravity/(0.5 * ::Max(0.05f, 4.0f * Texture->Friction))) * deltaTime;
		FVector Delta = Slide - Hit.Normal * (Slide | Hit.Normal);
		if( (Delta | Slide) >= 0 )
			Pawn->GetLevel()->MoveActor( Pawn, Delta, Pawn->Rotation, Hit);
		return false;
	}

	return false;
}
//-----------------------------------------------------------------------------
#endif

void APawn::physWalking(FLOAT deltaTime, INT Iterations)
{
	guard(APawn::physWalking);

#if defined(LEGEND) //LEGEND
	if( CheckSurfaces( this, deltaTime, Iterations ) )
		return;
#endif

	if ( Region.ZoneNumber == 0 )
	{
		// not in valid spot
		if ( Role == ROLE_Authority )
			debugf( TEXT("%s fell out of the world!"), GetName() );
		eventFellOutOfWorld();
		return;
	}

	//bound acceleration
	//goal - support +-Z gravity, but not other vectors
	Velocity.Z = 0;
	Acceleration.Z = 0;
	FVector AccelDir;
	if ( Acceleration.IsZero() )
		AccelDir = Acceleration;
	else
		AccelDir = Acceleration.SafeNormal();
	calcVelocity(AccelDir, deltaTime, GroundSpeed, Region.Zone->ZoneGroundFriction, 0, 1, 0);   
	
	FVector DesiredMove = Velocity;
	if ( IsA(APlayerPawn::StaticClass()) || (Region.Zone->ZoneVelocity.SizeSquared() > 90000) )
	{
		// Add effect of velocity zone
		// Rather than constant velocity, hacked to make sure that velocity being clamped when walking doesn't 
		// cause the zone velocity to have too much of an effect at fast frame rates

		DesiredMove = DesiredMove + Region.Zone->ZoneVelocity * 25 * deltaTime;
	}
	DesiredMove.Z = 0.0;
	//-------------------------------------------------------------------------------------------
	//Perform the move
	FVector GravDir = FVector(0,0,-1);
	if (Region.Zone->ZoneGravity.Z > 0)
		GravDir.Z = 1;
	FVector Down = GravDir * (MaxStepHeight + 2.0);
	FCheckResult Hit(1.0);
	OldLocation = Location;
	bJustTeleported = 0;
	int bCheckedFall = 0;
	int bMustJump = 0;

	FLOAT remainingTime = deltaTime;
	FLOAT timeTick;
	while ( (remainingTime > 0.0) && (Iterations < 8) )
	{
		Iterations++;
		if ( (remainingTime > 0.05) && (IsA(APlayerPawn::StaticClass()) ||
			(DesiredMove.SizeSquared() * remainingTime * remainingTime > 400.f)) )
				timeTick = Min(0.05f, remainingTime * 0.5f);
		else timeTick = remainingTime;
		remainingTime -= timeTick;
		FVector Delta = timeTick * DesiredMove;
		FVector subLoc = Location;
		FVector subMove = Delta;
		int bZeroMove = Delta.IsNearlyZero();
		if ( bZeroMove )
		{
			remainingTime = 0;
			bHitSlopedWall = 0;
		}
		else
		{
			FVector ForwardCheck = AccelDir * CollisionRadius;
			if ( !bAvoidLedges )
				ForwardCheck *= 0.5; 
			// if AI controlled, check for fall by doing trace forward
			// try to find reasonable walk along ledge
			if ( (!IsA(APlayerPawn::StaticClass()) || bIsWalking) && !bCanFly ) 
			{
				// check if clear in front
				FVector Destn = Location + Delta + ForwardCheck;
				GetLevel()->SingleLineCheck(Hit, this, Destn, Location, TRACE_VisBlocking);  
				if (Hit.Time == 1.0)
				{
					// clear in front - see if there is footing at walk destination
					FLOAT DesiredDist = Delta.Size();
					// check down enough to catch either step or slope
					FLOAT TestDown = ::Max( 4.f + MaxStepHeight + CollisionHeight, 4.f + CollisionHeight + CollisionRadius + DesiredDist);
					// try a point trace
					GetLevel()->SingleLineCheck(Hit, this, Destn + TestDown * GravDir, Destn , TRACE_VisBlocking);
					FLOAT MaxRadius = ::Min(14.f, 0.5f * CollisionRadius);
					// if point trace hit nothing, or hit a slope, do a trace with extent
					if ( (Hit.Time == 1.0) 
						|| ((Hit.Normal.Z > 0.7) && (Hit.Time * TestDown > CollisionHeight + MaxStepHeight + 4.f) 
							&& (Hit.Time * TestDown > CollisionHeight + 4.f + appSqrt(1 - Hit.Normal.Z * Hit.Normal.Z) * (CollisionRadius + DesiredDist)/Hit.Normal.Z)) )
						GetLevel()->SingleLineCheck(Hit, this, Destn + GravDir * (MaxStepHeight + 4.0), Destn , TRACE_VisBlocking, FVector(MaxRadius, MaxRadius, CollisionHeight));
					if (Hit.Time == 1.0)  
					{
						// We have a ledge!
						Destn = Location + DesiredDist * AccelDir + ForwardCheck;
						//first, try tracing back to get the ledge direction
						FVector DesiredDir = Delta/DesiredDist;
						FVector LedgeDown = GravDir * (CollisionHeight + 6.0);
						GetLevel()->SingleLineCheck(Hit, this, Location + LedgeDown - 2 * CollisionRadius * AccelDir, 
											Destn + LedgeDown , TRACE_VisBlocking);
						LedgeDown = GravDir * (MaxStepHeight + 6.0);
						FVector LedgeDir;
						int bMoveForward = 0;
						int bGoodMove = 0;
						if (Hit.Time < 1.0) //found a ledge
						{
							if ( bAvoidLedges )
							{
								LedgeDir = Hit.Normal;
								LedgeDir.Z = 0;
								Delta = -1 * DesiredSpeed * GroundSpeed * timeTick * LedgeDir;
								bMoveForward = 0;
								if ( bStopAtLedges )
									MoveTimer = -1;
								else
									MoveTimer -= 0.25;
							}
							else
							{
								LedgeDir.X = Hit.Normal.Y;
								LedgeDir.Y = -1 * Hit.Normal.X;
								LedgeDir.Z = 0;
								LedgeDir = LedgeDir.SafeNormal();
								if ( (LedgeDir | AccelDir) < 0 )
									LedgeDir *= -1;
								FLOAT DP = (LedgeDir | AccelDir );
								bMoveForward = ( (DP < 0.5) || (bCanJump && (DP < 0.7)) ) ;
								if ( DP < 0.7 )
									Delta = Min(0.8f, DesiredSpeed) * GroundSpeed * timeTick * LedgeDir;
								else
									Delta = DesiredSpeed * GroundSpeed * timeTick * LedgeDir;
							}
						}
						else 
						{
							Destn = Location + Delta + ForwardCheck;
							LedgeDir.X = DesiredDir.Y;
							LedgeDir.Y = -1 * DesiredDir.X;
							LedgeDir.Z = 0;
							bMoveForward = 1;
							Delta = Min(0.8f, DesiredSpeed) * GroundSpeed * timeTick * LedgeDir;
							Destn = Location + Delta;
							GetLevel()->SingleLineCheck(Hit, this, Destn, Location, TRACE_VisBlocking, GetCylinderExtent());
							if (Hit.Time == 1.0)
							{
								GetLevel()->SingleLineCheck(Hit, this, Destn + LedgeDown, Destn, TRACE_VisBlocking, GetCylinderExtent());
								if ( Hit.Time == 1.0 ) //reflect delta about desiredir
									Delta *= -1;
								else 
									bGoodMove = 1;
							}
							else 
								bGoodMove = 1;
						}
						if ( IsA(APlayerPawn::StaticClass()) )
						{
							bMoveForward = 0;
							if ( !bGoodMove )
							{
								Destn = Location + Delta + ForwardCheck;
								GetLevel()->SingleLineCheck(Hit, this, Destn, Location, TRACE_VisBlocking, GetCylinderExtent());
								if ( Hit.Time == 1.0 )
									GetLevel()->SingleLineCheck(Hit, this, Destn + LedgeDown, Destn, TRACE_VisBlocking, FVector(MaxRadius, MaxRadius, CollisionHeight));
								if (Hit.Time == 1.0)
								{
									Acceleration = FVector(0,0,0);
									Delta = FVector(0,0,0);
								}
							}
						}
						if ( bCanJump && bMoveForward )
						{
							if ( !IsProbing(NAME_MayFall) )
								Delta = AccelDir * DesiredDist;
							else if ( !bCheckedFall )
							{
								bCheckedFall = 1;
								bMoveForward = 0;
								eventMayFall();
								if ( bCanJump )
								{
									bMustJump = 1;
									Delta = AccelDir * DesiredDist;
								}
							}
						}
						if ( !bCanJump  ) //if can't jump, make sure this is valid
						{
							if ( bMoveForward ) //check if should just move forward
							{
								Destn = Location + DesiredDir * (DesiredDist + CollisionRadius);
								GetLevel()->SingleLineCheck(Hit, this, Destn, Location, TRACE_VisBlocking, GetCylinderExtent());
								if ( Hit.Time == 1.0 )
								{
									GetLevel()->SingleLineCheck(Hit, this, Destn + LedgeDown, Destn, TRACE_VisBlocking, GetCylinderExtent());
									if ( Hit.Time < 1.0 )
									{
										Destn = Location + DesiredDir * DesiredDist;
										GetLevel()->SingleLineCheck(Hit, this, Destn + LedgeDown, Destn, TRACE_VisBlocking, GetCylinderExtent());
									}
								}
								if ( Hit.Time < 1.0 )
									Delta = DesiredDir * DesiredDist;
								else 
								{
									bMoveForward = 0;
									if ( appFrand() < 2 * timeTick )
										MoveTimer = -1.0;
									else
										MoveTimer -= 0.1;
								}
							}
							if ( !bMoveForward && !bGoodMove )
							{
								Destn = Location + Delta + ForwardCheck;
								GetLevel()->SingleLineCheck(Hit, this, Destn, Location, TRACE_VisBlocking, GetCylinderExtent());
								if ( Hit.Time == 1.0 )
									GetLevel()->SingleLineCheck(Hit, this, Destn + LedgeDown, Destn, TRACE_VisBlocking, GetCylinderExtent());
								else if ( (Hit.Normal | DesiredDir) < MinHitWall )
									MoveTimer = -1.0;
								if (Hit.Time == 1.0)
								{
									GetLevel()->SingleLineCheck
										(Hit, this, Location + LedgeDown, Location , TRACE_VisBlocking, FVector(MaxRadius, MaxRadius, CollisionHeight));
									remainingTime = 0.0;
									MoveTimer = -1.0;
									Acceleration = FVector(0,0,0);
									if ( Hit.Time == 1.0 )
										Delta = -1 * GroundSpeed * timeTick * DesiredDir;
									else
										Delta = FVector(0,0,0);
								}
							}
						}
					}
				}
				subMove = Delta;
			}

			// check if might hit sloped wall, and decide if to change direction before move
			if ( bHitSlopedWall )
			{
				FLOAT DesiredDist = Delta.Size();
				FVector DesiredDir = Delta/DesiredDist;
				FVector CheckDir = DesiredDir * ::Max(30.f, DesiredDist + 4);
				GetLevel()->SingleLineCheck(Hit, this, Location + CheckDir, Location , TRACE_VisBlocking, GetCylinderExtent());
				bHitSlopedWall = ( (Hit.Time < 1.0) && (Hit.Normal.Z > 0.01) && (Hit.Normal.Z < 0.7) );
				if ( bHitSlopedWall )
				{
					Hit.Normal.Z = 0.0;
					Hit.Normal = Hit.Normal.SafeNormal();
					Delta = (Delta - Hit.Normal * (Delta | Hit.Normal));
				}
				else if ( this->IsA(APlayerPawn::StaticClass()) ) //make sure really done with sloped wall
				{
					FVector CheckLoc = Location;
					CheckLoc.Z = CheckLoc.Z - CollisionHeight + MaxStepHeight + 4;
					GetLevel()->SingleLineCheck(Hit, this, CheckLoc + 100 * DesiredDir, CheckLoc , TRACE_VisBlocking);
					bHitSlopedWall = ( (Hit.Time < 1.0) && (Hit.Normal.Z > 0.01) && (Hit.Normal.Z < 0.7) );
					if ( !bHitSlopedWall )
					{
						FVector LeftDir = FVector(DesiredDir.Y, -1 * DesiredDir.X, 0) + DesiredDir;
						LeftDir = LeftDir.SafeNormal();
						GetLevel()->SingleLineCheck(Hit, this, CheckLoc + 100 * LeftDir, CheckLoc , TRACE_VisBlocking);
						bHitSlopedWall = ( (Hit.Time < 1.0) && (Hit.Normal.Z > 0.01) && (Hit.Normal.Z < 0.7) );
					}
					if ( !bHitSlopedWall )
					{
						FVector LeftDir = FVector(-1 * DesiredDir.Y, DesiredDir.X, 0) + DesiredDir;
						LeftDir = LeftDir.SafeNormal();
						GetLevel()->SingleLineCheck(Hit, this, CheckLoc + 100 * LeftDir, CheckLoc , TRACE_VisBlocking);
						bHitSlopedWall = ( (Hit.Time < 1.0) && (Hit.Normal.Z > 0.01) && (Hit.Normal.Z < 0.7) );
					}
				} 
				GetLevel()->MoveActor(this, Delta, Rotation, Hit);
			}
			else
			{
				GetLevel()->MoveActor(this, Delta, Rotation, Hit);
				bHitSlopedWall = ( (Hit.Time < 1.0) && (Hit.Normal.Z > 0.01) && (Hit.Normal.Z < 0.7) );
			}

			if (Hit.Time < 1.0) //try to step up
			{
				FVector DesiredDir = Delta.SafeNormal();
				stepUp(GravDir, DesiredDir, Delta * (1.0 - Hit.Time), Hit);
				if ( Physics == PHYS_Falling ) // pawn decided to jump up
				{
					FLOAT DesiredDist = subMove.Size();
					FLOAT ActualDist = (Location - subLoc).Size2D();
					remainingTime += timeTick * (1 - Min(1.f,ActualDist/DesiredDist)); 
					eventFalling();
					if ( Physics == PHYS_Falling ) 
					{
						if (remainingTime > 0.01)
							physFalling(remainingTime, Iterations);
					}
					else if ( Physics == PHYS_Flying )
					{
						Velocity = FVector(0,0, AirSpeed);
						Acceleration = FVector(0,0,AccelRate);
						if (remainingTime > 0.01)
							physFlying(remainingTime, Iterations);
					}
					return;
				}
			}

			if ( this->IsA(APawn::StaticClass()) && (Physics == PHYS_Swimming) ) //just entered water
			{
				((APawn *)this)->startSwimming(Velocity, timeTick, remainingTime, Iterations);
				return;
			}
		}

		//drop to floor
		if ( bZeroMove )
		{
			FVector Foot = Location - FVector(0,0,CollisionHeight);
			GetLevel()->SingleLineCheck( Hit, this, Foot - FVector(0,0,20), Foot, TRACE_VisBlocking );
			FLOAT FloorDist = Hit.Time * 20;
			bZeroMove = ((Base == Hit.Actor) && (FloorDist <= 4.6) && (FloorDist >= 4.1));
		}
		if ( !bZeroMove )
		{
			GetLevel()->SingleLineCheck( Hit, this, Location + Down, Location, TRACE_AllColliding, GetCylinderExtent() );
			FLOAT FloorDist = Hit.Time * (MaxStepHeight + 2.0);

			if ( (Hit.Time < 1.0) && ((Hit.Actor != Base) || (FloorDist > 2.4)) ) 
			{
				GetLevel()->MoveActor(this, Down, Rotation, Hit);
				if (Hit.Actor != Base)
					SetBase(Hit.Actor);
				if ( this->IsA(APawn::StaticClass()) && (Physics == PHYS_Swimming) ) //just entered water
				{
					((APawn *)this)->startSwimming(Velocity, timeTick, remainingTime, Iterations);
					return;
				}
			}
			else if ( FloorDist < 1.9 )
			{
				FVector realNorm = Hit.Normal;
				GetLevel()->MoveActor(this, FVector(0,0,2.1 - FloorDist), Rotation, Hit);
				Hit.Time = 0;
				Hit.Normal = realNorm;
			}
			
			if ( !bMustJump && (Hit.Time < 1.0) && (Hit.Normal.Z >= 0.7) )  
			{
				if ( (Hit.Normal.Z < 1.0) && ((Hit.Normal.Z * Region.Zone->ZoneGroundFriction) < 3.3) ) //slide down slope, depending on friction and gravity
				{
					FVector Slide = (deltaTime * Region.Zone->ZoneGravity/(2 * ::Max(0.5f, Region.Zone->ZoneGroundFriction))) * deltaTime;
					Delta = Slide - Hit.Normal * (Slide | Hit.Normal);
					if( (Delta | Slide) >= 0 )
						GetLevel()->MoveActor(this, Delta, Rotation, Hit);
					if ( this->IsA(APawn::StaticClass()) && (Physics == PHYS_Swimming) ) //just entered water
					{
						((APawn *)this)->startSwimming(Velocity, timeTick, remainingTime, Iterations);
						return;
					}
				}				
			}
			else
			{
				if ( !bMustJump && bCanJump && !bCheckedFall && IsProbing(NAME_MayFall) )
				{
					bCheckedFall = 1;
					eventMayFall();
				}
				if ( !bMustJump && (!bCanJump || bIsWalking) ) 
				{
					Velocity = FVector(0,0,0);
					Acceleration = FVector(0,0,0);
					GetLevel()->FarMoveActor(this,OldLocation,0,0 );
					MoveTimer = -1.0;
					return;
				}
				else // falling
				{
					if ( Hit.Time < 1.0 )
						bHitSlopedWall = 1;
					FLOAT DesiredDist = subMove.Size();
					FLOAT ActualDist = (Location - subLoc).Size2D();
					if (DesiredDist == 0.0f)
						remainingTime = 0;
					else
						remainingTime += timeTick * (1 - Min(1.f,ActualDist/DesiredDist)); 
					Velocity.Z = 0.0;
					eventFalling();
					if (Physics == PHYS_Walking)
						setPhysics(PHYS_Falling); //default if script didn't change physics
					if ( !bMustJump && (Physics == PHYS_Falling) )
					{
						FLOAT velZ = Velocity.Z;
						if (!bJustTeleported && (deltaTime > remainingTime))
							Velocity = (Location - OldLocation)/(deltaTime - remainingTime);
						Velocity.Z = velZ;
						if (remainingTime > 0.01)
							physFalling(remainingTime, Iterations);
						return;
					}
					else 
					{
						Delta = remainingTime * DesiredMove;
						GetLevel()->MoveActor(this, Delta, Rotation, Hit); 
						remainingTime = 0;
					}
				}
			}
		}
	}

	//if ( Iterations > 7 )
	//	debugf("Over 7 iterations in physics!");
	// make velocity reflect actual move
	if (!bJustTeleported)
		Velocity = (Location - OldLocation) / deltaTime;
	Velocity.Z = 0.0;
 	unguard;
}

/* calcVelocity()
Calculates new velocity and acceleration for pawn for this tick
bounds acceleration and velocity, adds effects of friction and momentum
// bBrake only for walking?
// fixme - what is right for air turn rate - make it a pawn var?
// e.g. Max(bFluid * airbraking, friction)
*/
void APawn::calcVelocity(FVector AccelDir, FLOAT deltaTime, FLOAT maxSpeed, FLOAT friction, INT bFluid, INT bBrake, INT bBuoyant)
{
	guard(APawn::calcVelocity);
	FLOAT effectiveFriction = ::Max((FLOAT)bFluid,friction); 
	INT bWalkingPlayer = ( this->IsA(APlayerPawn::StaticClass()) && bIsWalking );
	if (bBrake && Acceleration.IsZero()) 
	{
		FVector OldVel = Velocity;
		FVector SumVel = FVector(0,0,0);

		FLOAT RemainingTime = deltaTime;
		// subdivide braking to get reasonably consistent results at lower frame rates
		// (important for packet loss situations w/ networking)
		while ( RemainingTime > 0.03 )
		{
			Velocity = Velocity - (2 * Velocity) * 0.03 * effectiveFriction; //don't drift to a stop, brake
			if ( (Velocity | OldVel) > 0.f )
				SumVel += 0.03 * Velocity/deltaTime;
			RemainingTime -= 0.03;
		}
		Velocity = Velocity - (2 * Velocity) * RemainingTime * effectiveFriction; //don't drift to a stop, brake
		if ( (Velocity | OldVel) > 0.f )
			SumVel += RemainingTime * Velocity/deltaTime;
		Velocity = SumVel;
		if ( ((OldVel | Velocity) < 0.0)
			|| (Velocity.SizeSquared() < 100) )//brake to a stop, not backwards
			Velocity = FVector(0,0,0);
	}
	else
	{
		FLOAT VelSize = Velocity.Size();
		if ( bWalkingPlayer )
		{
			if (Acceleration.SizeSquared() > 0.09 * AccelRate * AccelRate)
					Acceleration = AccelDir * AccelRate * 0.3;
		}
		else if (Acceleration.SizeSquared() > AccelRate * AccelRate)
			Acceleration = AccelDir * AccelRate;
		Velocity = Velocity - (Velocity - AccelDir * VelSize) * deltaTime * effectiveFriction;  
	}

	Velocity = Velocity * (1 - bFluid * friction * deltaTime) + Acceleration * deltaTime;

	if (!this->IsA(APlayerPawn::StaticClass()))
		maxSpeed *= DesiredSpeed;

	if ( bBuoyant )
		Velocity = Velocity + Region.Zone->ZoneGravity * deltaTime * (1.0 - Buoyancy/Mass);

	if ( bWalkingPlayer && (Velocity.SizeSquared() > 0.09 * maxSpeed * maxSpeed) )
	{
		FLOAT speed = Velocity.Size();
		Velocity = Velocity/speed;
		Velocity *= ::Max(0.3f * maxSpeed, speed * (1 - deltaTime * 2 * effectiveFriction)); 
	}
	else if (Velocity.SizeSquared() > maxSpeed * maxSpeed)
	{
		Velocity = Velocity.SafeNormal();
		Velocity *= maxSpeed;
	}


	unguard;
}

void APawn::stepUp(FVector GravDir, FVector DesiredDir, FVector Delta, FCheckResult &Hit)
{
	guard(APawn::stepUp);

	FVector Down = GravDir * MaxStepHeight;
	FVector Up = -1 * Down;
	GetLevel()->MoveActor(this, Up, Rotation, Hit); 
	GetLevel()->MoveActor(this, Delta, Rotation, Hit);
	if (Hit.Time < 1.0) 
	{
		if ( this->IsA(APlayerPawn::StaticClass()) && Hit.Actor->IsA(ADecoration::StaticClass()) && ((ADecoration *)(Hit.Actor))->bPushable
			&& ((Hit.Normal | DesiredDir) < -0.9) )
		{
			bJustTeleported = true;
			Velocity *= Mass/(Mass + Hit.Actor->Mass);
			processHitWall(Hit.Normal, Hit.Actor);
			if ( Physics == PHYS_Falling )
				return;
		}
		else if ((Abs(Hit.Normal.Z) < 0.2) && (Hit.Time * Delta.SizeSquared() > 144.0))
		{
			stepUp(GravDir, DesiredDir, Delta * (1 - Hit.Time), Hit);
			if ( Physics == PHYS_Falling )
				return;
		}
		else 
		{
			processHitWall(Hit.Normal, Hit.Actor);
			//adjust and try again
			FVector OriginalDelta = Delta;
			FVector OldHitNormal = Hit.Normal;
			Delta = (Delta - Hit.Normal * (Delta | Hit.Normal)) * (1.0 - Hit.Time);
			if( (Delta | OriginalDelta) >= 0 )
			{
				GetLevel()->MoveActor(this, Delta, Rotation, Hit);
				if (Hit.Time < 1.0)
				{
					processHitWall(Hit.Normal, Hit.Actor);
					if ( Physics == PHYS_Falling )
						return;
					TwoWallAdjust(DesiredDir, Delta, Hit.Normal, OldHitNormal, Hit.Time);
					GetLevel()->MoveActor(this, Delta, Rotation, Hit);
				}
			}
		}
	}
	GetLevel()->MoveActor(this, Down, Rotation, Hit);

	if ((Hit.Time < 1.0) && (Hit.Normal.Z < 0.5))
	{
		Delta = (Down - Hit.Normal * (Down | Hit.Normal))  * (1.0 - Hit.Time);
		if( (Delta | Down) >= 0 )
			GetLevel()->MoveActor(this, Delta, Rotation, Hit);
	} 

	unguard;
}

void AActor::processHitWall(FVector HitNormal, AActor *HitActor)
{
	guard(AActor::processHitWall);

	if ( HitActor->IsA(APawn::StaticClass()) )
		return;
	if ( this->IsA(APawn::StaticClass()) )
	{
		if ( Acceleration.IsZero() )
			return;
		FVector Dir = (((APawn *)this)->Destination - Location).SafeNormal();
		if ( Physics == PHYS_Walking )
		{
			HitNormal.Z = 0;
			Dir.Z = 0;
		}
		if ( ((APawn *)this)->MinHitWall < (Dir | HitNormal) )
			return;
		if ( !IsProbing(NAME_HitWall) && (Physics != PHYS_Falling) )
		{
			((APawn *)this)->MoveTimer = -1.0;
			((APawn *)this)->bFromWall = 1;
			return;
		}
	}
	else if ( !IsProbing(NAME_HitWall) )
		return;
	eventHitWall(HitNormal, HitActor);
	unguard;
}

#pragma DISABLE_OPTIMIZATION 
void AActor::processLanded(FVector HitNormal, AActor *HitActor, FLOAT remainingTime, INT Iterations)
{
	guard(AActor::processLanded);

	if ( !bIsPawn && Region.Zone->bBounceVelocity && (Region.Zone->ZoneVelocity != FVector(0,0,0)) )
	{
		Velocity = Region.Zone->ZoneVelocity + FVector(0,0,80);
		return;
	}
	if ( IsA(APawn::StaticClass()) ) //Check that it is a valid landing (not a BSP cut)
	{
		FCheckResult Hit(1.0);
		GetLevel()->SingleLineCheck(Hit, this, Location -  FVector(0,0,0.2 * CollisionRadius + 8),
			Location, TRACE_ProjTargets, 0.9 * GetCylinderExtent());  
		if ( Hit.Time == 1.0 ) //Not a valid landing
		{
			FVector Adjusted = Location;
			if ( GetLevel()->FindSpot(1.1 * GetCylinderExtent(), Adjusted, 0, 0) && (Adjusted != Location) )
			{
				GetLevel()->FarMoveActor(this, Adjusted, 0, 0);
				Velocity.X += appFrand() * 60 - 30;
				Velocity.Y += appFrand() * 60 - 30; 
				return;
			}
		}
	}
	else if ( IsA(ADecoration::StaticClass()) )
	{
		if ( ((ADecoration *)this)->numLandings < 5 ) // make sure its on a valid landing
		{
			FCheckResult Hit(1.0);
			GetLevel()->SingleLineCheck(Hit, this, Location -  FVector(0,0,(CollisionHeight + CollisionRadius + 8)),
				Location - FVector(0,0,(0.8 * CollisionHeight)) , TRACE_ProjTargets);  
			if ( !Hit.Actor )
			{
				FVector partExtent = 0.5 * GetCylinderExtent();
				partExtent.Z *= 2;
				int bQuad1 = GetLevel()->SingleLineCheck(Hit, this, Location + FVector(0.5 * CollisionRadius, 0.5 * CollisionRadius, -8),
					Location + FVector(0.5 * CollisionRadius, 0.5 * CollisionRadius, 0), TRACE_AllColliding, partExtent);
				int bQuad2 = GetLevel()->SingleLineCheck(Hit, this, Location + FVector(-0.5 * CollisionRadius, 0.5 * CollisionRadius, -8),
					Location + FVector(-0.5 * CollisionRadius, 0.5 * CollisionRadius, 0), TRACE_AllColliding, partExtent);
				int bQuad3 = GetLevel()->SingleLineCheck(Hit, this, Location + FVector(-0.5 * CollisionRadius, -0.5 * CollisionRadius, -8),
					Location + FVector(-0.5 * CollisionRadius, -0.5 * CollisionRadius, 0), TRACE_AllColliding, partExtent);
				int bQuad4 = GetLevel()->SingleLineCheck(Hit, this, Location + FVector(0.5 * CollisionRadius, -0.5 * CollisionRadius, -8),
					Location + FVector(0.5 * CollisionRadius, -0.5 * CollisionRadius, 0), TRACE_AllColliding, partExtent);
				
				if ( (bQuad1 + bQuad2 + bQuad3 + bQuad4 > 1) && !(bQuad1 + bQuad3 == 0) && !(bQuad2 + bQuad4 == 0) )
				{
					((ADecoration *)this)->numLandings++;
					Velocity = 2 * Clamp( -1.f * Velocity.Z, 30.f, 30.f + CollisionRadius) * 
								FVector((FLOAT)(bQuad1 + bQuad4 - bQuad2 - bQuad3), (FLOAT)(bQuad1 + bQuad2 - bQuad3 - bQuad4) , 0.5);
					return;
				}
			}
			if ( IsA(ACarcass::StaticClass()) && (HitNormal.Z < 0.9) && ((ACarcass *)this)->bSlidingCarcass )
			{
				if ( appFrand() < 0.2 )
					((ADecoration *)this)->numLandings++;
				Velocity = HitNormal * 120;
				Velocity.Z = 70;
				return;
			}	
			((ADecoration *)this)->numLandings = 0;
		}
		else
			((ADecoration *)this)->numLandings = 0;
	}

	eventLanded(HitNormal);
	if (Physics == PHYS_Falling)
	{
		if (this->IsA(APawn::StaticClass()))
			setPhysics(PHYS_Walking, HitActor);
		else
		{
			setPhysics(PHYS_None, HitActor);
			Velocity = FVector(0,0,0);
		}
	}
	if ((Physics == PHYS_Walking) && this->IsA(APawn::StaticClass()))
	{
		Acceleration = Acceleration.SafeNormal();
		if (remainingTime > 0.01)
			((APawn *)this)->physWalking(remainingTime, Iterations);
	}

	unguard;
}
#pragma ENABLE_OPTIMIZATION 

void AActor::physFalling(FLOAT deltaTime, INT Iterations)
{
	guard(AActor::physFalling);

	//bound acceleration, falling object has minimal ability to impact acceleration
	APawn *ThisPawn = this->IsA(APawn::StaticClass()) ? (APawn*)this : NULL;

#if defined(LEGEND) //LEGEND
	if( ThisPawn && CheckSurfaces( ThisPawn, deltaTime, Iterations ) )
		return;
#endif

	if ( Region.ZoneNumber == 0 )
	{
		// not in valid spot
		if ( (Role == ROLE_Authority)
			&& (IsA(AInventory::StaticClass()) || IsA(ADecoration::StaticClass()) || IsA(APawn::StaticClass())) )
			debugf( TEXT("%s fell out of the world!"), GetName() );
		eventFellOutOfWorld();
		return;
	}

	FLOAT BoundSpeed = 0; //Bound final 2d portion of velocity to this if non-zero
	FVector RealAcceleration = Acceleration;

	if (ThisPawn)
	{
		// For original Unreal air control, use ThisPawn->AirControl = 0.05
		// test for slope to avoid using air control to climb walls
		FLOAT AirControl = ThisPawn->AirControl;
		if( AirControl > 0.15f )
		{
			FVector TestWalk = ( AirControl * ThisPawn->AccelRate * Acceleration.SafeNormal() + Velocity ) * deltaTime;
			TestWalk.Z = 0;
			FCheckResult Hit(1.0);
			GetLevel()->SingleLineCheck( Hit, this, Location + TestWalk, Location, TRACE_VisBlocking, FVector( CollisionRadius, CollisionRadius, CollisionHeight ) );
			if( Hit.Actor != NULL )
				AirControl = 0.05f;
		}

		// boost maxAccel to increase player's control when falling
		FLOAT maxAccel = ThisPawn->AccelRate * AirControl;
		FVector Velocity2D = Velocity;
		Velocity2D.Z = 0;
		Acceleration.Z = 0;
		FLOAT speed2d = Velocity2D.Size2D(); 
		if (speed2d < 10.0) //allow initial burst
			maxAccel = maxAccel + (10 - speed2d)/deltaTime;
		else if ( speed2d >= ThisPawn->GroundSpeed )
		{
			if ( AirControl <= 0.05f )
				maxAccel = 1.f;
			else 
				BoundSpeed = speed2d;
		}

		if (Acceleration.SizeSquared() > maxAccel * maxAccel)
		{
			Acceleration = Acceleration.SafeNormal();
			Acceleration = Acceleration * maxAccel;
		}
	}
	FLOAT remainingTime = deltaTime;
	FLOAT timeTick = 0.1;
	int numBounces = 0;
	FCheckResult Hit(1.0);
	int AdjustApex = 0;

	while ( (remainingTime > 0.0) && (Iterations < 8) )
	{
		Iterations++;
		if (remainingTime > 0.1)
			timeTick = Min(0.1f, remainingTime * 0.5f);
		else timeTick = remainingTime;

		remainingTime -= timeTick;
		OldLocation = Location;
		bJustTeleported = 0;

		FVector OldVelocity = Velocity;
		if (!Region.Zone->bWaterZone)
		{
			if ( IsA(ADecoration::StaticClass()) && ((ADecoration *)this)->bBobbing ) 
				Velocity = OldVelocity + 0.5 * (Acceleration + 0.5 * Region.Zone->ZoneGravity) * timeTick; //average velocity for tick
			else if ( IsA(APlayerPawn::StaticClass()) && ((APawn *)this)->FootRegion.Zone->bWaterZone && (OldVelocity.Z < 0) )
				Velocity = OldVelocity * (1 - ((APawn *)this)->FootRegion.Zone->ZoneFluidFriction * timeTick)
							+ 0.5 * (Acceleration + Region.Zone->ZoneGravity) * timeTick; 
			else
				Velocity = OldVelocity + 0.5 * (Acceleration + Region.Zone->ZoneGravity) * timeTick; //average velocity for tick
		}
		else
		{
			Velocity = OldVelocity * (1 - 2 * Region.Zone->ZoneFluidFriction * timeTick) 
					+ 0.5 * (Acceleration + Region.Zone->ZoneGravity * (1.0 - Buoyancy/::Max(1.f,Mass))) * timeTick; 
		}

		if ( !AdjustApex && ((OldVelocity.Z > 0) != (Velocity.Z > 0))
			&& (Abs(OldVelocity.Z) > 5.f) && (Abs(Velocity.Z) > 5.f)) //sign of Z component changed
		{
			AdjustApex = 1;
			FLOAT part = Abs(OldVelocity.Z)/(Abs(OldVelocity.Z) + Abs(Velocity.Z));
			if ((part * timeTick > 0.015) && ((1 - part) * timeTick > 0.015))
			{
				remainingTime = remainingTime + timeTick * (1 - part);
				timeTick = timeTick * part;
				if (!Region.Zone->bWaterZone)
				{
					if ( IsA(ADecoration::StaticClass()) && ((ADecoration *)this)->bBobbing ) 
						Velocity = OldVelocity + 0.5 * (Acceleration + 0.5 * Region.Zone->ZoneGravity) * timeTick; //average velocity for tick
					else if ( IsA(APlayerPawn::StaticClass()) && ((APawn *)this)->FootRegion.Zone->bWaterZone  && (OldVelocity.Z < 0) )
						Velocity = OldVelocity * (1 - ((APawn *)this)->FootRegion.Zone->ZoneFluidFriction * timeTick)
									+ 0.5 * (Acceleration + Region.Zone->ZoneGravity) * timeTick; 
					else
						Velocity = OldVelocity + 0.5 * (Acceleration + Region.Zone->ZoneGravity) * timeTick; //average velocity for tick
				}
				else
					Velocity = OldVelocity * (1 - 2 * Region.Zone->ZoneFluidFriction * timeTick) 
					+ 0.5 * (Acceleration + Region.Zone->ZoneGravity * (1.0 - Buoyancy/::Max(1.f,Mass))) * timeTick; 
			}
		}
		else
			AdjustApex = 0;
		if ( BoundSpeed != 0 )
		{
			// using air control, so make sure not exceeding acceptable speed
			FVector Vel2D = Velocity;
			Vel2D.Z = 0;
			if ( Vel2D.SizeSquared() > BoundSpeed * BoundSpeed )
			{
				Vel2D = Vel2D.SafeNormal();
				Vel2D = Vel2D * BoundSpeed;
				Vel2D.Z = Velocity.Z;
				Velocity = Vel2D;
			}
		}
		FVector ZoneVel = FVector(0,0,0);
		if ( !bIsPawn || IsA(APlayerPawn::StaticClass()) || (Region.Zone->ZoneVelocity.SizeSquared() > 40000) )
			ZoneVel = Region.Zone->ZoneVelocity;

		FVector Adjusted = (Velocity + ZoneVel) * timeTick;

		GetLevel()->MoveActor(this, Adjusted, Rotation, Hit);
		if ( bDeleteMe )
			return;
		else if ( ThisPawn && (Physics == PHYS_Swimming) ) //just entered water
		{
			remainingTime = remainingTime + timeTick * (1.0 - Hit.Time);
			ThisPawn->startSwimming(OldVelocity, timeTick, remainingTime, Iterations);
			return;
		}
		else if ( Hit.Time < 1.0 )
		{
			if ( Hit.Actor->IsA(APlayerPawn::StaticClass()) && this->IsA(ADecoration::StaticClass()) )
				((ADecoration *)this)->numLandings = ::Max<INT>(0, ((ADecoration *)this)->numLandings - 1); 
			if (bBounce)
			{
				eventHitWall(Hit.Normal, Hit.Actor);
				if ( Physics == PHYS_None )
					return;
				else if ( numBounces < 2 )
					remainingTime += timeTick * (1.0 - Hit.Time);
				numBounces++;
			}
			else
			{
				if (Hit.Normal.Z > 0.7)
				{
					remainingTime += timeTick * (1.0 - Hit.Time);
					if (!bJustTeleported && (Hit.Time > 0.1) && (Hit.Time * timeTick > 0.003f) )
						Velocity = (Location - OldLocation)/(timeTick * Hit.Time);
					processLanded(Hit.Normal, Hit.Actor, remainingTime, Iterations);
					return;
				}
				else
				{
					processHitWall(Hit.Normal, Hit.Actor);
					FVector OldHitNormal = Hit.Normal;
					FVector Delta = (Adjusted - Hit.Normal * (Adjusted | Hit.Normal)) * (1.0 - Hit.Time);
					if( (Delta | Adjusted) >= 0 )
					{
						GetLevel()->MoveActor(this, Delta, Rotation, Hit);
						if (Hit.Time < 1.0) //hit second wall
						{
							if ( Hit.Normal.Z > 0.7 )
							{
								remainingTime = 0.0;
								processLanded(Hit.Normal, Hit.Actor, remainingTime, Iterations);
								return;
							}
							else 
								processHitWall(Hit.Normal, Hit.Actor);
		
							FVector DesiredDir = Adjusted.SafeNormal();
							TwoWallAdjust(DesiredDir, Delta, Hit.Normal, OldHitNormal, Hit.Time);
							int bDitch = ( (OldHitNormal.Z > 0) && (Hit.Normal.Z > 0) && (Delta.Z == 0) && ((Hit.Normal | OldHitNormal) < 0) );
							GetLevel()->MoveActor(this, Delta, Rotation, Hit);
							if ( bDitch || (Hit.Normal.Z > 0.7) )
							{
								remainingTime = 0.0;
								processLanded(Hit.Normal, Hit.Actor, remainingTime, Iterations);
								return;
							}
						}
					}
					FLOAT OldZ = OldVelocity.Z;
					OldVelocity = (Location - OldLocation)/timeTick;
					OldVelocity.Z = OldZ;
				}
			}
		}

		//if ( Iterations > 7 )
		//	debugf("More than 7 iterations in falling");
		if (!bBounce && !bJustTeleported)
		{
			// refine the velocity by figuring out the average actual velocity over the tick, and then the final velocity.
			// This particularly corrects for situations where level geometry affected the fall.
			Velocity = (Location - OldLocation)/timeTick - ZoneVel; //actual average velocity
			if ( (Velocity.Z < OldVelocity.Z) || (OldVelocity.Z >= 0) )
				Velocity = 2 * Velocity - OldVelocity; //end velocity has 2* accel of avg
			if (Velocity.SizeSquared() > Region.Zone->ZoneTerminalVelocity * Region.Zone->ZoneTerminalVelocity)
			{
				Velocity = Velocity.SafeNormal();
				Velocity *= Region.Zone->ZoneTerminalVelocity;
			}
		}
	}

	Acceleration = RealAcceleration;
	unguard;
}

void APawn::startSwimming(FVector OldVelocity, FLOAT timeTick, FLOAT remainingTime, INT Iterations)
{
	guard(APawn::startSwimming);
	//debugf("fell into water");
	FVector End = Location;
	findWaterLine(OldLocation, End);
	FLOAT waterTime = 0.0;
	if (End != Location)
	{	
		waterTime = timeTick * (End - Location).Size()/(Location - OldLocation).Size();
		remainingTime += waterTime;
		FCheckResult Hit(1.0);
		GetLevel()->MoveActor(this, End - Location, Rotation, Hit);
	}
	if (!bBounce && !bJustTeleported)
		{
			Velocity = (Location - OldLocation)/(timeTick - waterTime); //actual average velocity
			Velocity = 2 * Velocity - OldVelocity; //end velocity has 2* accel of avg
			if (Velocity.SizeSquared() > 16000000.0)
			{
				Velocity = Velocity.SafeNormal();
				Velocity *= 4000.0;
			}
		//FIXME - calc. velocity more correctly everywhere
		}
	if ((Velocity.Z > -160.f) && (Velocity.Z < 0)) //allow for falling out of water
		Velocity.Z = -80.f - Velocity.Size2D() * 0.7; //smooth bobbing
	if (remainingTime > 0.01)
		physSwimming(remainingTime, Iterations);

	unguard;
}

void APawn::physFlying(FLOAT deltaTime, INT Iterations)
{
	guard(APawn::physFlying);

	FVector AccelDir;

	if ( bCollideWorld && (Region.ZoneNumber == 0) )
	{
		// not in valid spot
		debugf( TEXT("%s flew out of the world!"), GetName());
		if ( !bIsPlayer )
			GetLevel()->DestroyActor( this );
		return;
	}
	if ( Acceleration.IsZero() )
		AccelDir = Acceleration;
	else
		AccelDir = Acceleration.SafeNormal();
	calcVelocity(AccelDir, deltaTime, AirSpeed, Region.Zone->ZoneFluidFriction, 1, 0, 0);  

	Iterations++;
	OldLocation = Location;
	bJustTeleported = 0;
	FVector ZoneVel;
	if ( this->IsA(APlayerPawn::StaticClass()) || (Region.Zone->ZoneVelocity.SizeSquared() > 90000) )
		ZoneVel = Region.Zone->ZoneVelocity;
	else
		ZoneVel = FVector(0,0,0);
	FVector Adjusted = (Velocity + ZoneVel) * deltaTime; 
	FCheckResult Hit(1.0);
	GetLevel()->MoveActor(this, Adjusted, Rotation, Hit);
	if (Hit.Time < 1.0) 
	{
		FVector GravDir = FVector(0,0,-1);
		if (Region.Zone->ZoneGravity.Z > 0)
			GravDir.Z = 1;
		FVector DesiredDir = Adjusted.SafeNormal();
		FVector VelDir = Velocity.SafeNormal();
		FLOAT UpDown = GravDir | VelDir;
		if ( (Abs(Hit.Normal.Z) < 0.2) && (UpDown < 0.5) && (UpDown > -0.2) )
		{
			FLOAT stepZ = Location.Z;
			stepUp(GravDir, DesiredDir, Adjusted * (1.0 - Hit.Time), Hit);
			OldLocation.Z = Location.Z + (OldLocation.Z - stepZ);
		}
		else
		{
			processHitWall(Hit.Normal, Hit.Actor);
			//adjust and try again
			FVector OldHitNormal = Hit.Normal;
			FVector Delta = (Adjusted - Hit.Normal * (Adjusted | Hit.Normal)) * (1.0 - Hit.Time);
			if( (Delta | Adjusted) >= 0 )
			{
				GetLevel()->MoveActor(this, Delta, Rotation, Hit);
				if (Hit.Time < 1.0) //hit second wall
				{
					processHitWall(Hit.Normal, Hit.Actor);
					TwoWallAdjust(DesiredDir, Delta, Hit.Normal, OldHitNormal, Hit.Time);
					GetLevel()->MoveActor(this, Delta, Rotation, Hit);
				}
			}
		}
	}

	if (!bJustTeleported)
		Velocity = (Location - OldLocation) / deltaTime;

	unguard;
}

/* Swimming uses gravity - but scaled by (mass - buoyancy)/mass
This is used only by pawns 

*/
// findWaterLine is temporary until trace supports zone change notification
FLOAT APawn::Swim(FVector Delta, FCheckResult &Hit)
{
	guard(APawn::Swim);
	FVector Start = Location;
	FLOAT airTime = 0.0;
	GetLevel()->MoveActor(this, Delta, Rotation, Hit);
	FVector End = Location;
	if (!Region.Zone->bWaterZone) //then left water
	{
		findWaterLine(Start, End);
		if (End != Location)
		{
			airTime = (End - Location).Size()/Delta.Size();
			GetLevel()->MoveActor(this, End - Location, Rotation, Hit);
		}
	}
	return airTime;
	unguard;
}

//get as close to waterline as possible, staying on same side as currently
void APawn::findWaterLine(FVector Start, FVector &End)
{
	guard(APawn::findWaterLine);
	if ((End - Start).SizeSquared() < 1.0)
		return; //current value of End is acceptable

	FVector MidPoint = 0.5 * (Start + End);
	FPointRegion NewRegion = GetLevel()->Model->PointRegion( Level, MidPoint );
	if( NewRegion.Zone->bWaterZone != Region.Zone->bWaterZone )
		Start = MidPoint; 
	else
		End = MidPoint;

	findWaterLine(Start, End);

	unguard;
}

void APawn::physSwimming(FLOAT deltaTime, INT Iterations)
{
	guard(APawn::physSwimming);

#if defined(LEGEND) //LEGEND
	if( CheckSurfaces( this, deltaTime, Iterations ) )
		return;
#endif

	if (!HeadRegion.Zone->bWaterZone && (Velocity.Z > 100.f))
		//damp positive Z out of water
		Velocity.Z = Velocity.Z * (1 - deltaTime);

	Iterations++;
	OldLocation = Location;
	bJustTeleported = 0;
	FVector AccelDir;
	if ( Acceleration.IsZero() )
		AccelDir = Acceleration;
	else
		AccelDir = Acceleration.SafeNormal();
	calcVelocity(AccelDir, deltaTime, WaterSpeed, Region.Zone->ZoneFluidFriction, 1, 0, 1);  
	FLOAT velZ = Velocity.Z;
	FVector ZoneVel;
	if ( this->IsA(APlayerPawn::StaticClass()) || (Region.Zone->ZoneVelocity.SizeSquared() > 90000) )
	{
		// Add effect of velocity zone
		// Rather than constant velocity, hacked to make sure that velocity being clamped when swimming doesn't 
		// cause the zone velocity to have too much of an effect at fast frame rates

		ZoneVel = Region.Zone->ZoneVelocity * 25 * deltaTime;
	}
	else
		ZoneVel = FVector(0,0,0);
	FVector Adjusted = (Velocity + ZoneVel) * deltaTime; 
	FCheckResult Hit(1.0);
	FLOAT remainingTime = deltaTime * Swim(Adjusted, Hit);

	if (Hit.Time < 1.0) 
	{
		FVector GravDir = FVector(0,0,-1);
		if (Region.Zone->ZoneGravity.Z > 0)
			GravDir.Z = 1;
		FVector DesiredDir = Adjusted.SafeNormal();
		FVector VelDir = Velocity.SafeNormal();
		FLOAT UpDown = GravDir | VelDir;
		if ( (Abs(Hit.Normal.Z) < 0.2) && (UpDown < 0.5) && (UpDown > -0.2) )
		{
			FLOAT stepZ = Location.Z;
			stepUp(GravDir, DesiredDir, Adjusted * (1.0 - Hit.Time), Hit);
			OldLocation.Z = Location.Z + (OldLocation.Z - stepZ);
		}
		else
		{
			processHitWall(Hit.Normal, Hit.Actor);
			//adjust and try again
			FVector OldHitNormal = Hit.Normal;
			FVector Delta = (Adjusted - Hit.Normal * (Adjusted | Hit.Normal)) * (1.0 - Hit.Time);
			if( (Delta | Adjusted) >= 0 )
			{
				remainingTime = remainingTime * (1.0 - Hit.Time) * Swim(Delta, Hit);
				if (Hit.Time < 1.0) //hit second wall
				{
					processHitWall(Hit.Normal, Hit.Actor);
					TwoWallAdjust(DesiredDir, Delta, Hit.Normal, OldHitNormal, Hit.Time);
					remainingTime = remainingTime * (1.0 - Hit.Time) * Swim(Delta, Hit);
				}
			}
		}
	}

	if (!bJustTeleported && (remainingTime < deltaTime))
	{
		int bWaterJump = (velZ != Velocity.Z); //changed by script
		if (bWaterJump)
			velZ = Velocity.Z;
		Velocity = (Location - OldLocation) / (deltaTime - remainingTime);
		if (bWaterJump)
			Velocity.Z = velZ;
	}

	if (!Region.Zone->bWaterZone)
	{
		if (Physics == PHYS_Swimming)
			setPhysics(PHYS_Falling); //in case script didn't change it (w/ zone change)
		if ((Velocity.Z < 160.f) && (Velocity.Z > 0)) //allow for falling out of water
			Velocity.Z = 40.f + Velocity.Size2D() * 0.4; //smooth bobbing
	}

	if (remainingTime > 0.01) //may have left water - if so, script might have set new physics mode
	{
		if (Physics == PHYS_Falling) 
			physFalling(remainingTime, Iterations);
		else if (Physics == PHYS_Flying)
			physFlying(remainingTime, Iterations);
	}

	unguard;
}

/* PhysProjectile is tailored for projectiles 
*/
void AActor::physProjectile(FLOAT deltaTime, INT Iterations)
{
	guard(AActor::physProjectile);

	//bound acceleration, calculate velocity, add effects of friction and momentum
	//friction affects projectiles less (more aerodynamic)
	FLOAT remainingTime = deltaTime;
	int numBounces = 0;

	if ( Region.ZoneNumber == 0 )
	{
		GetLevel()->DestroyActor( this );
		return;
	}

	OldLocation = Location;
	bJustTeleported = 0;
	FCheckResult Hit(1.0);

	while ( (remainingTime > 0.0) && (Iterations < 8) )
	{
		Iterations++;
		if ( Region.Zone->bWaterZone )
			Velocity = (Velocity * (1 - 0.2 * Region.Zone->ZoneFluidFriction * remainingTime));
		Velocity = Velocity	+ Acceleration * remainingTime;
		FLOAT timeTick = remainingTime;
		remainingTime = 0.0;

		if ( this->IsA(AProjectile::StaticClass()) 
			&& (Velocity.SizeSquared() > ((AProjectile *)this)->MaxSpeed * ((AProjectile *)this)->MaxSpeed) )
		{
			Velocity = Velocity.SafeNormal();
			Velocity *= ((AProjectile *)this)->MaxSpeed;
		}

		FVector Adjusted = Velocity * deltaTime; 
		Hit.Time = 1.0;
		GetLevel()->MoveActor(this, Adjusted, Rotation, Hit);
		
		if ( (Hit.Time < 1.0) && !bDeleteMe && !bJustTeleported )
		{
			FVector DesiredDir = Adjusted.SafeNormal();
			eventHitWall(Hit.Normal, Hit.Actor);
			if (bBounce)
			{
				if (numBounces < 2)
					remainingTime = timeTick * (1.0 - Hit.Time);
				numBounces++;
				if (Physics == PHYS_Falling)
					physFalling(remainingTime, Iterations);
			}
		}
	}

	//if ( Iterations > 7 )
	//	debugf("Projectile with too many physics iterations!");
	if (!bBounce && !bJustTeleported)
		Velocity = (Location - OldLocation) / deltaTime;

	unguard;
}

/*
physRolling() - intended for non-pawns which are rolling or sliding along a floor

*/

void AActor::physRolling(FLOAT deltaTime, INT Iterations)
{
	guard(APawn::physRolling);
	//bound acceleration
	//goal - support +-Z gravity, but not other vectors
	//note that Z components of velocity and acceleration are not zeroed
	FVector VelDir = Velocity.SafeNormal();
	FVector AccelDir = Acceleration.SafeNormal();
	Velocity = Velocity - (VelDir - AccelDir) * Velocity.Size()
		* deltaTime * Region.Zone->ZoneGroundFriction; 

	Velocity = Velocity * (1 - Region.Zone->ZoneFluidFriction * deltaTime) + Acceleration * deltaTime;
	FVector DesiredMove = Velocity + Region.Zone->ZoneVelocity;
	OldLocation = Location;
	bJustTeleported = 0;

	//-------------------------------------------------------------------------------------------
	//Perform the move
	FLOAT remainingTime = deltaTime;
	FLOAT timeTick = 0.1;
	FVector GravDir = FVector(0,0,-1);
	if (Region.Zone->ZoneGravity.Z > 0)
		GravDir.Z = 1; 
	FVector Down = GravDir * 16.0;
	FCheckResult Hit(1.0);
	int numBounces = 0;
	while ( (remainingTime > 0.0) && (Iterations < 8) )
	{
		Iterations++;
		if (remainingTime > 0.1)
			timeTick = Min(0.1f, remainingTime * 0.5f);
		else timeTick = remainingTime;

		remainingTime -= timeTick;
		FVector Delta = timeTick * DesiredMove;
		FVector SubMove = Delta;
		FVector SubLoc = Location;
		if (!Delta.IsNearlyZero())
		{
			GetLevel()->MoveActor(this, Delta, Rotation, Hit);
			if (Hit.Time < 1.0) 
			{
				eventHitWall(Hit.Normal, Hit.Actor);
				if (bBounce)
				{
					if (numBounces < 2)
						remainingTime += timeTick * (1.0 - Hit.Time);
					numBounces++;
				}
				else
				{
						//adjust and try again
						FVector OriginalDelta = Delta;
			
						// Try again.
						FVector OldHitNormal = Hit.Normal;
						Delta = (Delta - Hit.Normal * (Delta | Hit.Normal)) * (1.0 - Hit.Time);
						if( (Delta | OriginalDelta) >= 0 )
						{
							GetLevel()->MoveActor(this, Delta, Rotation, Hit);
							if (Hit.Time < 1.0)
							{
								eventHitWall(Hit.Normal, Hit.Actor);
								FVector DesiredDir = DesiredMove.SafeNormal();
								TwoWallAdjust(DesiredDir, Delta, Hit.Normal, OldHitNormal, Hit.Time);
								GetLevel()->MoveActor(this, Delta, Rotation, Hit);
							}
						}
				}
			}
		}

		//drop to floor
		GetLevel()->MoveActor(this, Down, Rotation, Hit);
		FLOAT DropTime = Hit.Time;
		FLOAT DropHitZ = Hit.Normal.Z;
		if (DropTime < 1.0) //slide down slope, depending on friction and gravity 
		{
			if ((Hit.Normal.Z < 1.0) && ((Hit.Normal.Z * Region.Zone->ZoneGroundFriction) < 3.3))
			{
				FVector Slide = (deltaTime * Region.Zone->ZoneGravity/(2 * ::Max(0.5f, Region.Zone->ZoneGroundFriction))) * deltaTime;
				Delta = Slide - Hit.Normal * (Slide | Hit.Normal);
				if( (Delta | Slide) >= 0 )
				{
					GetLevel()->MoveActor(this, Delta, Rotation, Hit);
					DropHitZ = ::Max(DropHitZ, Hit.Normal.Z);
				}
			}				
		}

		if ((DropTime == 1.0) || (DropHitZ < 0.7)) //then falling
		{
			FVector AdjustUp = -1 * (Down * DropTime); 
			GetLevel()->MoveActor(this, AdjustUp, Rotation, Hit);
			FLOAT DesiredDist = SubMove.Size();
			FLOAT ActualDist = (Location - SubLoc).Size2D();
			remainingTime += timeTick * (1 - Min(1.f,ActualDist/DesiredDist)); 
			eventFalling();
			if (Physics == PHYS_Rolling)
				setPhysics(PHYS_Falling); //default if script didn't change physics
			if (Physics == PHYS_Falling)
			{
				if (!bJustTeleported && (deltaTime > remainingTime))
					Velocity = (Location - OldLocation)/(deltaTime - remainingTime);
				Velocity.Z = 0.0;

				if (remainingTime > 0.005)
					physFalling(remainingTime, Iterations);
				return;
			}
			else 
			{
				Delta = remainingTime * DesiredMove;
				GetLevel()->MoveActor(this, Delta, Rotation, Hit); 
			}
		}
		else if( Hit.Actor != Base)
		{
			// Handle floor notifications (standing on other actors).
			//debugf("%s is now on floor %s",GetFullName(),Hit.Actor ? Hit.Actor->GetFullName() : "None");
			SetBase( Hit.Actor );
		}			//drop to floor

	}
	// make velocity reflect actual move
	if (!bJustTeleported)
		Velocity = (Location - OldLocation) / deltaTime;
 	unguard;
}


/*
physSpider()

*/
inline int APawn::checkFloor(FVector Dir, FCheckResult &Hit)
{
	GetLevel()->SingleLineCheck(Hit, 0, Location - MaxStepHeight * Dir, Location, TRACE_VisBlocking, GetCylinderExtent());
	if (Hit.Time < 1.0)
	{
		Floor = Hit.Normal;
		return 1;
	}
	return 0;
}

int APawn::findNewFloor(FVector OldLocation, FLOAT deltaTime, FLOAT remainingTime, INT Iterations)
{
	guard(APawn::findNewFloor);

	//look for floor
	FCheckResult Hit(1.0);
	//debugf("Find new floor for %s", GetFullName());
	if ( checkFloor(FVector(0,0,1), Hit) )
		return 1;
	if ( checkFloor(FVector(0,1,0), Hit) )
		return 1;
	if ( checkFloor(FVector(0,-1,0), Hit) )
		return 1;
	if ( checkFloor(FVector(1,0,0), Hit) )
		return 1;
	if ( checkFloor(FVector(-1,0,0), Hit) )
		return 1;

	// Fall
	eventFalling();
	if (Physics == PHYS_Spider)
		setPhysics(PHYS_Falling); //default if script didn't change physics
	if (Physics == PHYS_Falling)
	{
		FLOAT velZ = Velocity.Z;
		if (!bJustTeleported && (deltaTime > remainingTime))
			Velocity = (Location - OldLocation)/(deltaTime - remainingTime);
		Velocity.Z = velZ;
		if (remainingTime > 0.005)
			physFalling(remainingTime, Iterations);
	}

	return 0;

	unguard;
}

//#pragma DISABLE_OPTIMIZATION
void APawn::physSpider(FLOAT deltaTime, INT Iterations)
{
	guard(APawn::physSpider);

	//calculate velocity
	FVector AccelDir;
	if ( Acceleration.IsZero() ) 
	{
		AccelDir = Acceleration;
		FVector OldVel = Velocity;
		Velocity = Velocity - (2 * Velocity) * deltaTime * Region.Zone->ZoneGroundFriction; //don't drift to a stop, brake
		if ((OldVel | Velocity) < 0.0) //brake to a stop, not backwards
			Velocity = Acceleration;
	}
	else
	{
		AccelDir = Acceleration.SafeNormal();
		FLOAT VelSize = Velocity.Size();
		if (Acceleration.SizeSquared() > AccelRate * AccelRate)
			Acceleration = AccelDir * AccelRate;
		Velocity = Velocity - (Velocity - AccelDir * VelSize) * deltaTime * Region.Zone->ZoneGroundFriction;  
	}

	Velocity = Velocity + Acceleration * deltaTime;
	FLOAT maxSpeed = GroundSpeed * DesiredSpeed;
	Iterations++;

	if (Velocity.SizeSquared() > maxSpeed * maxSpeed)
	{
		Velocity = Velocity.SafeNormal();
		Velocity *= maxSpeed;
	}
	FVector ZoneVel;
	if ( Region.Zone->ZoneVelocity.SizeSquared() > 90000 )
		ZoneVel = Region.Zone->ZoneVelocity;
	else
		ZoneVel = FVector(0,0,0);
	FVector DesiredMove = Velocity + ZoneVel;
	FLOAT MoveSize = DesiredMove.Size();
	FVector DesiredDir = DesiredMove/MoveSize;

	//Perform the move
	// Look for supporting wall
	int bFindNewFloor = Floor.IsNearlyZero();
	FCheckResult Hit(1.0);
	FVector GravDir = -1 * Floor;
	FVector Down = GravDir * (MaxStepHeight + 4.0);
	DesiredRotation = Rotation;
	if (!bFindNewFloor)
	{
		GetLevel()->SingleLineCheck(Hit, 0, Location + Down, Location, TRACE_VisBlocking, GetCylinderExtent());
		bFindNewFloor = (Hit.Time == 1.0);
	}
	if (bFindNewFloor)
	{
		if ( !findNewFloor(Location, deltaTime, deltaTime, Iterations) ) //find new floor or fall
			return;
		else
		{
			GravDir = -1 * Floor;
			Down = GravDir * (MaxStepHeight + 4.0);
		}
	}

	DesiredRotation = Floor.Rotation();
	DesiredRotation.Pitch -= 16384;
	DesiredRotation.Roll = 0;

	// modify desired move based on floor
	FLOAT dotp = AccelDir | Floor;
	FVector realDir = DesiredDir;
	if ( (Floor.Z < 0.6) && (dotp > 0.9) )
	{
		Floor = FVector(0,0,0);
		eventFalling();
		setPhysics(PHYS_Falling); 
		physFalling(deltaTime, Iterations);
		return;
	}
	else
	{
		DesiredDir = DesiredDir - Floor * (DesiredDir | Floor);
		DesiredDir = DesiredDir.SafeNormal();
	}

	OldLocation = Location;
	bJustTeleported = 0;

	FLOAT remainingTime = deltaTime;
	FLOAT timeTick = 0.05;
	DesiredMove = MoveSize * DesiredDir;
	while ( (remainingTime > 0.0) && (Iterations < 8) )
	{
		Iterations++;
		if (remainingTime > 0.05)
			timeTick = Min(0.05f, remainingTime * 0.5f);
		else timeTick = remainingTime;
		remainingTime -= timeTick;
		FVector Delta = timeTick * DesiredMove;
		FVector subLoc = Location;
		FVector subMove = Delta;

		if (!Delta.IsNearlyZero())
		{
			GetLevel()->MoveActor(this, Delta, DesiredRotation, Hit);
			if (Hit.Time < 1.0) 
			{
				if (Hit.Normal.Z >= 0)
				{
					if ( ((Hit.Normal | realDir) < 0) && ((Floor | realDir) < 0) ) 
						eventHitWall(Hit.Normal, Hit.Actor);
					else
					{
						FVector Combo = (Hit.Normal + Floor).SafeNormal();
						if ( (realDir | Combo) > 0.9 )
							eventHitWall(Hit.Normal, Hit.Actor);
					}
					Floor = Hit.Normal;
					GravDir = -1 * Floor;
					Down = GravDir * (MaxStepHeight + 4.0);
				}
				else if ( (Hit.Normal | realDir) < 0 ) 
					eventHitWall(Hit.Normal, Hit.Actor);
				else if ( (Floor | realDir) > 0.7 )
				{
					eventFalling();
					if (Physics == PHYS_Spider)
						setPhysics(PHYS_Falling); //default if script didn't change physics
					if (Physics == PHYS_Falling)
					{
						FLOAT velZ = Velocity.Z;
						if (!bJustTeleported && (deltaTime > remainingTime))
							Velocity = (Location - OldLocation)/(deltaTime - remainingTime);
						Velocity.Z = velZ;
						if (remainingTime > 0.005)
							physFalling(remainingTime, Iterations);
						return;
					}
				}
				FVector DesiredDir = Delta.SafeNormal();
				stepUp(GravDir, DesiredDir, Delta * (1.0 - Hit.Time), Hit);
				if (Physics == PHYS_Falling)
				{
					if (remainingTime > 0.005)
						physFalling(remainingTime, Iterations);
					return;
				}
			}
		}

		//drop to floor
		GetLevel()->MoveActor(this, Down, Rotation, Hit);
		if (Hit.Time == 1.0) //then find new floor or fall
		{
			if ( findNewFloor(OldLocation, deltaTime, remainingTime, Iterations) )
			{
				GravDir = -1 * Floor;
				Down = GravDir * (MaxStepHeight + 4.0);
			}
			else
				return;
		}
		else 
		{
			Floor = Hit.Normal;
			if( Hit.Actor != Base && !Hit.Actor->IsA(APawn::StaticClass()) )
			// Handle floor notifications (standing on other actors).
				SetBase( Hit.Actor );
		}
	}

	// make velocity reflect actual move
	if (!bJustTeleported)
		Velocity = (Location - OldLocation) / deltaTime;
	unguard;
}
//#pragma ENABLE_OPTIMIZATION

void AActor::physTrailer(FLOAT deltaTime)
{
	guard(APawn::physTrailer);

	FRotator trailRot;
	if ( !Owner )
		return;
	if ( DrawType == DT_Sprite )
	{
		if ( bTrailerPrePivot )
			GetLevel()->FarMoveActor(this, Owner->Location + PrePivot, 0, 1);
		else if (bTrailerSameRotation )
			GetLevel()->FarMoveActor(this, Owner->Location - Mass * Owner->Rotation.Vector(), 0, 1);
		else
			GetLevel()->FarMoveActor(this, Owner->Location, 0, 1);
		return;
	}
	GetLevel()->FarMoveActor(this, Owner->Location, 0, 1);
	FCheckResult Hit(1.0);
	if ( bTrailerSameRotation )
		trailRot = Owner->Rotation;
	else if ( Owner->Velocity.IsNearlyZero() )
		trailRot = FRotator(16384,0,0);
	else
		trailRot = (-1 * Owner->Velocity).Rotation();

	GetLevel()->MoveActor(this, FVector(0,0,0), trailRot, Hit);

	unguard;
}
