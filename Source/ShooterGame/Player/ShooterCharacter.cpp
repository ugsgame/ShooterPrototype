// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ShooterGame.h"
#include "Weapons/ShooterWeapon.h"
#include "Weapons/ShooterDamageType.h"
#include "ShooterVehicle.h"
#include "WheeledVehicleMovementComponent.h"
#include "UI/ShooterHUD.h"
#include "Online/ShooterPlayerState.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "Sound/SoundNodeLocalPlayer.h"
#include "AudioThread.h"

static int32 NetVisualizeRelevancyTestPoints = 0;
FAutoConsoleVariableRef CVarNetVisualizeRelevancyTestPoints(
	TEXT("p.NetVisualizeRelevancyTestPoints"),
	NetVisualizeRelevancyTestPoints,
	TEXT("")
	TEXT("0: Disable, 1: Enable"),
	ECVF_Cheat);


static int32 NetEnablePauseRelevancy = 1;
FAutoConsoleVariableRef CVarNetEnablePauseRelevancy(
	TEXT("p.NetEnablePauseRelevancy"),
	NetEnablePauseRelevancy,
	TEXT("")
	TEXT("0: Disable, 1: Enable"),
	ECVF_Cheat);

AShooterCharacter::AShooterCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UShooterCharacterMovement>(ACharacter::CharacterMovementComponentName))
{
	Mesh1P = ObjectInitializer.CreateDefaultSubobject<USkeletalMeshComponent>(this, TEXT("PawnMesh1P"));
	Mesh1P->SetupAttachment(GetCapsuleComponent());
	Mesh1P->bOnlyOwnerSee = true;
	Mesh1P->bOwnerNoSee = false;
	Mesh1P->bCastDynamicShadow = false;
	Mesh1P->bReceivesDecals = false;
	Mesh1P->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
	Mesh1P->PrimaryComponentTick.TickGroup = TG_PrePhysics;
	Mesh1P->SetCollisionObjectType(ECC_Pawn);
	Mesh1P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh1P->SetCollisionResponseToAllChannels(ECR_Ignore);

	GetMesh()->bOnlyOwnerSee = false;
	GetMesh()->bOwnerNoSee = true;
	GetMesh()->bReceivesDecals = false;
	GetMesh()->SetCollisionObjectType(ECC_Pawn);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	GetMesh()->SetCollisionResponseToChannel(COLLISION_WEAPON, ECR_Block);
	GetMesh()->SetCollisionResponseToChannel(COLLISION_PROJECTILE, ECR_Block);
	GetMesh()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	GetMesh()->SetCollisionResponseToChannel(ECC_Vehicle, ECR_Block);

	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetCapsuleComponent()->SetCollisionResponseToChannel(COLLISION_PROJECTILE, ECR_Block);
	GetCapsuleComponent()->SetCollisionResponseToChannel(COLLISION_WEAPON, ECR_Ignore);

	/*
	SpringArm = ObjectInitializer.CreateDefaultSubobject<USpringArmComponent>(this, TEXT("SprintArm"));
	SpringArm->SetupAttachment(GetCapsuleComponent());
	SpringArm->bUsePawnControlRotation = true;
	SpringArm->TargetArmLength = 0.0f;
	SpringArm->SetRelativeLocation(FVector(0, 0, 63.5f));

	Camera = ObjectInitializer.CreateDefaultSubobject<UCameraComponent>(this, TEXT("Camera"));
	Camera->SetupAttachment(SpringArm);
	*/

	SetupSpringArmAndCamera(ObjectInitializer);

	PawnNoiseEmitter = ObjectInitializer.CreateDefaultSubobject<UPawnNoiseEmitterComponent>(this, TEXT("PawnNoiseEmitter"));

	TargetingSpeedModifier = 0.5f;
	bIsTargeting = false;
	RunningSpeedModifier = 1.5f;
	bDestroyAfterDeath = true;
	bWantsToRun = false;
	bWantsToFire = false;
	LowHealthPercentage = 0.5f;

	MaxHealth = 100.0f;
	Health = MaxHealth;

	BaseTurnRate = 45.f;
	BaseLookUpRate = 45.f;

	TPS_TargetArmLengthOffset = 100.0f;
	TPS_SocketOffset = FVector(0.0f, 50.0f, 0.0f);

	TSS_TargetArmLengthOffset = 600.0f;
	TSS_SprintArmRotationOffset = FRotator(-45.0f, 0.0f, 0.0f);

	bCameraFollowToCursor = true;
	bFaceToCursor = true;
	bShootWithMouseInWorld = false;
	RotationMidDistance = 50.0f;
	MoveForwardWorldDirection = FVector(1.0f, 0, 0);
	MoveRightWorldDirection = FVector(0, 1.0f, 0);
}

void AShooterCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	if (Role == ROLE_Authority)
	{
		Health = GetMaxHealth();
		SpawnDefaultInventory();
	}

	// change player type
	//ChangePlayerType(ePlayerType);

	// set initial mesh visibility (3rd person view)
	UpdatePawnMeshes();

	// create material instance for setting team colors (3rd person view)
	for (int32 iMat = 0; iMat < GetMesh()->GetNumMaterials(); iMat++)
	{
		MeshMIDs.Add(GetMesh()->CreateAndSetMaterialInstanceDynamic(iMat));
	}

	// play respawn effects
	if (GetNetMode() != NM_DedicatedServer)
	{
		if (RespawnFX)
		{
			UGameplayStatics::SpawnEmitterAtLocation(this, RespawnFX, GetActorLocation(), GetActorRotation());
		}

		if (RespawnSound)
		{
			UGameplayStatics::PlaySoundAtLocation(this, RespawnSound, GetActorLocation());
		}
	}
}

void AShooterCharacter::Destroyed()
{
	Super::Destroyed();
	DestroyInventory();
}

void AShooterCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();

	// switch mesh to 1st person view
	//UpdatePawnMeshes();
	AShooterPlayerController* PC = Cast<AShooterPlayerController>(Controller);
	if (PC)
	{
		OnChangePlayerType(PC->GetPlayerType());
	}

	// reattach weapon if needed
	SetCurrentWeapon(CurrentWeapon);

	// set team colors for 1st person view
	UMaterialInstanceDynamic* Mesh1PMID = Mesh1P->CreateAndSetMaterialInstanceDynamic(0);
	UpdateTeamColors(Mesh1PMID);
}

void AShooterCharacter::PossessedBy(class AController* InController)
{
	Super::PossessedBy(InController);

	// [server] as soon as PlayerState is assigned, set team colors of this pawn for local player
	UpdateTeamColorsAllMIDs();

	AShooterPlayerController* PC = Cast<AShooterPlayerController>(Controller);
	if (PC)
	{
		OnChangePlayerType(PC->GetPlayerType());
	}
}

void AShooterCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	// [client] as soon as PlayerState is assigned, set team colors of this pawn for local player
	if (GetPlayerState() != NULL)
	{
		UpdateTeamColorsAllMIDs();
	}
}

FRotator AShooterCharacter::GetAimOffsets() const
{
	if (PlayerType == EShooterPlayerType::TSS)
	{
		AShooterPlayerController* PC = Cast<AShooterPlayerController>(Controller);
		if (PC)
		{
			return bShootWithMouseInWorld ? FRotator(PC->GetControlRotation().Pitch, 0, 0) : FRotator(0,0,0);
		}
		else
		{
			return FRotator(0,0,0);
		}
	}
	else
	{
		const FVector AimDirWS = GetBaseAimRotation().Vector();
		const FVector AimDirLS = ActorToWorld().InverseTransformVectorNoScale(AimDirWS);
		const FRotator AimRotLS = AimDirLS.Rotation();
		return  AimRotLS;
	}
}

