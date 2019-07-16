// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ShooterTypes.h"
#include "VehicleDustType.generated.h"

/**
 * 
 */
UCLASS()
class SHOOTERGAME_API UVehicleDustType : public UDataAsset
{
	GENERATED_UCLASS_BODY()

		/** FX under wheel on asphalt */
		UPROPERTY(EditDefaultsOnly, Category = Effect)
		UParticleSystem* ConcreteFX;

	/** FX under wheel on dirt */
	UPROPERTY(EditDefaultsOnly, Category = Effect)
		UParticleSystem* DirtFX;

	/** FX under wheel on water */
	UPROPERTY(EditDefaultsOnly, Category = Effect)
		UParticleSystem* WaterFX;

	/** FX under wheel on grass */
	UPROPERTY(EditDefaultsOnly, Category = Effect)
		UParticleSystem* GrassFX;

	/** FX under wheel on gravel */
	UPROPERTY(EditDefaultsOnly, Category = Effect)
		UParticleSystem* GravelFX;

	/** min speed to show FX on asphalt */
	UPROPERTY(EditDefaultsOnly, Category = Speed)
		float ConcreteMinSpeed;

	/** min speed to show FX on dirt */
	UPROPERTY(EditDefaultsOnly, Category = Speed)
		float DirtMinSpeed;

	/** min speed to show FX on water */
	UPROPERTY(EditDefaultsOnly, Category = Speed)
		float WaterMinSpeed;

	/** min speed to show FX on grass */
	UPROPERTY(EditDefaultsOnly, Category = Speed)
		float GrassMinSpeed;

	/** min speed to show FX on gravel */
	UPROPERTY(EditDefaultsOnly, Category = Speed)
		float GravelMinSpeed;

	/** determine correct FX */
	UParticleSystem* GetDustFX(UPhysicalMaterial* PhysMaterial, float CurrentSpeed);

protected:

	/** get FX for material type */
	UParticleSystem* GetWheelFX(TEnumAsByte<EPhysicalSurface> MaterialType);

	/** get min speed for material type */
	float GetMinSpeed(TEnumAsByte<EPhysicalSurface> MaterialType);
};
