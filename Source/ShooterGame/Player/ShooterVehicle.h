// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WheeledVehicle.h"
#include "Components/PrimitiveComponent.h"
#include "ShooterVehicle.generated.h"

class UVehicleDustType;
class AShooterImpactEffect;

/**
 * 
 */
UCLASS()
class SHOOTERGAME_API AShooterVehicle
	: public AWheeledVehicle
{

	GENERATED_UCLASS_BODY()

	// Begin Actor overrides
	virtual void PostInitializeComponents() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void NotifyHit(UPrimitiveComponent* MyComp, AActor* Other, UPrimitiveComponent* OtherComp, bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalForce, const FHitResult& Hit) override;
	virtual void FellOutOfWorld(const UDamageType& dmgType) override;

	//UFUNCTION()
	virtual void OnRep_Controller() override;
	// End Actor overrides

	// Begin Pawn overrides
	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;
	virtual float TakeDamage(float Damage, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;
	virtual void TornOff() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;
	// End Pawn overrides

	/** Identifies if pawn is in its dying state */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Health, ReplicatedUsing = OnRep_Dying)
		uint32 bIsDying : 1;

	/** replicating death on client */
	UFUNCTION()
		void OnRep_Dying();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Pawn, ReplicatedUsing = OnRep_PlayerType)
		EShooterPlayerType PlayerType;

	/** replicating PlayerType on client */
	UFUNCTION()
	void OnRep_PlayerType();

	UFUNCTION(reliable, server, WithValidation)
	void SERVER_SetPlayerType(EShooterPlayerType ThePlayerType);

	// Current health of the Pawn
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = Health)
		float Health;

	/** Returns True if the pawn can die in the current state */
	virtual bool CanDie() const;

	/** Kills pawn. [Server/authority only] */
	virtual void Die();

	/** Event on death [Server/Client] */
	virtual void OnDeath();

	virtual void OnDriverDeath(class AShooterCharacter* Character);

	/** Called on the actor right before replication occurs */
	virtual void PreReplication(IRepChangedPropertyTracker & ChangedPropertyTracker) override;

	/** is handbrake active? */
	UFUNCTION(BlueprintCallable, Category = "Game|Vehicle")
	bool IsHandbrakeActive() const;

	/** get max health */
	int32 GetMaxHealth() const;

	float GetDamageValue() const;

	/** get current speed */
	float GetVehicleSpeed() const;

	/** get current RPM */
	float GetEngineRotationSpeed() const;

	/** get maximum RPM */
	float GetEngineMaxRotationSpeed() const;

	UFUNCTION(BlueprintCallable, Category = "Game|Pawn")
	EShooterPlayerType GetPlayerType() const;

	UFUNCTION(BlueprintCallable, Category = "Game|Pawn")
	TSubclassOf<UDamageType> GetDamageType()const;

	UFUNCTION(BlueprintCallable, Category = "Game|Vehilce")
	bool IsSitFull() const;

	UFUNCTION(BlueprintCallable, Category = "Game|Vehilce")
	bool IsDriverSitEmpty() const;

	UFUNCTION(BlueprintCallable, Category = "Game|Vehilce")
	FName GetEmptySit() const;

	UFUNCTION(BlueprintCallable, Category = "Game|Vehicle")
	FName GetExitSocket() const;

	UFUNCTION(BlueprintCallable, Category = "Game|Vehilce")
	bool EnterVechicle(class AShooterCharacter* Character);

	UFUNCTION(BlueprintCallable, Category = "Game|Vehilce")
	bool ExitVechicle(class AShooterCharacter* Character);

	UFUNCTION(reliable,server, WithValidation)
	void Server_EnterOrExitVehicle(class AShooterCharacter* Character);

	UFUNCTION(Reliable,NetMulticast)
	void Multicast_EnterOrExitVehicle(class AShooterCharacter* Character);
	//////////////////////////////////////////////////////////////////////////
	// Input handlers

	/** event call on handbrake input */
	void OnHandbrakePressed();
	void OnHandbrakeReleased();

	/** event call on player exit */
	void OnEnterOrExitVehcile();

	void MoveForward(float Val);
	void MoveRight(float Val);

	void Turn(float Val);
	void LookUp(float Val);

	/* Frame rate independent turn */
	void TurnAtRate(float Val);
	/* Frame rate independent lookup */
	void LookUpAtRate(float Val);

	void OnDelayExitVehcile(class AShooterCharacter* Character);

	struct FVehicleDesiredRPM
	{
		float DesiredRPM;
		float TimeStamp;
	};

	static bool GetVehicleDesiredRPM_AudioThread(const uint32 VehicleID, FVehicleDesiredRPM& OutDesiredRPM);