bool AShooterCharacter::IsEnemyFor(AController* TestPC) const
{
	if (TestPC == Controller || TestPC == NULL)
	{
		return false;
	}

	AShooterPlayerState* TestPlayerState = Cast<AShooterPlayerState>(TestPC->PlayerState);
	AShooterPlayerState* MyPlayerState = Cast<AShooterPlayerState>(GetPlayerState());

	bool bIsEnemy = true;
	if (GetWorld()->GetGameState())
	{
		const AShooterGameMode* DefGame = GetWorld()->GetGameState()->GetDefaultGameMode<AShooterGameMode>();
		if (DefGame && MyPlayerState && TestPlayerState)
		{
			bIsEnemy = DefGame->CanDealDamage(TestPlayerState, MyPlayerState);
		}
	}

	return bIsEnemy;
}

//////////////////////////////////////////////////////////////////////////
// Meshes

void AShooterCharacter::UpdatePawnMeshes()
{
	bool const bFirstPerson = IsFirstPerson();

	Mesh1P->VisibilityBasedAnimTickOption = !bFirstPerson ? EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered : EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	Mesh1P->SetOwnerNoSee(!bFirstPerson);

	GetMesh()->VisibilityBasedAnimTickOption = bFirstPerson ? EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered : EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	GetMesh()->SetOwnerNoSee(bFirstPerson);
}

void AShooterCharacter::UpdateTeamColors(UMaterialInstanceDynamic* UseMID)
{
	if (UseMID)
	{
		AShooterPlayerState* MyPlayerState = Cast<AShooterPlayerState>(GetPlayerState());
		if (MyPlayerState != NULL)
		{
			float MaterialParam = (float)MyPlayerState->GetTeamNum();
			UseMID->SetScalarParameterValue(TEXT("Team Color Index"), MaterialParam);
		}
	}
}

void AShooterCharacter::OnCameraUpdate(const FVector& CameraLocation, const FRotator& CameraRotation)
{
	USkeletalMeshComponent* DefMesh1P = Cast<USkeletalMeshComponent>(GetClass()->GetDefaultSubobjectByName(TEXT("PawnMesh1P")));
	const FMatrix DefMeshLS = FRotationTranslationMatrix(DefMesh1P->RelativeRotation, DefMesh1P->RelativeLocation);
	const FMatrix LocalToWorld = ActorToWorld().ToMatrixWithScale();

	// Mesh rotating code expect uniform scale in LocalToWorld matrix

	const FRotator RotCameraPitch(CameraRotation.Pitch, 0.0f, 0.0f);
	const FRotator RotCameraYaw(0.0f, CameraRotation.Yaw, 0.0f);

	const FMatrix LeveledCameraLS = FRotationTranslationMatrix(RotCameraYaw, CameraLocation) * LocalToWorld.Inverse();
	const FMatrix PitchedCameraLS = FRotationMatrix(RotCameraPitch) * LeveledCameraLS;
	const FMatrix MeshRelativeToCamera = DefMeshLS * LeveledCameraLS.Inverse();
	const FMatrix PitchedMesh = MeshRelativeToCamera * PitchedCameraLS;

	Mesh1P->SetRelativeLocationAndRotation(PitchedMesh.GetOrigin(), PitchedMesh.Rotator());
}

void AShooterCharacter::SetupSpringArmAndCamera(const FObjectInitializer& ObjectInitializer)
{
	SpringArm = ObjectInitializer.CreateDefaultSubobject<USpringArmComponent>(this, TEXT("SprintArm"));
	SpringArm->SetupAttachment(GetCapsuleComponent());
	SpringArm->bUsePawnControlRotation = true;
	SpringArm->TargetArmLength = 0.0f;
	SpringArm->SetRelativeLocation(FVector(0, 0, 63.5f));

	Camera = ObjectInitializer.CreateDefaultSubobject<UCameraComponent>(this, TEXT("Camera"));
	Camera->SetupAttachment(SpringArm);
}

void AShooterCharacter::RemoveSpringArmAndCamera()
{
	FDetachmentTransformRules DetachmentRules(EDetachmentRule::KeepRelative, true);

	Camera->DetachFromComponent(DetachmentRules);
	SpringArm->DetachFromComponent(DetachmentRules);

	Camera->DetachFromComponent(DetachmentRules);
	SpringArm->DetachFromComponent(DetachmentRules);
}

//////////////////////////////////////////////////////////////////////////
// Damage & death


void AShooterCharacter::FellOutOfWorld(const class UDamageType& dmgType)
{
	Die(Health, FDamageEvent(dmgType.GetClass()), NULL, NULL);
}

void AShooterCharacter::Suicide()
{
	KilledBy(this);
}

void AShooterCharacter::KilledBy(APawn* EventInstigator)
{
	if (Role == ROLE_Authority && !bIsDying)
	{
		AController* Killer = NULL;
		if (EventInstigator != NULL)
		{
			Killer = EventInstigator->Controller;
			LastHitBy = NULL;
		}

		Die(Health, FDamageEvent(UDamageType::StaticClass()), Killer, NULL);
	}
}


float AShooterCharacter::TakeDamage(float Damage, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, class AActor* DamageCauser)
{
	AShooterPlayerController* MyPC = Cast<AShooterPlayerController>(Controller);
	if (MyPC && MyPC->HasGodMode())
	{
		return 0.f;
	}

	if (Health <= 0.f)
	{
		return 0.f;
	}

	if (LockHealth)
	{
		return 0.f;
	}

	// Modify based on game rules.
	AShooterGameMode* const Game = GetWorld()->GetAuthGameMode<AShooterGameMode>();
	Damage = Game ? Game->ModifyDamage(Damage, this, DamageEvent, EventInstigator, DamageCauser) : 0.f;

	const float ActualDamage = Super::TakeDamage(Damage, DamageEvent, EventInstigator, DamageCauser);
	if (ActualDamage > 0.f)
	{
		Health -= ActualDamage;
		if (Health <= 0)
		{
			Die(ActualDamage, DamageEvent, EventInstigator, DamageCauser);
		}
		else
		{
			PlayHit(ActualDamage, DamageEvent, EventInstigator ? EventInstigator->GetPawn() : NULL, DamageCauser);
		}

		MakeNoise(1.0f, EventInstigator ? EventInstigator->GetPawn() : this);
	}

	return ActualDamage;
}


bool AShooterCharacter::CanDie(float KillingDamage, FDamageEvent const& DamageEvent, AController* Killer, AActor* DamageCauser) const
{
	if (bIsDying										// already dying
		|| IsPendingKill()								// already destroyed
		|| Role != ROLE_Authority						// not authority
		|| GetWorld()->GetAuthGameMode<AShooterGameMode>() == NULL
		|| GetWorld()->GetAuthGameMode<AShooterGameMode>()->GetMatchState() == MatchState::LeavingMap)	// level transition occurring
	{
		return false;
	}

	return true;
}


