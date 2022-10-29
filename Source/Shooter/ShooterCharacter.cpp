// Fill out your copyright notice in the Description page of Project Settings.

#include "ShooterCharacter.h"
#include "Item.h"
#include "Weapon.h"

#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Particles/ParticleSystemComponent.h"
#include "Sound/SoundCue.h"

#include "Components/WidgetComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"

// Sets default values
AShooterCharacter::AShooterCharacter()
	// Base rates for turning/looking up
	: BaseTurnRate(45.f)
	, BaseLookUpRate(45.f)
	// Turn rates for aiming/not aiming
	, HipTurnRate(90.f)
	, HipLookUpRate(90.f)
	, AimingTurnRate(20.f)
	, AimingLookUpRate(20.f)
	// Mouse look sensitivity scale factors
	, MouseHipTurnRate(1.f)
	, MouseHipLookUpRate(1.f)
	, MouseAimingTurnRate(0.2f)
	, MouseAimingLookUpRate(0.2f)
	// True when aiming the weapon
	, bAiming(false)
	// Camera field of view values
	, CameraDefaultFOV(0.f) // Set in BeginPlay
	, CameraCurrentFOV(0.f) // Set in BeginPlay
	, CameraZoomedFOV(35.f)
	, ZoomInterpSpeed(20.f)
	// Crosshair spread factos
	, CrosshairSpreadMultiplier(0.f)
	, CrosshairVelocityFactor(0.f)
	, CrosshairInAirFactor(0.f)
	, CrosshairAimFactor(0.f)
	, CrosshairShootingFactor(0.f)
	// Bullet fire timer variables
	, ShootTimeDuration(0.05f)
	, bFiringBullet(false)
	// Automatic fire variables
	, AutomaticFireRate(0.1f)
	, bShouldFire(true)
	, bFireButtonPressed(false)
	// Item trace variables
	, bShouldTraceForItems(false)
	, OverlappedItemCount(0)
	// Camera interp location variables
	, CameraInterpDistance(250.f)
	, CameraInterpElevation(65.f)
	// Starting ammo amounts
	, Starting9mmAmmo(85)
	, StartingARAmmo(120)
	// Combat variables
	, CombatState(ECombatState::ECS_Unoccupied)
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// Create a camera boom (pulls in towards the character if there is a collison)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 180.f; // The camera follows at this distance behind the character
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller
	CameraBoom->SocketOffset = FVector( 0.f, 50.f, 70.f );

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach camera to end of boom
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	// Don't rotate the characterm when the controller rotate.
	// Let the controller affect the camera only.
	bUseControllerRotationPitch	= false;
	bUseControllerRotationYaw	= true;
	bUseControllerRotationRoll	= false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = false; // Character moves in the direction of input 
	GetCharacterMovement()->RotationRate = FRotator(0.f, 540.f, 0.f); // ... at this rotation rate
	GetCharacterMovement()->JumpZVelocity = 600.f;
	GetCharacterMovement()->AirControl = 0.2f;

	// Create Hand Scene Component
	HandSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("HandSceneComp"));
}

// Called when the game starts or when spawned
void AShooterCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (FollowCamera)
	{
		CameraDefaultFOV = GetFollowCamera()->FieldOfView;
		CameraCurrentFOV = CameraDefaultFOV;
	}

	// Spawn the default weapon and equip it to the mesh
	EquipWeapon(SpawnDefaultWeapon());

	InitializeAmmoMap();
}

void AShooterCharacter::MoveForward(float Value)
{
	if (Controller && Value != 0)
	{
		// Find out which way is forward
		const FRotator Rotation{ Controller->GetControlRotation() };
		const FRotator YawRotation{ 0, Rotation.Yaw, 0 };
		const FVector Direction{ FRotationMatrix{YawRotation}.GetUnitAxis(EAxis::X) };
		
		AddMovementInput(Direction, Value);
	}
}