private:
	/** Spring arm that will offset the camera */
	UPROPERTY(Category = Camera, VisibleDefaultsOnly, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
		USpringArmComponent* SpringArm;

	/** Camera component that will be our viewpoint */
	UPROPERTY(Category = Camera, VisibleDefaultsOnly, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
		UCameraComponent* Camera;

	/** AudioThread authoritative cache of desired RPM keyed by owner ID for SoundNodeVehicleEngine to reference */
	static TMap<uint32, FVehicleDesiredRPM> BuggyDesiredRPMs;

protected:
	/** Base turn rate, in deg/sec. Other scaling may affect final turn rate. */
	float BaseTurnRate;

	/** Base lookup rate, in deg/sec. Other scaling may affect final lookup rate. */
	float BaseLookUpRate;

	UPROPERTY(Category = Health, EditDefaultsOnly)
		float DamageValue;
	/** dust FX config */
	UPROPERTY(Category = Effects, EditDefaultsOnly)
		UVehicleDustType* DustType;

	/** DamageType config */
	UPROPERTY(Category = Effects, EditDefaultsOnly)
		TSubclassOf<UDamageType> DamageType;

	/** impact FX config */
	UPROPERTY(Category = Effects, EditDefaultsOnly)
		TSubclassOf<AShooterImpactEffect> ImpactTemplate;

	/** The minimum amount of normal force that must be applied to the chassis to spawn an Impact Effect */
	UPROPERTY(EditAnywhere, Category = Effects)
		float ImpactEffectNormalForceThreshold;

	/** explosion FX */
	UPROPERTY(Category = Effects, EditDefaultsOnly)
		UParticleSystem* DeathFX;

	/** explosion sound */
	UPROPERTY(Category = Effects, EditDefaultsOnly)
		USoundCue* DeathSound;

	/** engine sound */
	UPROPERTY(Category = Effects, EditDefaultsOnly)
		USoundCue* EngineSound;
private:
	/** audio component for engine sounds */
	UPROPERTY(Category = Effects, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
		UAudioComponent* EngineAC;
protected:
	/** landing sound */
	UPROPERTY(Category = Effects, EditDefaultsOnly)
		USoundCue* LandingSound;

	/** dust FX components */
	UPROPERTY(Transient)
		UParticleSystemComponent* DustPSC[4];

	/** skid sound loop */
	UPROPERTY(Category = Effects, EditDefaultsOnly)
		USoundCue* SkidSound;

	/** skid sound stop */
	UPROPERTY(Category = Effects, EditDefaultsOnly)
		USoundCue* SkidSoundStop;

	/** skid fadeout time */
	UPROPERTY(Category = Effects, EditDefaultsOnly)
		float SkidFadeoutTime;

	/** skid effects cannot play if velocity is lower than this */
	UPROPERTY(Category = Effects, EditDefaultsOnly)
		float SkidThresholdVelocity;

	/** skid effects will play if absolute value of tire longitudinal slip is more than this. */
	UPROPERTY(Category = Effects, EditDefaultsOnly, meta = (ClampMin = "0.0", UIMin = "0.0", ClampMax = "1.0", UIMax = "1.0"))
		float LongSlipSkidThreshold;

	/** skid effects will play if absolute value of tire lateral slip is more than this. */
	UPROPERTY(Category = Effects, EditDefaultsOnly, meta = (ClampMin = "0.0", UIMin = "0.0", ClampMax = "1.0", UIMax = "1.0"))
		float LateralSlipSkidThreshold;

private:
	/** audio component for skid sounds */
	UPROPERTY()
		UAudioComponent* SkidAC;
protected:

	/** The amount of spring compression required during landing to play sound */
	UPROPERTY(Category = Effects, EditDefaultsOnly)
		float SpringCompressionLandingThreshold;

	/** whether tires are currently touching ground */
	bool bTiresTouchingGround;

	/** if skidding is shorter than this value, SkidSoundStop won't be played */
	UPROPERTY(Category = Effects, EditDefaultsOnly)
		float SkidDurationRequiredForStopSound;

	/** is vehicle currently skidding */
	bool bSkidding;

	/** time when skidding started */
	float SkidStartTime;

	/** camera shake on impact */
	UPROPERTY(Category = Effects, EditDefaultsOnly)
		TSubclassOf<UCameraShake> ImpactCameraShake;

	/** How much throttle forward (max 1.0f) or reverse (max -1.0f) */
	float ThrottleInput;

	/** How far the wheels are turned to the right (max 1.0f) or to the left (max -1.0f) */
	float TurnInput;

	/** is handbrake active? */
	uint32 bHandbrakeActive : 1;

	/** if key is being held to control the throttle, ignore other controllers */
	uint32 bKeyboardThrottle : 1;

	/** if key is being held to control the turning, ignore other controllers */
	uint32 bKeyboardTurn : 1;

	/** if turn left action key is pressed */
	uint32 bTurnLeftPressed : 1;

	/** if turn right action key is pressed */
	uint32 bTurnRightPressed : 1;

	/** if accelerate action key is pressed */
	uint32 bAcceleratePressed : 1;

	/** if break/reverse action key is pressed */
	uint32 bBreakReversePressed : 1;

	/** FPS type player camera config*/
	UPROPERTY(EditDefaultsOnly, Category = "Camera|FPS")
	float FPS_EyeHeightOffset;

	/** TPS type player camera config*/
	UPROPERTY(EditDefaultsOnly, Category = "Camera|TPS")
	float TPS_TargetArmLengthOffset;

	UPROPERTY(EditDefaultsOnly, Category = "Camera|TPS")
	FVector TPS_SocketOffset;

	UPROPERTY(EditDefaultsOnly, Category = "Camera|TPS")
	FVector TPS_TargetOffset;

	/** TSS type player camera config*/
	UPROPERTY(EditDefaultsOnly, Category = "Camera|TSS")
	float TSS_TargetArmLengthOffset;

	UPROPERTY(EditDefaultsOnly, Category = "Camera|TSS")
	FVector TSS_SocketOffset;

	UPROPERTY(EditDefaultsOnly, Category = "Camera|TSS")
	FVector TSS_TargetOffset;

	UPROPERTY(EditDefaultsOnly, Category = "Camera|TSS")
	FRotator TSS_SprintArmRotationOffset;

	UPROPERTY(BlueprintReadWrite, Category = "Player|Character")
	class AShooterCharacter* DriverCharacter;

	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly, Category = "Player|Socket")
	FName DriverSocket;

	UPROPERTY(EditDefaultsOnly, Replicated , Category = "Player|Socket")
	TArray<FName> AllSitSockets;

	UPROPERTY(EditDefaultsOnly, Replicated, Category = "Player|Scoket")
	TArray<FName> AllExitScokets;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Player|Character")
	TArray<class AShooterCharacter*> AllPassengers;

	/** when entering new surface type, spawn new particle system, allowing old one to fade away nicely */
	void SpawnNewWheelEffect(int WheelIndex);

	/** update effects under wheels */
	void UpdateWheelEffects(float DeltaTime);

	/** Plays explosion particle and audio. */
	void PlayDestructionFX();

	void OnChangePlayerType(EShooterPlayerType ePlayerType);
protected:
	/** Returns SpringArm subobject **/
	FORCEINLINE USpringArmComponent* GetSpringArm() const { return SpringArm; }
	/** Returns Camera subobject **/
	FORCEINLINE UCameraComponent* GetCamera() const { return Camera; }
	/** Returns EngineAC subobject **/
	FORCEINLINE UAudioComponent* GetEngineAC() const { return EngineAC; }
	/** Returns SkidAC subobject **/
	FORCEINLINE UAudioComponent* GetSkidAC() const { return SkidAC; }
	
	
};
