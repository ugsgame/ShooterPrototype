// Fill out your copyright notice in the Description page of Project Settings.

#include "ShooterGame.h"
#include "ShooterVehicle.h"
#include "VehicleWheel.h"
#include "ShooterCharacter.h"

#include "Particles/ParticleSystemComponent.h"
#include "Player/ShooterPlayerController.h"
#include "WheeledVehicleMovementComponent.h"
#include "Effects/ShooterImpactEffect.h"
#include "Effects/VehicleDustType.h"

#include "AudioThread.h"

TMap<uint32, AShooterVehicle::FVehicleDesiredRPM> AShooterVehicle::BuggyDesiredRPMs;

AShooterVehicle::AShooterVehicle(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer)
{
	/** Camera strategy:
	*  We want to keep a constant distance between car's location and camera.
	*  We want to keep roll and pitch fixed
	*	We want to interpolate yaw very slightly
	*	We want to keep car almost constant in screen space width and height (i.e. if you draw a box around the car its center would be near constant and its dimensions would only vary on sharp turns or declines */

	// Create a spring arm component
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm0"));
	SpringArm->TargetOffset = FVector(0.f, 0.f, 400.f);
	SpringArm->SetRelativeRotation(FRotator(0.f, 0.f, 0.f));
	SpringArm->SetupAttachment(RootComponent);
	SpringArm->TargetArmLength = 0.0f;
	SpringArm->bEnableCameraRotationLag = true;
	SpringArm->CameraRotationLagSpeed = 7.f;
	SpringArm->bInheritPitch = false;
	SpringArm->bInheritRoll = false;

	// Create camera component 
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera0"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
	Camera->bUsePawnControlRotation = false;
	Camera->FieldOfView = 90.f;

	EngineAC = CreateDefaultSubobject<UAudioComponent>(TEXT("EngineAudio"));
	EngineAC->SetupAttachment(GetMesh());

	SkidAC = CreateDefaultSubobject<UAudioComponent>(TEXT("SkidAudio"));
	SkidAC->bAutoActivate = false;	//we don't want to start skid right away
	SkidAC->SetupAttachment(GetMesh());
	SkidThresholdVelocity = 30;
	SkidFadeoutTime = 0.1f;
	LongSlipSkidThreshold = 0.3f;
	LateralSlipSkidThreshold = 0.3f;
	SkidDurationRequiredForStopSound = 1.5f;

	SpringCompressionLandingThreshold = 250000.f;
	bTiresTouchingGround = false;

	ImpactEffectNormalForceThreshold = 100000.f;

	BaseTurnRate = 45.f;
	BaseLookUpRate = 45.f;

	DamageValue = 100.0f;
	PlayerType = EShooterPlayerType::FPS;

	Health = 100.0f;

	TPS_TargetArmLengthOffset = 600.0f;
	TPS_SocketOffset = FVector(0.0f, 50.0f, 0.0f);

	TSS_TargetArmLengthOffset = 650.0f;
	TSS_SprintArmRotationOffset = FRotator(-45.0f, 0.0f, 0.0f);

	DriverSocket = FName(TEXT("DriverSocket"));
}

void AShooterVehicle::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	if (EngineAC)
	{
		EngineAC->SetSound(EngineSound);
		EngineAC->Play();
	}

	if (SkidAC)
	{
		SkidAC->SetSound(SkidSound);
		SkidAC->Stop();
	}
}

void AShooterVehicle::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	check(PlayerInputComponent);

	PlayerInputComponent->BindAxis("MoveForward", this, &AShooterVehicle::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &AShooterVehicle::MoveRight);

	PlayerInputComponent->BindAxis("Turn", this, &AShooterVehicle::Turn);
	PlayerInputComponent->BindAxis("TurnRate", this, &AShooterVehicle::TurnAtRate);
	PlayerInputComponent->BindAxis("LookUp", this, &AShooterVehicle::LookUp);
	PlayerInputComponent->BindAxis("LookUpRate", this, &AShooterVehicle::LookUpAtRate);

	PlayerInputComponent->BindAction("Handbrake", IE_Pressed, this, &AShooterVehicle::OnHandbrakePressed);
	PlayerInputComponent->BindAction("Handbrake", IE_Released, this, &AShooterVehicle::OnHandbrakeReleased);

	PlayerInputComponent->BindAction("Enter/ExitVehicle", IE_Pressed, this, &AShooterVehicle::OnEnterOrExitVehcile);
}

