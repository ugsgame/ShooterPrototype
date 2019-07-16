// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ShooterCharacter.h"
#include "ShooterBot.generated.h"

UCLASS()
class SHOOTERGAME_API AShooterBot : public AShooterCharacter
{
	GENERATED_UCLASS_BODY()

	virtual bool IsFirstPerson() const override;

	virtual void FaceRotation(FRotator NewRotation, float DeltaTime = 0.f) override;
};