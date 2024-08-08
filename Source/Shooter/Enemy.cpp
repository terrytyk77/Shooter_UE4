// Fill out your copyright notice in the Description page of Project Settings.


#include "Enemy.h"
#include "ShooterCharacter.h"
#include "EnemyController.h"
#include "Sound/SoundCue.h"
#include "Blueprint/UserWidget.h"
#include "Components/SphereComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "Particles/ParticleSystemComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/SkeletalMeshSocket.h"

// Sets default values
AEnemy::AEnemy()
	: Health(100.f)
	, MaxHealth(Health)
	, HealthBarDisplayTime(4.f)
	, bCanHitReact(true)
	, HitReactTimeMin(0.4f)
	, HitReactTimeMax(1.5f)
	, HitNumberDestroyTime(1.5f)
	, bStunned(false)
	, StunChance(0.35f)
	, AttackLFast(TEXT("AttackLFast"))
	, AttackRFast(TEXT("AttackRFast"))
	, AttackL(TEXT("AttackL"))
	, AttackR(TEXT("AttackR"))
	, BaseDamage(20.f)
	, LeftWeaponSocket(TEXT("FX_Trail_L_01"))
	, RightWeaponSocket(TEXT("FX_Trail_R_01"))
	, bCanAttack(true)
	, AttackWaitTime(1.f)
	, bDying(false)
	, DeathTime(4.f)
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// Create the Aggro Sphere
	AggroSphere = CreateDefaultSubobject<USphereComponent>(TEXT("AggroSphere"));
	AggroSphere->SetupAttachment(GetRootComponent());
	AggroSphere->SetSphereRadius(500.f);

	// Create the Attack Range Sphere
	AttackSphere = CreateDefaultSubobject<USphereComponent>(TEXT("AttackRangeSphere"));
	AttackSphere->SetupAttachment(GetRootComponent());
	AttackSphere->SetSphereRadius(140.f);

	// Construct left and right weapon collision boxes
	LeftWeaponCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("LeftWeaponBox"));
	LeftWeaponCollision->SetupAttachment(GetMesh(), FName("LeftWeaponBone"));
	RightWeaponCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("RightWeaponBox"));
	RightWeaponCollision->SetupAttachment(GetMesh(), FName("RightWeaponBone"));
}

// Called when the game starts or when spawned
void AEnemy::BeginPlay()
{
	Super::BeginPlay();

	AggroSphere->OnComponentBeginOverlap.AddUniqueDynamic(this, &ThisClass::AggroSphereBeginOverlap);
	AttackSphere->OnComponentBeginOverlap.AddUniqueDynamic(this, &ThisClass::AttackSphereBeginOverlap);
	AttackSphere->OnComponentEndOverlap.AddUniqueDynamic(this, &ThisClass::AttackSphereEndOverlap);

	// Bind functions to overlap events for weapon boxes
	LeftWeaponCollision->OnComponentBeginOverlap.AddUniqueDynamic(this, &ThisClass::OnLeftWeaponOverlap);
	RightWeaponCollision->OnComponentBeginOverlap.AddUniqueDynamic(this, &ThisClass::OnRightWeaponOverlap);
	// Set collision presets for weapon boxes
	LeftWeaponCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LeftWeaponCollision->SetCollisionObjectType(ECollisionChannel::ECC_WorldDynamic);
	LeftWeaponCollision->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
	LeftWeaponCollision->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Overlap);
	RightWeaponCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RightWeaponCollision->SetCollisionObjectType(ECollisionChannel::ECC_WorldDynamic);
	RightWeaponCollision->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Overlap);

	GetMesh()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Visibility, ECollisionResponse::ECR_Block);
	GetMesh()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Camera, ECollisionResponse::ECR_Ignore);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Camera, ECollisionResponse::ECR_Ignore);

	const FVector WorldPatrolPoint = UKismetMathLibrary::TransformLocation(GetActorTransform(), PatrolPoint);
	const FVector WorldPatrolPoint2 = UKismetMathLibrary::TransformLocation(GetActorTransform(), PatrolPoint2);

	// Get enemy controller
	EnemyController = Cast<AEnemyController>(GetController());
	if (EnemyController)
	{
		EnemyController->GetBlackboardComponent()->SetValueAsVector(TEXT("PatrolPoint"), WorldPatrolPoint);
		EnemyController->GetBlackboardComponent()->SetValueAsVector(TEXT("PatrolPoint2"), WorldPatrolPoint2);
		EnemyController->GetBlackboardComponent()->SetValueAsBool(TEXT("CanAttack"), bCanAttack);
		EnemyController->GetBlackboardComponent()->SetValueAsBool(FName("Dead"), false);
		EnemyController->RunBehaviorTree(BehaviorTree);
	}
}