void AShooterCharacter::MoveRight(float Value)
{
	if (Controller && Value != 0)
	{
		// Find out which way is right
		const FRotator Rotation{ Controller->GetControlRotation() };
		const FRotator YawRotation{ 0, Rotation.Yaw, 0 };
		const FVector Direction{ FRotationMatrix{YawRotation}.GetUnitAxis(EAxis::Y) };
		
		AddMovementInput(Direction, Value);
	}
}

void AShooterCharacter::TurnAtRate(float Rate)
{
	// Calculate delta for this frame from rate information
	AddControllerYawInput(Rate * BaseTurnRate * GetWorld()->GetDeltaSeconds()); // deg/sec * sec/frame
}

void AShooterCharacter::LookUpAtRate(float Rate)
{
	// Calculate delta for this frame from rate information
	AddControllerPitchInput(Rate * BaseLookUpRate * GetWorld()->GetDeltaSeconds()); // deg/sec * sec/frame
}

void AShooterCharacter::Turn(float Value)
{
	float TurnScaleFactor;

	if (bAiming)
		TurnScaleFactor = MouseAimingTurnRate;
	else
		TurnScaleFactor = MouseHipTurnRate;

	AddControllerYawInput(Value * TurnScaleFactor);
}

void AShooterCharacter::LookUp(float Value)
{
	float LookUpScaleFactor;

	if (bAiming)
		LookUpScaleFactor = MouseAimingLookUpRate;
	else
		LookUpScaleFactor = MouseHipLookUpRate;

	AddControllerPitchInput(Value * LookUpScaleFactor);
}

void AShooterCharacter::FireWeapon()
{
	if (!EquippedWeapon) return;
	if (CombatState != ECombatState::ECS_Unoccupied) return;

	if (WeaponHasAmmo())
	{
		PlayFireSound();
		SendBullet();
		PlayGunFireMontage();
		StartCrosshairBulletFire();
		EquippedWeapon->DecrementAmmo();
		StartFireTimer();
	}
}

bool AShooterCharacter::GetBeamEndLocation(const FVector& MuzzleSocketLocation, FVector& OutBeamLocation)
{
	// Check for crosshair trace hit
	FHitResult CrosshairHitResult;
	TraceUnderCrosshairs(CrosshairHitResult, OutBeamLocation);

	// Perform a second trace, this time from the gun barrel
	FHitResult WeaponTraceHit;
	const FVector WeaponTraceStart{ MuzzleSocketLocation };
	const FVector StartToEnd{ OutBeamLocation - MuzzleSocketLocation };
	const FVector WeaponTraceEnd{ MuzzleSocketLocation + StartToEnd * 1.25f };
	GetWorld()->LineTraceSingleByChannel(
		WeaponTraceHit,
		WeaponTraceStart,
		WeaponTraceEnd,
		ECollisionChannel::ECC_Visibility);

	if (WeaponTraceHit.bBlockingHit) // Object between barrel and BeamEndPoint?
	{
		OutBeamLocation = WeaponTraceHit.Location;
		return true;
	}

	return false;
}

void AShooterCharacter::AimingButtonPressed()
{
	bAiming = true;
}

void AShooterCharacter::AimingButtonReleased()
{
	bAiming = false;
}

void AShooterCharacter::CameraInterpZoom(float DeltaTime)
{
	// Set current camera field of view
	if (bAiming)
		// Interpolate to zoomed FOV
		CameraCurrentFOV = FMath::FInterpTo(CameraCurrentFOV, CameraZoomedFOV, DeltaTime, ZoomInterpSpeed);
	else
		// Interpolate to default FOV
		CameraCurrentFOV = FMath::FInterpTo(CameraCurrentFOV, CameraDefaultFOV, DeltaTime, ZoomInterpSpeed);

	FollowCamera->SetFieldOfView(CameraCurrentFOV);
}

void AShooterCharacter::SetLookRates()
{
	if (bAiming)
	{
		BaseTurnRate = AimingTurnRate;
		BaseLookUpRate = AimingLookUpRate;
	}
	else
	{
		BaseTurnRate = HipTurnRate;
		BaseLookUpRate = HipLookUpRate;
	}
}

