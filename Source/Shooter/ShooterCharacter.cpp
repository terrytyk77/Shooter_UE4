// Fill out your copyright notice in the Description page of Project Settings.

#include "ShooterCharacter.h"
#include "Item.h"
#include "Weapon.h"
#include "Ammo.h"

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
#include "Components/CapsuleComponent.h"

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
	, MouseAimingTurnRate(0.6f)
	, MouseAimingLookUpRate(0.6f)
	// True when aiming the weapon
	, bAiming(false)
	// Camera field of view values
	, CameraDefaultFOV(0.f) // Set in BeginPlay
	, CameraCurrentFOV(0.f) // Set in BeginPlay
	, CameraZoomedFOV(25.f)
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
	, bCrouching(false)
	, BaseMovementSpeed(650.f)
	, CrouchMovementSpeed(300.f)
	, StandingCapsuleHalfHeight(88.f)
	, CrouchingCapsuleHalfHeight(44.f)
	, BaseGroundFriction(2.f)
	, CrouchingGroundFriction(100.f)
	, bAimingButtonPressed(false)
	// Pickup sound timer properties
	, bShouldPlayPickupSound(true)
	, bShouldPlayEquipSound(true)
	, PickupSoundResetTime(0.2f)
	, EquipSoundResetTime(0.2f)
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// Create a camera boom (pulls in towards the character if there is a collison)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 250.f; // The camera follows at this distance behind the character
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller
	CameraBoom->SocketOffset = FVector( 0.f, 35.f, 80.f );

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach camera to end of boom
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	// Don't rotate the character when the controller rotate.
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

	// Create Interpolation Components
	WeaponInterpComp = CreateDefaultSubobject<USceneComponent>(TEXT("Weapon Interpolation Component"));
	WeaponInterpComp->SetupAttachment(GetFollowCamera());

	InterpComp1 = CreateDefaultSubobject<USceneComponent>(TEXT("Interpolation Component 1"));
	InterpComp1->SetupAttachment(GetFollowCamera());

	InterpComp2 = CreateDefaultSubobject<USceneComponent>(TEXT("Interpolation Component 2"));
	InterpComp2->SetupAttachment(GetFollowCamera());

	InterpComp3 = CreateDefaultSubobject<USceneComponent>(TEXT("Interpolation Component 3"));
	InterpComp3->SetupAttachment(GetFollowCamera());

	InterpComp4 = CreateDefaultSubobject<USceneComponent>(TEXT("Interpolation Component 4"));
	InterpComp4->SetupAttachment(GetFollowCamera());

	InterpComp5 = CreateDefaultSubobject<USceneComponent>(TEXT("Interpolation Component 5"));
	InterpComp5->SetupAttachment(GetFollowCamera());

	InterpComp6 = CreateDefaultSubobject<USceneComponent>(TEXT("Interpolation Component 6"));
	InterpComp6->SetupAttachment(GetFollowCamera());
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
	Inventory.Add(EquippedWeapon);
	EquippedWeapon->SetSlotIndex(0);
	EquippedWeapon->DisableCustomDepth();
	EquippedWeapon->DisableGlowMaterial();
	EquippedWeapon->SetCharacter(this);
	
	InitializeAmmoMap();

	GetCharacterMovement()->MaxWalkSpeed = BaseMovementSpeed;

	// Create FInterpLocation structs for each interp location. Add to array
	InitializeInterpLocations();
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
	bAimingButtonPressed = true;
	if (CombatState != ECombatState::ECS_Reloading)
		Aim();
}

void AShooterCharacter::AimingButtonReleased()
{
	bAimingButtonPressed = false;
	StopAiming();
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
			if (TraceHitItem && TraceHitItem->GetItemState() == EItemState::EIS_EquipInterping)
			{
				TraceHitItem = nullptr;
			}

			if (TraceHitItem)
			{
				if (UWidgetComponent* PickupWidget = TraceHitItem->GetPickupWidget())
				{
					PickupWidget->SetVisibility(true);
					TraceHitItem->EnableCustomDepth();
				}
			}
			
			// We hit an AItem last frame
			if (TraceHitItemLastFrame)
			{
				if (TraceHitItem != TraceHitItemLastFrame)
				{
					TraceHitItemLastFrame->GetPickupWidget()->SetVisibility(false);
					TraceHitItemLastFrame->DisableCustomDepth();
				}
			}
				
			// Store a reference to HitItem for next frame
			TraceHitItemLastFrame = TraceHitItem;
		}
	}
	else if (TraceHitItemLastFrame)
	{
		// No longer overlapping any items, Item last frame should not show widget
		TraceHitItemLastFrame->GetPickupWidget()->SetVisibility(false);
		TraceHitItemLastFrame->DisableCustomDepth();
	}
}

