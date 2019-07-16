// Fill out your copyright notice in the Description page of Project Settings.

#include "ShooterGame.h"
#include "ShooterWeapon_Melee.h"


AShooterWeapon_Melee::AShooterWeapon_Melee(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	CollisionComp1P = ObjectInitializer.CreateDefaultSubobject<UCapsuleComponent>(this, "CapsuleComp1P");
	CollisionComp1P->InitCapsuleSize(5.0f, 10.0f);
	CollisionComp1P->AlwaysLoadOnClient = true;
	CollisionComp1P->AlwaysLoadOnServer = true;
	CollisionComp1P->bTraceComplexOnMove = true;
	CollisionComp1P->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionComp1P->SetCollisionObjectType(COLLISION_WEAPON);
	CollisionComp1P->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionComp1P->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	CollisionComp1P->SetupAttachment(GetMesh1P());

	CollisionComp1P->OnComponentBeginOverlap.AddDynamic(this, &AShooterWeapon_Melee::OnOverlapBegin);
	CollisionComp1P->OnComponentEndOverlap.AddDynamic(this, &AShooterWeapon_Melee::OnOverlapEnd);

	CollisionComp3P = ObjectInitializer.CreateDefaultSubobject<UCapsuleComponent>(this, "CapsuleComp3P");
	CollisionComp3P->InitCapsuleSize(5.0f, 10.0f);
	CollisionComp3P->AlwaysLoadOnClient = true;
	CollisionComp3P->AlwaysLoadOnServer = true;
	CollisionComp3P->bTraceComplexOnMove = true;
	CollisionComp3P->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionComp3P->SetCollisionObjectType(COLLISION_WEAPON);
	CollisionComp3P->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionComp3P->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	CollisionComp3P->SetupAttachment(GetMesh3P());

	CollisionComp3P->OnComponentBeginOverlap.AddDynamic(this, &AShooterWeapon_Melee::OnOverlapBegin);
	CollisionComp3P->OnComponentEndOverlap.AddDynamic(this, &AShooterWeapon_Melee::OnOverlapEnd);

	CollisionComp = CollisionComp1P;

	bHitWithStartFire = true;
}

void AShooterWeapon_Melee::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	CollisionComp1P->MoveIgnoreActors.Add(Instigator);
	CollisionComp1P->MoveIgnoreActors.Add(this);

	CollisionComp3P->MoveIgnoreActors.Add(Instigator);
	CollisionComp3P->MoveIgnoreActors.Add(this);

	this->ApplyWeaponConfig(MeleeWeaponData);
}


void AShooterWeapon_Melee::ApplyWeaponConfig(FMeleeWeaponData& Data)
{
	Data.MeleeClass = AShooterWeapon_Melee::StaticClass();
}

void AShooterWeapon_Melee::OnImpact(const FHitResult& HitResult)
{
	//TODO:Spawn effect
}

void AShooterWeapon_Melee::SetHitEnable(bool Enable)
{
	if (Enable)
	{
		CollisionComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	}
	else
	{
		CollisionComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

}

void AShooterWeapon_Melee::FireWeapon()
{

}

void AShooterWeapon_Melee::OnStartFire()
{
	Super::OnStartFire();

	if (bHitWithStartFire)
	{
		SetHitEnable(true);
	}
}

void AShooterWeapon_Melee::OnStopFire()
{
	Super::OnStopFire();

	if (bHitWithStartFire)
	{
		SetHitEnable(false);
	}
}

void AShooterWeapon_Melee::AttachMeshToPawn()
{
	Super::AttachMeshToPawn();

	CollisionComp1P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CollisionComp3P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (MyPawn != NULL)
	{
		MyPawn->IsFirstPerson() ? CollisionComp = CollisionComp1P : CollisionComp = CollisionComp3P;	

		CollisionComp1P->MoveIgnoreActors.Add(MyPawn);
		CollisionComp3P->MoveIgnoreActors.Add(MyPawn);
	}

}

bool AShooterWeapon_Melee::ShouldDealDamage(AActor* TestActor) const
{
	// if we're an actor on the server, or the actor's role is authoritative, we should register damage
	if (TestActor)
	{
		if (GetNetMode() != NM_Client ||
			TestActor->Role == ROLE_Authority ||
			TestActor->GetTearOff())
		{
			return true;
		}
	}

	return false;
}

void AShooterWeapon_Melee::DealDamage(const FHitResult& Impact)
{
	FPointDamageEvent PointDmg;
	PointDmg.DamageTypeClass = MeleeWeaponData.DamageType;
	PointDmg.HitInfo = Impact;
	PointDmg.Damage = MeleeWeaponData.MeleeDamage;

	if (MyPawn)
	{
		Impact.GetActor()->TakeDamage(PointDmg.Damage, PointDmg, MyPawn->Controller, this);
		this->OnImpact(Impact);
	}

}

void AShooterWeapon_Melee::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// Other Actor is the actor that triggered the event. Check that is not ourself.  
	if ((OtherActor != nullptr) && (OtherActor != this) && (OtherComp != nullptr))
	{
		//TODO:handle damage
		if (ShouldDealDamage(OtherActor))
		{
			DealDamage(SweepResult);
		}
	}
}

void AShooterWeapon_Melee::OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{

}