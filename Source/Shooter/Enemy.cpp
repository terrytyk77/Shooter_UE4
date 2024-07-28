// Fill out your copyright notice in the Description page of Project Settings.


#include "Enemy.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundCue.h"
#include "Particles/ParticleSystemComponent.h"

// Sets default values
AEnemy::AEnemy()
	: Health(100.f)
	, MaxHealth(Health)
	, HealthBarDisplayTime(4.f)
	, bCanHitReact(true)
	, HitReactTimeMin(0.4f)
	, HitReactTimeMax(1.5f)
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void AEnemy::BeginPlay()
{
	Super::BeginPlay();

	GetMesh()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Visibility, ECollisionResponse::ECR_Block);
}

void AEnemy::ShowHealthBar_Implementation()
{
	GetWorldTimerManager().ClearTimer(HealthBarTimer);
	GetWorldTimerManager().SetTimer(HealthBarTimer, this, &ThisClass::HideHealthBar, HealthBarDisplayTime);
}

void AEnemy::Die()
{
	HideHealthBar();
	Destroy();
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

// Called every frame
void AEnemy::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

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

	ShowHealthBar();
	PlayHitMontage(FName{ "HitReactFront" });
}

float AEnemy::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
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