bool AShooterCharacter::Die(float KillingDamage, FDamageEvent const& DamageEvent, AController* Killer, AActor* DamageCauser)
{
	if (!CanDie(KillingDamage, DamageEvent, Killer, DamageCauser))
	{
		return false;
	}

	Health = FMath::Min(0.0f, Health);

	// if this is an environmental death then refer to the previous killer so that they receive credit (knocked into lava pits, etc)
	UDamageType const* const DamageType = DamageEvent.DamageTypeClass ? DamageEvent.DamageTypeClass->GetDefaultObject<UDamageType>() : GetDefault<UDamageType>();
	Killer = GetDamageInstigator(Killer, *DamageType);

	AController* const KilledPlayer = (Controller != NULL) ? Controller : Cast<AController>(GetOwner());
	GetWorld()->GetAuthGameMode<AShooterGameMode>()->Killed(Killer, KilledPlayer, this, DamageType);

	NetUpdateFrequency = GetDefault<AShooterCharacter>()->NetUpdateFrequency;
	GetCharacterMovement()->ForceReplicationUpdate();

	OnDeath(KillingDamage, DamageEvent, Killer ? Killer->GetPawn() : NULL, DamageCauser);
	return true;
}


void AShooterCharacter::OnDeath(float KillingDamage, struct FDamageEvent const& DamageEvent, class APawn* PawnInstigator, class AActor* DamageCauser)
{
	if (bIsDying)
	{
		return;
	}

	bReplicateMovement = false;
	TearOff();
	bIsDying = true;

	if (Role == ROLE_Authority)
	{
		ReplicateHit(KillingDamage, DamageEvent, PawnInstigator, DamageCauser, true);

		// play the force feedback effect on the client player controller
		APlayerController* PC = Cast<APlayerController>(Controller);
		if (PC && DamageEvent.DamageTypeClass)
		{
			UShooterDamageType *DamageType = Cast<UShooterDamageType>(DamageEvent.DamageTypeClass->GetDefaultObject());
			if (DamageType && DamageType->KilledForceFeedback)
			{
				FForceFeedbackParameters Params;
				Params.Tag = "Damage";
				PC->ClientPlayForceFeedback(DamageType->KilledForceFeedback, Params);
			}
		}
	}

	// cannot use IsLocallyControlled here, because even local client's controller may be NULL here
	if (GetNetMode() != NM_DedicatedServer && DeathSound && Mesh1P && Mesh1P->IsVisible())
	{
		UGameplayStatics::PlaySoundAtLocation(this, DeathSound, GetActorLocation());
	}

	// remove all weapons
	DestroyInventory();

	// switch back to 3rd person view
	UpdatePawnMeshes();
	DetachFromControllerPendingDestroy();

	StopAllAnimMontages();

	if (LowHealthWarningPlayer && LowHealthWarningPlayer->IsPlaying())
	{
		LowHealthWarningPlayer->Stop();
	}

	if (RunLoopAC)
	{
		RunLoopAC->Stop();
	}

	if (GetMesh())
	{
		static FName CollisionProfileName(TEXT("Ragdoll"));
		GetMesh()->SetCollisionProfileName(CollisionProfileName);
	}
	SetActorEnableCollision(true);

	// Death anim
	float DeathAnimDuration = PlayAnimMontage(DeathAnim);

	// Ragdoll
	if (DeathAnimDuration > 0.f)
	{
		// Trigger ragdoll a little before the animation early so the character doesn't
		// blend back to its normal position.
		const float TriggerRagdollTime = DeathAnimDuration - 0.7f;

		// Enable blend physics so the bones are properly blending against the montage.
		GetMesh()->bBlendPhysics = true;

		// Use a local timer handle as we don't need to store it for later but we don't need to look for something to clear
		FTimerHandle TimerHandle;
		GetWorldTimerManager().SetTimer(TimerHandle, this, &AShooterCharacter::SetRagdollPhysics, FMath::Max(0.1f, TriggerRagdollTime), false);
	}
	else
	{
		SetRagdollPhysics();
	}

	// disable collisions on capsule
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetCapsuleComponent()->SetCollisionResponseToAllChannels(ECR_Ignore);

	// Tell vehicle I am death If I'm a driver
	if (bIsInVehicle && bIsDirver && OwnerVehicle)
	{
		OwnerVehicle->OnDriverDeath(this);
	}
	OwnerVehicle = nullptr;
	bIsDirver = false;
	bIsInVehicle = false;
	//
	OnDeathDelegate.Broadcast(this);
	//
}

void AShooterCharacter::PlayHit(float DamageTaken, struct FDamageEvent const& DamageEvent, class APawn* PawnInstigator, class AActor* DamageCauser)
{
	if (Role == ROLE_Authority)
	{
		ReplicateHit(DamageTaken, DamageEvent, PawnInstigator, DamageCauser, false);

		// play the force feedback effect on the client player controller
		APlayerController* PC = Cast<APlayerController>(Controller);
		if (PC && DamageEvent.DamageTypeClass)
		{
			UShooterDamageType *DamageType = Cast<UShooterDamageType>(DamageEvent.DamageTypeClass->GetDefaultObject());
			if (DamageType && DamageType->HitForceFeedback)
			{
				FForceFeedbackParameters Params;
				Params.Tag = "Damage";
				PC->ClientPlayForceFeedback(DamageType->HitForceFeedback, Params);
			}
		}
	}

	if (DamageTaken > 0.f)
	{
		ApplyDamageMomentum(DamageTaken, DamageEvent, PawnInstigator, DamageCauser);
	}

	AShooterPlayerController* MyPC = Cast<AShooterPlayerController>(Controller);
	AShooterHUD* MyHUD = MyPC ? Cast<AShooterHUD>(MyPC->GetHUD()) : NULL;
	if (MyHUD)
	{
		MyHUD->NotifyWeaponHit(DamageTaken, DamageEvent, PawnInstigator);
	}

	if (PawnInstigator && PawnInstigator != this && PawnInstigator->IsLocallyControlled())
	{
		AShooterPlayerController* InstigatorPC = Cast<AShooterPlayerController>(PawnInstigator->Controller);
		AShooterHUD* InstigatorHUD = InstigatorPC ? Cast<AShooterHUD>(InstigatorPC->GetHUD()) : NULL;
		if (InstigatorHUD)
		{
			InstigatorHUD->NotifyEnemyHit();
		}
	}
}


void AShooterCharacter::SetRagdollPhysics()
{
	bool bInRagdoll = false;

	if (IsPendingKill())
	{
		bInRagdoll = false;
	}
	else if (!GetMesh() || !GetMesh()->GetPhysicsAsset())
	{
		bInRagdoll = false;
	}
	else
	{
		// initialize physics/etc
		GetMesh()->SetSimulatePhysics(true);
		GetMesh()->WakeAllRigidBodies();
		GetMesh()->bBlendPhysics = true;

		bInRagdoll = true;
	}

	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	GetCharacterMovement()->SetComponentTickEnabled(false);

	if (bDestroyAfterDeath)
	{
		if (!bInRagdoll)
		{
			// hide and set short lifespan
			TurnOff();
			SetActorHiddenInGame(true);
			SetLifeSpan(1.0f);
		}
		else
		{
			SetLifeSpan(10.0f);
		}
	}	
}