void AEnemy::ShowHealthBar_Implementation()
{
	GetWorldTimerManager().ClearTimer(HealthBarTimer);
	GetWorldTimerManager().SetTimer(HealthBarTimer, this, &ThisClass::HideHealthBar, HealthBarDisplayTime);
}

void AEnemy::Die()
{
	if (bDying)
	{
		return;
	}

	bDying = true;

	HideHealthBar();

	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && DeathMontage)
	{
		AnimInstance->Montage_Play(DeathMontage);
	}

	if (EnemyController)
	{
		EnemyController->GetBlackboardComponent()->SetValueAsBool(FName("Dead"), true);
		EnemyController->StopMovement();
	}
}

void AEnemy::PlayHitMontage(const FName& Section, float PlayRate)
{
	if (!bCanHitReact)
	{
		return;
	}

	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
	{
		AnimInstance->Montage_Play(HitMontage, PlayRate);
		AnimInstance->Montage_JumpToSection(Section, HitMontage);
	}

	bCanHitReact = false;
	const float HitReactTime{ FMath::FRandRange(HitReactTimeMin, HitReactTimeMax) };
	GetWorldTimerManager().SetTimer(HitReactTimer, this, &ThisClass::ResetHitReactTimer, HitReactTime);
}

void AEnemy::ResetHitReactTimer()
{
	bCanHitReact = true;
}

void AEnemy::StoreHitNumber(UUserWidget* const HitNumber, const FVector& Location)
{
	HitNumbers.Add(HitNumber, Location);
	
	FTimerHandle HitNumberTimer;
	FTimerDelegate HitNumberDelegate;
	HitNumberDelegate.BindUFunction(this, FName("DestroyHitNumber"), HitNumber);
	GetWorldTimerManager().SetTimer(HitNumberTimer, HitNumberDelegate, HitNumberDestroyTime, false);
}

void AEnemy::DestroyHitNumber(UUserWidget* HitNumber)
{
	HitNumbers.Remove(HitNumber);
	HitNumber->RemoveFromParent();
}

void AEnemy::UpdateHitNumbers()
{
	for (auto& HitPair : HitNumbers)
	{
		UUserWidget* HitNumber{ HitPair.Key };
		const FVector Location{ HitPair.Value };

		FVector2D ScreenPosition;
		UGameplayStatics::ProjectWorldToScreen(GetWorld()->GetFirstPlayerController(), Location, ScreenPosition);

		HitNumber->SetPositionInViewport(ScreenPosition);
	}
}

void AEnemy::AggroSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OtherActor || !EnemyController)
	{
		return;
	}

	if (AShooterCharacter* Character = Cast<AShooterCharacter>(OtherActor))
	{
		EnemyController->GetBlackboardComponent()->SetValueAsObject(TEXT("Target"), Character);
	}
}

void AEnemy::SetStunned(bool Stunned)
{
	bStunned = Stunned;

	if (EnemyController)
	{
		EnemyController->GetBlackboardComponent()->SetValueAsBool(TEXT("Stunned"), Stunned);
	}
}

void AEnemy::AttackSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OtherActor)
	{
		return;
	}

	if (AShooterCharacter* ShooterCharacter = Cast<AShooterCharacter>(OtherActor))
	{
		SetInAttackRange(true);
	}
}

void AEnemy::AttackSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (!OtherActor)
	{
		return;
	}

	if (AShooterCharacter* ShooterCharacter = Cast<AShooterCharacter>(OtherActor))
	{
		SetInAttackRange(false);
	}
}

void AEnemy::SetInAttackRange(bool InAttackRange)
{
	bInAttackRange = InAttackRange;

	if (EnemyController)
	{
		EnemyController->GetBlackboardComponent()->SetValueAsBool(TEXT("InAttackRange"), InAttackRange);
	}
}

