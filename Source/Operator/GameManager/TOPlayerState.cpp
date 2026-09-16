#include "TOPlayerState.h"
#include "TOPlayerController.h"
#include "Net/UnrealNetwork.h"
#include "../Character/TOCharacter.h"


ATOPlayerState::ATOPlayerState()
{
	bReplicates = true;
	AssignedPlayerIndex = -1;
	PlayerAlphabet = TEXT("");
	PlayerScore = 0;
	SelectedHairIndex = 0;
	SelectedTopIndex = 0;
	SelectedBottomIndex = 0;
}


void ATOPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 서버에서 값이 바뀌면 클라이언트들에게도 동기화되도록 설정
	DOREPLIFETIME(ATOPlayerState, SelectedCharacterID);
	DOREPLIFETIME(ATOPlayerState, bIsReadyToPlay);
	
	DOREPLIFETIME(ATOPlayerState, AssignedPlayerIndex);
	DOREPLIFETIME(ATOPlayerState, PlayerAlphabet);
	DOREPLIFETIME(ATOPlayerState, PlayerScore);
	DOREPLIFETIME(ATOPlayerState, PlayerNameString);

	DOREPLIFETIME(ATOPlayerState, SelectedHairIndex);
	DOREPLIFETIME(ATOPlayerState, SelectedTopIndex);
	DOREPLIFETIME(ATOPlayerState, SelectedBottomIndex);
}


void ATOPlayerState::AddScorePoints(int32 InScore)
{
	if (HasAuthority()) // 서버에서만 수정 가능
	{
		PlayerScore += InScore;
		SetScore(PlayerScore); // APlayerState 기본 제공 Score 변수도 동기화

		if (UWorld* World = GetWorld())
		{
			if (ATOPlayerController* LocalPC = Cast<ATOPlayerController>(World->GetFirstPlayerController()))
			{
				LocalPC->UpdateScoreBoardUI();
			}
		}
	}
}

void ATOPlayerState::OnRep_PlayerScore()
{
	if (UWorld* World = GetWorld())
	{
		if (ATOPlayerController* LocalPC = Cast<ATOPlayerController>(World->GetFirstPlayerController()))
		{
			LocalPC->UpdateScoreBoardUI();
		}
	}
}

void ATOPlayerState::OnRep_Score()
{
	Super::OnRep_Score();
	if (UWorld* World = GetWorld())
	{
		if (ATOPlayerController* LocalPC = Cast<ATOPlayerController>(World->GetFirstPlayerController()))
		{
			LocalPC->UpdateScoreBoardUI();
		}
	}
}

void ATOPlayerState::OnRep_PlayerName()
{
	Super::OnRep_PlayerName();
	if (UWorld* World = GetWorld())
	{
		if (ATOPlayerController* LocalPC = Cast<ATOPlayerController>(World->GetFirstPlayerController()))
		{
			LocalPC->UpdateScoreBoardUI();
		}
	}
}

void ATOPlayerState::Server_SetPlayerName_Implementation(const FString& NewName)
{
	SetPlayerNameString(NewName);
}

void ATOPlayerState::SetPlayerNameString(const FString& NewName)
{
	PlayerNameString = NewName;
	SetPlayerName(NewName);

	if (APawn* TargetPawn = GetPawn())
	{
		if (ATOCharacter* Char = Cast<ATOCharacter>(TargetPawn))
		{
			Char->UpdateNameTagWidget(NewName);
		}
	}

	if (UWorld* World = GetWorld())
	{
		if (ATOPlayerController* LocalPC = Cast<ATOPlayerController>(World->GetFirstPlayerController()))
		{
			LocalPC->UpdateScoreBoardUI();
		}
	}
}

// [클라이언트에서 실행] 서버로부터 닉네임 값이 동기화되면 자동으로 실행
void ATOPlayerState::OnRep_PlayerNameString()
{
	UE_LOG(LogTemp, Log, TEXT("PlayerName Replicated: %s"), *PlayerNameString);

	if (APawn* TargetPawn = GetPawn())
	{
		if (ATOCharacter* Char = Cast<ATOCharacter>(TargetPawn))
		{
			Char->UpdateNameTagWidget(PlayerNameString);
		}
	}

	if (UWorld* World = GetWorld())
	{
		if (ATOPlayerController* LocalPC = Cast<ATOPlayerController>(World->GetFirstPlayerController()))
		{
			LocalPC->UpdateScoreBoardUI();
		}
	}
}


void ATOPlayerState::Server_SetCustomization_Implementation(int32 InHairIndex, int32 InTopIndex, int32 InBottomIndex)
{
	SetCustomization(InHairIndex, InTopIndex, InBottomIndex);
}

void ATOPlayerState::SetCustomization(int32 InHairIndex, int32 InTopIndex, int32 InBottomIndex)
{
	SelectedHairIndex = InHairIndex;
	SelectedTopIndex = InTopIndex;
	SelectedBottomIndex = InBottomIndex;

	OnPlayerCustomizationChanged.Broadcast(SelectedHairIndex, SelectedTopIndex, SelectedBottomIndex);

	if (APawn* TargetPawn = GetPawn())
	{
		if (ATOCharacter* Char = Cast<ATOCharacter>(TargetPawn))
		{
			Char->ApplyCustomization(SelectedHairIndex, SelectedTopIndex, SelectedBottomIndex);
		}
	}
}

void ATOPlayerState::OnRep_Customization()
{
	OnPlayerCustomizationChanged.Broadcast(SelectedHairIndex, SelectedTopIndex, SelectedBottomIndex);

	if (APawn* TargetPawn = GetPawn())
	{
		if (ATOCharacter* Char = Cast<ATOCharacter>(TargetPawn))
		{
			Char->ApplyCustomization(SelectedHairIndex, SelectedTopIndex, SelectedBottomIndex);
		}
	}
}