void AShooterCharacter::ReplicateHit(float Damage, struct FDamageEvent const& DamageEvent, class APawn* PawnInstigator, class AActor* DamageCauser, bool bKilled)
{
	const float TimeoutTime = GetWorld()->GetTimeSeconds() + 0.5f;

	FDamageEvent const& LastDamageEvent = LastTakeHitInfo.GetDamageEvent();
	if ((PawnInstigator == LastTakeHitInfo.PawnInstigator.Get()) && (LastDamageEvent.DamageTypeClass == LastTakeHitInfo.DamageTypeClass) && (LastTakeHitTimeTimeout == TimeoutTime))
	{
		// same frame damage
		if (bKilled && LastTakeHitInfo.bKilled)
		{
			// Redundant death take hit, just ignore it
			return;
		}

		// otherwise, accumulate damage done this frame
		Damage += LastTakeHitInfo.ActualDamage;
	}

	LastTakeHitInfo.ActualDamage = Damage;
	LastTakeHitInfo.PawnInstigator = Cast<AShooterCharacter>(PawnInstigator);
	LastTakeHitInfo.DamageCauser = DamageCauser;
	LastTakeHitInfo.SetDamageEvent(DamageEvent);
	LastTakeHitInfo.bKilled = bKilled;
	LastTakeHitInfo.EnsureReplication();

	LastTakeHitTimeTimeout = TimeoutTime;
}

void AShooterCharacter::OnRep_LastTakeHitInfo()
{
	if (LastTakeHitInfo.bKilled)
	{
		OnDeath(LastTakeHitInfo.ActualDamage, LastTakeHitInfo.GetDamageEvent(), LastTakeHitInfo.PawnInstigator.Get(), LastTakeHitInfo.DamageCauser.Get());
	}
	else
	{
		PlayHit(LastTakeHitInfo.ActualDamage, LastTakeHitInfo.GetDamageEvent(), LastTakeHitInfo.PawnInstigator.Get(), LastTakeHitInfo.DamageCauser.Get());
	}
}

//Pawn::PlayDying sets this lifespan, but when that function is called on client, dead pawn's role is still SimulatedProxy despite bTearOff being true. 
void AShooterCharacter::TornOff()
{
	SetLifeSpan(25.f);
}

bool AShooterCharacter::IsMoving()
{
	return FMath::Abs(GetLastMovementInputVector().Size()) > 0.f;
}

//////////////////////////////////////////////////////////////////////////
// Inventory

void AShooterCharacter::SpawnDefaultInventory()
{
	if (Role < ROLE_Authority)
	{
		return;
	}

	int32 NumWeaponClasses = DefaultInventoryClasses.Num();
	for (int32 i = 0; i < NumWeaponClasses; i++)
	{
		if (DefaultInventoryClasses[i])
		{
			FActorSpawnParameters SpawnInfo;
			SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AShooterWeapon* NewWeapon = GetWorld()->SpawnActor<AShooterWeapon>(DefaultInventoryClasses[i], SpawnInfo);
			AddWeapon(NewWeapon);
		}
	}

	// equip first weapon in inventory
	if (Inventory.Num() > 0)
	{
		EquipWeapon(Inventory[0]);
	}
}

void AShooterCharacter::DestroyInventory()
{
	if (Role < ROLE_Authority)
	{
		return;
	}

	// remove all weapons from inventory and destroy them
	for (int32 i = Inventory.Num() - 1; i >= 0; i--)
	{
		AShooterWeapon* Weapon = Inventory[i];
		if (Weapon)
		{
			RemoveWeapon(Weapon);
			Weapon->Destroy();
		}
	}
}

void AShooterCharacter::AddWeapon(AShooterWeapon* Weapon)
{
	if (Weapon && Role == ROLE_Authority)
	{
		Weapon->OnEnterInventory(this);
		Inventory.AddUnique(Weapon);
	}
}

void AShooterCharacter::RemoveWeapon(AShooterWeapon* Weapon)
{
	if (Weapon && Role == ROLE_Authority)
	{
		Weapon->OnLeaveInventory();
		Inventory.RemoveSingle(Weapon);
	}
}

AShooterWeapon* AShooterCharacter::FindWeapon(TSubclassOf<AShooterWeapon> WeaponClass)
{
	for (int32 i = 0; i < Inventory.Num(); i++)
	{
		if (Inventory[i] && Inventory[i]->IsA(WeaponClass))
		{
			return Inventory[i];
		}
	}

	return NULL;
}

void AShooterCharacter::EquipWeapon(AShooterWeapon* Weapon)
{
	if (Weapon)
	{
		if (Role == ROLE_Authority)
		{
			SetCurrentWeapon(Weapon, CurrentWeapon);
		}
		else
		{
			ServerEquipWeapon(Weapon);
		}
	}
}

bool AShooterCharacter::ServerEquipWeapon_Validate(AShooterWeapon* Weapon)
{
	return true;
}

void AShooterCharacter::ServerEquipWeapon_Implementation(AShooterWeapon* Weapon)
{
	EquipWeapon(Weapon);
}

void AShooterCharacter::OnRep_CurrentWeapon(AShooterWeapon* LastWeapon)
{
	SetCurrentWeapon(CurrentWeapon, LastWeapon);
}

void AShooterCharacter::SetCurrentWeapon(AShooterWeapon* NewWeapon, AShooterWeapon* LastWeapon)
{
	AShooterWeapon* LocalLastWeapon = NULL;

	if (LastWeapon != NULL)
	{
		LocalLastWeapon = LastWeapon;
	}
	else if (NewWeapon != CurrentWeapon)
	{
		LocalLastWeapon = CurrentWeapon;
	}

	// unequip previous
	if (LocalLastWeapon)
	{
		LocalLastWeapon->OnUnEquip();
	}

	LastEquipWeapon = LocalLastWeapon;
	CurrentWeapon = NewWeapon;

	// equip new one
	if (NewWeapon)
	{
		NewWeapon->SetOwningPawn(this);	// Make sure weapon's MyPawn is pointing back to us. During replication, we can't guarantee APawn::CurrentWeapon will rep after AWeapon::MyPawn!

		NewWeapon->OnEquip(LastWeapon);

		NotifyBPOnEquipWeapon(NewWeapon);
	}
}


//////////////////////////////////////////////////////////////////////////
// Weapon usage

void AShooterCharacter::StartWeaponFire()
{
	if (CurrentWeapon && CurrentWeapon->GetCurrentState() != EWeaponState::Equipping)
	{
		if (!bWantsToFire)
		{
			bWantsToFire = true;
			CurrentWeapon->StartFire();
			PawnNoiseEmitter->MakeNoise(CurrentWeapon, CurrentWeapon->GetFireLoudness(), GetActorLocation());
		}
	}
	
}

void AShooterCharacter::StopWeaponFire()
{
	if (bWantsToFire)
	{
		bWantsToFire = false;
		if (CurrentWeapon)
		{
			CurrentWeapon->StopFire();
		}
	}
}

bool AShooterCharacter::CanFire() const
{
	return IsAlive();
}

bool AShooterCharacter::CanReload() const
{
	return true;
}

void AShooterCharacter::SetTargeting(bool bNewTargeting)
{
	bIsTargeting = bNewTargeting;

	if (TargetingSound)
	{
		UGameplayStatics::SpawnSoundAttached(TargetingSound, GetRootComponent());
	}

	if (Role < ROLE_Authority)
	{
		ServerSetTargeting(bNewTargeting);
	}
}

bool AShooterCharacter::ServerSetTargeting_Validate(bool bNewTargeting)
{
	return true;
}

void AShooterCharacter::ServerSetTargeting_Implementation(bool bNewTargeting)
{
	SetTargeting(bNewTargeting);
}

//////////////////////////////////////////////////////////////////////////
// Movement

void AShooterCharacter::SetRunning(bool bNewRunning, bool bToggle)
{
	bWantsToRun = bNewRunning;
	bWantsToRunToggled = bNewRunning && bToggle;

	if (Role < ROLE_Authority)
	{
		ServerSetRunning(bNewRunning, bToggle);
	}
}

