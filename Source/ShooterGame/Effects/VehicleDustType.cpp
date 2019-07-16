// Fill out your copyright notice in the Description page of Project Settings.

#include "ShooterGame.h"
#include "VehicleDustType.h"


UVehicleDustType::UVehicleDustType(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

UParticleSystem* UVehicleDustType::GetDustFX(UPhysicalMaterial* PhysMaterial, float CurrentSpeed)
{
	EPhysicalSurface SurfaceType = UPhysicalMaterial::DetermineSurfaceType(PhysMaterial);

	// determine particle FX
	UParticleSystem* WheelFX = GetWheelFX(SurfaceType);
	if (WheelFX)
	{
		const float MinSpeed = GetMinSpeed(SurfaceType);
		if (CurrentSpeed < MinSpeed)
		{
			WheelFX = nullptr;
		}
	}

	return WheelFX;
}

UParticleSystem* UVehicleDustType::GetWheelFX(TEnumAsByte<EPhysicalSurface> MaterialType)
{
	UParticleSystem* WheelFX = nullptr;

	switch (MaterialType)
	{
	case SHOOTER_SURFACE_Concrete:		WheelFX = ConcreteFX; break;
	case SHOOTER_SURFACE_Dirt:			WheelFX = DirtFX; break;
	case SHOOTER_SURFACE_Water:			WheelFX = WaterFX; break;
	case SHOOTER_SURFACE_Grass:			WheelFX = GrassFX; break;
	case SHOOTER_SURFACE_Gravel:		WheelFX = GravelFX; break;
	default:							WheelFX = nullptr; break;
	}

	return WheelFX;
}

float UVehicleDustType::GetMinSpeed(TEnumAsByte<EPhysicalSurface> MaterialType)
{
	float MinSpeed = 0.0f;

	switch (MaterialType)
	{
	case SHOOTER_SURFACE_Concrete:		MinSpeed = ConcreteMinSpeed; break;
	case SHOOTER_SURFACE_Dirt:			MinSpeed = DirtMinSpeed; break;
	case SHOOTER_SURFACE_Water:			MinSpeed = WaterMinSpeed; break;
	case SHOOTER_SURFACE_Grass:			MinSpeed = GrassMinSpeed; break;
	case SHOOTER_SURFACE_Gravel:		MinSpeed = GravelMinSpeed; break;
	default:							MinSpeed = 0.0f; break;
	}

	return MinSpeed;
}

