#pragma once

#include "Animation/AnimInstance.h"
#include "Animation/AnimStateMachineTypes.h"
#include "ShooterAnimInstance.generated.h"


UCLASS(Abstract)
class SHOOTERGAME_API UShooterAnimInstance : public UAnimInstance
{
	GENERATED_UCLASS_BODY()

public:
	/**¶¯»­ËÙ¶È*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Behavior)
		float Speed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Behavior)
		float Direction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Behavior)
		float AimPitch;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Behavior)
		float AimYaw;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Behavior)
		bool bIsJumping;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Behavior)
		bool bIsRunning;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Behavior)
		class AShooterCharacter*  OwnerCharacter;
public:

	virtual void NativeInitializeAnimation() override;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = Animation)
		FName GetCurrentStateNameWithName(FName MachineName);
protected:

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = Animation)
		int32 GetStateMachineIndex(FName MachineName);

	UFUNCTION(BlueprintCallable, Category = Animation)
		FName GetMontageSectionName(UAnimMontage * MontageToPlay, int32 SectionIndex);

	UFUNCTION(BlueprintCallable, Category = Animation)
		int32 GetMontageSectionIndex(UAnimMontage * MontageToPlay, FName SectionName);

	UFUNCTION(BlueprintCallable, Category = Animation)
		float GetMontageSectionLengthWithName(UAnimMontage * MontageToPlay, FName SectionName);

	UFUNCTION(BlueprintCallable, Category = Animation)
		float GetMontageSectionLengthWithIndex(UAnimMontage * MontageToPlay, int32 SectionIndex);

	UFUNCTION(BlueprintCallable, Category = Animation)
		float PlayMontageSection(UAnimMontage * MantageToPlay, FName SectionName, UCharacterMovementComponent * Character, bool RootMotion = false, EMovementMode MovementMode = MOVE_Walking);
};