// Fill out your copyright notice in the Description page of Project Settings.

#include "TOCharacter.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"

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
}

void ATOCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

void ATOCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ATOCharacter, bIsSeated);
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
	if (HasAuthority())
	{
		Multicast_PlayEmote(EmoteState);
	}
	else
	{
		Multicast_PlayEmote(EmoteState);
	}
}

void ATOCharacter::Multicast_PlayEmote_Implementation(ECharacterEmoteState EmoteState)
{
	UAnimInstance* AnimInst = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (!AnimInst) return;

	UAnimMontage* TargetMontage = nullptr;
	switch (EmoteState)
	{
	case ECharacterEmoteState::Thinking:
		TargetMontage = ThinkingMontage;
		break;
	case ECharacterEmoteState::SubmitAnswer:
		TargetMontage = SubmitMontage;
		break;
	case ECharacterEmoteState::CorrectAnswer:
		TargetMontage = CorrectMontage;
		break;
	case ECharacterEmoteState::WrongAnswer:
		TargetMontage = WrongMontage;
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
