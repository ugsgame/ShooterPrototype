// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Weapons/ShooterWeapon.h"
#include "ShooterWeapon_None.generated.h"

/**
 * 
 */
UCLASS()
class SHOOTERGAME_API AShooterWeapon_None : public AShooterWeapon
{
	GENERATED_UCLASS_BODY()
	
protected:

	//UFUNCTION(BlueprintCallable, Category = Weapon)
	virtual EShooterWeaponType GetWeaponType() const override
	{
		return EShooterWeaponType::None;
	}
	
	virtual void FireWeapon() override
	{

	}
};