bool AShooterCharacter::ServerSetRunning_Validate(bool bNewRunning, bool bToggle)
{
	return true;
}

void AShooterCharacter::ServerSetRunning_Implementation(bool bNewRunning, bool bToggle)
{
	SetRunning(bNewRunning, bToggle);
}

void AShooterCharacter::UpdateRunSounds()
{
	const bool bIsRunSoundPlaying = RunLoopAC != nullptr && RunLoopAC->IsActive();
	const bool bWantsRunSoundPlaying = IsRunning() && IsMoving();

	// Don't bother playing the sounds unless we're running and moving.
	if (!bIsRunSoundPlaying && bWantsRunSoundPlaying)
	{
		if (RunLoopAC != nullptr)
		{
			RunLoopAC->Play();
		}
		else if (RunLoopSound != nullptr)
		{
			RunLoopAC = UGameplayStatics::SpawnSoundAttached(RunLoopSound, GetRootComponent());
			if (RunLoopAC != nullptr)
			{
				RunLoopAC->bAutoDestroy = false;
			}
		}
	}
	else if (bIsRunSoundPlaying && !bWantsRunSoundPlaying)
	{
		RunLoopAC->Stop();
		if (RunStopSound != nullptr)
		{
			UGameplayStatics::SpawnSoundAttached(RunStopSound, GetRootComponent());
		}
	}
}

//////////////////////////////////////////////////////////////////////////
//Vehicle  interactive

bool AShooterCharacter::ServerEnterOrExitVehicle_Validate(AShooterVehicle* Vehicle)
{
	return true;
}

void AShooterCharacter::ServerEnterOrExitVehicle_Implementation(AShooterVehicle* Vehicle)
{
	if (Vehicle)
	{
		Vehicle->Multicast_EnterOrExitVehicle(this);
	}
}

//////////////////////////////////////////////////////////////////////////
// Animations

float AShooterCharacter::PlayAnimMontage(class UAnimMontage* AnimMontage, float InPlayRate, FName StartSectionName)
{
	USkeletalMeshComponent* UseMesh = GetPawnMesh();
	if (AnimMontage && UseMesh && UseMesh->AnimScriptInstance)
	{
		return UseMesh->AnimScriptInstance->Montage_Play(AnimMontage, InPlayRate);
	}

	return 0.0f;
}

void AShooterCharacter::StopAnimMontage(class UAnimMontage* AnimMontage)
{
	USkeletalMeshComponent* UseMesh = GetPawnMesh();
	if (AnimMontage && UseMesh && UseMesh->AnimScriptInstance &&
		UseMesh->AnimScriptInstance->Montage_IsPlaying(AnimMontage))
	{
		UseMesh->AnimScriptInstance->Montage_Stop(AnimMontage->BlendOut.GetBlendTime(), AnimMontage);
	}
}

void AShooterCharacter::StopAllAnimMontages()
{
	USkeletalMeshComponent* UseMesh = GetPawnMesh();
	if (UseMesh && UseMesh->AnimScriptInstance)
	{
		UseMesh->AnimScriptInstance->Montage_Stop(0.0f);
	}
}


//////////////////////////////////////////////////////////////////////////
// Input

void AShooterCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	check(PlayerInputComponent);

	PlayerInputComponent->BindAxis("MoveForward", this, &AShooterCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &AShooterCharacter::MoveRight);
	PlayerInputComponent->BindAxis("MoveUp", this, &AShooterCharacter::MoveUp);

	PlayerInputComponent->BindAxis("Turn", this, &AShooterCharacter::Turn);
	PlayerInputComponent->BindAxis("TurnRate", this, &AShooterCharacter::TurnAtRate);
	PlayerInputComponent->BindAxis("LookUp", this, &AShooterCharacter::LookUp);
	PlayerInputComponent->BindAxis("LookUpRate", this, &AShooterCharacter::LookUpAtRate);

	PlayerInputComponent->BindAction("Fire", IE_Pressed, this, &AShooterCharacter::OnStartFire);
	PlayerInputComponent->BindAction("Fire", IE_Released, this, &AShooterCharacter::OnStopFire);

	PlayerInputComponent->BindAction("Targeting", IE_Pressed, this, &AShooterCharacter::OnStartTargeting);
	PlayerInputComponent->BindAction("Targeting", IE_Released, this, &AShooterCharacter::OnStopTargeting);

	PlayerInputComponent->BindAction("NextWeapon", IE_Pressed, this, &AShooterCharacter::OnNextWeapon);
	PlayerInputComponent->BindAction("PrevWeapon", IE_Pressed, this, &AShooterCharacter::OnPrevWeapon);

	PlayerInputComponent->BindAction("Reload", IE_Pressed, this, &AShooterCharacter::OnReload);

	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &AShooterCharacter::OnStartJump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &AShooterCharacter::OnStopJump);

	PlayerInputComponent->BindAction("Run", IE_Pressed, this, &AShooterCharacter::OnStartRunning);
	PlayerInputComponent->BindAction("RunToggle", IE_Pressed, this, &AShooterCharacter::OnStartRunningToggle);
	PlayerInputComponent->BindAction("Run", IE_Released, this, &AShooterCharacter::OnStopRunning);

	PlayerInputComponent->BindAction("Enter/ExitVehicle", IE_Pressed, this, &AShooterCharacter::OnEnterOrExitVehicle);
}

void AShooterCharacter::MoveForward(float Val)
{
	if (Controller && Val != 0.f)
	{
		// Limit pitch when walking or falling
		if (PlayerType == EShooterPlayerType::TSS)
		{
			AddMovementInput(MoveForwardWorldDirection, Val);
		}
		else
		{
			const bool bLimitRotation = (GetCharacterMovement()->IsMovingOnGround() || GetCharacterMovement()->IsFalling());
			const FRotator Rotation = bLimitRotation ? GetActorRotation() : Controller->GetControlRotation();
			const FVector Direction = FRotationMatrix(Rotation).GetScaledAxis(EAxis::X);
			AddMovementInput(Direction, Val);
		}
	}
}

void AShooterCharacter::MoveRight(float Val)
{
	if (Val != 0.f)
	{
		if (PlayerType == EShooterPlayerType::TSS)
		{
			AddMovementInput(MoveRightWorldDirection, Val);
		}
		else
		{
			const FQuat Rotation = GetActorQuat();
			const FVector Direction = FQuatRotationMatrix(Rotation).GetScaledAxis(EAxis::Y);
			AddMovementInput(Direction, Val);
		}

	}
}

void AShooterCharacter::MoveUp(float Val)
{
	if (Val != 0.f)
	{
		// Not when walking or falling.
		if (GetCharacterMovement()->IsMovingOnGround() || GetCharacterMovement()->IsFalling())
		{
			return;
		}

		AddMovementInput(FVector::UpVector, Val);
	}
}

void AShooterCharacter::TurnAtRate(float Val)
{
	if (PlayerType != EShooterPlayerType::TSS)
	{
		// calculate delta for this frame from the rate information
		AddControllerYawInput(Val * BaseTurnRate * GetWorld()->GetDeltaSeconds());
	}

}

void AShooterCharacter::LookUpAtRate(float Val)
{
	if (PlayerType != EShooterPlayerType::TSS)
	{
		// calculate delta for this frame from the rate information
		AddControllerPitchInput(Val * BaseLookUpRate * GetWorld()->GetDeltaSeconds());
	}
}