void AShooterVehicle::MoveForward(float Val)
{
	AShooterPlayerController* MyPC = Cast<AShooterPlayerController>(GetController());
	UWheeledVehicleMovementComponent* VehicleMovementComp = GetVehicleMovementComponent();
	if (VehicleMovementComp == nullptr || (MyPC && MyPC->IsHandbrakeForced()))
	{
		return;
	}

	VehicleMovementComp->SetThrottleInput(Val);
}

void AShooterVehicle::MoveRight(float Val)
{
	AShooterPlayerController* MyPC = Cast<AShooterPlayerController>(GetController());
	UWheeledVehicleMovementComponent* VehicleMovementComp = GetVehicleMovementComponent();
	if (VehicleMovementComp == nullptr || (MyPC && MyPC->IsHandbrakeForced()))
	{
		return;
	}
	VehicleMovementComp->SetSteeringInput(Val);
}

void AShooterVehicle::Turn(float Val)
{
	if (PlayerType != EShooterPlayerType::TSS)
	{
		APawn::AddControllerYawInput(Val);
	}
}

void AShooterVehicle::LookUp(float Val)
{
	if (PlayerType != EShooterPlayerType::TSS)
	{
		APawn::AddControllerPitchInput(Val);
	}
}

void AShooterVehicle::TurnAtRate(float Val)
{
	if (PlayerType != EShooterPlayerType::TSS)
	{
		// calculate delta for this frame from the rate information
		AddControllerYawInput(Val * BaseTurnRate * GetWorld()->GetDeltaSeconds());
	}
}

void AShooterVehicle::LookUpAtRate(float Val)
{
	if (PlayerType != EShooterPlayerType::TSS)
	{
		// calculate delta for this frame from the rate information
		AddControllerPitchInput(Val * BaseLookUpRate * GetWorld()->GetDeltaSeconds());
	}

}

void AShooterVehicle::OnDelayExitVehcile(class AShooterCharacter* Character)
{
	if (Character)
	{
		Character->GetMesh()->SetCollisionResponseToChannel(ECC_Vehicle, ECollisionResponse::ECR_Block);
	}
}

void AShooterVehicle::OnHandbrakePressed()
{
	AShooterPlayerController *VehicleController = Cast<AShooterPlayerController>(GetController());
	UWheeledVehicleMovementComponent* VehicleMovementComp = GetVehicleMovementComponent();
	if (VehicleMovementComp != nullptr)
	{
		VehicleMovementComp->SetHandbrakeInput(true);
	}
}

void AShooterVehicle::OnHandbrakeReleased()
{
	bHandbrakeActive = false;
	UWheeledVehicleMovementComponent* VehicleMovementComp = GetVehicleMovementComponent();
	if (VehicleMovementComp != nullptr)
	{
		GetVehicleMovementComponent()->SetHandbrakeInput(false);
	}
}

void AShooterVehicle::OnEnterOrExitVehcile()
{
	if (DriverCharacter)
	{
		Server_EnterOrExitVehicle(DriverCharacter);
	}
}

void AShooterVehicle::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateWheelEffects(DeltaSeconds);

	if (AShooterPlayerController* VehiclePC = Cast<AShooterPlayerController>(GetController()))
	{
		const uint32 VehiclePCID = VehiclePC->GetUniqueID();
		FVehicleDesiredRPM DesiredRPM;
		DesiredRPM.DesiredRPM = GetEngineRotationSpeed();
		DesiredRPM.TimeStamp = GetWorld()->GetTimeSeconds();
		FAudioThread::RunCommandOnAudioThread([VehiclePCID, DesiredRPM]()
		{
			BuggyDesiredRPMs.Add(VehiclePCID, DesiredRPM);
		});
	}
}

void AShooterVehicle::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	if (AShooterPlayerController* VehiclePC = Cast<AShooterPlayerController>(NewController))
	{
		OnChangePlayerType(VehiclePC->GetPlayerType());

		SERVER_SetPlayerType(VehiclePC->GetPlayerType());
	}

}

void AShooterVehicle::UnPossessed()
{
	if (AShooterPlayerController* VehiclePC = Cast<AShooterPlayerController>(GetController()))
	{
		const uint32 VehiclePCID = VehiclePC->GetUniqueID();
		FAudioThread::RunCommandOnAudioThread([VehiclePCID]()
		{
			BuggyDesiredRPMs.Remove(VehiclePCID);
		});
	}

	// Super clears controller, so do the behavior first
	Super::UnPossessed();
}

