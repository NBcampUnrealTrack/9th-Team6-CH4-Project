// Fill out your copyright notice in the Description page of Project Settings.

#include "TOCharacter.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "../GameManager/TOPlayerController.h"
#include "../GameManager/TOPlayerState.h"
#include "EnhancedInputComponent.h"
#include "InputCoreTypes.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/WidgetComponent.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "TimerManager.h"


ATOCharacter::ATOCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bIsSeated = true;
}

void ATOCharacter::InitializeDefaultCustomizationMeshes()
{
	if (HairMeshList.Num() == 0)
	{
		static const TCHAR* HairPaths[] = {
			TEXT("/Game/Sample/Meshes/SK_Hair_1.SK_Hair_1"),
			TEXT("/Game/Sample/Meshes/SK_Hair_2.SK_Hair_2")
		};
		for (const TCHAR* P : HairPaths)
		{
			if (USkeletalMesh* MeshObj = LoadObject<USkeletalMesh>(nullptr, P))
			{
				HairMeshList.Add(MeshObj);
			}
		}
	}

	if (TopMeshList.Num() == 0)
	{
		static const TCHAR* TopPaths[] = {
			TEXT("/Game/Sample/Meshes/SK_Top_1.SK_Top_1"),
			TEXT("/Game/Sample/Meshes/SK_Top_2.SK_Top_2"),
			TEXT("/Game/Sample/Meshes/SK_Top_3.SK_Top_3"),
			TEXT("/Game/Sample/Meshes/SK_Top_4.SK_Top_4")
		};
		for (const TCHAR* P : TopPaths)
		{
			if (USkeletalMesh* MeshObj = LoadObject<USkeletalMesh>(nullptr, P))
			{
				TopMeshList.Add(MeshObj);
			}
		}
	}

	if (BottomMeshList.Num() == 0)
	{
		static const TCHAR* BottomPaths[] = {
			TEXT("/Game/Sample/Meshes/SK_Pants.SK_Pants"),
			TEXT("/Game/Sample/Meshes/SK_Shorts.SK_Shorts"),
			TEXT("/Game/Sample/Meshes/SK_Underwear.SK_Underwear")
		};
		for (const TCHAR* P : BottomPaths)
		{
			if (USkeletalMesh* MeshObj = LoadObject<USkeletalMesh>(nullptr, P))
			{
				BottomMeshList.Add(MeshObj);
			}
		}
	}
}

USkeletalMeshComponent* ATOCharacter::FindModularComponent(const FName& CompName) const
{
	TArray<USkeletalMeshComponent*> SkelComps;
	GetComponents<USkeletalMeshComponent>(SkelComps);

	// 1. Exact name match first
	for (USkeletalMeshComponent* Comp : SkelComps)
	{
		if (Comp && Comp != GetMesh())
		{
			const FString CompStr = Comp->GetName();
			if (CompStr.Equals(CompName.ToString(), ESearchCase::IgnoreCase))
			{
				return Comp;
			}
		}
	}

	// 2. Contains match (e.g. Hair_GEN_VARIABLE, Hair_C_1)
	for (USkeletalMeshComponent* Comp : SkelComps)
	{
		if (Comp && Comp != GetMesh())
		{
			const FString CompStr = Comp->GetName();
			if (CompStr.Contains(CompName.ToString(), ESearchCase::IgnoreCase))
			{
				return Comp;
			}
		}
	}
	return nullptr;
}

void ATOCharacter::BeginPlay()
{
	Super::BeginPlay();

	InitializeDefaultCustomizationMeshes();
	SetupLeaderPoseComponents();
	SetSeatedState(true);

	if (ATOPlayerState* TOPS = GetPlayerState<ATOPlayerState>())
	{
		ApplyCustomization(TOPS->SelectedHairIndex, TOPS->SelectedTopIndex, TOPS->SelectedBottomIndex);
		if (!TOPS->GetCustomPlayerName().IsEmpty())
		{
			UpdateNameTagWidget(TOPS->GetCustomPlayerName());
		}
	}
	else
	{
		ApplyCustomization(RepHairIndex, RepTopIndex, RepBottomIndex);
		if (!RepPlayerName.IsEmpty())
		{
			UpdateNameTagWidget(RepPlayerName);
		}
	}
}

void ATOCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	if (ATOPlayerController* TOPC = Cast<ATOPlayerController>(NewController))
	{
		if (TOPC->bHasCachedCustomization)
		{
			ApplyCustomization(TOPC->CachedHairIndex, TOPC->CachedTopIndex, TOPC->CachedBottomIndex);
		}
		if (TOPC->bHasCachedPlayerName)
		{
			UpdateNameTagWidget(TOPC->CachedPlayerName);
		}
	}

	if (ATOPlayerState* TOPS = GetPlayerState<ATOPlayerState>())
	{
		ApplyCustomization(TOPS->SelectedHairIndex, TOPS->SelectedTopIndex, TOPS->SelectedBottomIndex);
		if (!TOPS->GetCustomPlayerName().IsEmpty())
		{
			UpdateNameTagWidget(TOPS->GetCustomPlayerName());
		}
	}
}

void ATOCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	if (ATOPlayerState* TOPS = GetPlayerState<ATOPlayerState>())
	{
		ApplyCustomization(TOPS->SelectedHairIndex, TOPS->SelectedTopIndex, TOPS->SelectedBottomIndex);
		if (!TOPS->GetCustomPlayerName().IsEmpty())
		{
			UpdateNameTagWidget(TOPS->GetCustomPlayerName());
		}
	}
}


void ATOCharacter::ApplyCustomization_Implementation(int32 InHairIndex, int32 InTopIndex, int32 InBottomIndex)
{
	InitializeDefaultCustomizationMeshes();

	CurrentHairIndex = InHairIndex;
	CurrentTopIndex = InTopIndex;
	CurrentBottomIndex = InBottomIndex;

	if (HasAuthority())
	{
		RepHairIndex = InHairIndex;
		RepTopIndex = InTopIndex;
		RepBottomIndex = InBottomIndex;
	}

	USkeletalMeshComponent* MainMesh = GetMesh();
	if (!MainMesh) return;

	// 1. 헤어 메시 교체
	if (USkeletalMeshComponent* HairComp = FindModularComponent(TEXT("Hair")))
	{
		if (HairMeshList.IsValidIndex(CurrentHairIndex) && HairMeshList[CurrentHairIndex])
		{
			HairComp->SetSkeletalMeshAsset(HairMeshList[CurrentHairIndex]);
		}
		HairComp->SetLeaderPoseComponent(MainMesh);
	}

	// 2. 상의(Top) 메시 교체
	if (USkeletalMeshComponent* TopComp = FindModularComponent(TEXT("Top")))
	{
		if (TopMeshList.IsValidIndex(CurrentTopIndex) && TopMeshList[CurrentTopIndex])
		{
			TopComp->SetSkeletalMeshAsset(TopMeshList[CurrentTopIndex]);
		}
		TopComp->SetLeaderPoseComponent(MainMesh);
	}

	// 3. 하의(Bottom) 메시 교체
	if (USkeletalMeshComponent* BottomComp = FindModularComponent(TEXT("Bottom")))
	{
		if (BottomMeshList.IsValidIndex(CurrentBottomIndex) && BottomMeshList[CurrentBottomIndex])
		{
			BottomComp->SetSkeletalMeshAsset(BottomMeshList[CurrentBottomIndex]);
		}
		BottomComp->SetLeaderPoseComponent(MainMesh);
	}

	SetupLeaderPoseComponents();
	UE_LOG(LogTemp, Log, TEXT("[TOCharacter] Applied customization: Hair=%d, Top=%d, Bottom=%d"),
		CurrentHairIndex, CurrentTopIndex, CurrentBottomIndex);
}

void ATOCharacter::OnRep_CharacterCustomization()
{
	ApplyCustomization(RepHairIndex, RepTopIndex, RepBottomIndex);
}

void ATOCharacter::AddMovementInput(FVector WorldDirection, float ScaleValue, bool bForce)
{
	if (bIsSeated)
	{
		// 착석 중에는 WASD 이동을 완전히 무시하여 게스트가 의자에서 미끄러지는 버그 방지
		return;
	}

	Super::AddMovementInput(WorldDirection, ScaleValue, bForce);
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
	// B 키 입력 시 TogglePerspective() 호출 (시점 전환)
	PlayerInputComponent->BindKey(EKeys::B, IE_Pressed, this, &ATOCharacter::TogglePerspective);
}

void ATOCharacter::RequestToggleHUD()
{
	if (ATOPlayerController* PC = Cast<ATOPlayerController>(GetController()))
	{
		PC->ToggleHUD();
	}
}