void AShooterCharacter::Turn(float Val)
{
	if (PlayerType != EShooterPlayerType::TSS)
	{
		APawn::AddControllerYawInput(Val);
	}
}

void AShooterCharacter::LookUp(float Val)
{
	if (PlayerType != EShooterPlayerType::TSS)
	{
		APawn::AddControllerPitchInput(Val);
	}
}

void AShooterCharacter::OnStartFire()
{
	AShooterPlayerController* MyPC = Cast<AShooterPlayerController>(Controller);
	if (MyPC && MyPC->IsGameInputAllowed())
	{
		if (IsRunning())
		{
			SetRunning(false, false);
		}
		StartWeaponFire();
	}
}

void AShooterCharacter::OnStopFire()
{
	StopWeaponFire();
}

void AShooterCharacter::OnStartTargeting()
{
	AShooterPlayerController* MyPC = Cast<AShooterPlayerController>(Controller);
	if (MyPC && MyPC->IsGameInputAllowed())
	{
		if (IsRunning())
		{
			SetRunning(false, false);
		}
		SetTargeting(true);
	}
}

void AShooterCharacter::OnStopTargeting()
{
	SetTargeting(false);
}

void AShooterCharacter::OnNextWeapon()
{
	AShooterPlayerController* MyPC = Cast<AShooterPlayerController>(Controller);
	if (MyPC && MyPC->IsGameInputAllowed())
	{
		if (Inventory.Num() >= 2 && (CurrentWeapon == NULL || CurrentWeapon->GetCurrentState() != EWeaponState::Equipping))
		{
			const int32 CurrentWeaponIdx = Inventory.IndexOfByKey(CurrentWeapon);
			AShooterWeapon* NextWeapon = Inventory[(CurrentWeaponIdx + 1) % Inventory.Num()];
			EquipWeapon(NextWeapon);
		}
	}
}

void AShooterCharacter::OnPrevWeapon()
{
	AShooterPlayerController* MyPC = Cast<AShooterPlayerController>(Controller);
	if (MyPC && MyPC->IsGameInputAllowed())
	{
		if (Inventory.Num() >= 2 && (CurrentWeapon == NULL || CurrentWeapon->GetCurrentState() != EWeaponState::Equipping))
		{
			const int32 CurrentWeaponIdx = Inventory.IndexOfByKey(CurrentWeapon);
			AShooterWeapon* PrevWeapon = Inventory[(CurrentWeaponIdx - 1 + Inventory.Num()) % Inventory.Num()];
			EquipWeapon(PrevWeapon);
		}
	}
}

void AShooterCharacter::OnReload()
{
	AShooterPlayerController* MyPC = Cast<AShooterPlayerController>(Controller);
	if (MyPC && MyPC->IsGameInputAllowed())
	{
		if (CurrentWeapon)
		{
			CurrentWeapon->StartReload();
		}
	}
}

void AShooterCharacter::OnStartRunning()
{
	AShooterPlayerController* MyPC = Cast<AShooterPlayerController>(Controller);
	if (MyPC && MyPC->IsGameInputAllowed())
	{
		if (IsTargeting())
		{
			SetTargeting(false);
		}
		StopWeaponFire();
		SetRunning(true, false);
	}
}

void AShooterCharacter::OnStartRunningToggle()
{
	AShooterPlayerController* MyPC = Cast<AShooterPlayerController>(Controller);
	if (MyPC && MyPC->IsGameInputAllowed())
	{
		if (IsTargeting())
		{
			SetTargeting(false);
		}
		StopWeaponFire();
		SetRunning(true, true);
	}
}

void AShooterCharacter::OnStopRunning()
{
	SetRunning(false, false);
}

void AShooterCharacter::OnEnterOrExitVehicle()
{
	if (OwnerVehicle)
	{
		if (bCanEnterVehicle || bIsInVehicle)
		{
			ServerEnterOrExitVehicle(OwnerVehicle);
		}
	}

}

void AShooterCharacter::OnVehicleHit(AShooterVehicle* Vehicle)
{
	float VehicleSpeed = FMath::Abs(Vehicle->GetVehicleMovementComponent()->GetForwardSpeed());

	if (VehicleSpeed > 10)
	{
		if (VehicleSpeed > VehicleKillSpeed)
		{
			this->KilledBy(Vehicle);
		}
		else
		{
			//TODO:此时应该模拟物理
			TSubclassOf<UDamageType> const ValidDamageTypeClass = Vehicle->GetDamageType();
			FDamageEvent DamageEvent(ValidDamageTypeClass);
			this->TakeDamage(Vehicle->GetDamageValue() * VehicleSpeed / VehicleKillSpeed, DamageEvent, this->GetController(), this);
		}

	}
}

bool AShooterCharacter::IsRunning() const
{
	if (!GetCharacterMovement())
	{
		return false;
	}

	return (bWantsToRun || bWantsToRunToggled) && !GetVelocity().IsZero() && (GetVelocity().GetSafeNormal2D() | GetActorForwardVector()) > -0.1;
}

void AShooterCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bWantsToRunToggled && !IsRunning())
	{
		SetRunning(false, false);
	}
	AShooterPlayerController* MyPC = Cast<AShooterPlayerController>(Controller);
	if (MyPC && MyPC->HasHealthRegen())
	{
		if (this->Health < this->GetMaxHealth())
		{
			this->Health += 5 * DeltaSeconds;
			if (Health > this->GetMaxHealth())
			{
				Health = this->GetMaxHealth();
			}
		}
	}

	if (GEngine->UseSound())
	{
		if (LowHealthSound)
		{
			if ((this->Health > 0 && this->Health < this->GetMaxHealth() * LowHealthPercentage) && (!LowHealthWarningPlayer || !LowHealthWarningPlayer->IsPlaying()))
			{
				LowHealthWarningPlayer = UGameplayStatics::SpawnSoundAttached(LowHealthSound, GetRootComponent(),
					NAME_None, FVector(ForceInit), EAttachLocation::KeepRelativeOffset, true);
				LowHealthWarningPlayer->SetVolumeMultiplier(0.0f);
			}
			else if ((this->Health > this->GetMaxHealth() * LowHealthPercentage || this->Health < 0) && LowHealthWarningPlayer && LowHealthWarningPlayer->IsPlaying())
			{
				LowHealthWarningPlayer->Stop();
			}
			if (LowHealthWarningPlayer && LowHealthWarningPlayer->IsPlaying())
			{
				const float MinVolume = 0.3f;
				const float VolumeMultiplier = (1.0f - (this->Health / (this->GetMaxHealth() * LowHealthPercentage)));
				LowHealthWarningPlayer->SetVolumeMultiplier(MinVolume + (1.0f - MinVolume) * VolumeMultiplier);
			}
		}

		UpdateRunSounds();
	}

	APlayerController* PC = Cast<APlayerController>(GetController());
	const bool bLocallyControlled = (PC ? PC->IsLocalController() : false);
	const uint32 UniqueID = GetUniqueID();
	FAudioThread::RunCommandOnAudioThread([UniqueID, bLocallyControlled]()
	{
		USoundNodeLocalPlayer::GetLocallyControlledActorCache().Add(UniqueID, bLocallyControlled);
	});

	TArray<FVector> PointsToTest;
	BuildPauseReplicationCheckPoints(PointsToTest);

	if (NetVisualizeRelevancyTestPoints == 1)
	{
		for (FVector PointToTest : PointsToTest)
		{
			DrawDebugSphere(GetWorld(), PointToTest, 10.0f, 8, FColor::Red);
		}
	}

	if (PC && bLocallyControlled)
	{
		if (PlayerType == EShooterPlayerType::TSS)
		{
			//PC->bShowMouseCursor = true;

			if (bFaceToCursor)
			{
				/* Rotate with mouse Hit in world loaction */				
				FHitResult HitResult;
				bool IsHit = PC->GetHitResultUnderCursorByChannel(ETraceTypeQuery::TraceTypeQuery1, true, HitResult);
				float distance = FVector::Distance(HitResult.Location, GetActorLocation());
				if (distance >= RotationMidDistance && IsHit)
				{
					FVector dirFormCuror = HitResult.Location - GetActorLocation();
					dirFormCuror.Normalize();
					FRotator TargetRotaor = FRotationMatrix::MakeFromX(dirFormCuror).Rotator();
					FRotator NewRotaor = FMath::RInterpTo(PC->GetControlRotation(), TargetRotaor, DeltaSeconds, 5.0f);
					PC->SetControlRotation(bShootWithMouseInWorld?NewRotaor:FRotator(0, NewRotaor.Yaw,0));
				}

			}
		}
		else
		{
			//PC->bShowMouseCursor = false;
		}
	}
}