bool AShooterVehicle::GetVehicleDesiredRPM_AudioThread(const uint32 VehicleID, FVehicleDesiredRPM& OutDesiredRPM)
{
	check(IsInAudioThread());
	if (FVehicleDesiredRPM* DesiredRPM = BuggyDesiredRPMs.Find(VehicleID))
	{
		OutDesiredRPM = *DesiredRPM;
		return true;
	}
	return false;
}

void AShooterVehicle::SpawnNewWheelEffect(int WheelIndex)
{
	DustPSC[WheelIndex] = NewObject<UParticleSystemComponent>(this);
	DustPSC[WheelIndex]->bAutoActivate = true;
	DustPSC[WheelIndex]->bAutoDestroy = false;
	DustPSC[WheelIndex]->RegisterComponentWithWorld(GetWorld());
	DustPSC[WheelIndex]->AttachToComponent(GetMesh(), FAttachmentTransformRules::KeepRelativeTransform, GetVehicleMovement()->WheelSetups[WheelIndex].BoneName);
}

void AShooterVehicle::UpdateWheelEffects(float DeltaTime)
{
	if (GetVehicleMovement() && bTiresTouchingGround == false && LandingSound)	//we don't update bTiresTouchingGround until later in this function, so we can use it here to determine whether we're landing
	{
		float MaxSpringForce = GetVehicleMovement()->GetMaxSpringForce();
		if (MaxSpringForce > SpringCompressionLandingThreshold)
		{
			UGameplayStatics::PlaySoundAtLocation(this, LandingSound, GetActorLocation());
		}
	}

	bTiresTouchingGround = false;

	if (DustType && !bIsDying &&
		GetVehicleMovement() && GetVehicleMovement()->Wheels.Num() > 0)
	{
		const float CurrentSpeed = GetVehicleSpeed();
		for (int32 i = 0; i < ARRAY_COUNT(DustPSC); i++)
		{
			UPhysicalMaterial* ContactMat = GetVehicleMovement()->Wheels[i]->GetContactSurfaceMaterial();
			if (ContactMat != nullptr)
			{
				bTiresTouchingGround = true;
			}
			UParticleSystem* WheelFX = DustType->GetDustFX(ContactMat, CurrentSpeed);

			const bool bIsActive = DustPSC[i] != nullptr && !DustPSC[i]->bWasDeactivated && !DustPSC[i]->bWasCompleted;
			UParticleSystem* CurrentFX = DustPSC[i] != nullptr ? DustPSC[i]->Template : nullptr;
			if (WheelFX != nullptr && (CurrentFX != WheelFX || !bIsActive))
			{
				if (DustPSC[i] == nullptr || !DustPSC[i]->bWasDeactivated)
				{
					if (DustPSC[i] != nullptr)
					{
						DustPSC[i]->SetActive(false);
						DustPSC[i]->bAutoDestroy = true;
					}
					SpawnNewWheelEffect(i);
				}
				DustPSC[i]->SetTemplate(WheelFX);
				DustPSC[i]->ActivateSystem();
			}
			else if (WheelFX == nullptr && bIsActive)
			{
				DustPSC[i]->SetActive(false);
			}
		}
	}

	if (SkidAC != nullptr)
	{
		FVector Vel = GetVelocity();
		bool bVehicleStopped = Vel.SizeSquared2D() < SkidThresholdVelocity*SkidThresholdVelocity;
		bool TireSlipping = GetVehicleMovement()->CheckSlipThreshold(LongSlipSkidThreshold, LateralSlipSkidThreshold);
		bool bWantsToSkid = bTiresTouchingGround && !bVehicleStopped && TireSlipping;

		float CurrTime = GetWorld()->GetTimeSeconds();
		if (bWantsToSkid && !bSkidding)
		{
			bSkidding = true;
			SkidAC->Play();
			SkidStartTime = CurrTime;
		}
		if (!bWantsToSkid && bSkidding)
		{
			bSkidding = false;
			SkidAC->FadeOut(SkidFadeoutTime, 0);
			if (CurrTime - SkidStartTime > SkidDurationRequiredForStopSound)
			{
				UGameplayStatics::PlaySoundAtLocation(this, SkidSoundStop, GetActorLocation());
			}
		}
	}
}

