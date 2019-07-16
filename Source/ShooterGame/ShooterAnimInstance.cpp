
#include "ShooterGame.h"
#include "ShooterAnimInstance.h"

UShooterAnimInstance::UShooterAnimInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

void UShooterAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	OwnerCharacter = Cast<AShooterCharacter>(TryGetPawnOwner());
}

FName UShooterAnimInstance::GetCurrentStateNameWithName(FName MachineName)
{
	int32 index = UAnimInstance::GetStateMachineIndex(MachineName);
	return UAnimInstance::GetCurrentStateName(index);
}


int32 UShooterAnimInstance::GetStateMachineIndex(FName MachineName)
{
	return UAnimInstance::GetStateMachineIndex(MachineName);
}

FName UShooterAnimInstance::GetMontageSectionName(UAnimMontage * MontageToPlay, int32 SectionIndex)
{
	return  MontageToPlay->GetSectionName(SectionIndex);
}

int32 UShooterAnimInstance::GetMontageSectionIndex(UAnimMontage * MontageToPlay, FName SectionName)
{
	return MontageToPlay->GetSectionIndex(SectionName);
}

float UShooterAnimInstance::GetMontageSectionLengthWithName(UAnimMontage * MontageToPlay, FName SectionName)
{
	int32 sectionIndex = MontageToPlay->GetSectionIndex(SectionName);
	return MontageToPlay->GetSectionLength(sectionIndex);
}

float UShooterAnimInstance::GetMontageSectionLengthWithIndex(UAnimMontage * MontageToPlay, int32 SectionIndex)
{
	return MontageToPlay->GetSectionLength(SectionIndex);
}

float UShooterAnimInstance::PlayMontageSection(UAnimMontage * MantageToPlay, FName SectionName, UCharacterMovementComponent * Character, bool RootMotion /* = false */, EMovementMode MovementMode)
{

	if (RootMotion)
	{
		Character->SetMovementMode(MovementMode);
	}

	this->Montage_Play(MantageToPlay);
	this->Montage_JumpToSection(SectionName, MantageToPlay);

	int32 sectionIndex = MantageToPlay->GetSectionIndex(SectionName);
	return MantageToPlay->GetSectionLength(sectionIndex);
}