void AShooterCharacter::CalculateCrosshairSpread(float DeltaTime)
{
	FVector2D WalkSpeedRange{ 0.f, GetCharacterMovement()->MaxWalkSpeed };
	FVector2D VelocityMultiplierRange{ 0, 1 };

	FVector Velocity{ GetVelocity() };
	Velocity.Z = 0;

	// Calculate crosshair velocity factor
	CrosshairVelocityFactor = FMath::GetMappedRangeValueClamped(WalkSpeedRange, VelocityMultiplierRange, Velocity.Size());

	// Calculate crosshair in air factor
	if (GetCharacterMovement()->IsFalling()) // Is in air?
		// Spread the crosshairs slowly while in air
		CrosshairInAirFactor = FMath::FInterpTo(CrosshairInAirFactor, 2.25f, DeltaTime, 2.25f);
	else // Character is on the ground
		// Shrink the crosshairs rapidely while on the ground
		CrosshairInAirFactor = FMath::FInterpTo(CrosshairInAirFactor, 0.f, DeltaTime, 30.f);

	// Calculate crosshair aim factor
	if(bAiming) // Is aiming?
		// Shrink the crosshairs rapidely while aiming
		CrosshairAimFactor = FMath::FInterpTo(CrosshairAimFactor, 0.6f, DeltaTime, 30.f);
	else
		// Spread the crosshairs rapidely while not aiming
		CrosshairAimFactor = FMath::FInterpTo(CrosshairAimFactor, 0, DeltaTime, 30.f);

	// True 0.05 second after firing
	if (bFiringBullet) // Is firing?
		CrosshairShootingFactor = FMath::FInterpTo(CrosshairShootingFactor, .3f, DeltaTime, 60.f);
	else
		CrosshairShootingFactor = FMath::FInterpTo(CrosshairShootingFactor, 0.f, DeltaTime, 60.f);

	CrosshairSpreadMultiplier = 0.5f + CrosshairVelocityFactor + CrosshairInAirFactor - CrosshairAimFactor + CrosshairShootingFactor;
}

void AShooterCharacter::StartCrosshairBulletFire()
{
	bFiringBullet = true;

	GetWorldTimerManager().SetTimer(CrosshairShootTimer, this, &AShooterCharacter::FinishCrosshairBulletFire, ShootTimeDuration);
}

void AShooterCharacter::FinishCrosshairBulletFire()
{
	bFiringBullet = false;
}

void AShooterCharacter::FireButtonPressed()
{
	bFireButtonPressed = true;
	FireWeapon();
}

void AShooterCharacter::FireButtonReleased()
{
	bFireButtonPressed = false;
}

void AShooterCharacter::StartFireTimer()
{
	CombatState = ECombatState::ECS_FireTimerInProgress;
	GetWorldTimerManager().SetTimer(AutoFireTimer, this, &AShooterCharacter::AutoFireReset, AutomaticFireRate);
}

void AShooterCharacter::AutoFireReset()
{
	CombatState = ECombatState::ECS_Unoccupied;

	if (WeaponHasAmmo())
	{
		if (bFireButtonPressed)
			FireWeapon();
	}
	else
	{
		ReloadWeapon();
	}
}