bool AShooterVehicle::IsHandbrakeActive() const
{
	return bHandbrakeActive;
}

int32 AShooterVehicle::GetMaxHealth() const
{
	return GetClass()->GetDefaultObject<AShooterCharacter>()->Health;
}

float AShooterVehicle::GetDamageValue() const
{
	return DamageValue;
}

float AShooterVehicle::GetVehicleSpeed() const
{
	return (GetVehicleMovement()) ? FMath::Abs(GetVehicleMovement()->GetForwardSpeed()) : 0.0f;
}

float AShooterVehicle::GetEngineRotationSpeed() const
{
	return (GetVehicleMovement()) ? FMath::Abs(GetVehicleMovement()->GetEngineRotationSpeed()) : 0.0f;
}

float AShooterVehicle::GetEngineMaxRotationSpeed() const
{
	return (GetVehicleMovement()) ? FMath::Abs(GetVehicleMovement()->MaxEngineRPM) : 1.f;
}

EShooterPlayerType AShooterVehicle::GetPlayerType() const
{
	return PlayerType;
}

TSubclassOf<UDamageType> AShooterVehicle::GetDamageType() const
{
	return  DamageType ? DamageType : TSubclassOf<UDamageType>(UDamageType::StaticClass());
}

bool AShooterVehicle::IsSitFull() const
{
	int PassengersNum = AllPassengers.Num();
	int SitSocketsNum = AllSitSockets.Num();
	return PassengersNum >= SitSocketsNum;
}

bool AShooterVehicle::IsDriverSitEmpty() const
{
	for (int i = 0; i < AllPassengers.Num(); i++)
	{
		UCapsuleComponent* CapsuleComponent = AllPassengers[i]->GetCapsuleComponent();
		FName Socket = CapsuleComponent->GetAttachSocketName();
		GEngine->AddOnScreenDebugMessage(0, 2.0f, FColor::Red, Socket.ToString());
		if (Socket.IsEqual(DriverSocket))
		{
			return false;
		}
	}
	return true;
}

FName AShooterVehicle::GetEmptySit() const
{
	if (IsDriverSitEmpty())
	{
		return DriverSocket;
	}
	else
	{
		if (AllSitSockets.Num() > 0)
		{
			TArray<FName> EmptySits = AllSitSockets;
			for (int i = 0; i < AllPassengers.Num(); i++)
			{
				UCapsuleComponent* CapsuleComponent = AllPassengers[i]->GetCapsuleComponent();
				EmptySits.Remove(CapsuleComponent->GetAttachSocketName());
			}
			if (EmptySits.Num() > 0)
			{
				return EmptySits[0];
			}
			else
			{
				return FName(TEXT("None"));
			}
		}
		else
		{
			return FName(TEXT("None"));
		}
	}
}

//TODO:需要获取合适的socket
FName AShooterVehicle::GetExitSocket() const
{
	if (AllExitScokets.Num() > 0)
	{
		return AllExitScokets[FMath::FRandRange(0, AllExitScokets.Num() - 1)];
	}
	return FName(TEXT("None"));
}

bool AShooterVehicle::EnterVechicle(AShooterCharacter* Character)
{
	AShooterCharacter* LocalCharacter = Character;
	if (LocalCharacter)
	{
		UCapsuleComponent* CapsuleComp = LocalCharacter->GetCapsuleComponent();
		UCharacterMovementComponent* MovementComp = LocalCharacter->GetCharacterMovement();
	

		FName LocalSitSocket = GetEmptySit();
		if (!LocalSitSocket.IsEqual(FName(TEXT("None"))))
		{
			MovementComp->SetMovementMode(MOVE_None, 0);
			CapsuleComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

			LocalCharacter->GetMesh()->SetCollisionResponseToChannel(ECC_Vehicle, ECollisionResponse::ECR_Ignore);

			FAttachmentTransformRules TranfRules(EAttachmentRule::SnapToTarget, true);
			TranfRules.LocationRule = EAttachmentRule::SnapToTarget;
			TranfRules.RotationRule = EAttachmentRule::SnapToTarget;
			TranfRules.ScaleRule = EAttachmentRule::KeepWorld;

			CapsuleComp->AttachToComponent(GetMesh(), TranfRules, LocalSitSocket);

			SetOwner(LocalCharacter);
			AllPassengers.Add(LocalCharacter);

			bool IsDriver = (LocalSitSocket.IsEqual(DriverSocket));

			LocalCharacter->OnEnterVehicle(this, IsDriver);

			if (IsDriver)
			{
				if (LocalCharacter->GetController())
				{
					if (Role == ROLE_Authority)LocalCharacter->GetController()->Possess(this);
					DriverCharacter = LocalCharacter;
				}
			}
			return true;
		}
		else
		{
			return false;
		}
	}

	return  false;

}

