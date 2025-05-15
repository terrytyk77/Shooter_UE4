// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "EnemyController.generated.h"

class UBlackboardComponent;
class UBehaviorTreeComponent;

UCLASS()
class SHOOTER_API AEnemyController : public AAIController
{
	GENERATED_BODY()
	
	AEnemyController();

public:
	FORCEINLINE UBlackboardComponent* GetBlackboardComponent() const { return Blackboard; }
	FORCEINLINE UBehaviorTreeComponent* GetBehaviorTreeComponent() const { return BehaviorTreeComponent; }

public:
	virtual void OnPossess(APawn* InPawn) override;

private:
	/** Blackboard component for this enemy */
	UPROPERTY(BlueprintReadWrite, Category = "AI Behavior", meta = (AllowPrivateAccess = "true"))
	UBlackboardComponent* BlackboardComponent;
	
	/** Behavior tree component for this enemy */
	UPROPERTY(BlueprintReadWrite, Category = "AI Behavior", meta = (AllowPrivateAccess = "true"))
	UBehaviorTreeComponent* BehaviorTreeComponent;
};
