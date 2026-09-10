// Fill out your copyright notice in the Description page of Project Settings.

#include "TOCharacter.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "../GameManager/TOPlayerController.h"
#include "EnhancedInputComponent.h"
#include "InputCoreTypes.h"

ATOCharacter::ATOCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
}

void ATOCharacter::BeginPlay()
{
	Super::BeginPlay();

	SetupLeaderPoseComponents();
	UpdateSeatedStateVisuals();
}

void ATOCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (IsLocallyControlled())
	{
		const FRotator ControlRot = GetControlRotation();
		const FRotator ActorRot = GetActorRotation();
		const FRotator DeltaRot = (ControlRot - ActorRot).GetNormalized();

		// Clamping Yaw [-70, 70] and Pitch [-40, 30] for natural head rotation
		const float TargetYaw = FMath::Clamp(DeltaRot.Yaw, -70.0f, 70.0f);
		// Invert pitch: looking down yields negative pitch, but mesh Component Space Roll needs positive rotation to tilt head down
		const float TargetPitch = -FMath::Clamp(DeltaRot.Pitch, -40.0f, 30.0f);
		const FRotator TargetHeadRot(TargetPitch, TargetYaw, 0.0f);

		HeadRotation = FMath::RInterpTo(HeadRotation, TargetHeadRot, DeltaTime, 12.0f);

		if (!HasAuthority())
		{
			if (!ReplicatedHeadRotation.Equals(TargetHeadRot, 1.0f))
			{
				ReplicatedHeadRotation = TargetHeadRot;
				Server_UpdateHeadRotation(TargetHeadRot);
			}
		}
		else
		{
			ReplicatedHeadRotation = TargetHeadRot;
		}
	}
	else
	{
		HeadRotation = FMath::RInterpTo(HeadRotation, ReplicatedHeadRotation, DeltaTime, 12.0f);
	}
}

void ATOCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (ToggleHUDAction)
		{
			EnhancedInputComponent->BindAction(ToggleHUDAction, ETriggerEvent::Started, this, &ATOCharacter::RequestToggleHUD);
		}
	}

	// Tab 키 입력 시 PC->ToggleHUD() 호출 (기본 키 바인딩)
	PlayerInputComponent->BindKey(EKeys::Tab, IE_Pressed, this, &ATOCharacter::RequestToggleHUD);
}

void ATOCharacter::RequestToggleHUD()
{
	if (ATOPlayerController* PC = Cast<ATOPlayerController>(GetController()))
	{
		PC->ToggleHUD();
	}
}

void ATOCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ATOCharacter, bIsSeated);
	DOREPLIFETIME_CONDITION(ATOCharacter, ReplicatedHeadRotation, COND_SkipOwner);
}

void ATOCharacter::SetupLeaderPoseComponents()
{
	USkeletalMeshComponent* MainMesh = GetMesh();
	if (!MainMesh) return;

	// Automatically scan and sync all child USkeletalMeshComponents dynamically created in Blueprint (e.g. shorts, hair, top) under MainMesh
	TArray<USceneComponent*> ChildComponents;
	MainMesh->GetChildrenComponents(true, ChildComponents);
	for (USceneComponent* Child : ChildComponents)
	{
		if (USkeletalMeshComponent* SkelChild = Cast<USkeletalMeshComponent>(Child))
		{
			if (SkelChild != MainMesh)
			{
				SkelChild->SetLeaderPoseComponent(MainMesh);
			}
		}
	}
}

void ATOCharacter::RegisterModularMesh(USkeletalMeshComponent* ModularMesh)
{
	USkeletalMeshComponent* MainMesh = GetMesh();
	if (ModularMesh && MainMesh && ModularMesh != MainMesh)
	{
		ModularMesh->SetLeaderPoseComponent(MainMesh);
	}
}

void ATOCharacter::SetSeatedState(bool bSeated)
{
	bIsSeated = bSeated;
	UpdateSeatedStateVisuals();
}

void ATOCharacter::OnRep_IsSeated()
{
	UpdateSeatedStateVisuals();
}