bool AShooterVehicle::ExitVechicle(AShooterCharacter* Character)
{
	AShooterCharacter* LocalCharacter = Character;
	if (LocalCharacter)
	{
		UCapsuleComponent* CapsuleComp = LocalCharacter->GetCapsuleComponent();
		UCharacterMovementComponent* MovementComp = LocalCharacter->GetCharacterMovement();

		if (AllPassengers.Remove(LocalCharacter))
		{
			FVector SocketLocation = GetMesh()->GetSocketLocation(GetExitSocket());
			LocalCharacter->SetActorLocation(SocketLocation);

			MovementComp->SetMovementMode(MOVE_Walking);
			CapsuleComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

			FDetachmentTransformRules TranfRules(EDetachmentRule::KeepWorld, true);
			TranfRules.LocationRule = EDetachmentRule::KeepWorld;
			TranfRules.RotationRule = EDetachmentRule::KeepRelative;
			TranfRules.ScaleRule = EDetachmentRule::KeepWorld;
			CapsuleComp->DetachFromComponent(TranfRules);

			LocalCharacter->OnExitVehicle(this);

			FTimerHandle TimerHandle;
			FTimerDelegate TimerDel = FTimerDelegate::CreateUObject(this,&AShooterVehicle::OnDelayExitVehcile,Character);
			GetWorldTimerManager().SetTimer(TimerHandle, TimerDel, 1.0f, false);

			if (DriverCharacter && LocalCharacter->GetName() == DriverCharacter->GetName())
			{
				if (this->GetController())
				{
					if (Role == ROLE_Authority) this->GetController()->Possess(LocalCharacter);
					LocalCharacter->SetOwner(nullptr);
					DriverCharacter = nullptr;
				}
			}

			return true;
		}

		return false;
	}
	return false;
}

void AShooterVehicle::NotifyHit(UPrimitiveComponent* MyComp, AActor* Other, UPrimitiveComponent* OtherComp, bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalForce, const FHitResult& Hit)
{
	Super::NotifyHit(MyComp, Other, OtherComp, bSelfMoved, HitLocation, HitNormal, NormalForce, Hit);

	if (ImpactTemplate && NormalForce.SizeSquared() > FMath::Square(ImpactEffectNormalForceThreshold))
	{
		FTransform const SpawnTransform(HitNormal.Rotation(), HitLocation);
		AShooterImpactEffect* EffectActor = GetWorld()->SpawnActorDeferred<AShooterImpactEffect>(ImpactTemplate, SpawnTransform);
		if (EffectActor)
		{
			float DotBetweenHitAndUpRotation = FVector::DotProduct(HitNormal, GetMesh()->GetUpVector());
			EffectActor->SurfaceHit = Hit;
			EffectActor->HitForce = NormalForce;
			EffectActor->bWheelLand = DotBetweenHitAndUpRotation > 0.8f;
			UGameplayStatics::FinishSpawningActor(EffectActor, SpawnTransform);
		}
	}

	if (ImpactCameraShake)
	{
		AShooterPlayerController* PC = Cast<AShooterPlayerController>(Controller);
		if (PC != nullptr && PC->IsLocalController())
		{
			PC->ClientPlayCameraShake(ImpactCameraShake, 1);
		}
	}

	AShooterCharacter* Character = Cast<AShooterCharacter>(Other);
	float VehicleSpeed =  FMath::Abs(GetVehicleMovementComponent()->GetForwardSpeed());
	if (Character && VehicleSpeed>0)
	{		
		Character->OnVehicleHit(this);
	}
}

void AShooterVehicle::FellOutOfWorld(const class UDamageType& dmgType)
{
	Die();
}

void AShooterVehicle::OnRep_Controller()
{
	Super::OnRep_Controller();

	AShooterPlayerController* PC = Cast<AShooterPlayerController>(Controller);
	if (PC)
	{
		OnChangePlayerType(PC->GetPlayerType());
	}
}

