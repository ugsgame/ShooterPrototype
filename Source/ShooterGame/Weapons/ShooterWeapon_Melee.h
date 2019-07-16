// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Weapons/ShooterWeapon.h"
#include "Components/PrimitiveComponent.h"
#include "ShooterWeapon_Melee.generated.h"

/**
 * 
 */

USTRUCT()
struct  FMeleeWeaponData
{
	GENERATED_USTRUCT_BODY()

	/** projectile class */
	UPROPERTY(EditDefaultsOnly, Category = Melee)
	TSubclassOf<class AShooterWeapon_Melee> MeleeClass;

	UPROPERTY(EditDefaultsOnly, Category = WeaponStat)
	int32 MeleeDamage;

	/** type of damage */
	UPROPERTY(EditDefaultsOnly, Category = WeaponStat)
	TSubclassOf<UDamageType> DamageType;

	FMeleeWeaponData()
	{
		MeleeClass = NULL;
		MeleeDamage = 20;
		DamageType = UDamageType::StaticClass();
	}
};

UCLASS()
class SHOOTERGAME_API AShooterWeapon_Melee : public AShooterWeapon
{
	GENERATED_UCLASS_BODY()

	/** initial setup */
    virtual void PostInitializeComponents() override;

	void ApplyWeaponConfig(FMeleeWeaponData& Data);
	
	/** handle hit */
	UFUNCTION()
	void OnImpact(const FHitResult& HitResult);

	UFUNCTION(BlueprintCallable, Category = Melee)
	void SetHitEnable(bool Enable);

	//UFUNCTION(BlueprintCallable, Category = Weapon)
	virtual EShooterWeaponType GetWeaponType() const override
	{
		return EShooterWeaponType::Melee;
	}
protected:

	/** weapon config */
	UPROPERTY(EditDefaultsOnly, Category = Config)
	FMeleeWeaponData MeleeWeaponData;

	UPROPERTY(EditDefaultsOnly, Category = Melee)
	bool bHitWithStartFire;

	/** collisions */
	UPROPERTY(VisibleDefaultsOnly, Category = Melee)
	UCapsuleComponent* CollisionComp1P;

	UPROPERTY(VisibleDefaultsOnly, Category = Melee)
	UCapsuleComponent* CollisionComp3P;

	UCapsuleComponent* CollisionComp;

protected:

	//////////////////////////////////////////////////////////////////////////
	// Weapon usage

	/** [local] weapon specific fire implementation */
	virtual void FireWeapon() override;

	virtual void OnStartFire() override;

	virtual void OnStopFire() override;

	virtual void AttachMeshToPawn() override;

	bool ShouldDealDamage(AActor* TestActor) const;

	void DealDamage(const FHitResult& Impact);

	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	UFUNCTION()
	void OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);
};