bool AShooterCharacter::TraceUnderCrosshairs(FHitResult& OutHitResult, FVector& OutHitLocation)
{
	// Get current size of the viewport
	FVector2D ViewportSize;
	if (GEngine && GEngine->GameViewport)
		GEngine->GameViewport->GetViewportSize(ViewportSize);

	// Get screen space location of crosshairs
	FVector2D CrosshairLocation(ViewportSize.X / 2.f, ViewportSize.Y / 2.f);
	FVector CrosshairWorldPosition;
	FVector CrosshairWorldDirection;

	// Get world position and direction of crosshairs
	bool bScreenToWorld = UGameplayStatics::DeprojectScreenToWorld(
		UGameplayStatics::GetPlayerController(this, 0),
		CrosshairLocation,
		CrosshairWorldPosition,
		CrosshairWorldDirection);

	if (bScreenToWorld)
	{
		// Trace from Crosshair world location outward
		const FVector Start{ CrosshairWorldPosition };
		const FVector End{ Start + CrosshairWorldDirection * 50'000.f };
		OutHitLocation = End;
		GetWorld()->LineTraceSingleByChannel(OutHitResult, Start, End, ECollisionChannel::ECC_Visibility);

		if (OutHitResult.bBlockingHit)
		{
			OutHitLocation = OutHitResult.Location;
			return true;
		}
	}

	return false;
}

void AShooterCharacter::TraceForItems()
{
	if (bShouldTraceForItems)
	{
		FHitResult ItemTraceResult;
		FVector HitLocation;
		if (TraceUnderCrosshairs(ItemTraceResult, HitLocation))
		{
			TraceHitItem = Cast<AItem>(ItemTraceResult.Actor);
			if (TraceHitItem)
				if (UWidgetComponent* PickupWidget = TraceHitItem->GetPickupWidget())
					PickupWidget->SetVisibility(true);

			// We hit an AItem last frame
			if (TraceHitItemLastFrame)
				if (TraceHitItem != TraceHitItemLastFrame)
					TraceHitItemLastFrame->GetPickupWidget()->SetVisibility(false);

			// Store a reference to HitItem for next frame
			TraceHitItemLastFrame = TraceHitItem;
		}
	}
	else if (TraceHitItemLastFrame)
	{
		// No longer overlapping any items, Item last frame should not show widget
		TraceHitItemLastFrame->GetPickupWidget()->SetVisibility(false);
	}
}

AWeapon* AShooterCharacter::SpawnDefaultWeapon()
{
	// Check the TSubClassOf variable
	if (DefaultWeaponClass)
		// Spawn the weapon
		return GetWorld()->SpawnActor<AWeapon>(DefaultWeaponClass);

	return nullptr;
}

void AShooterCharacter::EquipWeapon(AWeapon* WeaponToEquip)
{
	if (WeaponToEquip)
	{
		// Get the Hand Socket
		const USkeletalMeshSocket* HandSocket = GetMesh()->GetSocketByName(FName("RightHandSocket"));
		if (HandSocket)
			// Attach the Weapon to the hand socket RightHandSocket
			HandSocket->AttachActor(WeaponToEquip, GetMesh());
		
		// Set EquippedWeapon to the newly spawned Weapon
		EquippedWeapon = WeaponToEquip;
		EquippedWeapon->SetItemState(EItemState::EIS_Equipped);
	}
}

void AShooterCharacter::DropWeapon()
{
	if (EquippedWeapon)
	{
		FDetachmentTransformRules DetachmentTransformRules(EDetachmentRule::KeepWorld, true);
		EquippedWeapon->GetItemMesh()->DetachFromComponent(DetachmentTransformRules);
		
		EquippedWeapon->SetItemState(EItemState::EIS_Falling);
		EquippedWeapon->ThrowWeapon();
	}
}

void AShooterCharacter::SelectButtonPressed()
{
	if (TraceHitItem)
		TraceHitItem->StartItemCurve(this);
}

void AShooterCharacter::SelectButtonReleased()
{
}

void AShooterCharacter::SwapWeapon(AWeapon* WeaponToSwap)
{
	DropWeapon();
	EquipWeapon(WeaponToSwap);
	TraceHitItem = nullptr;
	TraceHitItemLastFrame = nullptr;
}

 void AShooterCharacter::InitializeAmmoMap()
{
	 AmmoMap.Add(EAmmoType::EAT_9mm, Starting9mmAmmo);
	 AmmoMap.Add(EAmmoType::EAT_AR, StartingARAmmo);
}

 bool AShooterCharacter::WeaponHasAmmo()
 {
	 if (!EquippedWeapon) return false;
	 return EquippedWeapon->GetAmmo() > 0;
 }

 void AShooterCharacter::PlayFireSound()
 {
	 if (FireSound)
		 UGameplayStatics::PlaySound2D(this, FireSound);
 }

 void AShooterCharacter::SendBullet()
 {
	 const USkeletalMeshSocket* BarrelSocket = EquippedWeapon->GetItemMesh()->GetSocketByName("BarrelSocket");
	 if (BarrelSocket)
	 {
		 const FTransform SocketTransform = BarrelSocket->GetSocketTransform(EquippedWeapon->GetItemMesh());

		 if (MuzzleFlash)
			 UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), MuzzleFlash, SocketTransform);

		 FVector BeamEnd;
		 bool bBeamEnd = GetBeamEndLocation(
			 SocketTransform.GetLocation(),
			 BeamEnd);

		 if (bBeamEnd)
		 {
			 if (ImpactParticles)
				 UGameplayStatics::SpawnEmitterAtLocation(
					 GetWorld(),
					 ImpactParticles,
					 BeamEnd);
		 }

		 if (BeamParticles)
		 {
			 UParticleSystemComponent* Beam = UGameplayStatics::SpawnEmitterAtLocation(
				 GetWorld(),
				 BeamParticles,
				 SocketTransform);

			 if (Beam)
				 Beam->SetVectorParameter(FName("Target"), BeamEnd);
		 }
	 }
 }

 void AShooterCharacter::PlayGunFireMontage()
 {
	 UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	 if (AnimInstance && HipFireMontage)
	 {
		 AnimInstance->Montage_Play(HipFireMontage);
		 AnimInstance->Montage_JumpToSection(FName("StartFire"));
	 }
 }

 void AShooterCharacter::ReloadButtonPressed()
 {
	 ReloadWeapon();
 }

 void AShooterCharacter::ReloadWeapon()
 {
	 if (CombatState != ECombatState::ECS_Unoccupied) return;
	 if (!EquippedWeapon) return;

	 // Do we have ammo of the correct type?
	 if (CarryingAmmo()) // replace with CarryingAmmo()
	 {
		 CombatState = ECombatState::ECS_Reloading;
		 UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
		 if (AnimInstance && ReloadMontage)
		 {
			 AnimInstance->Montage_Play(ReloadMontage);
			 AnimInstance->Montage_JumpToSection(EquippedWeapon->GetReloadMontageSection());
		 }
	 }
 }

 bool AShooterCharacter::CarryingAmmo()
 {
	 if (!EquippedWeapon) return false;

	 EAmmoType AmmoType = EquippedWeapon->GetAmmoType();

	 if (AmmoMap.Contains(AmmoType))
	 {
		 return AmmoMap[AmmoType] > 0;
	 }

	 return false;
 }

 void AShooterCharacter::GrabClip()
 {
	 if (!EquippedWeapon) return;
	 if (!HandSceneComponent) return;

	 // Index for the clip bone on the Equipped Weapon
	 int32 ClipBoneIndex{ EquippedWeapon->GetItemMesh()->GetBoneIndex(EquippedWeapon->GetClipBoneName()) };
	 // Store the transform of the clip
	 ClipTransform = EquippedWeapon->GetItemMesh()->GetBoneTransform(ClipBoneIndex);

	 FAttachmentTransformRules AttachmentRules{ EAttachmentRule::KeepRelative, true };
	 HandSceneComponent->AttachToComponent(GetMesh(), AttachmentRules, FName(TEXT("hand_l")));
	 HandSceneComponent->SetWorldTransform(ClipTransform);

	 EquippedWeapon->SetMovingClip(true);
 }

 void AShooterCharacter::ReleaseClip()
 {
	 EquippedWeapon->SetMovingClip(false);

 }