AWeapon* AShooterCharacter::SpawnDefaultWeapon()
{
	// Check the TSubClassOf variable
	if (DefaultWeaponClass)
	{
		// Spawn the weapon
		return GetWorld()->SpawnActor<AWeapon>(DefaultWeaponClass);
	}

	return nullptr;
}

void AShooterCharacter::EquipWeapon(AWeapon* WeaponToEquip)
{
	if (WeaponToEquip)
	{
		// Get the Hand Socket
		const USkeletalMeshSocket* HandSocket = GetMesh()->GetSocketByName(FName("RightHandSocket"));
		if (HandSocket)
		{
			// Attach the Weapon to the hand socket RightHandSocket
			HandSocket->AttachActor(WeaponToEquip, GetMesh());
		}

		if (EquippedWeapon)
		{
			EquipItemDelegate.Broadcast(EquippedWeapon->GetSlotIndex(), WeaponToEquip->GetSlotIndex());
		}
		else
		{
			// -1 == no EquippedWeapon yet. No need to reverse the icon animation
			EquipItemDelegate.Broadcast(-1, WeaponToEquip->GetSlotIndex());
		}

		// Set EquippedWeapon to the newly spawned Weapon
		EquippedWeapon = WeaponToEquip;
		EquippedWeapon->SetItemState(EItemState::EIS_Equipped);
		EquippedWeapon->PlayEquipSound();
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
	if (CombatState != ECombatState::ECS_Unoccupied)
	{
		return;
	}

	if (TraceHitItem)
	{
		TraceHitItem->StartItemCurve(this, true);
		TraceHitItem = nullptr;
	}
}

void AShooterCharacter::SelectButtonReleased()
{
}

void AShooterCharacter::SwapWeapon(AWeapon* WeaponToSwap)
{
	if (Inventory.Num() > EquippedWeapon->GetSlotIndex())
	{
		Inventory[EquippedWeapon->GetSlotIndex()] = WeaponToSwap;
		WeaponToSwap->SetSlotIndex(EquippedWeapon->GetSlotIndex());
	}

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
	 if (CarryingAmmo() && !EquippedWeapon->ClipIsFull()) // replace with CarryingAmmo()
	 {
		 if (bAiming) 
			 StopAiming();

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

 void AShooterCharacter::CrouchButtonPressed()
 {
	 if (!GetCharacterMovement()->IsFalling())
		 bCrouching = !bCrouching;

	 if (bCrouching)
	 {
		 GetCharacterMovement()->MaxWalkSpeed = CrouchMovementSpeed;
		 GetCharacterMovement()->GroundFriction = CrouchingGroundFriction;
	 }
	 else
	 {
		 GetCharacterMovement()->MaxWalkSpeed = BaseMovementSpeed;
		 GetCharacterMovement()->GroundFriction = BaseGroundFriction;
	 }
 }

 void AShooterCharacter::Jump()
 {
	 if (bCrouching)
	 {
		 bCrouching = false;
		 GetCharacterMovement()->MaxWalkSpeed = BaseMovementSpeed;
	 }
	 else
	 {
		 Super::Jump();
	 }
 }

 void AShooterCharacter::InterpCapsuleHalfHeight(float DeltaTime)
 {
	 float TargetCapsuleHalfHeight;
	 if (bCrouching)
		 TargetCapsuleHalfHeight = CrouchingCapsuleHalfHeight;
	 else
		 TargetCapsuleHalfHeight = StandingCapsuleHalfHeight;

	 const float InterpHalfHeight{ FMath::FInterpTo(GetCapsuleComponent()->GetScaledCapsuleHalfHeight(), TargetCapsuleHalfHeight, DeltaTime, 20.f) };
	 
	 // Negative value if crouching, Positive value if standing
	 const float DeltaCapsuleHalfHeight{ InterpHalfHeight - GetCapsuleComponent()->GetScaledCapsuleHalfHeight() };
	 const FVector MeshOffset{ 0.f, 0.f, -DeltaCapsuleHalfHeight };
	 GetMesh()->AddLocalOffset(MeshOffset);

	 GetCapsuleComponent()->SetCapsuleHalfHeight(InterpHalfHeight);
 }

 void AShooterCharacter::Aim()
 {
	 bAiming = true;
	 GetCharacterMovement()->MaxWalkSpeed = CrouchMovementSpeed;
 }

 void AShooterCharacter::StopAiming()
 {
	 bAiming = false;

	 if (!bCrouching)
		 GetCharacterMovement()->MaxWalkSpeed = BaseMovementSpeed;
 }

 void AShooterCharacter::PickupAmmo(AAmmo* Ammo)
 {
	 // check to see if AmmoMap contains Ammo's AmmoType
	 if (AmmoMap.Find(Ammo->GetAmmoType()))
	 {
		 // Get amount of ammo in our AmmoMap for Ammo's type
		 int32 AmmoCount{ AmmoMap[Ammo->GetAmmoType()] };
		 AmmoCount += Ammo->GetItemCount();
		 // Set the amount of ammo in the Map for this type
		 AmmoMap[Ammo->GetAmmoType()] = AmmoCount;
	 }

	 if (EquippedWeapon->GetAmmoType() == Ammo->GetAmmoType())
	 {
		 // Check to see if the gun is empty
		 if (EquippedWeapon->GetAmmo() == 0)
		 {
			 ReloadWeapon();
		 }
	 }

	 Ammo->Destroy();
 }

 void AShooterCharacter::InitializeInterpLocations()
 {
	 InterpLocations.Reserve(7);
	 InterpLocations.Emplace(WeaponInterpComp);
	 InterpLocations.Emplace(InterpComp1);
	 InterpLocations.Emplace(InterpComp2);
	 InterpLocations.Emplace(InterpComp3);
	 InterpLocations.Emplace(InterpComp4);
	 InterpLocations.Emplace(InterpComp5);
	 InterpLocations.Emplace(InterpComp6);
 }

 void AShooterCharacter::FKeyPressed()
 {
	 ExchangeInventoryItems(EquippedWeapon->GetSlotIndex(), 0);
 }

 void AShooterCharacter::OneKeyPressed()
 {
	 ExchangeInventoryItems(EquippedWeapon->GetSlotIndex(), 1);
 }

 void AShooterCharacter::TwoKeyPressed()
 {
	 ExchangeInventoryItems(EquippedWeapon->GetSlotIndex(), 2);
 }

 void AShooterCharacter::ThreeKeyPressed()
 {
	 ExchangeInventoryItems(EquippedWeapon->GetSlotIndex(), 3);
 }

 void AShooterCharacter::FourKeyPressed()
 {
	 ExchangeInventoryItems(EquippedWeapon->GetSlotIndex(), 4);
 }

 void AShooterCharacter::FiveKeyPressed()
 {
	 ExchangeInventoryItems(EquippedWeapon->GetSlotIndex(), 5);
 }

 void AShooterCharacter::ExchangeInventoryItems(int32 CurrentItemIndex, int32 NewItemIndex)
 {
	 if (CombatState != ECombatState::ECS_Unoccupied || CurrentItemIndex == NewItemIndex || NewItemIndex == EquippedWeapon->GetSlotIndex() || NewItemIndex >= Inventory.Num())
	 {
		 return;
	 }

	 auto OldEquippedWeapon = EquippedWeapon;
	 auto NewWeapon = Cast<AWeapon>(Inventory[NewItemIndex]);
	 EquipWeapon(NewWeapon);

	 OldEquippedWeapon->SetItemState(EItemState::EIS_PickedUp);
	 NewWeapon->SetItemState(EItemState::EIS_Equipped);

	 CombatState = ECombatState::ECS_Equipping;
	 UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	 if (AnimInstance && EquipMontage)
	 {
		 AnimInstance->Montage_Play(EquipMontage, 1.0f);
		 AnimInstance->Montage_JumpToSection(FName("Equip"));
	 }
 }

 int32 AShooterCharacter::GetInterpLocationIndex()
 {
	 int32 LowestIndex = 1;
	 int32 LowestCount = MAX_int32;

	 for (int32 i = 1; i < InterpLocations.Num(); ++i)
	 {
		if (InterpLocations[i].ItemCount < LowestCount)
		{
			LowestIndex = i;
			LowestCount = InterpLocations[i].ItemCount;
		}
	 }

	 return LowestIndex;
 }

 void AShooterCharacter::IncrementInterpLocItemCount(int32 Index, int32 Amount)
 {
	 if (Amount < -1 || Amount > 1)
	 {
		 return;
	 }

	 if (InterpLocations.Num() < Index)
	 {
		 return;
	 }

	 InterpLocations[Index].ItemCount += Amount;
 }

 void AShooterCharacter::StartPickupSoundTimer()
 {
	 bShouldPlayPickupSound = false;
	 GetWorldTimerManager().SetTimer(PickupSoundTimer, this, &ThisClass::ResetPickupSoundTimer, PickupSoundResetTime);
 }

 void AShooterCharacter::StartEquipSoundTimer()
 {
	 bShouldPlayEquipSound = false;
	 GetWorldTimerManager().SetTimer(EquipSoundTimer, this, &ThisClass::ResetEquipSoundTimer, EquipSoundResetTime);
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

	// Check OverlappedItemCount, then trace for items
	TraceForItems();

	// Interpolate the capsule half height based on crouching/standing
	InterpCapsuleHalfHeight(DeltaTime);
}

// Called to bind functionality to input
void AShooterCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	check(PlayerInputComponent);

	PlayerInputComponent->BindAxis("MoveForward", this, &ThisClass::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &ThisClass::MoveRight);
	PlayerInputComponent->BindAxis("TurnRate", this, &ThisClass::TurnAtRate);
	PlayerInputComponent->BindAxis("LookUpRate", this, &ThisClass::LookUpAtRate);
	PlayerInputComponent->BindAxis("Turn", this, &ThisClass::Turn);
	PlayerInputComponent->BindAxis("LookUp", this, &ThisClass::LookUp);

	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ThisClass::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released,this, &ACharacter::StopJumping);
	
	PlayerInputComponent->BindAction("FireButton", IE_Pressed, this, &ThisClass::FireButtonPressed);
	PlayerInputComponent->BindAction("FireButton", IE_Released, this, &ThisClass::FireButtonReleased);

	PlayerInputComponent->BindAction("AimingButton", IE_Pressed, this, &ThisClass::AimingButtonPressed);
	PlayerInputComponent->BindAction("AimingButton", IE_Released, this, &ThisClass::AimingButtonReleased);

	PlayerInputComponent->BindAction("Select", IE_Pressed, this, &ThisClass::SelectButtonPressed);
	PlayerInputComponent->BindAction("Select", IE_Released, this, &ThisClass::SelectButtonReleased);

	PlayerInputComponent->BindAction("ReloadButton", IE_Pressed, this, &ThisClass::ReloadButtonPressed);

	PlayerInputComponent->BindAction("Crouch", IE_Pressed, this, &ThisClass::CrouchButtonPressed);

	PlayerInputComponent->BindAction("FKey", IE_Pressed, this, &ThisClass::FKeyPressed);
	PlayerInputComponent->BindAction("1Key", IE_Pressed, this, &ThisClass::OneKeyPressed);
	PlayerInputComponent->BindAction("2Key", IE_Pressed, this, &ThisClass::TwoKeyPressed);
	PlayerInputComponent->BindAction("3Key", IE_Pressed, this, &ThisClass::ThreeKeyPressed);
	PlayerInputComponent->BindAction("4Key", IE_Pressed, this, &ThisClass::FourKeyPressed);
	PlayerInputComponent->BindAction("5Key", IE_Pressed, this, &ThisClass::FiveKeyPressed);
}

void AShooterCharacter::FinishReloading()
{
	// Update the Combat State
	CombatState = ECombatState::ECS_Unoccupied;
	if (bAimingButtonPressed) Aim();
	if (!EquippedWeapon) return;
	const EAmmoType AmmoType{ EquippedWeapon->GetAmmoType() };

	// Update the AmmoMap
	if (AmmoMap.Contains(AmmoType))
	{
		// Amount of ammo the Character is carrying of the EquippedWeapon type
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

void AShooterCharacter::FinishEquipping()
{
	CombatState = ECombatState::ECS_Unoccupied;
}

void AShooterCharacter::ResetPickupSoundTimer()
{
	bShouldPlayPickupSound = true;
}

void AShooterCharacter::ResetEquipSoundTimer()
{
	bShouldPlayEquipSound = true;
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

// No longer needed; AItem has GetInterpLocation
/*FVector AShooterCharacter::GetCameraInterpLocation()
{
	const FVector CameraWorldLocation{ FollowCamera->GetComponentLocation() };
	const FVector CameraForward{ FollowCamera->GetForwardVector() };
	return CameraWorldLocation + CameraForward * CameraInterpDistance + FVector(0, 0, CameraInterpElevation);
}*/

void AShooterCharacter::GetPickupItem(AItem* Item)
{
	if (AWeapon* Weapon = Cast<AWeapon>(Item))
	{
		if (Inventory.Num() < INVENTORY_CAPACITY)
		{
			Weapon->SetSlotIndex(Inventory.Num());
			Inventory.Add(Weapon);
			Weapon->SetItemState(EItemState::EIS_PickedUp);
		}
		else // Inventory is full! Swap with EquippedWeapon
		{
			SwapWeapon(Weapon);
		}
	}


	if (AAmmo* Ammo = Cast<AAmmo>(Item))
	{
		PickupAmmo(Ammo);
	}
}

FInterpLocation AShooterCharacter::GetInterpLocation(int32 Index)
{
	if (Index <= InterpLocations.Num())
	{
		return InterpLocations[Index];
	}

	return FInterpLocation();
}
