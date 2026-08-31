// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "TOCharacter.generated.h"

class UAnimMontage;
class USkeletalMeshComponent;

UENUM(BlueprintType)
enum class ECharacterEmoteState : uint8
{
	None            UMETA(DisplayName = "None"),
	Thinking        UMETA(DisplayName = "Thinking"),
	SubmitAnswer    UMETA(DisplayName = "Submit Answer"),
	CorrectAnswer   UMETA(DisplayName = "Correct Answer (+3)"),
	WrongAnswer     UMETA(DisplayName = "Wrong Answer"),
	GuessSuccess    UMETA(DisplayName = "Guess Success (+1)"),
	Defeat          UMETA(DisplayName = "Defeat")
};

UCLASS()
class OPERATOR_API ATOCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ATOCharacter();

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:	
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	// =========================================================================
	// Dynamic Modular Mesh Leader Pose Sync
	// =========================================================================
	UFUNCTION(BlueprintCallable, Category = "Operator|ModularMesh")
	void SetupLeaderPoseComponents();

	UFUNCTION(BlueprintCallable, Category = "Operator|ModularMesh")
	void RegisterModularMesh(USkeletalMeshComponent* ModularMesh);

	// =========================================================================
	// Seating State & Logic
	// =========================================================================
	UPROPERTY(ReplicatedUsing = OnRep_IsSeated, EditAnywhere, BlueprintReadWrite, Category = "Operator|Seating")
	bool bIsSeated = false;

	UFUNCTION(BlueprintCallable, Category = "Operator|Seating")
	void SetSeatedState(bool bSeated);

protected:
	UFUNCTION()
	void OnRep_IsSeated();

	void UpdateSeatedStateVisuals();

public:
	// =========================================================================
	// Hand Socket Card Attachment
	// =========================================================================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Operator|Card")
	FName HandCardSocketName = TEXT("hand_r_Socket");

	UFUNCTION(BlueprintCallable, Category = "Operator|Card")
	void AttachActorToHandSocket(AActor* TargetActor);

	// =========================================================================
	// Animation Montages & Directing Emotes
	// =========================================================================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Operator|Animation")
	UAnimMontage* SitIdleMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Operator|Animation")
	UAnimMontage* ThinkingMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Operator|Animation")
	UAnimMontage* SubmitMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Operator|Animation")
	UAnimMontage* CorrectMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Operator|Animation")
	UAnimMontage* WrongMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Operator|Animation")
	UAnimMontage* GuessSuccessMontage;

	UFUNCTION(BlueprintCallable, Category = "Operator|Animation")
	void PlayEmote(ECharacterEmoteState EmoteState);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayEmote(ECharacterEmoteState EmoteState);
};