void AShooterCharacter::BeginDestroy()
{
	Super::BeginDestroy();

	if (!GExitPurge)
	{
		const uint32 UniqueID = GetUniqueID();
		FAudioThread::RunCommandOnAudioThread([UniqueID]()
		{
			USoundNodeLocalPlayer::GetLocallyControlledActorCache().Remove(UniqueID);
		});
	}
}

void AShooterCharacter::OnStartJump()
{
	AShooterPlayerController* MyPC = Cast<AShooterPlayerController>(Controller);
	if (MyPC && MyPC->IsGameInputAllowed())
	{
		bPressedJump = true;
	}
}

void AShooterCharacter::OnStopJump()
{
	bPressedJump = false;
	StopJumping();
}

//////////////////////////////////////////////////////////////////////////
// Replication

void AShooterCharacter::PreReplication(IRepChangedPropertyTracker & ChangedPropertyTracker)
{
	Super::PreReplication(ChangedPropertyTracker);

	// Only replicate this property for a short duration after it changes so join in progress players don't get spammed with fx when joining late
	DOREPLIFETIME_ACTIVE_OVERRIDE(AShooterCharacter, LastTakeHitInfo, GetWorld() && GetWorld()->GetTimeSeconds() < LastTakeHitTimeTimeout);
}

void AShooterCharacter::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// only to local owner: weapon change requests are locally instigated, other clients don't need it
	DOREPLIFETIME_CONDITION(AShooterCharacter, Inventory, COND_OwnerOnly);

	// everyone except local owner: flag change is locally instigated
	DOREPLIFETIME_CONDITION(AShooterCharacter, bIsTargeting, COND_SkipOwner);

	DOREPLIFETIME_CONDITION(AShooterCharacter, bWantsToRun, COND_SkipOwner);

	DOREPLIFETIME_CONDITION(AShooterCharacter, LastTakeHitInfo, COND_Custom);

	// everyone
	DOREPLIFETIME(AShooterCharacter, CurrentWeapon);
	DOREPLIFETIME(AShooterCharacter, Health);

	DOREPLIFETIME(AShooterCharacter, OwnerVehicle);
	DOREPLIFETIME(AShooterCharacter, bIsInVehicle);
	DOREPLIFETIME(AShooterCharacter, bCanEnterVehicle);
	DOREPLIFETIME(AShooterCharacter, bHideInVehicle);
}

bool AShooterCharacter::IsReplicationPausedForConnection(const FNetViewer& ConnectionOwnerNetViewer)
{
	if (NetEnablePauseRelevancy == 1)
	{
		APlayerController* PC = Cast<APlayerController>(ConnectionOwnerNetViewer.InViewer);
		check(PC);

		FVector ViewLocation;
		FRotator ViewRotation;
		PC->GetPlayerViewPoint(ViewLocation, ViewRotation);

		static FName NAME_LineOfSight = FName(TEXT("LineOfSight"));
		FCollisionQueryParams CollisionParams(NAME_LineOfSight, true, PC->GetPawn());
		CollisionParams.AddIgnoredActor(this);

		TArray<FVector> PointsToTest;
		BuildPauseReplicationCheckPoints(PointsToTest);

		for (FVector PointToTest : PointsToTest)
		{
			if (!GetWorld()->LineTraceTestByChannel(PointToTest, ViewLocation, ECC_Visibility, CollisionParams))
			{
				return false;
			}
		}

		return true;
	}

	return false;
}

void AShooterCharacter::OnReplicationPausedChanged(bool bIsReplicationPaused)
{
	GetMesh()->SetHiddenInGame(bIsReplicationPaused, true);
}

AShooterWeapon* AShooterCharacter::GetWeapon() const
{
	return CurrentWeapon;
}

int32 AShooterCharacter::GetInventoryCount() const
{
	return Inventory.Num();
}

AShooterWeapon* AShooterCharacter::GetInventoryWeapon(int32 index) const
{
	return Inventory[index];
}

USkeletalMeshComponent* AShooterCharacter::GetPawnMesh() const
{
	return IsFirstPerson() ? Mesh1P : GetMesh();
}

USkeletalMeshComponent* AShooterCharacter::GetSpecifcPawnMesh(bool WantFirstPerson) const
{
	return WantFirstPerson == true ? Mesh1P : GetMesh();
}

FName AShooterCharacter::GetInventoryWeaponPoint(int32 index) const
{
	if (index>=0 && index< InventoryWeaponPoints.Num())
	{
		return InventoryWeaponPoints[index];
	}
	return FName(TEXT("None"));
}

float AShooterCharacter::GetTargetingSpeedModifier() const
{
	return TargetingSpeedModifier;
}

bool AShooterCharacter::IsTargeting() const
{
	return bIsTargeting;
}

float AShooterCharacter::GetRunningSpeedModifier() const
{
	return RunningSpeedModifier;
}

bool AShooterCharacter::IsFiring() const
{
	return bWantsToFire;
};

bool AShooterCharacter::IsFirstPerson() const
{
	if (PlayerType == EShooterPlayerType::FPS)
	{
		return IsAlive() && Controller && Controller->IsLocalPlayerController();
	}
	else
	{
		return false;
	}

}