void ATOCharacter::UpdateSeatedStateVisuals()
{
	if (bIsSeated)
	{
		if (GetCharacterMovement())
		{
			GetCharacterMovement()->SetMovementMode(MOVE_None);
		}
		if (SitIdleMontage && GetMesh() && GetMesh()->GetAnimInstance())
		{
			GetMesh()->GetAnimInstance()->Montage_Play(SitIdleMontage);
		}
	}
	else
	{
		if (GetCharacterMovement())
		{
			GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		}
	}
}

void ATOCharacter::AttachActorToHandSocket(AActor* TargetActor)
{
	if (!TargetActor) return;

	if (GetMesh() && GetMesh()->DoesSocketExist(HandCardSocketName))
	{
		TargetActor->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, HandCardSocketName);
	}
	else if (GetMesh())
	{
		TargetActor->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	}
}

void ATOCharacter::PlayEmote(ECharacterEmoteState EmoteState)
{
	// Local immediate playback for responsiveness
	UAnimInstance* AnimInst = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (AnimInst)
	{
		UAnimMontage* TargetMontage = nullptr;
		switch (EmoteState)
		{
		case ECharacterEmoteState::Victory:
			TargetMontage = VictoryMontage ? VictoryMontage : CorrectMontage;
			break;
		case ECharacterEmoteState::Defeat:
			TargetMontage = DefeatMontage ? DefeatMontage : WrongMontage;
			break;
		case ECharacterEmoteState::Pointing:
			TargetMontage = PointingMontage;
			break;
		case ECharacterEmoteState::Thinking:
			TargetMontage = ThinkingMontage;
			break;
		case ECharacterEmoteState::SubmitAnswer:
			TargetMontage = SubmitMontage;
			break;
		case ECharacterEmoteState::CorrectAnswer:
			TargetMontage = CorrectMontage ? CorrectMontage : VictoryMontage;
			break;
		case ECharacterEmoteState::WrongAnswer:
			TargetMontage = WrongMontage ? WrongMontage : DefeatMontage;
			break;
		case ECharacterEmoteState::GuessSuccess:
			TargetMontage = GuessSuccessMontage;
			break;
		default:
			break;
		}

		if (TargetMontage)
		{
			AnimInst->Montage_Play(TargetMontage);
		}
	}

	// Server replication
	if (HasAuthority())
	{
		Multicast_PlayEmote(EmoteState);
	}
	else
	{
		Server_PlayEmote(EmoteState);
	}
}

void ATOCharacter::Server_PlayEmote_Implementation(ECharacterEmoteState EmoteState)
{
	Multicast_PlayEmote(EmoteState);
}

void ATOCharacter::Multicast_PlayEmote_Implementation(ECharacterEmoteState EmoteState)
{
	if (IsLocallyControlled()) return; // Already played locally

	UAnimInstance* AnimInst = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (!AnimInst) return;

	UAnimMontage* TargetMontage = nullptr;
	switch (EmoteState)
	{
	case ECharacterEmoteState::Victory:
		TargetMontage = VictoryMontage ? VictoryMontage : CorrectMontage;
		break;
	case ECharacterEmoteState::Defeat:
		TargetMontage = DefeatMontage ? DefeatMontage : WrongMontage;
		break;
	case ECharacterEmoteState::Pointing:
		TargetMontage = PointingMontage;
		break;
	case ECharacterEmoteState::Thinking:
		TargetMontage = ThinkingMontage;
		break;
	case ECharacterEmoteState::SubmitAnswer:
		TargetMontage = SubmitMontage;
		break;
	case ECharacterEmoteState::CorrectAnswer:
		TargetMontage = CorrectMontage ? CorrectMontage : VictoryMontage;
		break;
	case ECharacterEmoteState::WrongAnswer:
		TargetMontage = WrongMontage ? WrongMontage : DefeatMontage;
		break;
	case ECharacterEmoteState::GuessSuccess:
		TargetMontage = GuessSuccessMontage;
		break;
	default:
		break;
	}

	if (TargetMontage)
	{
		AnimInst->Montage_Play(TargetMontage);
	}
}

void ATOCharacter::Server_UpdateHeadRotation_Implementation(const FRotator& NewHeadRotation)
{
	ReplicatedHeadRotation = NewHeadRotation;
}