// Called every frame
void AShooterCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Handle interpolation for zoom when aiming
	CameraInterpZoom(DeltaTime);

	// Change look sensitivity based on aiming
	SetLookRates();

	// Calculate crosshair spread multiplier
	CalculateCrosshairSpread(DeltaTime);

	//Check OverlappedItemCount, then trace for items
	TraceForItems();
}

// Called to bind functionality to input
void AShooterCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	check(PlayerInputComponent);

	PlayerInputComponent->BindAxis("MoveForward", this, &AShooterCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &AShooterCharacter::MoveRight);
	PlayerInputComponent->BindAxis("TurnRate", this, &AShooterCharacter::TurnAtRate);
	PlayerInputComponent->BindAxis("LookUpRate", this, &AShooterCharacter::LookUpAtRate);
	PlayerInputComponent->BindAxis("Turn", this, &AShooterCharacter::Turn);
	PlayerInputComponent->BindAxis("LookUp", this, &AShooterCharacter::LookUp);

	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released,this, &ACharacter::StopJumping);
	
	PlayerInputComponent->BindAction("FireButton", IE_Pressed, this, &AShooterCharacter::FireButtonPressed);
	PlayerInputComponent->BindAction("FireButton", IE_Released, this, &AShooterCharacter::FireButtonReleased);

	PlayerInputComponent->BindAction("AimingButton", IE_Pressed, this, &AShooterCharacter::AimingButtonPressed);
	PlayerInputComponent->BindAction("AimingButton", IE_Released, this, &AShooterCharacter::AimingButtonReleased);

	PlayerInputComponent->BindAction("Select", IE_Pressed, this, &AShooterCharacter::SelectButtonPressed);
	PlayerInputComponent->BindAction("Select", IE_Released, this, &AShooterCharacter::SelectButtonReleased);

	PlayerInputComponent->BindAction("ReloadButton", IE_Pressed, this, &AShooterCharacter::ReloadButtonPressed);
}