void ATOCharacter::TogglePerspective_Implementation()
{
	bIsThirdPerson = !bIsThirdPerson;

	USpringArmComponent* SpringArm = FindComponentByClass<USpringArmComponent>();
	if (SpringArm)
	{
		if (bIsThirdPerson)
		{
			SpringArm->TargetArmLength = 300.0f;
			SpringArm->bUsePawnControlRotation = true;
			SpringArm->bInheritPitch = true;
			SpringArm->bInheritYaw = true;
			SpringArm->bInheritRoll = false;

			if (APlayerController* PC = Cast<APlayerController>(GetController()))
			{
				if (PC->PlayerCameraManager)
				{
					PC->PlayerCameraManager->ViewPitchMin = -80.0f;
					PC->PlayerCameraManager->ViewPitchMax = 80.0f;
					PC->PlayerCameraManager->ViewYawMin = 0.0f;
					PC->PlayerCameraManager->ViewYawMax = 359.999f;
				}
			}
		}
		else
		{
			SpringArm->TargetArmLength = 0.0f;
			SpringArm->bUsePawnControlRotation = true;

			if (APlayerController* PC = Cast<APlayerController>(GetController()))
			{
				if (PC->PlayerCameraManager)
				{
					const float ActorYaw = GetActorRotation().Yaw;
					PC->PlayerCameraManager->ViewPitchMin = -40.0f;
					PC->PlayerCameraManager->ViewPitchMax = 30.0f;
					PC->PlayerCameraManager->ViewYawMin = ActorYaw - 70.0f;
					PC->PlayerCameraManager->ViewYawMax = ActorYaw + 70.0f;
				}
			}
		}
	}
}

void ATOCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ATOCharacter, bIsSeated);
	DOREPLIFETIME(ATOCharacter, RepHairIndex);
	DOREPLIFETIME(ATOCharacter, RepTopIndex);
	DOREPLIFETIME(ATOCharacter, RepBottomIndex);
	DOREPLIFETIME(ATOCharacter, RepPlayerName);
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
			GetCharacterMovement()->DisableMovement();
			GetCharacterMovement()->SetMovementMode(MOVE_None);
			GetCharacterMovement()->Velocity = FVector::ZeroVector;
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

void ATOCharacter::OnRep_RepPlayerName()
{
	UpdateNameTagWidget(RepPlayerName);
}

void ATOCharacter::UpdateNameTagWidget(const FString& NewPlayerName)
{
	if (NewPlayerName.IsEmpty()) return;

	if (HasAuthority())
	{
		RepPlayerName = NewPlayerName;
	}

	// 1. NameTag WidgetComponent 검색
	UWidgetComponent* NameTagComp = nullptr;
	TArray<UWidgetComponent*> WidgetComps;
	GetComponents<UWidgetComponent>(WidgetComps);
	for (UWidgetComponent* Comp : WidgetComps)
	{
		if (Comp && Comp->GetName().Contains(TEXT("NameTag"), ESearchCase::IgnoreCase))
		{
			NameTagComp = Comp;
			break;
		}
	}
	if (!NameTagComp && WidgetComps.Num() > 0)
	{
		NameTagComp = WidgetComps[0];
	}

	if (!NameTagComp) return;

	// 2. UserWidgetObject 획득
	UUserWidget* UserWidget = NameTagComp->GetUserWidgetObject();
	if (!UserWidget)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				NameTagRetryTimerHandle,
				this,
				&ATOCharacter::RetryUpdateNameTagWidget,
				0.1f,
				false
			);
		}
		return;
	}

	// 3. Txt_PlayerName 텍스트 블록에 직접 설정
	if (UTextBlock* NameText = Cast<UTextBlock>(UserWidget->GetWidgetFromName(FName(TEXT("Txt_PlayerName")))))
	{
		NameText->SetText(FText::FromString(NewPlayerName));
		UE_LOG(LogTemp, Log, TEXT("[TOCharacter] Set NameTag Text directly: %s"), *NewPlayerName);
	}

	// 4. 위젯에 SetPlayerName 이벤트가 존재하면 호출
	if (UFunction* SetNameFunc = UserWidget->FindFunction(FName(TEXT("SetPlayerName"))))
	{
		FProperty* ParamProp = SetNameFunc->FindPropertyByName(FName(TEXT("NewName")));
		if (!ParamProp)
		{
			ParamProp = SetNameFunc->FindPropertyByName(FName(TEXT("InText")));
		}

		if (ParamProp && ParamProp->IsA<FStrProperty>())
		{
			struct { FString StrVal; } Params;
			Params.StrVal = NewPlayerName;
			UserWidget->ProcessEvent(SetNameFunc, &Params);
		}
		else
		{
			struct { FText TextVal; } Params;
			Params.TextVal = FText::FromString(NewPlayerName);
			UserWidget->ProcessEvent(SetNameFunc, &Params);
		}
	}
}

void ATOCharacter::RetryUpdateNameTagWidget()
{
	if (!RepPlayerName.IsEmpty())
	{
		UpdateNameTagWidget(RepPlayerName);
	}
	else if (ATOPlayerState* TOPS = GetPlayerState<ATOPlayerState>())
	{
		if (!TOPS->GetCustomPlayerName().IsEmpty())
		{
			UpdateNameTagWidget(TOPS->GetCustomPlayerName());
		}
	}
}