void AShooterCharacter::OnChangePlayerType(EShooterPlayerType ePlayerType)
{
	//
	PlayerType = ePlayerType;

	//TODO:change player type 
	if (SpringArm)
	{
		switch (PlayerType)
		{
		case EShooterPlayerType::FPS:
			BaseEyeHeight = GetClass()->GetDefaultObject<AShooterCharacter>()->BaseEyeHeight + FPS_EyeHeightOffset;
			SpringArm->TargetArmLength = 0;
			SpringArm->SocketOffset = FVector(0);
			SpringArm->TargetOffset = FVector(0);
			SpringArm->RelativeRotation = FRotator(0, 0, 0);
			SpringArm->bUsePawnControlRotation = true;
			SpringArm->bInheritPitch = true;
			SpringArm->bInheritRoll = true;
			SpringArm->bInheritYaw = true;
			SpringArm->bEnableCameraLag = false;
			SpringArm->bEnableCameraRotationLag = false;

			if (Controller)
			{
				FRotator OldRotator = Controller->GetControlRotation();
				Controller->SetControlRotation(FRotator(0, OldRotator.Yaw, 0));
			}

			break;
		case EShooterPlayerType::TPS:
			BaseEyeHeight = GetClass()->GetDefaultObject<AShooterCharacter>()->BaseEyeHeight;
			SpringArm->TargetArmLength = GetClass()->GetDefaultObject<AShooterCharacter>()->SpringArm->TargetArmLength + TPS_TargetArmLengthOffset;
			SpringArm->SocketOffset = GetClass()->GetDefaultObject<AShooterCharacter>()->SpringArm->SocketOffset + TPS_SocketOffset;
			SpringArm->TargetOffset = GetClass()->GetDefaultObject<AShooterCharacter>()->SpringArm->TargetOffset + TPS_TargetOffset;
			SpringArm->RelativeRotation = FRotator(0, 0, 0);
			SpringArm->bUsePawnControlRotation = true;
			SpringArm->bInheritPitch = true;
			SpringArm->bInheritRoll = true;
			SpringArm->bInheritYaw = true;

			SpringArm->bEnableCameraLag = GetClass()->GetDefaultObject<AShooterCharacter>()->SpringArm->bEnableCameraLag;
			SpringArm->bEnableCameraRotationLag = GetClass()->GetDefaultObject<AShooterCharacter>()->SpringArm->bEnableCameraRotationLag;

			if (Controller)
			{
				FRotator OldRotator = Controller->GetControlRotation();
				Controller->SetControlRotation(FRotator(0, OldRotator.Yaw, 0));
			}
			break;
		case EShooterPlayerType::TSS:
			BaseEyeHeight = GetClass()->GetDefaultObject<AShooterCharacter>()->BaseEyeHeight;
			SpringArm->TargetArmLength = GetClass()->GetDefaultObject<AShooterCharacter>()->SpringArm->TargetArmLength + TSS_TargetArmLengthOffset;
			SpringArm->SocketOffset = GetClass()->GetDefaultObject<AShooterCharacter>()->SpringArm->SocketOffset + TSS_SocketOffset;
			SpringArm->TargetOffset = GetClass()->GetDefaultObject<AShooterCharacter>()->SpringArm->TargetOffset + TSS_TargetOffset;
			SpringArm->RelativeRotation = GetClass()->GetDefaultObject<AShooterCharacter>()->SpringArm->RelativeRotation + TSS_SprintArmRotationOffset;
			SpringArm->bUsePawnControlRotation = false;
			SpringArm->bInheritPitch = false;
			SpringArm->bInheritRoll = false;
			SpringArm->bInheritYaw = false;

			SpringArm->bEnableCameraLag = GetClass()->GetDefaultObject<AShooterCharacter>()->SpringArm->bEnableCameraLag;
			SpringArm->bEnableCameraRotationLag = false;

			break;
		case EShooterPlayerType::VRS:
			BaseEyeHeight = GetClass()->GetDefaultObject<AShooterCharacter>()->BaseEyeHeight;
			SpringArm->TargetArmLength = 0;
			SpringArm->SocketOffset = FVector(0);
			SpringArm->TargetOffset = FVector(0);
			SpringArm->RelativeRotation = FRotator(0, 0, 0);
			SpringArm->bUsePawnControlRotation = true;

			SpringArm->bEnableCameraLag = false;
			SpringArm->bEnableCameraRotationLag = false;

			if (Controller)Controller->SetControlRotation(FRotator());
			break;
		default:
			break;
		}
	}

	// set initial mesh visibility (3rd person view)
	UpdatePawnMeshes();

	//
	AShooterWeapon* CurWeapon = GetWeapon();
	if (CurWeapon)
	{
		CurWeapon->OnPlayerTypeChanged(PlayerType);
	}

}

FVector AShooterCharacter::GetPositionOfMouseInWorld()
{
	APlayerController* PC = Cast<APlayerController>(Controller);
	if (PC)
	{
		FHitResult Hit(ForceInit);
		PC->GetHitResultUnderCursorByChannel(ETraceTypeQuery::TraceTypeQuery1, true, Hit);
		return Hit.Location;
	}
	return FVector();
}

int32 AShooterCharacter::GetMaxHealth() const
{
	//return GetClass()->GetDefaultObject<AShooterCharacter>()->Health;
	return MaxHealth;
}

bool AShooterCharacter::IsAlive() const
{
	return Health > 0;
}

bool AShooterCharacter::IsDriver() const
{
	return bIsDirver;
}

bool AShooterCharacter::IsInVehicle() const
{
	return bIsInVehicle;
}

float AShooterCharacter::GetLowHealthPercentage() const
{
	return LowHealthPercentage;
}

EShooterPlayerType AShooterCharacter::GetPlayerType() const
{
	return PlayerType;
}

void AShooterCharacter::UpdateTeamColorsAllMIDs()
{
	for (int32 i = 0; i < MeshMIDs.Num(); ++i)
	{
		UpdateTeamColors(MeshMIDs[i]);
	}
}

void AShooterCharacter::OnVehicleDoorTrigerEnter(class AShooterVehicle* Vehicle)
{
	OwnerVehicle = Vehicle;
	bCanEnterVehicle = true;
}

void AShooterCharacter::OnVehicleDoorTrigerExit(class AShooterVehicle* Vehicle)
{
	OwnerVehicle = nullptr;
	bCanEnterVehicle = false;
}

void AShooterCharacter::OnEnterVehicle_Implementation(class AShooterVehicle* Vehicle,bool IsDirver)
{
	bIsDirver = IsDirver;
	OwnerVehicle = Vehicle;
	bIsInVehicle = true;
	bCanEnterVehicle = false;

	GetMesh()->SetHiddenInGame(bHideInVehicle);

	NotifyBPEnterVhicle(Vehicle, IsDirver);
}

void AShooterCharacter::OnExitVehicle_Implementation(class AShooterVehicle* Vehicle)
{
	OwnerVehicle = nullptr;
	bIsInVehicle = false;
	bIsDirver = false;

	GetMesh()->SetHiddenInGame(false);

	NotifyBPExitVhicle(Vehicle);
}

void AShooterCharacter::BuildPauseReplicationCheckPoints(TArray<FVector>& RelevancyCheckPoints)
{
	FBoxSphereBounds Bounds = GetCapsuleComponent()->CalcBounds(GetCapsuleComponent()->GetComponentTransform());
	FBox BoundingBox = Bounds.GetBox();
	float XDiff = Bounds.BoxExtent.X * 2;
	float YDiff = Bounds.BoxExtent.Y * 2;

	RelevancyCheckPoints.Add(BoundingBox.Min);
	RelevancyCheckPoints.Add(FVector(BoundingBox.Min.X + XDiff, BoundingBox.Min.Y, BoundingBox.Min.Z));
	RelevancyCheckPoints.Add(FVector(BoundingBox.Min.X, BoundingBox.Min.Y + YDiff, BoundingBox.Min.Z));
	RelevancyCheckPoints.Add(FVector(BoundingBox.Min.X + XDiff, BoundingBox.Min.Y + YDiff, BoundingBox.Min.Z));
	RelevancyCheckPoints.Add(FVector(BoundingBox.Max.X - XDiff, BoundingBox.Max.Y, BoundingBox.Max.Z));
	RelevancyCheckPoints.Add(FVector(BoundingBox.Max.X, BoundingBox.Max.Y - YDiff, BoundingBox.Max.Z));
	RelevancyCheckPoints.Add(FVector(BoundingBox.Max.X - XDiff, BoundingBox.Max.Y - YDiff, BoundingBox.Max.Z));
	RelevancyCheckPoints.Add(BoundingBox.Max);
}