void AShooterCharacter::FinishReloading()
{
	// Update the Combat State
	CombatState = ECombatState::ECS_Unoccupied;
	if (!EquippedWeapon) return;
	const EAmmoType AmmoType{ EquippedWeapon->GetAmmoType() };

	// Update the AmmoMap
	if (AmmoMap.Contains(AmmoType))
	{
		// Amout of ammo the Character is carrying of the EquippedWeapon type
		int32 CarriedAmmo = AmmoMap[AmmoType];

		// Space left in the magazine of EquippedWeapon
		const int32 MagEmptySpace = EquippedWeapon->GetMagazineCapacity() - EquippedWeapon->GetAmmo();

		if (MagEmptySpace > CarriedAmmo)
		{
			// Reload the magazine with all the ammo we are carrying
			EquippedWeapon->ReloadAmmo(CarriedAmmo);
			CarriedAmmo = 0;
			AmmoMap.Add(AmmoType, CarriedAmmo);
		}
		else
		{
			// Fill the magazine
			EquippedWeapon->ReloadAmmo(MagEmptySpace);
			CarriedAmmo -= MagEmptySpace;
			AmmoMap.Add(AmmoType, CarriedAmmo);
		}
	}
}

void AShooterCharacter::IncrementOverlappedItemCount(int8 Amount)
{
	if (OverlappedItemCount + Amount <= 0)
	{
		OverlappedItemCount = 0;
		bShouldTraceForItems = false;
	}
	else
	{
		OverlappedItemCount += Amount;
		bShouldTraceForItems = true;
	}
}

FVector AShooterCharacter::GetCameraInterpLocation()
{
	const FVector CameraWorldLocation{ FollowCamera->GetComponentLocation() };
	const FVector CameraForward{ FollowCamera->GetForwardVector() };
	return CameraWorldLocation + CameraForward * CameraInterpDistance + FVector(0, 0, CameraInterpElevation);
}

void AShooterCharacter::GetPickupItem(AItem* Item)
{
	if (AWeapon* Weapon = Cast<AWeapon>(Item))
	{
		SwapWeapon(Weapon);
	}
}