float AShooterVehicle::TakeDamage(float Damage, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, class AActor* DamageCauser)
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

	// Modify based on game rules.
	AShooterGameMode* const Game = GetWorld()->GetAuthGameMode<AShooterGameMode>();
	Damage = Game ? Game->ModifyDamage(Damage, this, DamageEvent, EventInstigator, DamageCauser) : 0.f;

	const float ActualDamage = Super::TakeDamage(Damage, DamageEvent, EventInstigator, DamageCauser);

	if (ActualDamage > 0.f)
	{
		Health -= ActualDamage;
		if (Health <= 0)
		{
			Die();
		}

		MakeNoise(1.0f, EventInstigator ? EventInstigator->GetPawn() : this);
	}

	return ActualDamage;
}

bool AShooterVehicle::CanDie() const
{
	if (bIsDying										// already dying
		|| IsPendingKill()								// already destroyed
		|| Role != ROLE_Authority)						// not authority
	{
		return false;
	}

	return true;
}

void AShooterVehicle::Die()
{
	if (CanDie())
	{
		OnDeath();
	}
}

void AShooterVehicle::OnRep_Dying()
{
	if (bIsDying == true)
	{
		OnDeath();
	}
}

void AShooterVehicle::TornOff()
{
	Super::TornOff();

	SetLifeSpan(1.0f);
}

void AShooterVehicle::OnDeath()
{
	AShooterPlayerController* MyPC = Cast<AShooterPlayerController>(GetController());
	bReplicateMovement = false;
	TearOff();
	bIsDying = true;

	DetachFromControllerPendingDestroy();

	// hide and disable
	TurnOff();
	SetActorHiddenInGame(true);

	if (EngineAC)
	{
		EngineAC->Stop();
	}

	if (SkidAC)
	{
		SkidAC->Stop();
	}

	PlayDestructionFX();
	// Give use a finite lifespan
	SetLifeSpan(0.2f);

	// Kill all passengers
	for (int i = 0;i< AllPassengers.Num();i++)
	{
		AllPassengers[i]->KilledBy(this);
	}
	AllPassengers.Empty();
	//
}

void AShooterVehicle::OnDriverDeath(AShooterCharacter* Character)
{
	int Index;
	if (AllPassengers.Find(Character, Index))
	{
		if (this->GetController() && Role == ROLE_Authority)
		{
			this->GetController()->Possess(Character);
		}
	}
}

void AShooterVehicle::PlayDestructionFX()
{
	if (DeathFX)
	{
		UGameplayStatics::SpawnEmitterAtLocation(this, DeathFX, GetActorLocation(), GetActorRotation());
	}

	if (DeathSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, DeathSound, GetActorLocation());
	}
}

