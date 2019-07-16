// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "AIController.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "ShooterAIController.generated.h"

class UBehaviorTreeComponent;
class UBlackboardComponent;

/************************************************************************/
/*                 AI的状态                                             */
/************************************************************************/
UENUM(BlueprintType)
enum class EShooterAIState: uint8
{
	EShooter_Idle		UMETA(DisplayName = "Idle"),
	EShooter_Patrol		UMETA(DisplayName = "Patrol"),
	EShooter_Search		UMETA(DisplayName = "Search"),
	EShooter_Fight		UMETA(DisplayName = "Fight"),
	EShooter_Follow		UMETA(DisplayName = "Follow")

};


UCLASS(config=Game)
class SHOOTERGAME_API AShooterAIController : public AAIController
{
	GENERATED_UCLASS_BODY()



public:
	UPROPERTY(BlueprintReadWrite,transient)
	UBlackboardComponent* BlackboardComp;

	/* Cached BT component */
	UPROPERTY(BlueprintReadWrite,transient)
	UBehaviorTreeComponent* BehaviorComp;


public:

	// Begin AController interface
	virtual void GameHasEnded(class AActor* EndGameFocus = NULL, bool bIsWinner = false) override;
	virtual void BeginInactiveState() override;

protected:
	virtual void OnPossess(class APawn* InPawn) override;
	virtual void OnUnPossess() override;
	// End APlayerController interface

public:
	virtual void Respawn();

	void CheckAmmo(const class AShooterWeapon* CurrentWeapon);

	void SetEnemy(class APawn* InPawn);

	class AShooterCharacter* GetEnemy() const;

	/* If there is line of sight to current enemy, start firing at them */
	UFUNCTION(BlueprintCallable, Category=Behavior)
	void ShootEnemy();

	/* Finds the closest enemy and sets them as current target */
	UFUNCTION(BlueprintCallable, Category=Behavior)
	void FindClosestEnemy();

	UFUNCTION(BlueprintCallable, Category = Behavior)
	bool FindClosestEnemyWithLOS(AShooterCharacter* ExcludeEnemy);
		
	bool HasWeaponLOSToEnemy(AActor* InEnemyActor, const bool bAnyEnemy) const;

	// Begin AAIController interface
	/** Update direction AI is looking based on FocalPoint */
	virtual void UpdateControlRotation(float DeltaTime, bool bUpdatePawn = true) override;
	// End AAIController interface


	UFUNCTION(BlueprintCallable, Category = "AI BB function")
	bool GetAI_IsNearEnemy();

	UFUNCTION(BlueprintCallable, Category = "AI BB Function")
	void SetAI_IsNearEnemy(bool isNear);


protected:
	// Check of we have LOS to a character
	bool LOSTrace(AShooterCharacter* InEnemyChar) const;

	int32 EnemyKeyID;
	int32 NeedAmmoKeyID;

	/** Handle for efficient management of Respawn timer */
	FTimerHandle TimerHandle_Respawn;

	UPROPERTY(EditAnywhere, Category = Health)
	bool bAllowRespawn;

public:
	/** Returns BlackboardComp subobject **/
	FORCEINLINE UBlackboardComponent* GetBlackboardComp() const { return BlackboardComp; }
	/** Returns BehaviorComp subobject **/
	FORCEINLINE UBehaviorTreeComponent* GetBehaviorComp() const { return BehaviorComp; }

	
	//更新ai数据
	void UpdateAIData();
	//获取当前AI状态
	EShooterAIState GetAI_State();
	//设置AI状态
	void SetAI_State(const EShooterAIState AIState);

	UFUNCTION(BlueprintCallable, Category = "Fight")
	APawn* GetPL_fPlayer() const;
	void SetPL_fPlayer(APawn* Player);

};