void AEnemy::PlayAttackMontage(const FName& Section, float PlayRate)
{
	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
	{
		AnimInstance->Montage_Play(AttackMontage, PlayRate);
		AnimInstance->Montage_JumpToSection(Section, AttackMontage);
	}

	bCanAttack = false;
	GetWorldTimerManager().SetTimer(AttackWaitTimer, this, &ThisClass::ResetCanAttack, AttackWaitTime);
	if (EnemyController)
	{
		EnemyController->GetBlackboardComponent()->SetValueAsBool(FName("CanAttack"), false);
	}
}

const FName& AEnemy::GetAttackSectionName()
{
	const int32 Section{ FMath::RandRange(1, 4) };
	switch (Section)
	{
	case 1:
		return AttackLFast;
	case 2:
		return AttackRFast;
	case 3:
		return AttackL;
	case 4:
	default:
		return AttackR;
	}
}

void AEnemy::OnLeftWeaponOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OtherActor)
	{
		return;
	}

	if (AShooterCharacter* Character = Cast<AShooterCharacter>(OtherActor))
	{
		DoDamage(Character);
		SpawnBlood(Character, LeftWeaponSocket);
	}
}

void AEnemy::OnRightWeaponOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OtherActor)
	{
		return;
	}

	if (AShooterCharacter* Character = Cast<AShooterCharacter>(OtherActor))
	{
		DoDamage(Character);
		SpawnBlood(Character, RightWeaponSocket);
	}
}

void AEnemy::ActivateLeftWeapon()
{
	LeftWeaponCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
}

void AEnemy::DeactivateLeftWeapon()
{
	LeftWeaponCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AEnemy::ActivateRightWeapon()
{
	RightWeaponCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
}

void AEnemy::DeactivateRightWeapon()
{
	RightWeaponCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AEnemy::DoDamage(AShooterCharacter* Victim)
{
	UGameplayStatics::ApplyDamage(Victim, BaseDamage, EnemyController, this, UDamageType::StaticClass());

	// Attempt to stun the Character
	const float Stun{ FMath::FRand() };
	if (Stun <= Victim->GetStunChance())
	{
		Victim->Stun();
	}

	if (Victim->GetMeleeImpactSound())
	{
		UGameplayStatics::PlaySoundAtLocation(this, Victim->GetMeleeImpactSound(), GetActorLocation());
	}
}

void AEnemy::SpawnBlood(AShooterCharacter* Victim, const FName& SocketName)
{
	const USkeletalMeshSocket* TipSocket{ GetMesh()->GetSocketByName(SocketName) };
	if (TipSocket)
	{
		const FTransform SocketTransform{ TipSocket->GetSocketTransform(GetMesh()) };
		if (Victim->GetBloodParticles())
		{
			UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), Victim->GetBloodParticles(), SocketTransform);
		}
	}
}

void AEnemy::ResetCanAttack()
{
	bCanAttack = true;
	if (EnemyController)
	{
		EnemyController->GetBlackboardComponent()->SetValueAsBool(FName("CanAttack"), true);
	}
}

void AEnemy::FinishDeath()
{
	GetMesh()->bPauseAnims = true;
	GetWorldTimerManager().SetTimer(DeathTimer, this, &ThisClass::DestroyEnemy, DeathTime);
}

void AEnemy::DestroyEnemy()
{
	Destroy();
}

// Called every frame
void AEnemy::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UpdateHitNumbers();
}

// Called to bind functionality to input
void AEnemy::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}

void AEnemy::BulletHit_Implementation(const FHitResult& HitResult)
{
	if (IsValid(ImpactSound))
	{
		UGameplayStatics::PlaySoundAtLocation(this, ImpactSound, GetActorLocation());
	}

	if (IsValid(ImpactParticles))
	{
		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), ImpactParticles, HitResult.Location, FRotator{ 0.f }, true);
	}

	if (bDying)
	{
		return;
	}

	ShowHealthBar();

	// Determine whether bullet hit stuns
	const float Stunned = FMath::FRand();
	if (Stunned <= StunChance)
	{
		// Stun the Enemy
		PlayHitMontage(FName{ "HitReactFront" });
		SetStunned(true);
	}
}

float AEnemy::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	// Set the Target Blackboard key to aggro the character
	if (EnemyController)
	{
		EnemyController->GetBlackboardComponent()->SetValueAsObject("Target", DamageCauser);
	}

	if (Health - DamageAmount <= 0.f)
	{
		Health = 0.f;
		Die();
	}
	else
	{
		Health -= DamageAmount;
	}

	return DamageAmount;
}