void AShooterVehicle::OnChangePlayerType(EShooterPlayerType ePlayerType)
{
	switch (ePlayerType)
	{
	case EShooterPlayerType::FPS:
	case EShooterPlayerType::TPS:
		BaseEyeHeight = GetClass()->GetDefaultObject<AShooterVehicle>()->BaseEyeHeight;
		SpringArm->TargetArmLength = GetClass()->GetDefaultObject<AShooterVehicle>()->SpringArm->TargetArmLength + TPS_TargetArmLengthOffset;
		SpringArm->SocketOffset = GetClass()->GetDefaultObject<AShooterVehicle>()->SpringArm->SocketOffset + TPS_SocketOffset;
		SpringArm->TargetOffset = GetClass()->GetDefaultObject<AShooterVehicle>()->SpringArm->TargetOffset + TPS_TargetOffset;
		SpringArm->RelativeRotation = FRotator(0, 0, 0);
		SpringArm->bUsePawnControlRotation = true;
		SpringArm->bInheritPitch = true;
		SpringArm->bInheritRoll = true;
		SpringArm->bInheritYaw = true;

		SpringArm->bEnableCameraLag = GetClass()->GetDefaultObject<AShooterVehicle>()->SpringArm->bEnableCameraLag;
		SpringArm->bEnableCameraRotationLag = GetClass()->GetDefaultObject<AShooterVehicle>()->SpringArm->bEnableCameraRotationLag;

		if (Controller)
		{
			FRotator OldRotator = Controller->GetControlRotation();
			Controller->SetControlRotation(FRotator(0, OldRotator.Yaw, 0));
		}
		break;
	case EShooterPlayerType::TSS:
		BaseEyeHeight = GetClass()->GetDefaultObject<AShooterVehicle>()->BaseEyeHeight;
		SpringArm->TargetArmLength = GetClass()->GetDefaultObject<AShooterVehicle>()->SpringArm->TargetArmLength + TSS_TargetArmLengthOffset;
		SpringArm->SocketOffset = GetClass()->GetDefaultObject<AShooterVehicle>()->SpringArm->SocketOffset + TSS_SocketOffset;
		SpringArm->TargetOffset = GetClass()->GetDefaultObject<AShooterVehicle>()->SpringArm->TargetOffset + TSS_TargetOffset;
		SpringArm->RelativeRotation = GetClass()->GetDefaultObject<AShooterVehicle>()->SpringArm->RelativeRotation + TSS_SprintArmRotationOffset;
		SpringArm->bUsePawnControlRotation = false;
		SpringArm->bInheritPitch = false;
		SpringArm->bInheritRoll = false;
		SpringArm->bInheritYaw = false;

		SpringArm->bEnableCameraLag = GetClass()->GetDefaultObject<AShooterVehicle>()->SpringArm->bEnableCameraLag;
		SpringArm->bEnableCameraRotationLag = false;

		break;
	default:
		BaseEyeHeight = GetClass()->GetDefaultObject<AShooterVehicle>()->BaseEyeHeight;
		SpringArm->TargetArmLength = GetClass()->GetDefaultObject<AShooterVehicle>()->SpringArm->TargetArmLength + TPS_TargetArmLengthOffset;
		SpringArm->SocketOffset = GetClass()->GetDefaultObject<AShooterVehicle>()->SpringArm->SocketOffset + TPS_SocketOffset;
		SpringArm->TargetOffset = GetClass()->GetDefaultObject<AShooterVehicle>()->SpringArm->TargetOffset + TPS_TargetOffset;
		SpringArm->RelativeRotation = FRotator(0, 0, 0);
		SpringArm->bUsePawnControlRotation = true;
		SpringArm->bInheritPitch = true;
		SpringArm->bInheritRoll = true;
		SpringArm->bInheritYaw = true;

		SpringArm->bEnableCameraLag = GetClass()->GetDefaultObject<AShooterVehicle>()->SpringArm->bEnableCameraLag;
		SpringArm->bEnableCameraRotationLag = GetClass()->GetDefaultObject<AShooterVehicle>()->SpringArm->bEnableCameraRotationLag;

		if (Controller)
		{
			FRotator OldRotator = Controller->GetControlRotation();
			Controller->SetControlRotation(FRotator(0, OldRotator.Yaw, 0));
		}
		break;
	}
}

void AShooterVehicle::OnRep_PlayerType()
{
	OnChangePlayerType(PlayerType);
}

bool AShooterVehicle::SERVER_SetPlayerType_Validate(EShooterPlayerType ThePlayerType)
{
	return true;
}

void AShooterVehicle::SERVER_SetPlayerType_Implementation(EShooterPlayerType ThePlayerType)
{
	PlayerType = ThePlayerType;

	OnRep_PlayerType();
}

bool AShooterVehicle::Server_EnterOrExitVehicle_Validate(class AShooterCharacter* Character)
{
	return true;
}

void AShooterVehicle::Server_EnterOrExitVehicle_Implementation(class AShooterCharacter* Character)
{
	Multicast_EnterOrExitVehicle(Character);
}

void AShooterVehicle::Multicast_EnterOrExitVehicle_Implementation(class AShooterCharacter* Character)
{
	if (Character)
	{
		if (Character->IsInVehicle())
		{
			ExitVechicle(Character);
		}
		else if (!IsSitFull())
		{
			EnterVechicle(Character);
		}
	}
}


void AShooterVehicle::PreReplication(IRepChangedPropertyTracker & ChangedPropertyTracker)
{
	Super::PreReplication(ChangedPropertyTracker);

	// Only replicate this property for a short duration after it changes so join in progress players don't get spammed with fx when joining late
	//DOREPLIFETIME_ACTIVE_OVERRIDE(AShooterVehicle, PlayerType, false);
}

void AShooterVehicle::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// everyone
	DOREPLIFETIME(AShooterVehicle, PlayerType);
	DOREPLIFETIME(AShooterVehicle, bIsDying);

	DOREPLIFETIME(AShooterVehicle, AllSitSockets);
	DOREPLIFETIME(AShooterVehicle, AllExitScokets);
	DOREPLIFETIME(AShooterVehicle, AllPassengers);
}


