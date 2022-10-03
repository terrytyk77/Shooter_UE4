// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Item.generated.h"

class UBoxComponent;
class USphereComponent;
class UWidgetComponent;
class AShooterCharacter;
class UCurveFloat;

UENUM(BlueprintType)
enum class EItemRarity : uint8
{
	EIR_Common		UMETA(DisplayName = "Common"),
	EIR_Uncommon	UMETA(DisplayName = "Uncommon"),
	EIR_Rare		UMETA(DisplayName = "Rare"),
	EIR_Epic		UMETA(DisplayName = "Epic"),
	EIR_Legendary	UMETA(DisplayName = "Legendary"),

	EIR_MAX			UMETA(DisplayName = "DeultMax")
};

UENUM(BlueprintType)
enum class EItemState : uint8
{
	EIS_Pickup			UMETA(DisplayName = "Pickup"),
	EIS_EquipInterping	UMETA(DisplayName = "EquipInterping"),
	EIS_PickedUp		UMETA(DisplayName = "PickedUp"),
	EIS_Equipped		UMETA(DisplayName = "Equipped"),
	EIS_Falling			UMETA(DisplayName = "Falling"),

	EIS_MAX				UMETA(DisplayName = "DeultMax")
};


UCLASS()
class SHOOTER_API AItem : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AItem();

	// Called every frame
	virtual void Tick(float DeltaTime) override;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	/** Called when Begin Overlapping AreaSphere */
	UFUNCTION()
	void OnSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	
	/** Called when End Overlapping AreaSphere */
	UFUNCTION()
	void OnSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	/** Sets the ActiveStars array of bools based on rarity */
	void SetActiveStars();

	/** Sets properties of the Item's components based on State */
	void SetItemProperties(EItemState State);

	/** Called when ItemInterpTimer is finished */
	void FinishInterping();

	/** Handle item interpolation when in the EquipInterping state */
	void ItemInterp(float DeltaTime);

private:
	/** Skeletal Mesh for the item */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item Properties", meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* ItemMesh;

	/** Line trace colldies with box to show HUD widgets */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Properties", meta = (AllowPrivateAccess = "true"))
	UBoxComponent* CollisionBox;

	/** Popup widget for when the player looks at the item */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Properties", meta = (AllowPrivateAccess = "true"))
	UWidgetComponent* PickupWidget;

	/** Enables item tracing when overlapped */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Properties", meta = (AllowPrivateAccess = "true"))
	USphereComponent* AreaSphere;
	
	/** The name which appear on the Pickup Widget */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Properties", meta = (AllowPrivateAccess = "true"))
	FString ItemName;

	/** Item count (ammo, etc.) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Properties", meta = (AllowPrivateAccess = "true"))
	int32 ItemCount;

	/** Item rarity - determines the number of stars in Pickup Widget */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Properties", meta = (AllowPrivateAccess = "true"))
	EItemRarity ItemRarity;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item Properties", meta = (AllowPrivateAccess = "true"))
	TArray<bool> ActiveStars;

	/** State of the item */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item Properties", meta = (AllowPrivateAccess = "true"))
	EItemState ItemState;

	/** The curve asset to use for the item's z location when interping */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item Properties", meta = (AllowPrivateAccess = "true"))
	UCurveFloat* ItemZCurve;

	/** Starting location when interping begins */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item Properties", meta = (AllowPrivateAccess = "true"))
	FVector ItemInterpStartLocation;

	/** Target interp location in front of the camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item Properties", meta = (AllowPrivateAccess = "true"))
	FVector CameraTargetLocation;

	/** True when interping */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item Properties", meta = (AllowPrivateAccess = "true"))
	bool bInterping;

	/** Plays when we start interping */
	FTimerHandle ItemInterpTimer;
	/** Duration of the curve and timer */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item Properties", meta = (AllowPrivateAccess = "true"))
	float ZCurveTime;

	/** Pointer to character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item Properties", meta = (AllowPrivateAccess = "true"))
	AShooterCharacter* Character;

	/** X and Y for the Item while interping in the EquipInterping state */
	float ItemInterpX;
	float ItemInterpY;

public:
	FORCEINLINE UWidgetComponent* GetPickupWidget() const { return PickupWidget; }
	FORCEINLINE USphereComponent* GetAreaSphere() const { return AreaSphere; }
	FORCEINLINE UBoxComponent* GetCollsionBox() const { return CollisionBox; }
	FORCEINLINE USkeletalMeshComponent* GetItemMesh() const { return ItemMesh; }
	FORCEINLINE EItemState GetItemState() const { return ItemState; }
	void SetItemState(EItemState State);

	/** Called from the AShooterCharacter class */
	void StartItemCurve(AShooterCharacter* Char);
};
